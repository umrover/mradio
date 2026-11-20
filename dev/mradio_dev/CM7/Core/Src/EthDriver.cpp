#include "EthDriver.hpp"
#include "main.h"
#include <cstring>

// global ethernet handle and dma descriptors
extern ETH_HandleTypeDef heth;
extern ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
extern ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];

// rx buffer pool
#define ETH_RX_BUFFER_SIZE 1536U
#define ETH_RX_BUFFER_CNT ETH_RX_DESC_CNT

__attribute__((section(".Rx_PoolSection"), aligned(32)))
static uint8_t rx_pool[ETH_RX_BUFFER_CNT][ETH_RX_BUFFER_SIZE];
static volatile uint32_t rx_pool_alloc_idx = 0;

// hal ethernet callbacks
extern "C" {
    void HAL_ETH_RxAllocateCallback(uint8_t **buff) {
        *buff = rx_pool[rx_pool_alloc_idx];
        rx_pool_alloc_idx = (rx_pool_alloc_idx + 1) % ETH_RX_BUFFER_CNT;
    }

    void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length) {
        *pStart = buff;
        *pEnd = buff;
        SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(buff), Length);
    }

    void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *handlerEth) {
        (void)handlerEth;
        mrover::EthDriver::getInstance().notifyRxFromISR();
    }

    void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *handlerEth) {
        if ((HAL_ETH_GetDMAError(handlerEth) & ETH_DMACSR_RBU) == ETH_DMACSR_RBU) {
            mrover::EthDriver::getInstance().notifyRxFromISR();
        }
    }
}

namespace mrover {
    // signal incoming packet
    void EthDriver::notifyRxFromISR() {
        if (rx_semaphore != nullptr) {
            osSemaphoreRelease(rx_semaphore);
        }
    }

    // initialize EthDriver
    void EthDriver::init() {
        if (rx_semaphore != nullptr) {
            return;
        }

        // enable promiscuous mode
        ETH_MACFilterConfigTypeDef filter_conf;
        HAL_ETH_GetMACFilterConfig(&heth, &filter_conf);
        filter_conf.PromiscuousMode = ENABLE;
        filter_conf.PassAllMulticast = ENABLE;
        HAL_ETH_SetMACFilterConfig(&heth, &filter_conf);

        // create rtos rx semaphore and rx thread before starting interrupts
        rx_semaphore = osSemaphoreNew(1, 0, nullptr);

        const osThreadAttr_t rx_attr = {
            .name = "EthRxTask",
            .attr_bits = 0,
            .cb_mem = nullptr,
            .cb_size = 0,
            .stack_mem = nullptr,
            .stack_size = 512 * 4,
            .priority = (osPriority_t)osPriorityRealtime,
            .tz_module = 0,
            .reserved = 0
        };
        rx_task = osThreadNew(rxTaskEntry, nullptr, &rx_attr);

        // start ethernet interrupts
        HAL_ETH_Start_IT(&heth);
    }

    // entry function for rx monitor task
    void EthDriver::rxTaskEntry(void* argument) {
        (void)argument;
        EthDriver& driver = EthDriver::getInstance();
        uint8_t* frame_ptr = nullptr;

        for (;;) {
            if (osSemaphoreAcquire(driver.rx_semaphore, osWaitForever) == osOK) {
                while (HAL_ETH_ReadData(&heth, reinterpret_cast<void**>(&frame_ptr)) == HAL_OK) {
                    uint16_t length = static_cast<uint16_t>(heth.RxDescList.RxDataLength);
                    handleEthRx(frame_ptr, length);
                }
            }
        }
    }

    // transmit an ethernet frame
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
