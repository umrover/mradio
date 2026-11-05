#include "EthDriver.hpp"
#include "err.h"

// initialize udp and tcp servers
void EthDriver::init() {
    // initialize udp server
    upcb = udp_new();
    if (upcb) {
        err_t err = udp_bind(upcb, netif_ip_addr4(&gnetif), udp_port);

        if (err == ERR_OK) {
            udp_recv(upcb, &EthDriver::udpRecvCallback, nullptr);
        } else {
            udp_remove(upcb);
            upcb = nullptr;
        }
    }

    // initialize tcp server
    tpcb = tcp_new();
    if (tpcb) {
        err_t err = tcp_bind(tpcb, netif_ip_addr4(&gnetif), tcp_port);

        if (err == ERR_OK) {
            tpcb = tcp_listen(tpcb);
            tcp_accept(tpcb, &EthDriver::tcpAcceptCallback);
        } else {
            tcp_close(tpcb);
            tpcb = nullptr;
        }
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
    int len = snprintf(buf, sizeof(buf), "UDP received %.*s\n", p->len, (char *)p->payload);
    
    // transmit buf
    udpTransmit(addr, port, buf, len);

    // free pbuf
    pbuf_free(p);
}

bool EthDriver::udpTransmit(const ip_addr_t *addr, u16_t port, const char* data, uint16_t len) {
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

err_t EthDriver::tcpAcceptCallback(void *arg, struct tcp_pcb *new_tpcb, err_t err) {
    if (err != ERR_OK || new_tpcb == nullptr) {
        return ERR_VAL;
    }

    // set up callbacks for tcp connection
    tcp_recv(new_tpcb, &EthDriver::tcpRecvCallback);
    tcp_arg(new_tpcb, nullptr); 

    return ERR_OK;
}

err_t EthDriver::tcpRecvCallback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    return EthDriver::getInstance().tcpHandlePacket(tpcb, p, err);
}

err_t EthDriver::tcpHandlePacket(struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    // if p is null the remote host closed the connection
    if (p == nullptr) {
        closeTcpConnection(tpcb);
        return ERR_OK;
    } 
    
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    // ack tcp data
    tcp_recved(tpcb, p->tot_len);

    // build reply
    char buf[100];
    int len = snprintf(buf, sizeof(buf), "TCP received %.*s\n", p->len, (char *)p->payload);
    
    // transmit buf
    tcpTransmit(tpcb, buf, len);

    // free pbuf
    pbuf_free(p);
    return ERR_OK;
}

bool EthDriver::tcpTransmit(struct tcp_pcb *tpcb, const char* data, uint16_t len) {
    if (tpcb == nullptr || data == nullptr || len == 0) {
        return false;
    }

    // ensure there is space in sndbuf
    if (tcp_sndbuf(tpcb) < len) {
        return false; 
    }

    // write data to sndbuf
    err_t err = tcp_write(tpcb, data, len, TCP_WRITE_FLAG_COPY);
    
    // if write was successful flush data
    if (err == ERR_OK) {
        err = tcp_output(tpcb);
    }
    
    return err == ERR_OK;
}

void EthDriver::closeTcpConnection(struct tcp_pcb *tpcb) {
    // unregister callbacks and close tcp connection
    if (tpcb != nullptr) {
        tcp_arg(tpcb, nullptr);
        tcp_recv(tpcb, nullptr);
        tcp_err(tpcb, nullptr);
        tcp_poll(tpcb, nullptr, 0);
        tcp_close(tpcb);
    }
}