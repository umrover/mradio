#
# NUCLEO-F439ZI + Seeed XIAO HaLow board. LL drivers only, so no HAL timebase competes with FreeRTOS for SysTick.
#

CORE = arm-cortex-m4f

MMHAL_CHIP_TYPE ?= mmhal_mm6108
BUILD_DEFINES += MMHAL_CHIP_TYPE=$(MMHAL_CHIP_TYPE)

FW_MBIN ?= mm6108.mbin

LL_DIR = $(APP_DIR)/drivers/stm32f4xx-hal-driver
DEVICE_DIR = $(APP_DIR)/drivers/cmsis-device-f4

SRCS_C += $(addprefix $(LL_DIR)/Src/stm32f4xx_ll_,dma.c exti.c gpio.c rcc.c rng.c spi.c usart.c utils.c)
SRCS_C += $(DEVICE_DIR)/Source/Templates/system_stm32f4xx.c
SRCS_S += $(DEVICE_DIR)/Source/Templates/gcc/startup_stm32f439xx.s

SRCS_C += $(wildcard $(PLATFORM_DIR)/*.c)
SRCS_H += $(wildcard $(PLATFORM_DIR)/*.h)
SRCS_C += $(wildcard $(PLATFORM_DIR)/mm_shims/*.c)
SRCS_H += $(wildcard $(PLATFORM_DIR)/mm_shims/*.h)

INCLUDES += $(PLATFORM_DIR)
INCLUDES += $(PLATFORM_DIR)/mm_shims
INCLUDES += $(LL_DIR)/Inc
INCLUDES += $(DEVICE_DIR)/Include
MMIOT_INCLUDES += src/bsps/stm32cubeh7/Drivers/CMSIS/Include

BUILD_DEFINES += STM32F439xx
BUILD_DEFINES += USE_FULL_LL_DRIVER

APP_CFLAGS += -Wno-unused-parameter

LINKFLAGS += --entry Reset_Handler
LINKFLAGS += -Wl,--undefined=uxTopUsedPriority
LINKFLAGS += -Wl,--print-memory-usage

LD_FILES += $(PLATFORM_DIR)/flash.ld
