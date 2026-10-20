#
# demo/ ported to a NUCLEO-F439ZI while the H755 boards are unavailable. App sources are shared via the src symlink.
#
#   make APP=ap_mode    board 1, HaLow access point at 192.168.1.1
#   make APP=button     board 2, station at 192.168.1.2; its user button blinks board 1's LED
#

APP ?= button
ifeq ($(filter $(APP),ap_mode button),)
$(error APP must be ap_mode or button, got '$(APP)')
endif
APP_NAME = $(APP)

MMIOT_ROOT ?= $(abspath $(CURDIR)/../demo/mm-iot-sdk/framework)

# Morse expects its xpack gcc 14.2 in /opt/morse; the STM32CubeCLT gcc 14.3 links the same prebuilt libs
ifeq ($(wildcard /opt/morse/xpack-arm-none-eabi-gcc-14.2.1-1.1),)
TOOLCHAIN_DIR ?= $(firstword $(wildcard /opt/st/stm32cubeclt_*/GNU-tools-for-STM32))
endif

-include $(MMIOT_ROOT)/mk/misc.mk

BUILDTYPE ?= production
BUILD_DIR ?= build/$(APP)

ELF_FILE = $(BUILD_DIR)/$(APP_NAME).elf
BIN_FILE = $(BUILD_DIR)/$(APP_NAME).bin

ifeq ($(VERBOSE),)
QUIET = @
endif

BUILD_SUPPLICANT_CRYPTO_FROM_SOURCE ?= y
BUILD_WITH_MBEDTLS_ECC_HW_CRYPTO ?= y
IP_STACK ?= lwip
MMIPAL_IPV4_ENABLED ?= 1
MMIPAL_IPV6_ENABLED ?= 0

# Settings are compiled in so the boards need no config store programming
EXCLUDE_MMCONFIG = 1
BCF_MBIN_REL = mm6108/bcfs/bcf_mf16858.mbin
COUNTRY_CODE ?= US
DEMO_SSID ?= mradio
DEMO_PASSPHRASE ?= mradiodemo

BUILD_DEFINES += COUNTRY_CODE=$(COUNTRY_CODE)
BUILD_DEFINES += SSID=$(DEMO_SSID) AP_SSID=$(DEMO_SSID)
BUILD_DEFINES += SAE_PASSPHRASE=$(DEMO_PASSPHRASE)

ifeq ($(APP),ap_mode)
ENABLE_AP_MODE = y
MAX_STAS ?= 4
BUILD_DEFINES += MAX_STAS=$(MAX_STAS)
else
# The AP runs no DHCP server, so the station takes the default static 192.168.1.2
BUILD_DEFINES += ENABLE_DHCP=0
endif

APP_DIR = $(CURDIR)
PLATFORM_DIR = $(APP_DIR)/platform
SRCS_C = $(wildcard $(APP_DIR)/src/common/*.c) $(wildcard $(APP_DIR)/src/$(APP)/*.c)
SRCS_H = $(wildcard $(APP_DIR)/src/common/*.h)
INCLUDES += $(APP_DIR)/src/common

CFLAGS += -Werror -Wall -Wextra
CONLYFLAGS += -Wc++-compat
CONLYFLAGS += -std=gnu11

BUILD_DEFINES += EXCLUDE_MMCONFIG=$(EXCLUDE_MMCONFIG)

ifneq ($(ENABLE_AP_MODE),)
BUILD_SUPPLICANT_WITH_AP=y
BUILD_DEFINES += AP_MODE_HEAP=$(shell echo $$(($(MAX_STAS) * 5000)))
DEBUG_BUILD_DEFINES += MAX_ALLOCATIONS=$(shell echo $$((250 + ($(MAX_STAS) * 40))))
MMPKTMEM_TX_POOL_N_BLOCKS = $(shell echo $$((32 + ($(MAX_STAS) * 2))))
MMPKTMEM_RX_POOL_N_BLOCKS = $(shell echo $$((32 + ($(MAX_STAS) * 2))))
BUILD_DEFINES += MMPKTMEM_TX_MGMT_POOL_N_BLOCKS=4
endif

ifneq ($(BUILD_SUPPLICANT_WITH_AP)$(BUILD_SUPPLICANT_WITH_DPP)$(BUILD_MORSELIB_FROM_SOURCE),)
BUILD_SUPPLICANT_FROM_SOURCE=y
endif

all:

include $(MMIOT_ROOT)/mk/utils.mk
include $(MMIOT_ROOT)/mk/app_utils.mk

include $(APP_DIR)/platform.mk

include $(MMIOT_ROOT)/mk/core-$(CORE).mk

include $(MMIOT_ROOT)/mk/mmconfig.mk
include $(MMIOT_ROOT)/mk/slip.mk
include $(MMIOT_ROOT)/mk/mmutils.mk
include $(MMIOT_ROOT)/mk/littlefs.mk
include $(MMIOT_ROOT)/mk/freertos-common.mk
include $(MMIOT_ROOT)/mk/freertos-$(CORE).mk
include $(MMIOT_ROOT)/mk/morselib.mk
include $(MMIOT_ROOT)/mk/morsefirmware.mk
include $(MMIOT_ROOT)/mk/wpa_supplicant.mk
include $(MMIOT_ROOT)/mk/mbedtls.mk
include $(MMIOT_ROOT)/mk/mmipal.mk
include $(MMIOT_ROOT)/mk/mmpktmem.mk
include $(MMIOT_ROOT)/mk/$(IP_STACK).mk
include $(MMIOT_ROOT)/mk/mmregdb.mk

SRCS_H += $(addprefix $(MMIOT_ROOT)/,$(MMIOT_SRCS_H))
$(eval $(call check_for_files,$(SRCS_H)))

OBJS += $(patsubst $(APP_DIR)/%.s,$(BUILD_DIR)/%.o,$(SRCS_S))
OBJS += $(patsubst $(APP_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS_C))
OBJS += $(patsubst %.c,$(BUILD_DIR)/%.o,$(MMIOT_SRCS_C))
OBJS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(patsubst %.s,$(BUILD_DIR)/%.o,$(MMIOT_SRCS_S)))

NON_OBJ_OBJS := $(filter-out %.o,$(OBJS))
ifneq ($(NON_OBJ_OBJS),)
$(error OBJS contains non-.o entries: $(NON_OBJ_OBJS))
endif

LIBS += $(addprefix $(MMIOT_ROOT)/,$(MMIOT_LIBS))

CFLAGS += $(addprefix -I,$(INCLUDES))
CFLAGS += $(addprefix -I$(MMIOT_ROOT)/,$(MMIOT_INCLUDES))
CFLAGS += $(addprefix -D,$(BUILD_DEFINES))
CFLAGS += $(CSPECS)

ifeq ($(DISABLE_FILEID_GENERATION),)
FILEHASHES_LIST=$(BUILD_DIR)/filehashes.txt
CFLAGS += -DMMOSAL_FILEID=$(call uint32_hash,$<)
endif

ifeq ($(BUILDTYPE),debug)
DEBUG_CFLAGS += -Og -g3 -gdwarf-5 -fvar-tracking-assignments
DEBUG_BUILD_DEFINES += MMOSAL_TRACK_ALLOCATIONS ENABLE_MMOSAL_DEV_ASSERT HALT_ON_ASSERT RESET_MM_ON_HALT
CFLAGS += $(DEBUG_CFLAGS)
BUILD_DEFINES += $(DEBUG_BUILD_DEFINES)
else
CFLAGS += -Os -g
endif

LINKER ?= $(CC)
LINKFLAG_PREFIX = -Wl,-T
LINKFLAGS += $(addprefix $(LINKFLAG_PREFIX),$(LD_FILES))

.PHONY: all bin clean flash
all: $(ELF_FILE) $(BIN_FILE) $(APP_DIR)/compile_commands.json
bin: $(BIN_FILE)

# With both boards plugged in, pick one by ST-LINK serial: STM32_Programmer_CLI --list
flash: $(ELF_FILE)
	STM32_Programmer_CLI --connect port=swd $(if $(SN),sn=$(SN)) reset=HWrst mode=UR --erase all --write $(ELF_FILE) --verify --start

# For clangd. Both apps' sources get this app's flags; source dirs are prerequisites so adding a file regenerates it
comma := ,
CLANGD_SRCS_C = $(sort $(SRCS_C) $(wildcard $(APP_DIR)/src/*/*.c))
clangd_entry = {"directory": "$(APP_DIR)", "file": "$(1)", "command": "$(subst ",\",$(CC) -c $(CFLAGS) $(CONLYFLAGS) $(2) $(1))"}
$(APP_DIR)/compile_commands.json: $(APP_DIR)/Makefile $(APP_DIR)/platform.mk $(sort $(dir $(CLANGD_SRCS_C)))
	@echo "$(MSGPFX)Generating $@"
	$(file >$@,[$(subst } {,}$(comma) {,$(strip \
		$(foreach src,$(CLANGD_SRCS_C),$(call clangd_entry,$(src),$(APP_CFLAGS))) \
		$(foreach src,$(MMIOT_SRCS_C),$(call clangd_entry,$(MMIOT_ROOT)/$(src),$(call file_cflags,$(src))))))])

clean:
	-rm -r build

$(BUILD_DIR)/%.o: $(MMIOT_ROOT)/%.s $(SRCS_H)
	@echo "$(MSGPFX)Assembling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(AS) -o $@ -c $(CFLAGS) $<

$(BUILD_DIR)/%.o: $(MMIOT_ROOT)/%.S $(SRCS_H)
	@echo "$(MSGPFX)Assembling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(AS) -o $@ -c $(CFLAGS) $<

$(BUILD_DIR)/%.o: $(MMIOT_ROOT)/%.c $(SRCS_H)
	@echo "$(MSGPFX)Compiling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(CC) -o $@ -c $(CFLAGS) $(CONLYFLAGS) $(call file_cflags,$(patsubst $(MMIOT_ROOT)/%,%,$<)) $<

$(BUILD_DIR)/%.o: $(APP_DIR)/%.s $(SRCS_H)
	@echo "$(MSGPFX)Assembling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(AS) -o $@ -c $(CFLAGS) $<

$(BUILD_DIR)/%.o: $(APP_DIR)/%.c $(SRCS_H)
	@echo "$(MSGPFX)Compiling $<"
	@mkdir -p $(dir $@)
	$(QUIET)$(CC) -o $@ -c $(CFLAGS) $(CONLYFLAGS) $(APP_CFLAGS) $<

$(ELF_FILE): $(OBJS) $(LIBS) $(LD_FILES) $(FILEHASHES_LIST)
	@echo "$(MSGPFX)Linking $@"
	$(QUIET)$(LINKER) -o $@ $(OBJS) $(CFLAGS) $(LINKFLAGS) $(LIBS)

$(BIN_FILE): $(ELF_FILE)
	@echo "$(MSGPFX)Generating $@"
	$(QUIET)$(OBJCOPY) -Obinary $< $@

ifeq ($(DISABLE_FILEID_GENERATION),)
$(FILEHASHES_LIST): $(SRCS_C) $(addprefix $(MMIOT_ROOT)/,$(MMIOT_SRCS_C))
	@mkdir -p $(dir $@)
	@bash -c 'echo -e $(foreach src,$^,$(call uint32_hash,$(src)) $(src)\\n)' > $@
endif
