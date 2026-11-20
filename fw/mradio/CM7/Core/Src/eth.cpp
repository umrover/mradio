#include "eth.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "cmsis_os2.h"
#include "lwip/etharp.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "stm32h7xx_hal.h"

extern "C" {
// main.c keeps these but no longer calls MX_ETH_Init, whose failure would halt the board
extern ETH_HandleTypeDef heth;
extern ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
extern ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];
}

namespace mrover::eth {
namespace {

constexpr size_t FRAME_BUFFER_BYTES = 1536;
constexpr uint32_t TX_TIMEOUT_MS = 20;
constexpr uint32_t LINK_POLL_MS = 500;

constexpr uint32_t PHY_BMSR = 0x01;
constexpr uint32_t PHY_ID1 = 0x02;
constexpr uint32_t PHY_STS = 0x10;
constexpr uint32_t BMSR_LINK = 1U << 2;
constexpr uint32_t BMSR_AUTONEG_DONE = 1U << 5;
constexpr uint32_t STS_10_MBPS = 1U << 1;
constexpr uint32_t STS_FULL_DUPLEX = 1U << 2;
constexpr uint32_t TI_ID1 = 0x2000;

constexpr osThreadAttr_t ETH_TASK_ATTRIBUTES = {
    .name = "eth",
    .stack_size = 1024 * 4,
    .priority = osPriorityAboveNormal,
};

using FrameBuffer = std::array<uint8_t, FRAME_BUFFER_BYTES>;

// in SRAM3, which the MPU leaves uncached so the DMA and CPU agree, see eth.ld
// frames are copied out at once, so the HAL refills descriptors in the order it drains them
[[gnu::section(".eth_buffers")]] std::array<FrameBuffer, ETH_RX_DESC_CNT * 2> rx_buffers;
[[gnu::section(".eth_buffers")]] FrameBuffer tx_buffer;
size_t next_rx_buffer = 0;

std::array<uint8_t, ETH_HWADDR_LEN> mac_address;
netif eth_netif;
uint32_t phy_address = 0;
osSemaphoreId_t rx_ready = nullptr;

bool read_phy(uint32_t reg, uint32_t &value) {
    return HAL_ETH_ReadPHYRegister(&heth, phy_address, reg, &value) == HAL_OK;
}

bool find_phy() {
    for (uint32_t address = 0; address < 32; ++address) {
        uint32_t id = 0;
        if (HAL_ETH_ReadPHYRegister(&heth, address, PHY_ID1, &id) == HAL_OK && id == TI_ID1) {
            phy_address = address;
            return true;
        }
    }
    return false;
}

bool phy_link_up() {
    uint32_t status = 0;
    // the link bit latches low, so the first read clears an old drop
    if (!read_phy(PHY_BMSR, status) || !read_phy(PHY_BMSR, status)) {
        return false;
    }
    return (status & BMSR_LINK) != 0 && (status & BMSR_AUTONEG_DONE) != 0;
}

void bring_link_up() {
    uint32_t status = 0;
    if (!read_phy(PHY_STS, status)) {
        return;
    }
    const bool fast = (status & STS_10_MBPS) == 0;
    const bool full_duplex = (status & STS_FULL_DUPLEX) != 0;
    ETH_MACConfigTypeDef mac_config{};
    HAL_ETH_GetMACConfig(&heth, &mac_config);
    mac_config.Speed = fast ? ETH_SPEED_100M : ETH_SPEED_10M;
    mac_config.DuplexMode = full_duplex ? ETH_FULLDUPLEX_MODE : ETH_HALFDUPLEX_MODE;
    if (HAL_ETH_SetMACConfig(&heth, &mac_config) != HAL_OK || HAL_ETH_Start_IT(&heth) != HAL_OK) {
        std::printf("eth: mac start failed\n");
        return;
    }
    LOCK_TCPIP_CORE();
    netif_set_link_up(&eth_netif);
    UNLOCK_TCPIP_CORE();
    std::printf("eth: link up %s %s duplex\n", fast ? "100M" : "10M",
                full_duplex ? "full" : "half");
}

void take_link_down() {
    LOCK_TCPIP_CORE();
    netif_set_link_down(&eth_netif);
    UNLOCK_TCPIP_CORE();
    HAL_ETH_Stop_IT(&heth);
    std::printf("eth: link down\n");
}

void receive_frames() {
    void *frame = nullptr;
    while (HAL_ETH_ReadData(&heth, &frame) == HAL_OK) {
        auto *const packet = static_cast<pbuf *>(frame);
        if (packet != nullptr && eth_netif.input(packet, &eth_netif) != ERR_OK) {
            pbuf_free(packet);
        }
        frame = nullptr;
    }
}

[[noreturn]] void eth_task(void * /*argument*/) {
    bool link_up = false;
    uint32_t last_poll = osKernelGetTickCount() - LINK_POLL_MS;
    for (;;) {
        static_cast<void>(osSemaphoreAcquire(rx_ready, LINK_POLL_MS));
        if (link_up) {
            receive_frames();
        }
        if (osKernelGetTickCount() - last_poll < LINK_POLL_MS) {
            continue;
        }
        last_poll = osKernelGetTickCount();
        const bool phy_up = phy_link_up();
        if (phy_up && !link_up) {
            bring_link_up();
            link_up = netif_is_link_up(&eth_netif);
        }
        else if (!phy_up && link_up) {
            take_link_down();
            link_up = false;
        }
    }
}

err_t transmit(netif * /*interface*/, pbuf *frame) {
    if (frame->tot_len > tx_buffer.size()) {
        return ERR_IF;
    }
    pbuf_copy_partial(frame, tx_buffer.data(), frame->tot_len, 0);
    ETH_BufferTypeDef buffer{};
    buffer.buffer = tx_buffer.data();
    buffer.len = frame->tot_len;
    ETH_TxPacketConfigTypeDef tx_config{};
    tx_config.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
    tx_config.CRCPadCtrl = ETH_CRC_PAD_INSERT;
    tx_config.Length = frame->tot_len;
    tx_config.TxBuffer = &buffer;
    // blocks until the DMA has read tx_buffer, so the next frame can reuse it
    return HAL_ETH_Transmit(&heth, &tx_config, TX_TIMEOUT_MS) == HAL_OK ? ERR_OK : ERR_IF;
}

err_t init_netif(netif *interface) {
    interface->name[0] = 'e';
    interface->name[1] = 't';
    interface->output = etharp_output;
    interface->linkoutput = transmit;
    interface->mtu = 1500;
    interface->hwaddr_len = ETH_HWADDR_LEN;
    std::memcpy(interface->hwaddr, mac_address.data(), ETH_HWADDR_LEN);
    interface->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    return ERR_OK;
}

} // namespace

bool start(const ip4_addr_t &ip, const ip4_addr_t &mask) {
    // locally administered, from the chip's unique ID so boards on one switch differ
    const uint32_t uid = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
    mac_address = {0x02,
                   0x80,
                   0xE1,
                   static_cast<uint8_t>(uid >> 16),
                   static_cast<uint8_t>(uid >> 8),
                   static_cast<uint8_t>(uid)};

    __HAL_RCC_D2SRAM3_CLK_ENABLE();
    heth.Instance = ETH;
    heth.Init.MACAddr = mac_address.data();
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc = DMATxDscrTab;
    heth.Init.RxDesc = DMARxDscrTab;
    heth.Init.RxBuffLen = FRAME_BUFFER_BYTES;
    if (HAL_ETH_Init(&heth) != HAL_OK) {
        std::printf("eth: mac init failed, likely no RMII clock from the PHY\n");
        return false;
    }
    if (!find_phy()) {
        std::printf("eth: no PHY answered on MDIO\n");
        return false;
    }

    rx_ready = osSemaphoreNew(1, 0, nullptr);
    if (rx_ready == nullptr) {
        std::printf("eth: rx semaphore create failed\n");
        return false;
    }
    HAL_NVIC_SetPriority(ETH_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(ETH_IRQn);

    LOCK_TCPIP_CORE();
    const bool added = netif_add(&eth_netif, &ip, &mask, IP4_ADDR_ANY4, nullptr, init_netif,
                                 tcpip_input) != nullptr;
    if (added) {
        netif_set_up(&eth_netif);
    }
    UNLOCK_TCPIP_CORE();
    if (!added) {
        std::printf("eth: netif add failed\n");
        return false;
    }

    if (osThreadNew(eth_task, nullptr, &ETH_TASK_ATTRIBUTES) == nullptr) {
        std::printf("eth: task create failed\n");
        return false;
    }
    std::printf("eth: phy at %lu, mac %02x:%02x:%02x:%02x:%02x:%02x\n",
                static_cast<unsigned long>(phy_address), mac_address[0], mac_address[1],
                mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
    return true;
}

netif &interface() { return eth_netif; }

} // namespace mrover::eth

extern "C" {

void ETH_IRQHandler(void) { HAL_ETH_IRQHandler(&heth); }

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef * /*handle*/) {
    osSemaphoreRelease(mrover::eth::rx_ready);
}

void HAL_ETH_RxAllocateCallback(uint8_t **buffer) {
    using namespace mrover::eth;
    *buffer = rx_buffers[next_rx_buffer].data();
    next_rx_buffer = (next_rx_buffer + 1) % rx_buffers.size();
}

void HAL_ETH_RxLinkCallback(void **start, void **end, uint8_t *buffer, uint16_t length) {
    pbuf *const segment = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if (segment == nullptr) {
        // drops the frame, and the HAL skips its remaining segments since start is null
        if (*start != nullptr) {
            pbuf_free(static_cast<pbuf *>(*start));
        }
        *start = nullptr;
        *end = nullptr;
        return;
    }
    pbuf_take(segment, buffer, length);
    if (*start == nullptr) {
        *start = segment;
    }
    else {
        pbuf_cat(static_cast<pbuf *>(*start), segment);
    }
    *end = segment;
}
}
