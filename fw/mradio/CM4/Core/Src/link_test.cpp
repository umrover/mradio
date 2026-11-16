#include "link_test.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "FreeRTOS.h"
#include "lwip/def.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "mmosal.h"
#include "mmping.h"
#include "mmpkt.h"
#include "mmwlan.h"
#include "stm32h7xx.h"
#include "task.h"

#ifndef LINK_TEST_RATE_KBPS
#define LINK_TEST_RATE_KBPS 0
#endif
#ifndef LINK_TEST_PING
#define LINK_TEST_PING 1
#endif
#ifndef LINK_TEST_RX_HOOK
#error LINK_TEST_RX_HOOK must name the morselib rx frame function wrapped with -Wl,--wrap
#endif
#define LINK_TEST_PASTE(prefix, name) prefix##name
#define LINK_TEST_WRAP(name) LINK_TEST_PASTE(__wrap_, name)
#define LINK_TEST_REAL(name) LINK_TEST_PASTE(__real_, name)

// Mirrors the leading fields of the internal struct in morselib/src/internal/mmdrv.h for SDK 2.14.3
struct mmdrv_rx_metadata {
    int16_t rssi;
    uint16_t freq_100khz;
    uint8_t bw_mhz;
    uint8_t flags;
    int8_t noise_dbm;
};

namespace {
std::atomic<int32_t> rssi_sum{0};
std::atomic<uint32_t> rssi_frames{0};
std::atomic<int32_t> noise_sum{0};
std::atomic<uint32_t> rx_frames{0};
} // namespace

extern "C" {
void LINK_TEST_REAL(LINK_TEST_RX_HOOK)(mmpkt *rx_frame, uint16_t channel);

// runs on the morselib driver task for every frame the chip hands up
void LINK_TEST_WRAP(LINK_TEST_RX_HOOK)(mmpkt *rx_frame, uint16_t channel) {
    const mmdrv_rx_metadata *const metadata = mmpkt_get_metadata(rx_frame).rx;
    if (metadata != nullptr) {
        // a saturated receiver reports rssi 0, so 0 is treated as unmeasured
        if (metadata->rssi < 0) {
            rssi_sum.fetch_add(metadata->rssi, std::memory_order_relaxed);
            rssi_frames.fetch_add(1, std::memory_order_relaxed);
        }
        noise_sum.fetch_add(metadata->noise_dbm, std::memory_order_relaxed);
        rx_frames.fetch_add(1, std::memory_order_relaxed);
    }
    LINK_TEST_REAL(LINK_TEST_RX_HOOK)(rx_frame, channel);
}

// the latest window report, so a bench script can read it over SWD when the console is unavailable
char link_test_report[320] = {};

// mmwlan_get_rc_stats only covers the station side, so an AP build supplies its own lookup
__attribute__((weak)) mmwlan_rc_stats *mrover_ap_rc_stats(void);
// block ack originator state for tid 0, -1 where morselib internals are not linked in
__attribute__((weak)) int mrover_ap_ba_status(void);
// tx pauses and paused ms per window for the packet pool and the chip's traffic control
__attribute__((weak)) void mrover_pause_stats(uint32_t pauses[2], uint32_t paused_ms[2]);
// frames queued in the driver for chip pages and frames on the chip awaiting tx status
__attribute__((weak)) void mrover_tx_queue_depths(uint32_t *station_queue, uint32_t *status_queue,
                                                  uint32_t *waiting_for_pages, uint32_t *awaiting_status);
__attribute__((weak)) void mrover_depths_at_pause(uint32_t depths[4]);
}

namespace mrover::link_test {
namespace {
constexpr uint16_t FLOOD_PORT = 5001;
constexpr uint16_t FLOOD_PAYLOAD = 1460;
constexpr uint32_t FLOOD_READY_TIMEOUT_MS = 100;
constexpr uint32_t WINDOW_MS = 5000;
constexpr uint32_t QUEUE_SAMPLE_MS = 10;
constexpr uint32_t PING_INTERVAL_MS = 1000;
constexpr unsigned TASK_STACK_WORDS = 1024;
// a backward jump larger than this means the sender restarted its session and ids began again at 0
constexpr int64_t REORDER_WINDOW = 64;
constexpr size_t MAX_RATES = 64;

char local_addr[16] = {};
char peer_addr[16] = {};
std::atomic<bool> peer_up{false};

std::atomic<uint32_t> rx_bytes{0};
std::atomic<uint32_t> rx_packets{0};
std::atomic<uint32_t> lost_packets{0};
std::atomic<uint32_t> out_of_order_packets{0};
std::atomic<uint32_t> tx_bytes{0};
std::atomic<uint32_t> tx_stalls{0};
std::atomic<uint32_t> tx_errors{0};

// only touched on the tcpip thread
bool synced = false;
int64_t expected_id = 0;

void track_sequence(int64_t id) {
    if (!synced) {
        synced = true;
    } else if (id > expected_id) {
        lost_packets.fetch_add(static_cast<uint32_t>(id - expected_id), std::memory_order_relaxed);
    } else if (id < expected_id && expected_id - id <= REORDER_WINDOW) {
        out_of_order_packets.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    expected_id = id + 1;
}

void on_flood_packet(void * /*arg*/, udp_pcb * /*pcb*/, pbuf *packet, const ip_addr_t * /*addr*/,
                     u16_t /*port*/) {
    rx_bytes.fetch_add(packet->tot_len, std::memory_order_relaxed);
    rx_packets.fetch_add(1, std::memory_order_relaxed);

    uint32_t id_network_order = 0;
    if (pbuf_copy_partial(packet, &id_network_order, sizeof id_network_order, 0) == sizeof id_network_order) {
        // iperf senders mark the final datagram of a session by negating its id
        int64_t id = static_cast<int32_t>(lwip_ntohl(id_network_order));
        if (id < 0) {
            id = -id;
        }
        track_sequence(id);
    }
    pbuf_free(packet);
}

void flood(void * /*arg*/) {
    ip_addr_t peer{};
    if (ipaddr_aton(peer_addr, &peer) == 0) {
        std::printf("link test: bad peer %s\n", peer_addr);
        return;
    }
    LOCK_TCPIP_CORE();
    udp_pcb *const pcb = udp_new();
    UNLOCK_TCPIP_CORE();
    if (pcb == nullptr) {
        std::printf("link test: flood pcb alloc failed\n");
        return;
    }

    uint32_t next_id = 0;
    // token bucket in bytes, refilled per elapsed ms and capped at 4 frames so a stall cannot bank a burst
    uint32_t budget_bytes = 0;
    uint32_t refilled_at_ms = mmosal_get_time_ms();
    for (;;) {
        if (!peer_up.load(std::memory_order_relaxed)) {
            mmosal_task_sleep(100);
            continue;
        }
        if (LINK_TEST_RATE_KBPS != 0) {
            const uint32_t now_ms = mmosal_get_time_ms();
            const uint64_t refill = uint64_t{now_ms - refilled_at_ms} * LINK_TEST_RATE_KBPS / 8;
            refilled_at_ms = now_ms;
            const uint64_t topped_up = budget_bytes + refill;
            budget_bytes = static_cast<uint32_t>(topped_up < 4U * FLOOD_PAYLOAD ? topped_up : 4U * FLOOD_PAYLOAD);
            if (budget_bytes < FLOOD_PAYLOAD) {
                mmosal_task_sleep(1);
                continue;
            }
            budget_bytes -= FLOOD_PAYLOAD;
        }
        // waiting here instead of inside mmnetif_tx keeps the tcpip core lock free for rx
        if (mmwlan_tx_wait_until_ready(FLOOD_READY_TIMEOUT_MS) != MMWLAN_SUCCESS) {
            tx_stalls.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        err_t err = ERR_MEM;
        LOCK_TCPIP_CORE();
        pbuf *const packet = pbuf_alloc(PBUF_TRANSPORT, FLOOD_PAYLOAD, PBUF_RAM);
        if (packet != nullptr) {
            std::memset(packet->payload, 0, FLOOD_PAYLOAD);
            const uint32_t id_network_order = lwip_htonl(next_id);
            std::memcpy(packet->payload, &id_network_order, sizeof id_network_order);
            err = udp_sendto(pcb, packet, &peer, FLOOD_PORT);
            pbuf_free(packet);
        }
        UNLOCK_TCPIP_CORE();
        if (err == ERR_OK) {
            tx_bytes.fetch_add(FLOOD_PAYLOAD, std::memory_order_relaxed);
            ++next_id;
        } else {
            tx_errors.fetch_add(1, std::memory_order_relaxed);
            mmosal_task_sleep(1);
        }
    }
}

struct RateWindow {
    uint32_t busiest_info = 0;
    uint32_t busiest_sent = 0;
    uint32_t sent = 0;
    uint32_t success = 0;
};

// cumulative rc counters from the previous window, reset when the rate table changes size
std::array<uint32_t, MAX_RATES> previous_sent{};
std::array<uint32_t, MAX_RATES> previous_success{};
uint32_t previous_entries = 0;

RateWindow sample_rates() {
    RateWindow window{};
    mmwlan_rc_stats *stats = mmwlan_get_rc_stats();
    if (stats == nullptr && mrover_ap_rc_stats != nullptr) {
        stats = mrover_ap_rc_stats();
    }
    if (stats == nullptr) {
        previous_entries = 0;
        return window;
    }
    const uint32_t entries = stats->n_entries < MAX_RATES ? stats->n_entries : MAX_RATES;
    const bool same_table = entries == previous_entries;
    for (uint32_t i = 0; i < entries; ++i) {
        const uint32_t sent = stats->total_sent[i] - (same_table ? previous_sent[i] : 0);
        const uint32_t success = stats->total_success[i] - (same_table ? previous_success[i] : 0);
        window.sent += sent;
        window.success += success;
        if (sent > window.busiest_sent) {
            window.busiest_sent = sent;
            window.busiest_info = stats->rate_info[i];
        }
        previous_sent[i] = stats->total_sent[i];
        previous_success[i] = stats->total_success[i];
    }
    previous_entries = entries;
    mmwlan_free_rc_stats(stats);
    return window;
}

void start_ping_window() {
    if (LINK_TEST_PING == 0) {
        return;
    }
    mmping_args args = MMPING_ARGS_DEFAULT;
    std::strncpy(args.ping_src, local_addr, sizeof args.ping_src - 1);
    std::strncpy(args.ping_target, peer_addr, sizeof args.ping_target - 1);
    args.ping_interval_ms = PING_INTERVAL_MS;
    args.ping_count = WINDOW_MS / PING_INTERVAL_MS;
    static_cast<void>(mmping_start(&args));
}

void report_windows(void * /*arg*/) {
    uint32_t previous_cycles = DWT->CYCCNT;
    uint32_t previous_idle = ulTaskGetIdleRunTimeCounter();
    for (;;) {
        start_ping_window();
        std::array<uint64_t, 4> queue_sums{};
        uint32_t queue_samples = 0;
        for (uint32_t elapsed_ms = 0; elapsed_ms < WINDOW_MS; elapsed_ms += QUEUE_SAMPLE_MS) {
            mmosal_task_sleep(QUEUE_SAMPLE_MS);
            if (mrover_tx_queue_depths != nullptr) {
                std::array<uint32_t, 4> depths{};
                mrover_tx_queue_depths(&depths[0], &depths[1], &depths[2], &depths[3]);
                for (size_t i = 0; i < depths.size(); ++i) {
                    queue_sums[i] += depths[i];
                }
                ++queue_samples;
            }
        }
        uint32_t pause_depths[4] = {};
        if (mrover_depths_at_pause != nullptr) {
            mrover_depths_at_pause(pause_depths);
        }
        std::array<unsigned long, 4> queue_depths{};
        for (size_t i = 0; i < queue_depths.size() && queue_samples != 0; ++i) {
            queue_depths[i] = static_cast<unsigned long>(queue_sums[i] / queue_samples);
        }

        struct mmping_stats ping{};
        mmping_stats(&ping);
        mmping_stop();

        const uint64_t received = rx_bytes.exchange(0, std::memory_order_relaxed);
        const uint32_t packets = rx_packets.exchange(0, std::memory_order_relaxed);
        const uint32_t lost = lost_packets.exchange(0, std::memory_order_relaxed);
        const uint32_t out_of_order = out_of_order_packets.exchange(0, std::memory_order_relaxed);
        const uint64_t sent = tx_bytes.exchange(0, std::memory_order_relaxed);
        const uint32_t stalls = tx_stalls.exchange(0, std::memory_order_relaxed);
        const uint32_t errors = tx_errors.exchange(0, std::memory_order_relaxed);

        // the exchanges are not atomic together, so a frame landing between them skews one window by one frame
        const int32_t window_rssi_sum = rssi_sum.exchange(0, std::memory_order_relaxed);
        const uint32_t measured_frames = rssi_frames.exchange(0, std::memory_order_relaxed);
        const int32_t window_noise_sum = noise_sum.exchange(0, std::memory_order_relaxed);
        const uint32_t frames = rx_frames.exchange(0, std::memory_order_relaxed);
        const long rssi_dbm =
            measured_frames == 0 ? 0 : static_cast<long>(window_rssi_sum / static_cast<int32_t>(measured_frames));
        const long noise_dbm = frames == 0 ? 0 : static_cast<long>(window_noise_sum / static_cast<int32_t>(frames));

        // the kernel clocks idle time with DWT cycles, and both counters wrap every 21 s at 200 MHz, which unsigned deltas over a 5 s window survive
        const uint32_t cycles = DWT->CYCCNT;
        const uint32_t idle = ulTaskGetIdleRunTimeCounter();
        const uint32_t window_cycles = cycles - previous_cycles;
        const unsigned long idle_percent =
            window_cycles == 0 ? 0 : static_cast<unsigned long>(uint64_t{idle - previous_idle} * 100 / window_cycles);
        previous_cycles = cycles;
        previous_idle = idle;
        const int ba_status = mrover_ap_ba_status != nullptr ? mrover_ap_ba_status() : -1;
        uint32_t pauses[2] = {};
        uint32_t paused_ms[2] = {};
        if (mrover_pause_stats != nullptr) {
            mrover_pause_stats(pauses, paused_ms);
        }

        const RateWindow rates = sample_rates();
        const unsigned rate_bw_mhz = 1U << (rates.busiest_info & 0xFU);
        const unsigned rate_mcs = (rates.busiest_info >> 4) & 0xFU;
        const char *const rate_gi = ((rates.busiest_info >> 8) & 1U) != 0 ? "sgi" : "lgi";

        std::snprintf(link_test_report, sizeof link_test_report, "t=%lus tx %lu kbps rx %lu kbps pkts %lu lost %lu ooo %lu ping %lu/%lu avg %lu ms "
                    "rssi %ld noise %ld frames %lu rate mcs%u %uMHz %s att %lu ok %lu stall %lu err %lu idle %lu ba %d heap %lu pool %lu/%lums tc %lu/%lums q sta %lu status %lu pages %lu chip %lu at pause %lu/%lu/%lu/%lu\n",
                    static_cast<unsigned long>(mmosal_get_time_ms() / 1000),
                    static_cast<unsigned long>(sent * 8 / WINDOW_MS),
                    static_cast<unsigned long>(received * 8 / WINDOW_MS), static_cast<unsigned long>(packets),
                    static_cast<unsigned long>(lost), static_cast<unsigned long>(out_of_order),
                    static_cast<unsigned long>(ping.ping_recv_count),
                    static_cast<unsigned long>(ping.ping_total_count),
                    static_cast<unsigned long>(ping.ping_avg_time_ms), rssi_dbm, noise_dbm,
                    static_cast<unsigned long>(frames), rate_mcs, rate_bw_mhz, rate_gi,
                    static_cast<unsigned long>(rates.sent), static_cast<unsigned long>(rates.success),
                    static_cast<unsigned long>(stalls), static_cast<unsigned long>(errors), idle_percent, ba_status,
                    static_cast<unsigned long>(xPortGetMinimumEverFreeHeapSize()),
                    static_cast<unsigned long>(pauses[0]), static_cast<unsigned long>(paused_ms[0]),
                    static_cast<unsigned long>(pauses[1]), static_cast<unsigned long>(paused_ms[1]), queue_depths[0],
                    queue_depths[1], queue_depths[2], queue_depths[3], static_cast<unsigned long>(pause_depths[0]),
                    static_cast<unsigned long>(pause_depths[1]), static_cast<unsigned long>(pause_depths[2]),
                    static_cast<unsigned long>(pause_depths[3]));
        std::printf("%s", link_test_report);
    }
}

bool start_flood_receiver() {
    LOCK_TCPIP_CORE();
    udp_pcb *const pcb = udp_new();
    const bool bound = pcb != nullptr && udp_bind(pcb, IP_ANY_TYPE, FLOOD_PORT) == ERR_OK;
    if (bound) {
        udp_recv(pcb, on_flood_packet, nullptr);
    } else if (pcb != nullptr) {
        udp_remove(pcb);
    }
    UNLOCK_TCPIP_CORE();
    return bound;
}
} // namespace

void set_peer_up(bool up) { peer_up.store(up, std::memory_order_relaxed); }

bool start(const char *local_ip, const char *peer_ip, bool flood_peer) {
    if (local_ip == nullptr || peer_ip == nullptr || std::strlen(local_ip) >= sizeof local_addr ||
        std::strlen(peer_ip) >= sizeof peer_addr) {
        std::printf("link test: bad addresses\n");
        return false;
    }
    std::strncpy(local_addr, local_ip, sizeof local_addr - 1);
    std::strncpy(peer_addr, peer_ip, sizeof peer_addr - 1);

    if (!start_flood_receiver()) {
        std::printf("link test: udp bind failed\n");
        return false;
    }
    if (mmosal_task_create(report_windows, nullptr, MMOSAL_TASK_PRI_LOW, TASK_STACK_WORDS, "link_test") ==
        nullptr) {
        std::printf("link test: report task create failed\n");
        return false;
    }
    if (flood_peer &&
        mmosal_task_create(flood, nullptr, MMOSAL_TASK_PRI_LOW, TASK_STACK_WORDS, "flood") == nullptr) {
        std::printf("link test: flood task create failed\n");
        return false;
    }
    std::printf("link test: %s, %s %s port %u, %lu ms windows\n", local_addr, flood_peer ? "flooding" : "receiving from",
                peer_addr, FLOOD_PORT, static_cast<unsigned long>(WINDOW_MS));
    return true;
}
} // namespace mrover::link_test
