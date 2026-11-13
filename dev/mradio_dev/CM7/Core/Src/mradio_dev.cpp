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

    PacketHandler packet_handler;

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
                packet_handler.checkDeadline(static_cast<PacketType>(i));
            }

            // check rx buffer
            if (rx_ring_buf.pop(slot)) {
                if (slot.protocol_type == ProtocolType::TCP) {
                    if (slot.tpcb != nullptr) {
                        EthDriver::getInstance().tcpTransmit(static_cast<struct tcp_pcb*>(slot.tpcb), slot.packet, slot.len);
                    }
                } else if (slot.protocol_type == ProtocolType::UDP) {
                    packet_handler.degroupPackets(slot.packet, slot.len, [](const DegroupedPacket& pkt) {
                        ip_addr_t dest_ip;
                        ip_addr_set_ip4_u32(&dest_ip, pkt.header.dest_ip);

                        EthDriver::getInstance().udpTransmit(&dest_ip, pkt.header.dest_port, pkt.payload, pkt.header.len);
                    });
                }
            }
        }
    }
}