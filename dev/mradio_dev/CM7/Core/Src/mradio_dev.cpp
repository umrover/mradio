#include "PacketInfo.hpp"
#include "main.h"
#include "EthDriver.hpp"
#include "RingBuffer.hpp"
#include "PacketHandler.hpp"
#include "stm32h7xx_hal.h"
#include "cmsis_os2.h"
#include <cstdint>
#include <cstring>

namespace mrover {
    __attribute__((section(".RxRingBuffer"))) RingBuffer rx_ring_buf;
    __attribute__((section(".TxRingBuffer"))) RingBuffer tx_ring_buf;

    PacketHandler packet_handler;
    static osMutexId_t eth_tx_mutex = nullptr;

    void initRingBuffers() {
        rx_ring_buf.init();
        tx_ring_buf.init();
    }

    extern "C" {
        void postInit() {
            initRingBuffers();
            eth_tx_mutex = osMutexNew(nullptr);
        }

        void lwipInit() {
            EthDriver::getInstance().init();
        }

        // handles new ethernet frame
        void handleEthRx(const uint8_t* frame, uint16_t len) {
            // check for invalid frame
            if (frame == nullptr || len < 14 || len > MTU) {
                return;
            }

            // aquire tx mutex
            if (eth_tx_mutex != nullptr) {
                osMutexAcquire(eth_tx_mutex, osWaitForever);
            }

            // check the ethernet type (IPv4, ARP, ICMP, etc.)
            uint16_t ether_type = (static_cast<uint16_t>(frame[12]) << 8) | frame[13];

            // check if this is an IPv4 packet
            if (ether_type == 0x0800 && len >= 34) {
                uint8_t ip_proto = frame[23]; // get protocol
                uint8_t ip_tos = frame[15]; // get tos

                // if this is a udp packet with a valid type then group it
                if (ip_proto == 17 && ip_tos < NUM_PACKET_TYPES) {
                    packet_handler.groupPacket(frame, len, static_cast<PacketType>(ip_tos));
                    
                    // release tx mutex
                    if (eth_tx_mutex != nullptr) {
                        osMutexRelease(eth_tx_mutex);
                    }
                    
                    return;
                }
            }

            // send all other traffic without grouping
            static BufferSlot buf_slot;
            buf_slot.slot_type = SlotType::SINGLE_FRAME;
            buf_slot.len = len;
            std::memcpy(buf_slot.packet, frame, len);
            tx_ring_buf.push(buf_slot);

            // release tx mutex
            if (eth_tx_mutex != nullptr) {
                osMutexRelease(eth_tx_mutex);
            }
        }

        // checks the rx ring buffer periodically
        void checkRxBuf() {
            static BufferSlot slot;

            // aquire tx mutex
            if (eth_tx_mutex != nullptr) {
                osMutexAcquire(eth_tx_mutex, osWaitForever);
            }

            // flush groups exceeding deadline
            for (size_t i = 0; i < NUM_PACKET_TYPES; i++) {
                packet_handler.checkDeadline(static_cast<PacketType>(i));
            }

            // release tx mutex
            if (eth_tx_mutex != nullptr) {
                osMutexRelease(eth_tx_mutex);
            }

            // check for rx frames that need to be handled
            if (rx_ring_buf.pop(slot)) {
                if (slot.slot_type == SlotType::SINGLE_FRAME) {
                    EthDriver::getInstance().sendFrame(slot.packet, slot.len);
                } else if (slot.slot_type == SlotType::GROUPED_PACKET) {
                    packet_handler.degroupPackets(slot.packet, slot.len, [](const uint8_t* frame, uint16_t len) {
                        EthDriver::getInstance().sendFrame(frame, len);
                    });
                }
            }
        }
    }
}
