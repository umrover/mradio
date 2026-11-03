// #include "RingBuffer.hpp"

#include "lwip.h"
#include "lwip/udp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include <cstdint>

#define UDP_PORT 8080
extern struct netif gnetif;

// class for interfacing with ethernet peripherals
class EthDriver {
private:
    uint16_t udp_port = UDP_PORT;
    struct udp_pcb *upcb = nullptr;

    // default constructor
    EthDriver() = default;

    // receive callback for UDP packets
    static void udpRecvCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

    // UDP packet handler
    void udpHandlePacket(struct pbuf *p, const ip_addr_t *addr, u16_t port);

public:
    // singleton access
    static EthDriver& getInstance() {
        static EthDriver instance;
        return instance;
    }

    // delete copy and move constructors/assignments
    EthDriver(const EthDriver&) = delete;
    EthDriver& operator=(const EthDriver&) = delete;
    EthDriver(EthDriver&&) = delete;
    EthDriver& operator=(EthDriver&&) = delete;

    // initialize EthDriver singleton instance
    void init();

    // transmit a UDP packet
    bool udpTransmit(const ip_addr_t *addr, u16_t port, char* data, uint16_t len);
};