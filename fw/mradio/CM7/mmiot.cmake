set(MMIOT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../mm-iot-sdk/framework)
set(MMIOT_SHIMS ${MMIOT_ROOT}/src/platforms/mm-ekh08-h753/mm_shims)
set(MMIOT_FW ${MMIOT_ROOT}/morsefirmware)
set(MMIOT_FW_MBIN mm6108.mbin)
# Calibration for the MM6108-MF16858 module on the Wio-WM6108, the same card the dev board used
set(MMIOT_BCF_MBIN bcf_mf16858.mbin)

# Replaced by mmosal_shim_libc_stubs.c and the SDK heap, which adds the realloc mmosal needs
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_5.c
    PROPERTIES HEADER_FILE_ONLY TRUE
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mmiot/mmhal_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mmiot/mmhal_os.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mmiot/mmhal_wlan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mmiot/libc_file_stubs.c
    ${MMIOT_SHIMS}/mm_logging.c
    ${MMIOT_SHIMS}/mmhal_core.c
    ${MMIOT_SHIMS}/mmhal_wlan_binaries.c
    ${MMIOT_SHIMS}/mmosal_shim_alloc.c
    ${MMIOT_SHIMS}/mmosal_shim_freertos.c
    ${MMIOT_SHIMS}/mmosal_shim_libc_stubs.c
    ${MMIOT_ROOT}/src/freertos/mmiot/heap_5_mm.c
    ${MMIOT_ROOT}/src/mmutils/mmutils_wlan.c
    ${MMIOT_ROOT}/src/mmutils/mmbuf.c
    ${MMIOT_ROOT}/src/mmutils/mmcrc.c
    ${MMIOT_ROOT}/src/mmpktmem/mmpktmem_heap.c
    ${MMIOT_ROOT}/src/mmregdb/mmregdb.c
    ${CMAKE_CURRENT_BINARY_DIR}/${MMIOT_FW_MBIN}.o
    ${CMAKE_CURRENT_BINARY_DIR}/${MMIOT_BCF_MBIN}.o
)

# Appended after the Cube paths so the SDK's FreeRTOSConfig.h does not shadow ours
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${MMIOT_ROOT}/morselib/include
    ${MMIOT_SHIMS}
    ${MMIOT_ROOT}/src/freertos/mmiot/include
    ${MMIOT_ROOT}/src/mmutils
    ${MMIOT_ROOT}/src/mmpktmem
    ${MMIOT_ROOT}/src/mmregdb
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    MMHAL_CHIP_TYPE=mmhal_mm6108
    EXCLUDE_MMCONFIG
    INCLUDE_BCF_FILE_IN_APPLICATION
    LOOKAROUND_FAIL_MAX=50
    # mmutils.h, included by mmlog.h, mmipal.h and lwipopts.h since SDK 2.14, calls POSIX strnlen, which strict C++ hides
    $<$<COMPILE_LANGUAGE:CXX>:_POSIX_C_SOURCE=200809L>
)

# Built from source as the SDK does by default, since the deprecated prebuilt libmorse.a has no supplicant AP support
file(GLOB_RECURSE MORSELIB_SRCS ${MMIOT_ROOT}/morselib/src/*.c ${MMIOT_ROOT}/morselib/mmrc/*.c)

set(HOSTAP_DIR ${MMIOT_ROOT}/src/hostap)
set(HOSTAP_SRCS
    crypto_mbedtls_mm.c drivers_morse.c morse_stubs.c os_mmosal.c
    src/common/dragonfly.c src/common/hw_features_common.c src/common/ieee802_11_common.c
    src/common/ptksa_cache.c src/common/sae.c src/common/wpa_common.c
    src/crypto/aes-ctr.c src/crypto/aes-siv.c src/crypto/aes-unwrap.c src/crypto/dh_groups.c
    src/crypto/sha256-kdf.c src/crypto/sha384-kdf.c src/crypto/sha512-kdf.c src/crypto/tls_none.c
    src/drivers/driver_common.c src/eap_common/eap_common.c src/eap_peer/eap.c src/eap_peer/eap_methods.c
    src/eapol_supp/eapol_supp_sm.c src/rsn_supp/pmksa_cache.c src/rsn_supp/preauth.c src/rsn_supp/wpa.c
    src/rsn_supp/wpa_ie.c src/utils/bitfield.c src/utils/common.c src/utils/crc32.c src/utils/wpa_debug.c
    src/utils/wpabuf.c
    wpa_supplicant/autoscan.c wpa_supplicant/autoscan_exponential.c wpa_supplicant/bgscan.c
    wpa_supplicant/bgscan_simple.c wpa_supplicant/bss.c wpa_supplicant/bssid_ignore.c wpa_supplicant/config.c
    wpa_supplicant/eap_register.c wpa_supplicant/events.c wpa_supplicant/notify.c wpa_supplicant/op_classes.c
    wpa_supplicant/scan.c wpa_supplicant/sme.c wpa_supplicant/wmm_ac.c wpa_supplicant/wnm_sta.c
    wpa_supplicant/wpas_glue.c wpa_supplicant/wpa_supplicant.c
)
# The SDK's BUILD_SUPPLICANT_WITH_AP set
list(APPEND HOSTAP_SRCS
    src/ap/ap_config.c src/ap/ap_drv_ops.c src/ap/ap_list.c src/ap/ap_mlme.c src/ap/authsrv.c src/ap/beacon.c
    src/ap/bss_load.c src/ap/comeback_token.c src/ap/dfs.c src/ap/drv_callbacks.c src/ap/eap_user_db.c
    src/ap/hostapd.c src/ap/hw_features.c src/ap/ieee802_11.c src/ap/ieee802_11_auth.c src/ap/ieee802_11_ht.c
    src/ap/ieee802_11_s1g.c src/ap/ieee802_11_shared.c src/ap/ieee802_1x.c src/ap/neighbor_db.c
    src/ap/pmksa_cache_auth.c src/ap/rrm.c src/ap/sta_info.c src/ap/tkip_countermeasures.c src/ap/utils.c
    src/ap/wmm.c src/ap/wpa_auth.c src/ap/wpa_auth_glue.c src/ap/wpa_auth_ie.c
    src/eap_server/eap_server.c src/eap_server/eap_server_methods.c src/eapol_auth/eapol_auth_sm.c
    src/pasn/pasn_responder.c wpa_supplicant/ap.c
)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE CONFIG_AP=1 NEED_AP_MLME=1)
list(TRANSFORM HOSTAP_SRCS PREPEND ${HOSTAP_DIR}/)

# The SDK's mbedtls.mk list minus TLS, X.509 and the lwIP socket shim, which the supplicant never calls
set(MBEDTLS_SRCS
    aes.c aria.c asn1parse.c asn1write.c base64.c bignum.c bignum_core.c bignum_mod.c bignum_mod_raw.c camellia.c
    ccm.c chacha20.c chachapoly.c cipher.c cipher_wrap.c cmac.c constant_time.c ctr_drbg.c des.c dhm.c ecdh.c
    ecdsa.c ecjpake.c ecp.c ecp_curves.c entropy.c entropy_poll.c gcm.c hkdf.c hmac_drbg.c mbedtls_platform.c
    md.c md5.c nist_kw.c oid.c pk.c pk_ecc.c pk_wrap.c pkcs5.c platform.c platform_util.c poly1305.c ripemd160.c
    rsa.c rsa_alt_helpers.c sha1.c sha256.c sha512.c
)
list(TRANSFORM MBEDTLS_SRCS PREPEND ${MMIOT_ROOT}/src/mbedtls/library/)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MORSELIB_SRCS} ${HOSTAP_SRCS} ${MBEDTLS_SRCS})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${MMIOT_ROOT}/morselib/src
    ${MMIOT_ROOT}/morselib/src/internal
    ${MMIOT_ROOT}/morselib/src/umac/rc/mmrc_osal
    ${MMIOT_ROOT}/morselib/mmrc/src/core
    ${HOSTAP_DIR}
    ${HOSTAP_DIR}/src
    ${HOSTAP_DIR}/src/common
    ${HOSTAP_DIR}/src/crypto
    ${HOSTAP_DIR}/src/utils
    ${HOSTAP_DIR}/wpa_supplicant
    ${MMIOT_ROOT}/src/mbedtls/include
    # morselib reaches hostap headers as hostap/...
    ${MMIOT_ROOT}/src
)

# Same supplicant configuration as the SDK's wpa_supplicant.mk, which warns it is the only tested one
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    IEEE8021X_EAPOL=1 CONFIG_SME=1 CONFIG_SAE=1 CONFIG_OWE=1 CONFIG_ECC=1
    CONFIG_SHA256=1 CONFIG_SHA384=1 CONFIG_SHA512=1 CONFIG_FIPS=1
    CONFIG_NO_ACCOUNTING=1 CONFIG_NO_BSS_TRANS_MGMT=1 CONFIG_NO_CONFIG_BLOBS=1 CONFIG_NO_CONFIG_WRITE=1
    CONFIG_NO_RADIUS=1 CONFIG_NO_RANDOM_POOL=1 CONFIG_NO_RC4=1 CONFIG_NO_ROBUST_AV=1 CONFIG_NO_RRM=1
    CONFIG_NO_VLAN=1 CONFIG_NO_WPA_MSG=1 CONFIG_NO_STDOUT_DEBUG=1
    CONFIG_WNM=1 CONFIG_IEEE80211AH=1 CONFIG_DRIVER_NL80211_MORSE=1 CONFIG_S1G_TWT=1
    CONFIG_BGSCAN=1 CONFIG_BGSCAN_SIMPLE=1 CONFIG_AUTOSCAN=1 CONFIG_AUTOSCAN_EXPONENTIAL=1
    CONFIG_OPENSSL_INTERNAL_AES_WRAP=1 OS_NO_C_LIB_DEFINES=1 MM_IOT MM_IOT_DPP_HEADER=1
    MAX_NUM_MLD_LINKS=1 MAX_NUM_MLO_LINKS=1 WPA_SUPPLICANT_CLEANUP_INTERVAL=120
    # The AP example's board hookups for mbedTLS heap, entropy and time, which mbedTLS cannot build without
    MBEDTLS_CONFIG_FILE="${MMIOT_ROOT}/../examples/ap_mode/src/mbedtls_config.h"
)

# The radio stack runs at -Os like Morse's production builds, since -O3 grows it past the 1 MB bank
set_source_files_properties(${MORSELIB_SRCS} PROPERTIES COMPILE_OPTIONS "-Os")
set_source_files_properties(${HOSTAP_SRCS} PROPERTIES COMPILE_OPTIONS
    "-Os;-Wno-unused-but-set-variable;-Wno-unused-function;-Wno-unused-parameter;-Wno-unused-variable;-Wno-format;-Wno-maybe-uninitialized;-Wno-dangling-else;-Wno-type-limits;-Wno-sign-compare;-Wno-parentheses;-Wno-packed-not-aligned;-Wno-misleading-indentation"
)
set_source_files_properties(${MBEDTLS_SRCS} PROPERTIES COMPILE_OPTIONS "-Os;-Wno-sign-compare;-Wno-unused-but-set-variable")

# Same objcopy rules as the SDK's morsefirmware.mk
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MMIOT_FW_MBIN}.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-littlearm -B arm
        ${MMIOT_FW_MBIN} ${CMAKE_CURRENT_BINARY_DIR}/${MMIOT_FW_MBIN}.o
        --redefine-sym _binary_mm6108_mbin_start=firmware_binary_start
        --redefine-sym _binary_mm6108_mbin_end=firmware_binary_end
        --rename-section .data=.rodata._fw_mbin,contents,alloc,load,readonly,data
        --set-section-alignment .data=4
    WORKING_DIRECTORY ${MMIOT_FW}
    DEPENDS ${MMIOT_FW}/${MMIOT_FW_MBIN}
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MMIOT_BCF_MBIN}.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-littlearm -B arm
        ${MMIOT_BCF_MBIN} ${CMAKE_CURRENT_BINARY_DIR}/${MMIOT_BCF_MBIN}.o
        --redefine-sym _binary_bcf_mf16858_mbin_start=bcf_binary_start
        --redefine-sym _binary_bcf_mf16858_mbin_end=bcf_binary_end
        --rename-section .data=.rodata,contents,alloc,load,readonly,data
        --set-section-alignment .data=4
    WORKING_DIRECTORY ${MMIOT_FW}/mm6108/bcfs
    DEPENDS ${MMIOT_FW}/mm6108/bcfs/${MMIOT_BCF_MBIN}
)

file(GLOB MMIOT_LWIP_SRCS
    ${MMIOT_ROOT}/src/lwip/src/core/*.c
    ${MMIOT_ROOT}/src/lwip/src/core/ipv4/*.c
)
# Only the tcpip thread, since nothing here uses the netconn or socket APIs
list(APPEND MMIOT_LWIP_SRCS ${MMIOT_ROOT}/src/lwip/src/api/tcpip.c)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${MMIOT_LWIP_SRCS}
    ${MMIOT_ROOT}/src/lwip/src/netif/ethernet.c
    ${MMIOT_ROOT}/src/lwip/port/arch/sys_arch.c
    ${MMIOT_ROOT}/src/mmipal/lwip/mmipal_lwip.c
    ${MMIOT_ROOT}/src/mmipal/lwip/mmnetif.c
    ${MMIOT_ROOT}/src/mmping/mmping_lwip.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${MMIOT_ROOT}/src/lwip/src/include
    ${MMIOT_ROOT}/src/lwip/src/include/compat/posix
    ${MMIOT_ROOT}/src/lwip/port
    ${MMIOT_ROOT}/src/mmipal
    ${MMIOT_ROOT}/src/mmipal/lwip
    ${MMIOT_ROOT}/src/mmping
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    LWIP_IPV4=1
    LWIP_IPV6=0
    # The radio link only carries UDP and ping, so TCP is dead weight in flash
    LWIP_TCP=0
    MMIPAL_IPV4_ENABLED=1
    MMIPAL_IPV6_ENABLED=0
    # Morse's on-demand timer patch assumes TCP, and plain periodic timers cost nothing with power save off
    MORSE_LWIP_TIMERS_ON_DEMAND=0
)

set_source_files_properties(${MMIOT_LWIP_SRCS} PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter)
