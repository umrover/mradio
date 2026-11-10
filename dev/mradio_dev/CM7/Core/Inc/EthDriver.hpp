// #include "RingBuffer.hpp"

#include "lwip.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip.h"
#include "lwip/tcpip.h"
#include "err.h"
#include "PacketGrouper.hpp"
#include <cstdint>

#define UDP_PORT 8080
#define TCP_PORT 8082

extern struct netif gnetif;

namespace mrover {
    extern PacketGrouper packet_grouper;

    // class for interfacing with ethernet peripherals
    class EthDriver {
    private:
        uint16_t udp_port = UDP_PORT;
        struct udp_pcb *upcb = nullptr;

        uint16_t tcp_port = TCP_PORT;
        struct tcp_pcb *tpcb = nullptr;

        // default constructor
        EthDriver() = default;

        // UDP callback and packet handler
        static void udpRecvCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
        void udpHandlePacket(struct pbuf *p, const ip_addr_t *addr, u16_t port);

        // TCP callback and packet handler
        static err_t tcpAcceptCallback(void *arg, struct tcp_pcb *new_tpcb, err_t err);
        static err_t tcpRecvCallback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
        err_t tcpHandlePacket(struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

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
        bool udpTransmit(const ip_addr_t *addr, u16_t port, const uint8_t* data, uint16_t len);

        // transmit a TCP packet
        bool tcpTransmit(struct tcp_pcb *tpcb, const uint8_t* data, uint16_t len);

        // close the TCP connection
        void closeTcpConnection(struct tcp_pcb *tpcb);
    };
}