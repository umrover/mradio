#include "link_test.hpp"

#include <array>
#include <atomic>
#include <climits>
#include <cstdio>
#include <cstring>

#include "lwip/def.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "mmosal.h"
#include "mmping.h"
#include "mmwlan.h"

namespace mrover::link_test {
namespace {

constexpr uint16_t FLOOD_PORT = 5001;
constexpr uint16_t FLOOD_PAYLOAD = 1460;
constexpr uint32_t FLOOD_READY_TIMEOUT_MS = 100;
// caps the token bucket so a stall cannot bank a burst
constexpr uint32_t FLOOD_BURST_BYTES = 4U * FLOOD_PAYLOAD;
constexpr uint32_t WINDOW_MS = 5000;
constexpr uint32_t PING_INTERVAL_MS = 1000;
constexpr uint32_t TASK_STACK_WORDS = 1024;
// a backward jump larger than this means the sender restarted and its ids began again at 0
constexpr int64_t REORDER_WINDOW = 64;
constexpr uint32_t MAX_RATES = 64;
constexpr std::size_t ADDRESS_CAPACITY = 16;

std::array<char, ADDRESS_CAPACITY> local_addr{};
std::array<char, ADDRESS_CAPACITY> peer_addr{};
Config settings{};
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

bool copy_address(std::array<char, ADDRESS_CAPACITY> &destination, std::string_view text) {
    if (text.empty() || text.size() >= destination.size()) {
        return false;
    }
    std::memcpy(destination.data(), text.data(), text.size());
    destination[text.size()] = '\0';
    return true;
}

void track_sequence(int64_t id) {
    if (!synced) {
        synced = true;
    }
    else if (id > expected_id) {
        lost_packets.fetch_add(static_cast<uint32_t>(id - expected_id), std::memory_order_relaxed);
    }
    else if (id < expected_id && expected_id - id <= REORDER_WINDOW) {
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
    if (pbuf_copy_partial(packet, &id_network_order, sizeof id_network_order, 0) ==
        sizeof id_network_order) {
        // iperf senders mark the last datagram of a session by negating its id
        int64_t id = static_cast<int32_t>(lwip_ntohl(id_network_order));
        if (id < 0) {
            id = -id;
        }
        track_sequence(id);
    }
    pbuf_free(packet);
}

bool flood_budget_allows_frame(uint32_t &budget_bytes, uint32_t &refilled_at_ms) {
    if (settings.flood_rate_kbps == 0) {
        return true;
    }
    const uint32_t now_ms = mmosal_get_time_ms();
    const uint64_t refill = uint64_t{now_ms - refilled_at_ms} * settings.flood_rate_kbps / 8;
    refilled_at_ms = now_ms;
    const uint64_t topped_up = budget_bytes + refill;
    budget_bytes =
        static_cast<uint32_t>(topped_up < FLOOD_BURST_BYTES ? topped_up : FLOOD_BURST_BYTES);
    if (budget_bytes < FLOOD_PAYLOAD) {
        return false;
    }
    budget_bytes -= FLOOD_PAYLOAD;
    return true;
}

void flood_task(void * /*arg*/) {
    ip_addr_t peer{};
    if (ipaddr_aton(peer_addr.data(), &peer) == 0) {
        std::printf("link test: bad peer %s\n", peer_addr.data());
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
    uint32_t budget_bytes = 0;
    uint32_t refilled_at_ms = mmosal_get_time_ms();
    for (;;) {
        if (!peer_up.load(std::memory_order_relaxed)) {
            mmosal_task_sleep(100);
            continue;
        }
        if (!flood_budget_allows_frame(budget_bytes, refilled_at_ms)) {
            mmosal_task_sleep(1);
            continue;
        }
        // waiting here instead of inside the netif keeps the tcpip core lock free for rx
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
        }
        else {
            tx_errors.fetch_add(1, std::memory_order_relaxed);
            mmosal_task_sleep(1);
        }
    }
}

struct RateWindow {
    bool known = false;
    uint32_t busiest_info = 0;
    uint32_t sent = 0;
    uint32_t success = 0;
};

// cumulative rc counters from the previous window, reset when the rate table changes size
std::array<uint32_t, MAX_RATES> previous_sent{};
std::array<uint32_t, MAX_RATES> previous_success{};
uint32_t previous_entries = 0;

// morselib only keeps rate stats for the station side
RateWindow sample_rates() {
    RateWindow window{};
    mmwlan_rc_stats *const stats = mmwlan_get_rc_stats();
    if (stats == nullptr) {
        previous_entries = 0;
        return window;
    }
    const uint32_t entries = stats->n_entries < MAX_RATES ? stats->n_entries : MAX_RATES;
    const bool same_table = entries == previous_entries;
    uint32_t busiest_sent = 0;
    for (uint32_t i = 0; i < entries; ++i) {
        const uint32_t sent = stats->total_sent[i] - (same_table ? previous_sent[i] : 0);
        const uint32_t success = stats->total_success[i] - (same_table ? previous_success[i] : 0);
        window.sent += sent;
        window.success += success;
        if (sent > busiest_sent) {
            busiest_sent = sent;
            window.busiest_info = stats->rate_info[i];
            window.known = true;
        }
        previous_sent[i] = stats->total_sent[i];
        previous_success[i] = stats->total_success[i];
    }
    previous_entries = entries;
    mmwlan_free_rc_stats(stats);
    return window;
}

void start_ping_window() {
    mmping_args args = MMPING_ARGS_DEFAULT;
    std::strncpy(args.ping_src, local_addr.data(), sizeof args.ping_src - 1);
    std::strncpy(args.ping_target, peer_addr.data(), sizeof args.ping_target - 1);
    args.ping_interval_ms = PING_INTERVAL_MS;
    args.ping_count = WINDOW_MS / PING_INTERVAL_MS;
    static_cast<void>(mmping_start(&args));
}

void print_window(uint32_t window_ms, const struct mmping_stats &ping, const RateWindow &rates) {
    const uint64_t sent_bytes = tx_bytes.exchange(0, std::memory_order_relaxed);
    const uint64_t received_bytes = rx_bytes.exchange(0, std::memory_order_relaxed);
    std::printf(
        "link test %s: t=%lus tx %lu kbps rx %lu kbps pkts %lu lost %lu ooo %lu ping %lu/%lu "
        "avg %lu ms",
        settings.is_ap ? "ap" : "sta", static_cast<unsigned long>(mmosal_get_time_ms() / 1000),
        static_cast<unsigned long>(sent_bytes * 8 / window_ms),
        static_cast<unsigned long>(received_bytes * 8 / window_ms),
        static_cast<unsigned long>(rx_packets.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long>(lost_packets.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long>(out_of_order_packets.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long>(ping.ping_recv_count),
        static_cast<unsigned long>(ping.ping_total_count),
        static_cast<unsigned long>(ping.ping_avg_time_ms));

    // the ap has no single peer to read rssi from, and morselib returns INT32_MIN when it has none
    const int32_t rssi = settings.is_ap ? INT32_MIN : mmwlan_get_rssi();
    if (rssi == INT32_MIN) {
        std::printf(" rssi na");
    }
    else {
        std::printf(" rssi %ld", static_cast<long>(rssi));
    }
    if (rates.known) {
        std::printf(" rate mcs%u %uMHz %s", static_cast<unsigned>((rates.busiest_info >> 4) & 0xFU),
                    1U << (rates.busiest_info & 0xFU),
                    ((rates.busiest_info >> 8) & 1U) != 0 ? "sgi" : "lgi");
    }
    else {
        std::printf(" rate na");
    }
    std::printf(" att %lu ok %lu stall %lu err %lu\n", static_cast<unsigned long>(rates.sent),
                static_cast<unsigned long>(rates.success),
                static_cast<unsigned long>(tx_stalls.exchange(0, std::memory_order_relaxed)),
                static_cast<unsigned long>(tx_errors.exchange(0, std::memory_order_relaxed)));
}

void report_task(void * /*arg*/) {
    uint32_t window_started_ms = mmosal_get_time_ms();
    for (;;) {
        start_ping_window();
        mmosal_task_sleep(WINDOW_MS);
        struct mmping_stats ping{};
        mmping_stats(&ping);
        // stopping waits on a ping stuck without a peer, which stretches the window past 5 s
        mmping_stop();
        const uint32_t now_ms = mmosal_get_time_ms();
        const uint32_t window_ms = now_ms - window_started_ms;
        window_started_ms = now_ms;
        print_window(window_ms == 0 ? 1 : window_ms, ping, sample_rates());
    }
}

bool start_flood_receiver() {
    LOCK_TCPIP_CORE();
    udp_pcb *const pcb = udp_new();
    const bool bound = pcb != nullptr && udp_bind(pcb, IP_ANY_TYPE, FLOOD_PORT) == ERR_OK;
    if (bound) {
        udp_recv(pcb, on_flood_packet, nullptr);
    }
    else if (pcb != nullptr) {
        udp_remove(pcb);
    }
    UNLOCK_TCPIP_CORE();
    return bound;
}

} // namespace

void set_peer_up(bool up) { peer_up.store(up, std::memory_order_relaxed); }

bool start(const Config &config) {
    if (!copy_address(local_addr, config.local_ip) || !copy_address(peer_addr, config.peer_ip)) {
        std::printf("link test: bad addresses\n");
        return false;
    }
    settings = config;
    if (!start_flood_receiver()) {
        std::printf("link test: udp bind failed\n");
        return false;
    }
    if (mmosal_task_create(report_task, nullptr, MMOSAL_TASK_PRI_LOW, TASK_STACK_WORDS,
                           "link_test") == nullptr) {
        std::printf("link test: report task create failed\n");
        return false;
    }
    if (config.flood && mmosal_task_create(flood_task, nullptr, MMOSAL_TASK_PRI_LOW,
                                           TASK_STACK_WORDS, "flood") == nullptr) {
        std::printf("link test: flood task create failed\n");
        return false;
    }
    std::printf("link test: %s %s %s port %u", local_addr.data(),
                config.flood ? "flooding" : "receiving from", peer_addr.data(), FLOOD_PORT);
    if (config.flood && config.flood_rate_kbps != 0) {
        std::printf(" at %lu kbps", static_cast<unsigned long>(config.flood_rate_kbps));
    }
    std::printf(", %lu ms windows\n", static_cast<unsigned long>(WINDOW_MS));
    return true;
}

} // namespace mrover::link_test
