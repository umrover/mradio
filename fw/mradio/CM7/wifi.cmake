set(AIROC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../airoc)
set(WHD_ROOT ${AIROC_ROOT}/wifi-host-driver/COMPONENT_WIFI6)

file(GLOB AIROC_SRCS
    ${WHD_ROOT}/src/*.c
    ${WHD_ROOT}/src/bus_protocols/*.c
    ${AIROC_ROOT}/pal/common/Src/*.c
    ${AIROC_ROOT}/pal/targets/TARGET_STM32/Src/*.c
    ${AIROC_ROOT}/pal/abstraction-rtos/source/*.c
    ${AIROC_ROOT}/pal/abstraction-rtos/source/COMPONENT_FREERTOS/*.c
)
# the UART half of the pal serves Bluetooth and needs HAL UART, which this firmware leaves out
list(FILTER AIROC_SRCS EXCLUDE REGEX "stm32_cyhal_uart\\.c$")
list(APPEND AIROC_SRCS
    ${WHD_ROOT}/resources/resource_imp/whd_resources.c
    ${WHD_ROOT}/resources/firmware/55572A1_bin.c
    ${WHD_ROOT}/resources/clm/55572A1_clm_blob.c
    ${AIROC_ROOT}/whd-bsp-integration/cybsp_wifi.c
    ${AIROC_ROOT}/whd-bsp-integration/COMPONENT_LWIP/cy_network_buffer_lwip.c
    ${AIROC_ROOT}/lwip-network-interface-integration/source/cy_network_mw_core.c
    ${AIROC_ROOT}/lwip-network-interface-integration/source/cy_lwip_dhcp_server.c
    ${AIROC_ROOT}/wifi-connection-manager/source/cy_wcm.c
    ${AIROC_ROOT}/connectivity-utilities/cy_log/cy_log.c
    ${AIROC_ROOT}/connectivity-utilities/cy_string/cy_string_utils.c
    ${AIROC_ROOT}/connectivity-utilities/linked_list/cy_linked_list.c
    ${AIROC_ROOT}/connectivity-utilities/JSON_parser/cy_json_parser.c
    ${AIROC_ROOT}/connectivity-utilities/network/cy_nw_helper_common.c
    ${AIROC_ROOT}/connectivity-utilities/network/COMPONENT_LWIP/cy_nw_helper.c
    # only cy_wcm_ping uses sockets, so they live here in bank 2 rather than with the rest of lwIP
    ${MMIOT_ROOT}/src/lwip/src/api/sockets.c
)

# a library, so wifi.ld can place all of it in bank 2
add_library(airoc STATIC ${AIROC_SRCS})
target_include_directories(airoc PUBLIC
    ${WHD_ROOT}/inc
    ${WHD_ROOT}/src
    ${WHD_ROOT}/src/include
    ${WHD_ROOT}/resources/resource_imp
    ${WHD_ROOT}/resources/firmware
    ${WHD_ROOT}/resources/clm
    ${WHD_ROOT}/resources/nvram
    ${AIROC_ROOT}/wifi-host-driver
    ${AIROC_ROOT}/whd-bsp-integration
    ${AIROC_ROOT}/lwip-network-interface-integration/include
    ${AIROC_ROOT}/lwip-network-interface-integration/source
    ${AIROC_ROOT}/wifi-connection-manager/include
    ${AIROC_ROOT}/wifi-connection-manager/source/COMPONENT_55572
    ${AIROC_ROOT}/connectivity-utilities
    ${AIROC_ROOT}/connectivity-utilities/cy_log
    ${AIROC_ROOT}/connectivity-utilities/cy_string
    ${AIROC_ROOT}/connectivity-utilities/linked_list
    ${AIROC_ROOT}/connectivity-utilities/JSON_parser
    ${AIROC_ROOT}/connectivity-utilities/network
    ${AIROC_ROOT}/pal/core-lib/include
    ${AIROC_ROOT}/pal/hal/include
    ${AIROC_ROOT}/pal/common/Inc
    ${AIROC_ROOT}/pal/targets/TARGET_STM32/Inc
    ${AIROC_ROOT}/pal/abstraction-rtos/include
    ${AIROC_ROOT}/pal/abstraction-rtos/include/COMPONENT_FREERTOS
)
# same HAL, FreeRTOS and lwIP config as the firmware itself
target_include_directories(airoc PRIVATE
    $<TARGET_PROPERTY:${CMAKE_PROJECT_NAME},INCLUDE_DIRECTORIES>
)
# the same symbols the CubeMX pack sets in wifi_demo, BLHS_SUPPORT for the CYW55572 bootloader handshake
target_compile_definitions(airoc PUBLIC CY_USING_HAL CYBSP_WIFI_CAPABLE BLHS_SUPPORT)
target_compile_definitions(airoc PRIVATE
    $<TARGET_PROPERTY:${CMAKE_PROJECT_NAME},COMPILE_DEFINITIONS>
)
option(WIFI_SDIO_1BIT "1-bit SDIO for a board whose D1 to D3 lines fail" OFF)
target_compile_definitions(airoc PUBLIC CYBSP_WIFI_SDIO_1BIT=$<BOOL:${WIFI_SDIO_1BIT}>)
# vendor code, kept small and quiet
target_compile_options(airoc PRIVATE -Os -w -include cmsis_compiler.h)

target_link_libraries(${CMAKE_PROJECT_NAME} airoc)
