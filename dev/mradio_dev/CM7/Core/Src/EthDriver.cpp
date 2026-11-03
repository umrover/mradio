#include "EthDriver.hpp"
#include "err.h"

// initialize member variables and ethernet peripherals
void EthDriver::init() {
    // initialize upcb
    upcb = udp_new();
    if (!upcb) {
        return;
    }

    err_t err = udp_bind(upcb, netif_ip_addr4(&gnetif), udp_port);

    if (err == ERR_OK) {
        // Pass the static function as the callback
        udp_recv(upcb, &EthDriver::udpRecvCallback, nullptr);
    } else {
        udp_remove(upcb);
        upcb = nullptr;
    }
}

void EthDriver::udpRecvCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p == nullptr) {
        return;
    }

    // call udpHandlePacket
    EthDriver::getInstance().udpHandlePacket(p, addr, port);
}

void EthDriver::udpHandlePacket(struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    // build reply
    char buf[100];
    int len = sprintf(buf, "received %s\n", (char *)p->payload);
    
    // transmit buf
    udpTransmit(addr, port, buf, len);

    // free pbuf
    pbuf_free(p);
}

bool EthDriver::udpTransmit(const ip_addr_t *addr, u16_t port, char* data, uint16_t len) {
    struct pbuf *tx_buf;

    // allocate pbuf for for reply
    tx_buf = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!tx_buf) {
        return false; // out of memory
    }

    // fill pbuf with data
    pbuf_take(tx_buf, data, len);

    // transmit tx_buf
    err_t err = udp_sendto(upcb, tx_buf, addr, port);

    // free tx_buf
    pbuf_free(tx_buf);

    return err == ERR_OK;
}