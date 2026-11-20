/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

#include "stm32h7xx_ll_rtc.h"
#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_spi.h"
#include "stm32h7xx_ll_usart.h"
#include "stm32h7xx_ll_system.h"
#include "stm32h7xx_ll_gpio.h"
#include "stm32h7xx_ll_exti.h"
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_cortex.h"
#include "stm32h7xx_ll_utils.h"
#include "stm32h7xx_ll_pwr.h"
#include "stm32h7xx_ll_dma.h"
#include "stm32h7xx_ll_hsem.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define SPI_PERIPH (SPI1)
#define SPI_DMA_PERIPH (DMA1)
#define SPI_RX_DMA_STREAM (LL_DMA_STREAM_1)
#define SPI_TX_DMA_STREAM (LL_DMA_STREAM_0)

#define SPI_IRQn (EXTI9_5_IRQn)
#define SPI_IRQ_LINE (LL_EXTI_LINE_6)

#define LOG_USART (USART1)
#define LOG_USART_IRQ (USART1_IRQn)
#define LOG_USART_IRQ_HANDLER USART1_IRQHandler
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

void PostInit(void);

void mmhal_wlan_spi_irq(void);
void mmhal_wlan_dma_rx_irq(void);
void mmhal_wlan_dma_tx_irq(void);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define WM_RESET_Pin LL_GPIO_PIN_2
#define WM_RESET_GPIO_Port GPIOE
#define WM_WAKE_Pin LL_GPIO_PIN_3
#define WM_WAKE_GPIO_Port GPIOE
#define WM_BUSY_Pin LL_GPIO_PIN_4
#define WM_BUSY_GPIO_Port GPIOE
#define M7_DBG1_Pin LL_GPIO_PIN_9
#define M7_DBG1_GPIO_Port GPIOF
#define M7_DBG2_Pin LL_GPIO_PIN_10
#define M7_DBG2_GPIO_Port GPIOF
#define ETH_RESET_Pin LL_GPIO_PIN_4
#define ETH_RESET_GPIO_Port GPIOA
#define ETH_INT_Pin LL_GPIO_PIN_5
#define ETH_INT_GPIO_Port GPIOA
#define ETH_INT_EXTI_IRQn EXTI9_5_IRQn
#define WL_REG_ON_Pin LL_GPIO_PIN_6
#define WL_REG_ON_GPIO_Port GPIOD
#define WL_DEV_WAKE_Pin LL_GPIO_PIN_7
#define WL_DEV_WAKE_GPIO_Port GPIOD
#define WL_HOST_WAKE_Pin LL_GPIO_PIN_9
#define WL_HOST_WAKE_GPIO_Port GPIOG
#define WL_HOST_WAKE_EXTI_IRQn EXTI9_5_IRQn
#define SPI1_CS_Pin LL_GPIO_PIN_14
#define SPI1_CS_GPIO_Port GPIOG
#define SPI1_MOSI_Pin LL_GPIO_PIN_5
#define SPI1_MOSI_GPIO_Port GPIOB
#define SPI_INT_Pin LL_GPIO_PIN_6
#define SPI_INT_GPIO_Port GPIOB
#define SPI_INT_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */
#define LED_WIFI_5_Pin LL_GPIO_PIN_15
#define LED_WIFI_5_GPIO_Port GPIOD
#define LED_WIFI_2_4_Pin LL_GPIO_PIN_6
#define LED_WIFI_2_4_GPIO_Port GPIOG
#define LED_HALOW_Pin LL_GPIO_PIN_7
#define LED_HALOW_GPIO_Port GPIOG
#define LED_SYSTEM_Pin LL_GPIO_PIN_8
#define LED_SYSTEM_GPIO_Port GPIOG

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
