#include "PacketInfo.hpp"
#include "main.h"
#include "EthDriver.hpp"
#include "RingBuffer.hpp"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio.h"
#include <cstdint>

namespace mrover {
    __attribute__((section(".RxRingBuffer"))) RingBuffer rx_ring_buf;
    __attribute__((section(".TxRingBuffer"))) RingBuffer tx_ring_buf;

    PacketGrouper packet_grouper;

    void initRingBuffers() {
        rx_ring_buf.init();
        tx_ring_buf.init();
    }

    extern "C" {
        void postInit() {
            initRingBuffers();
        }

        void lwipInit() {
            EthDriver::getInstance().init();
        }

        void checkRxBuf() {
            static BufferSlot slot;

            // flush groups exceeding deadline
            for (size_t i = 0; i < NUM_PACKET_TYPES; i++) {
                packet_grouper.checkDeadline(static_cast<PacketType>(i));
            }

            // check rx buffer
            if (rx_ring_buf.pop(slot)) {
                if (slot.protocol_type == ProtocolType::TCP) {
                    if (slot.tpcb != nullptr) {
                        EthDriver::getInstance().tcpTransmit(static_cast<struct tcp_pcb*>(slot.tpcb), slot.packet, slot.len);
                    }
                } else if (slot.protocol_type == ProtocolType::UDP) {
                    size_t offset = 0;
                    while (offset + sizeof(PacketSubHeader) <= slot.len) {
                        const auto* sub_hdr = reinterpret_cast<const PacketSubHeader*>(slot.packet + offset);
                        offset += sizeof(PacketSubHeader);

                        if (offset + sub_hdr->len > slot.len) break; // sanity check

                        const uint8_t* payload = slot.packet + offset;
                        ip_addr_t dest_ip;
                        ip_addr_set_ip4_u32(&dest_ip, sub_hdr->ip);

                        EthDriver::getInstance().udpTransmit(&dest_ip, sub_hdr->port, payload, sub_hdr->len);
                        offset += sub_hdr->len;
                    }
                }
            }
        }
    }
}