#include "umac/ap/umac_ap.h"
#include "umac/ba/umac_ba_data.h"
#include "umac/data/umac_data.h"
#include "umac/datapath/umac_datapath_data.h"
#include "umac/rc/umac_rc.h"
#include "mmdrv.h"
#include "driver/driver.h"
#include "driver/morse_driver/mm6108/pageset.h"
#include "mmosal.h"

#define AP_RC_STATS_MAX_AID (8)

/* ponytail: reads the station's rc table off the umac thread, so a disassociation mid-read can fault; route through a umac event if this outlives the bench */
static struct umac_sta_data *first_station(void)
{
    struct umac_data *umacd = umac_data_get_umacd();
    for (uint16_t aid = 1; aid < AP_RC_STATS_MAX_AID; aid++)
    {
        struct umac_sta_data *stad = umac_ap_lookup_sta_by_aid(umacd, aid);
        if (stad != NULL)
        {
            return stad;
        }
    }
    return NULL;
}

struct mmwlan_rc_stats *mrover_ap_rc_stats(void)
{
    struct umac_sta_data *stad = first_station();
    return stad == NULL ? NULL : umac_rc_get_rc_stats(stad);
}

int mrover_ap_ba_status(void)
{
    struct umac_sta_data *stad = first_station();
    if (stad == NULL)
    {
        return -1;
    }
    const struct umac_ba_session *session = &umac_sta_data_get_ba(stad)->sessions.originator[0];
    /* status in the tens, negotiated window in the units and up, e.g. 316 is success with 16 frames */
    return session->status * 100 + session->buffer_size;
}

enum pause_source
{
    PAUSE_SOURCE_PKTMEM,
    PAUSE_SOURCE_TRAFFIC_CTRL,
    PAUSE_SOURCE_COUNT
};

struct pause_tracker
{
    bool paused;
    uint32_t paused_since_ms;
    uint32_t pauses;
    uint32_t paused_ms;
};

static struct pause_tracker pause_trackers[PAUSE_SOURCE_COUNT];

void mrover_tx_queue_depths(uint32_t *station_queue, uint32_t *status_queue, uint32_t *waiting_for_pages,
                            uint32_t *awaiting_status);

/* queue depths summed over every pool pause in the window, to see where the full pool's frames sit */
static uint32_t depth_at_pause_sums[4];
static uint32_t depth_at_pause_samples;

static void track_pause(uint16_t sources_mask, bool paused)
{
    const uint32_t now_ms = mmosal_get_time_ms();
    MMOSAL_TASK_ENTER_CRITICAL();
    for (unsigned source = 0; source < PAUSE_SOURCE_COUNT; source++)
    {
        struct pause_tracker *tracker = &pause_trackers[source];
        if ((sources_mask & (1U << source)) == 0 || tracker->paused == paused)
        {
            continue;
        }
        if (paused)
        {
            tracker->pauses++;
            tracker->paused_since_ms = now_ms;
        }
        else
        {
            tracker->paused_ms += now_ms - tracker->paused_since_ms;
        }
        tracker->paused = paused;
    }
    MMOSAL_TASK_EXIT_CRITICAL();
}

void __real_mmdrv_host_set_tx_paused(uint16_t sources_mask, bool paused);
void __real_mmdrv_host_update_tx_paused(uint16_t sources_mask, mmdrv_host_update_tx_paused_cb_t cb);

void __wrap_mmdrv_host_set_tx_paused(uint16_t sources_mask, bool paused)
{
    track_pause(sources_mask, paused);
    __real_mmdrv_host_set_tx_paused(sources_mask, paused);
}

void __wrap_mmdrv_host_update_tx_paused(uint16_t sources_mask, mmdrv_host_update_tx_paused_cb_t cb)
{
    const bool paused = cb();
    if (paused)
    {
        uint32_t depths[4];
        mrover_tx_queue_depths(&depths[0], &depths[1], &depths[2], &depths[3]);
        MMOSAL_TASK_ENTER_CRITICAL();
        for (unsigned i = 0; i < 4; i++)
        {
            depth_at_pause_sums[i] += depths[i];
        }
        depth_at_pause_samples++;
        MMOSAL_TASK_EXIT_CRITICAL();
    }
    track_pause(sources_mask, paused);
    __real_mmdrv_host_update_tx_paused(sources_mask, cb);
}

/* pauses and paused ms per source since the last call, pktmem first then chip traffic control */
void mrover_pause_stats(uint32_t pauses[2], uint32_t paused_ms[2])
{
    const uint32_t now_ms = mmosal_get_time_ms();
    MMOSAL_TASK_ENTER_CRITICAL();
    for (unsigned source = 0; source < PAUSE_SOURCE_COUNT; source++)
    {
        struct pause_tracker *tracker = &pause_trackers[source];
        if (tracker->paused)
        {
            tracker->paused_ms += now_ms - tracker->paused_since_ms;
            tracker->paused_since_ms = now_ms;
        }
        pauses[source] = tracker->pauses;
        paused_ms[source] = tracker->paused_ms;
        tracker->pauses = 0;
        tracker->paused_ms = 0;
    }
    MMOSAL_TASK_EXIT_CRITICAL();
}

struct driver_data *mrover_driver_data(void);

/* frames in the umac station queue, in the umac tx status queue, queued for chip pages, and on the chip awaiting tx status */
void mrover_tx_queue_depths(uint32_t *station_queue, uint32_t *status_queue, uint32_t *waiting_for_pages,
                            uint32_t *awaiting_status)
{
    struct umac_sta_data *stad = first_station();
    *station_queue = stad == NULL ? 0 : umac_sta_data_get_queued_len(stad);
    *status_queue = umac_data_get_datapath(umac_data_get_umacd())->tx_status_q.len;

    struct morse_pageset *pageset = mrover_driver_data()->chip_if->to_chip_pageset;
    uint32_t waiting = 0;
    uint32_t pending = 0;
    if (pageset != NULL)
    {
        for (unsigned aci = 0; aci < PAGESET_TX_SKBQ_MAX; aci++)
        {
            waiting += pageset->data_qs[aci].skbq.len;
            pending += pageset->data_qs[aci].pending.len;
        }
    }
    *waiting_for_pages = waiting;
    *awaiting_status = pending;
}

/* mean queue depths at the moment of each pool pause since the last call */
void mrover_depths_at_pause(uint32_t depths[4])
{
    MMOSAL_TASK_ENTER_CRITICAL();
    for (unsigned i = 0; i < 4; i++)
    {
        depths[i] = depth_at_pause_samples == 0 ? 0 : depth_at_pause_sums[i] / depth_at_pause_samples;
        depth_at_pause_sums[i] = 0;
    }
    depth_at_pause_samples = 0;
    MMOSAL_TASK_EXIT_CRITICAL();
}
