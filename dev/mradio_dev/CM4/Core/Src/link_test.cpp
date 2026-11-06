#include "link_test.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "mmipal.h"
#include "mmosal.h"
#include "mmping.h"
#include "mmpkt.h"
#include "mmwlan.h"

// Mirrors the leading fields of the internal struct in morselib/src/internal/mmdrv.h for SDK 2.13.1
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
void __real_mmint_5145db60(mmpkt *rx_frame, uint16_t channel);

// mmdrv_host_process_rx_frame, runs on the morselib driver task for every frame the chip hands up
void __wrap_mmint_5145db60(mmpkt *rx_frame, uint16_t channel) {
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
    __real_mmint_5145db60(rx_frame, channel);
}
}

namespace mrover::link_test {
namespace {
constexpr char LOCAL_IP[] = "192.168.1.2";
constexpr char NETMASK[] = "255.255.255.0";
constexpr char AP_IP[] = "192.168.1.1";
constexpr uint16_t FLOOD_PORT = 5001;
constexpr uint32_t WINDOW_MS = 5000;
constexpr uint32_t PING_INTERVAL_MS = 1000;
constexpr unsigned REPORT_STACK_WORDS = 1024;
// a backward jump larger than this means the sender restarted its session and ids began again at 0
constexpr int64_t REORDER_WINDOW = 64;

std::atomic<uint32_t> rx_bytes{0};
std::atomic<uint32_t> rx_packets{0};
std::atomic<uint32_t> lost_packets{0};
std::atomic<uint32_t> out_of_order_packets{0};

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
        // the final datagram of a session carries its id negated
        int64_t id = static_cast<int32_t>(lwip_ntohl(id_network_order));
        if (id < 0) {
            id = -id;
        }
        track_sequence(id);
    }
    pbuf_free(packet);
}

void print_link_status(const mmipal_link_status *link_status) {
    std::printf("link %s, ip %s\n", link_status->link_state == MMIPAL_LINK_UP ? "up" : "down",
                link_status->ip_addr);
}

void start_ping_window() {
    mmping_args args = MMPING_ARGS_DEFAULT;
    std::strncpy(args.ping_src, LOCAL_IP, sizeof args.ping_src - 1);
    std::strncpy(args.ping_target, AP_IP, sizeof args.ping_target - 1);
    args.ping_interval_ms = PING_INTERVAL_MS;
    args.ping_count = WINDOW_MS / PING_INTERVAL_MS;
    static_cast<void>(mmping_start(&args));
}

void report_windows(void * /*arg*/) {
    for (;;) {
        start_ping_window();
        mmosal_task_sleep(WINDOW_MS);

        struct mmping_stats ping{};
        mmping_stats(&ping);
        mmping_stop();

        const uint64_t bytes = rx_bytes.exchange(0, std::memory_order_relaxed);
        const uint32_t packets = rx_packets.exchange(0, std::memory_order_relaxed);
        const uint32_t lost = lost_packets.exchange(0, std::memory_order_relaxed);
        const uint32_t out_of_order = out_of_order_packets.exchange(0, std::memory_order_relaxed);
        const auto kbps = static_cast<unsigned long>(bytes * 8 / WINDOW_MS);

        // the exchanges are not atomic together, so a frame landing between them skews one window by one frame
        const int32_t window_rssi_sum = rssi_sum.exchange(0, std::memory_order_relaxed);
        const uint32_t measured_frames = rssi_frames.exchange(0, std::memory_order_relaxed);
        const int32_t window_noise_sum = noise_sum.exchange(0, std::memory_order_relaxed);
        const uint32_t frames = rx_frames.exchange(0, std::memory_order_relaxed);
        const long rssi_dbm =
            measured_frames == 0 ? 0 : static_cast<long>(window_rssi_sum / static_cast<int32_t>(measured_frames));
        const long noise_dbm = frames == 0 ? 0 : static_cast<long>(window_noise_sum / static_cast<int32_t>(frames));

        std::printf("t=%lus rx %lu kbps pkts %lu lost %lu ooo %lu ping %lu/%lu avg %lu ms "
                    "rssi %ld dBm over %lu frames noise %ld dBm\n",
                    static_cast<unsigned long>(mmosal_get_time_ms() / 1000), kbps,
                    static_cast<unsigned long>(packets), static_cast<unsigned long>(lost),
                    static_cast<unsigned long>(out_of_order), static_cast<unsigned long>(ping.ping_recv_count),
                    static_cast<unsigned long>(ping.ping_total_count),
                    static_cast<unsigned long>(ping.ping_avg_time_ms), rssi_dbm,
                    static_cast<unsigned long>(measured_frames), noise_dbm);
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

bool start() {
    mmipal_init_args args = MMIPAL_INIT_ARGS_DEFAULT;
    args.mode = MMIPAL_STATIC;
    std::strncpy(args.ip_addr, LOCAL_IP, sizeof args.ip_addr - 1);
    std::strncpy(args.netmask, NETMASK, sizeof args.netmask - 1);
    std::strncpy(args.gateway_addr, AP_IP, sizeof args.gateway_addr - 1);
    if (mmipal_init(&args) != MMIPAL_SUCCESS) {
        std::printf("link test: ip init failed\n");
        return false;
    }
    mmipal_set_link_status_callback(print_link_status);

    if (!start_flood_receiver()) {
        std::printf("link test: udp bind failed\n");
        return false;
    }

    if (mmosal_task_create(report_windows, nullptr, MMOSAL_TASK_PRI_LOW, REPORT_STACK_WORDS, "link_test") ==
        nullptr) {
        std::printf("link test: report task create failed\n");
        return false;
    }
    std::printf("link test: %s, flood port %u, %lu ms windows\n", LOCAL_IP, FLOOD_PORT,
                static_cast<unsigned long>(WINDOW_MS));
    return true;
}
} // namespace mrover::link_test
