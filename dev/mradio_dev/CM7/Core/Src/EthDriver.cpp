#include "EthDriver.hpp"
#include "stm32h7xx_hal.h"
#include <cstring>

namespace mrover {
    void EthDriver::init() {
        // intialize ethernet as ethernet bridge
    }

    bool EthDriver::sendFrame(const uint8_t* frame, uint16_t len) {
        // check for invalid frame
        if (frame == nullptr || len == 0 || len > MTU) {
            return false;
        }

        // flush frame from dcache so dma reads actual frame
        SCB_CleanDCache_by_Addr(const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(frame)), len);

        // create tx_buffer
        ETH_BufferTypeDef tx_buffer;
        std::memset(&tx_buffer, 0, sizeof(ETH_BufferTypeDef));
        tx_buffer.buffer = const_cast<uint8_t*>(frame);
        tx_buffer.len = len;
        tx_buffer.next = nullptr;

        // create tx_config
        ETH_TxPacketConfig tx_config;
        std::memset(&tx_config, 0, sizeof(ETH_TxPacketConfig));
        tx_config.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
        tx_config.CRCPadCtrl = ETH_CRC_PAD_INSERT;
        tx_config.Length = len;
        tx_config.TxBuffer = &tx_buffer;
        tx_config.pData = const_cast<uint8_t*>(frame);

        // transmit packet
        HAL_StatusTypeDef status = HAL_ETH_Transmit(&heth, &tx_config, 50);

        // release tx descriptor
        HAL_ETH_ReleaseTxPacket(&heth);

        return status == HAL_OK;
    }
}
