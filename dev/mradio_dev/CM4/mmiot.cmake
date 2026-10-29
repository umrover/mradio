set(MMIOT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../mm-iot-sdk/framework)
set(MMIOT_SHIMS ${MMIOT_ROOT}/src/platforms/mm-ekh08-h753/mm_shims)
set(MMIOT_FW ${MMIOT_ROOT}/morsefirmware)
set(MMIOT_FW_MBIN mm6108.mbin)
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
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    ${MMIOT_ROOT}/morselib/lib/arm-cortex-m4f/libmorse.a
)

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
