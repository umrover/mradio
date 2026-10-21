# NUCLEO-H755ZI-Q (CM7) on the SDK's mm-ekh08-h753 platform; platform/ holds only changed files

CORE = arm-cortex-m7f

MMHAL_CHIP_TYPE ?= mmhal_mm6108
BUILD_DEFINES += MMHAL_CHIP_TYPE=$(MMHAL_CHIP_TYPE)

FW_MBIN ?= mm6108.mbin

BSP_DRIVERS_DIR = src/bsps/stm32cubeh7
SDK_PLATFORM_DIR = src/platforms/mm-ekh08-h753

HAL_SRCS_C = $(addprefix stm32h7xx_hal_,spi rcc rcc_ex flash flash_ex gpio dma dma_ex pwr pwr_ex \
	cortex exti tim tim_ex uart rng rng_ex)
HAL_SRCS_C += stm32h7xx_hal
HAL_SRCS_C += $(addprefix stm32h7xx_ll_,rcc rtc utils exti spi gpio dma usart)
MMIOT_SRCS_C += $(patsubst %,$(BSP_DRIVERS_DIR)/Drivers/STM32H7xx_HAL_Driver/Src/%.c,$(HAL_SRCS_C))

MMIOT_SRCS_C += $(addprefix $(SDK_PLATFORM_DIR)/bsp/Core/Src/,stm32h7xx_it.c stm32h7xx_hal_msp.c \
	stm32h7xx_hal_timebase_tim.c system_stm32h7xx.c)
MMIOT_SRCS_C += $(addprefix $(SDK_PLATFORM_DIR)/mm_shims/,mm_logging.c mmhal_core.c mmhal_littlefs.c \
	mmhal_wlan_binaries.c mmosal_shim_alloc.c mmosal_shim_fileio.c mmosal_shim_freertos.c \
	mmosal_shim_libc_stubs.c)
MMIOT_SRCS_S += $(SDK_PLATFORM_DIR)/bsp/startup_stm32h753zitx.s

SRCS_C += $(wildcard $(PLATFORM_DIR)/*.c)

MMIOT_INCLUDES += $(SDK_PLATFORM_DIR)/bsp/Core/Inc
MMIOT_INCLUDES += $(SDK_PLATFORM_DIR)/mm_shims
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/STM32H7xx_HAL_Driver/Inc
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Include
MMIOT_INCLUDES += $(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32H7xx/Include

BUILD_DEFINES += USE_HAL_DRIVER
BUILD_DEFINES += STM32H753xx
BUILD_DEFINES += USE_FULL_LL_DRIVER

CONLYFLAGS += -include $(MMIOT_ROOT)/$(BSP_DRIVERS_DIR)/Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h753xx.h
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-c++-compat
CFLAGS-$(BSP_DRIVERS_DIR) += -Wno-unused-parameter
CFLAGS-$(SDK_PLATFORM_DIR) += -Wno-unused-parameter
APP_CFLAGS += -Wno-unused-parameter

LINKFLAGS += --entry Reset_Handler
LINKFLAGS += -Wl,--defsym,HEAPSIZE=0x1000
LINKFLAGS += -Wl,--defsym,STACKSIZE=0x1000
LINKFLAGS += -Wl,--defsym,_CSTACK_Base=0x00000000
LINKFLAGS += -Wl,--defsym,_CSTACK_Length=0x1000
LINKFLAGS += -Wl,--undefined=uxTopUsedPriority
LINKFLAGS += -Wl,--print-memory-usage

LD_FILES += $(PLATFORM_DIR)/flash.ld
