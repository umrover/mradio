/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal_wlan.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mmutils.h"

#include "main.h"

/* At least 74 clocks, see section 6.4.1.1 of the SD physical layer spec */
#define BYTE_TRAIN 16

MM_STATIC_ASSERT((BYTE_TRAIN >= 10), "BYTE_TRAIN must be at least 10.");

static struct mmosal_semb *dma_semb_handle;
#define DMA_WAIT_TMO (10)

#define DMA_TRANSFER_MIN_LENGTH (16)

static mmhal_irq_handler_t spi_irq_handler = NULL;

void mmhal_wlan_hard_reset(void)
{
    mmhal_wlan_assert_reset(true);
    mmosal_task_sleep(5);
    mmhal_wlan_assert_reset(false);
    mmosal_task_sleep(20);
}

void mmhal_wlan_assert_reset(bool assert_reset)
{
    if (assert_reset)
    {
        LL_GPIO_ResetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
    }
    else
    {
        LL_GPIO_SetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
    }
}

void mmhal_wlan_spi_cs_assert(void)
{
    LL_GPIO_ResetOutputPin(SPI_CS_GPIO_Port, SPI_CS_Pin);
}

void mmhal_wlan_spi_cs_deassert(void)
{
    LL_GPIO_SetOutputPin(SPI_CS_GPIO_Port, SPI_CS_Pin);
}

uint8_t mmhal_wlan_spi_rw(uint8_t data)
{
    while (!LL_SPI_IsActiveFlag_TXE(SPI_PERIPH))
    {
    }
    LL_SPI_TransmitData8(SPI_PERIPH, data);
    while (!LL_SPI_IsActiveFlag_RXNE(SPI_PERIPH))
    {
    }
    return LL_SPI_ReceiveData8(SPI_PERIPH);
}

static void spi_dma_streams_disable(void)
{
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);
    while (LL_DMA_IsEnabledStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM) ||
           LL_DMA_IsEnabledStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM))
    {
    }
}

/* F4 streams refuse to start while a previous transfer's event flags are still set */
static void spi_dma_flags_clear(void)
{
    LL_DMA_ClearFlag_TC0(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_HT0(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_TE0(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_DME0(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_FE0(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_TC3(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_HT3(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_TE3(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_DME3(SPI_DMA_PERIPH);
    LL_DMA_ClearFlag_FE3(SPI_DMA_PERIPH);
}

static void spi_dma_transfer(uint32_t tx_address, bool tx_increment,
                             uint32_t rx_address, bool rx_increment, unsigned len)
{
    uint32_t spi_data_register = LL_SPI_DMA_GetRegAddr(SPI_PERIPH);

    spi_dma_streams_disable();
    spi_dma_flags_clear();

    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM,
                            rx_increment ? LL_DMA_MEMORY_INCREMENT : LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM,
                            tx_increment ? LL_DMA_MEMORY_INCREMENT : LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_ConfigAddresses(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, spi_data_register, rx_address,
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_ConfigAddresses(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, tx_address, spi_data_register,
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, len);
    LL_DMA_SetDataLength(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, len);

    /* RM0090 order: RX request, both streams, then the TX request that starts clocking */
    LL_SPI_EnableDMAReq_RX(SPI_PERIPH);
    LL_DMA_EnableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_EnableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);
    LL_SPI_EnableDMAReq_TX(SPI_PERIPH);

    bool ok = mmosal_semb_wait(dma_semb_handle, DMA_WAIT_TMO);
    MMOSAL_ASSERT(ok);

    LL_SPI_DisableDMAReq_TX(SPI_PERIPH);
    LL_SPI_DisableDMAReq_RX(SPI_PERIPH);
    spi_dma_streams_disable();
}

void mmhal_wlan_spi_read_buf(uint8_t *buf, unsigned len)
{
    if (len < DMA_TRANSFER_MIN_LENGTH)
    {
        for (unsigned ii = 0; ii < len; ii++)
        {
            buf[ii] = mmhal_wlan_spi_rw(0xFF);
        }
        return;
    }

    uint8_t dummy_tx = 0xFF;
    spi_dma_transfer((uint32_t)&dummy_tx, false, (uint32_t)buf, true, len);
}

void mmhal_wlan_spi_write_buf(const uint8_t *buf, unsigned len)
{
    if (len < DMA_TRANSFER_MIN_LENGTH)
    {
        for (unsigned ii = 0; ii < len; ii++)
        {
            (void)mmhal_wlan_spi_rw(buf[ii]);
        }
        return;
    }

    uint8_t dummy_rx;
    spi_dma_transfer((uint32_t)buf, true, (uint32_t)&dummy_rx, false, len);
}

void mmhal_wlan_send_training_seq(void)
{
    mmhal_wlan_spi_cs_deassert();

    LL_GPIO_SetOutputPin(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin);
    LL_GPIO_SetPinMode(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin, LL_GPIO_MODE_OUTPUT);

    for (unsigned ii = 0; ii <= BYTE_TRAIN; ii++)
    {
        (void)mmhal_wlan_spi_rw(0xFF);
    }

    LL_GPIO_ResetOutputPin(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin);
    LL_GPIO_SetPinMode(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin, LL_GPIO_MODE_ALTERNATE);
}

void mmhal_wlan_register_spi_irq_handler(mmhal_irq_handler_t handler)
{
    spi_irq_handler = handler;
}

bool mmhal_wlan_spi_irq_is_asserted(void)
{
    return !LL_GPIO_IsInputPinSet(SPI_IRQ_GPIO_Port, SPI_IRQ_Pin);
}

void mmhal_wlan_set_spi_irq_enabled(bool enabled)
{
    if (enabled)
    {
        LL_EXTI_EnableIT_0_31(SPI_IRQ_LINE);

        /* The IRQ is level-like but wired as an edge, so catch one that was already pending */
        if (mmhal_wlan_spi_irq_is_asserted() && spi_irq_handler != NULL)
        {
            spi_irq_handler();
        }
        NVIC_EnableIRQ(SPI_IRQn);
    }
    else
    {
        NVIC_DisableIRQ(SPI_IRQn);
        LL_EXTI_DisableIT_0_31(SPI_IRQ_LINE);
    }
}

void mmhal_wlan_init(void)
{
    dma_semb_handle = mmosal_semb_create("dma_semb_handle");
    mmhal_wlan_assert_reset(false);
}

void mmhal_wlan_deinit(void)
{
    mmosal_semb_delete(dma_semb_handle);
    mmhal_wlan_assert_reset(true);
}

void mmhal_wlan_wake_assert(void)
{
    LL_GPIO_SetOutputPin(WAKE_GPIO_Port, WAKE_Pin);
}

void mmhal_wlan_wake_deassert(void)
{
    LL_GPIO_ResetOutputPin(WAKE_GPIO_Port, WAKE_Pin);
}

/* The XIAO HaLow board leaves BUSY unconnected (R17 DNP) and pulls WAKE high, so the chip never sleeps */
bool mmhal_wlan_busy_is_asserted(void)
{
    return true;
}

void mmhal_wlan_register_busy_irq_handler(mmhal_irq_handler_t handler)
{
    MM_UNUSED(handler);
}

void mmhal_wlan_set_busy_irq_enabled(bool enabled)
{
    MM_UNUSED(enabled);
}

void SPI_IRQ_HANDLER(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(SPI_IRQ_LINE))
    {
        LL_EXTI_ClearFlag_0_31(SPI_IRQ_LINE);
        if (spi_irq_handler != NULL)
        {
            spi_irq_handler();
        }
    }
}

void SPI_RX_DMA_IRQ_HANDLER(void)
{
    LL_DMA_ClearFlag_TC0(SPI_DMA_PERIPH);
    mmosal_semb_give_from_isr(dma_semb_handle);
}

/* Locally administered and stable per board, so the AP sees the same station across reboots */
void mmhal_read_mac_addr(uint8_t *mac_addr)
{
    if (mm_mac_addr_is_valid_sta(mac_addr))
    {
        return;
    }

    uint32_t uid = LL_GetUID_Word0() ^ LL_GetUID_Word1() ^ LL_GetUID_Word2();
    mac_addr[0] = 0x02;
    mac_addr[1] = 0x00;
    memcpy(&mac_addr[2], &uid, sizeof(uid));
}

const struct mmhal_chip *mmhal_get_chip(void)
{
    return &MMHAL_CHIP_TYPE;
}
