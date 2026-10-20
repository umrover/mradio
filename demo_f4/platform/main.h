#pragma once

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_rng.h"
#include "stm32f4xx_ll_spi.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_utils.h"

#define SYSTEM_CLOCK_HZ         (168000000)

#define SPI_PERIPH              (SPI1)
#define SPI_DMA_PERIPH          (DMA2)
#define SPI_DMA_CHANNEL         (LL_DMA_CHANNEL_3)
#define SPI_RX_DMA_STREAM       (LL_DMA_STREAM_0)
#define SPI_TX_DMA_STREAM       (LL_DMA_STREAM_3)
#define SPI_RX_DMA_IRQn         (DMA2_Stream0_IRQn)
#define SPI_RX_DMA_IRQ_HANDLER  DMA2_Stream0_IRQHandler

#define SPI_IRQn                (EXTI3_IRQn)
#define SPI_IRQ_LINE            (LL_EXTI_LINE_3)
#define SPI_IRQ_HANDLER         EXTI3_IRQHandler

#define LOG_USART               (USART3)
#define LOG_USART_IRQ           (USART3_IRQn)
#define LOG_USART_IRQ_HANDLER   USART3_IRQHandler

#define B1_Pin                  LL_GPIO_PIN_13
#define B1_GPIO_Port            GPIOC
#define WAKE_Pin                LL_GPIO_PIN_0
#define WAKE_GPIO_Port          GPIOC
#define SPI_IRQ_Pin             LL_GPIO_PIN_3
#define SPI_IRQ_GPIO_Port       GPIOC
#define RESET_N_Pin             LL_GPIO_PIN_3
#define RESET_N_GPIO_Port       GPIOA
#define SPI_SCK_Pin             LL_GPIO_PIN_5
#define SPI_SCK_GPIO_Port       GPIOA
#define SPI_MISO_Pin            LL_GPIO_PIN_6
#define SPI_MISO_GPIO_Port      GPIOA
#define SPI_MOSI_Pin            LL_GPIO_PIN_5
#define SPI_MOSI_GPIO_Port      GPIOB
#define SPI_CS_Pin              LL_GPIO_PIN_14
#define SPI_CS_GPIO_Port        GPIOD
#define LED_GREEN_Pin           LL_GPIO_PIN_0
#define LED_GREEN_GPIO_Port     GPIOB
#define LED_BLUE_Pin            LL_GPIO_PIN_7
#define LED_BLUE_GPIO_Port      GPIOB
#define LED_RED_Pin             LL_GPIO_PIN_14
#define LED_RED_GPIO_Port       GPIOB
#define LOG_USART_TX_Pin        LL_GPIO_PIN_8
#define LOG_USART_RX_Pin        LL_GPIO_PIN_9
#define LOG_USART_GPIO_Port     GPIOD
