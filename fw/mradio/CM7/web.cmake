set(MRADIO_WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../web)

execute_process(
    COMMAND git describe --always --dirty --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE MRADIO_FW_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT MRADIO_FW_VERSION)
    set(MRADIO_FW_VERSION unknown)
endif()
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE MRADIO_FW_VERSION="${MRADIO_FW_VERSION}")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/index.html.o)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index.html.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-littlearm -B arm
        index.html ${CMAKE_CURRENT_BINARY_DIR}/index.html.o
        # read only data to avoid using ram
        --rename-section .data=.rodata.index_html,contents,alloc,load,readonly,data
    WORKING_DIRECTORY ${MRADIO_WEB_DIR}
    DEPENDS ${MRADIO_WEB_DIR}/index.html
)

set(MMIOT_HTTPD_SRCS
    ${MMIOT_ROOT}/src/lwip/src/apps/http/httpd.c
    ${MMIOT_ROOT}/src/lwip/src/apps/http/fs.c
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MMIOT_HTTPD_SRCS})
set_source_files_properties(${MMIOT_HTTPD_SRCS} PROPERTIES COMPILE_OPTIONS
    # fs_close frees the response before its ACK, so lwIP has to copy it for retransmits
    # a compile option since CMake drops function-like compile definitions
    "-Os;-Wno-unused-parameter;-DHTTP_IS_DATA_VOLATILE(hs)=TCP_WRITE_FLAG_COPY"
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    LWIP_HTTPD_SUPPORT_POST=1
    LWIP_HTTPD_DYNAMIC_HEADERS=1
    # every route is served by fs_open_custom in web.cpp
    LWIP_HTTPD_CUSTOM_FILES=1
    HTTPD_USE_CUSTOM_FSDATA=1
)

# ArduinoJson's templates take about 95K at -O0, which leaves a Debug build no room in the bank
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/web.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/config.cpp
    PROPERTIES COMPILE_OPTIONS -Os
)
