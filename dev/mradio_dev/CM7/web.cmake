set(MRADIO_WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../web)

# captures git tag version into MRADIO_FW_VERSION
execute_process(
    COMMAND git describe --always --dirty --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE MRADIO_FW_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# if not git repo
if(NOT MRADIO_FW_VERSION)
    set(MRADIO_FW_VERSION unknown)
endif()
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE MRADIO_FW_VERSION="${MRADIO_FW_VERSION}")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/index.html.o)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index.html.o
    # html to binary
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-littlearm -B arm
        index.html ${CMAKE_CURRENT_BINARY_DIR}/index.html.o
        # read only data to avoid using ram
        --rename-section .data=.rodata.index_html,contents,alloc,load,readonly,data
    WORKING_DIRECTORY ${MRADIO_WEB_DIR}
    DEPENDS ${MRADIO_WEB_DIR}/index.html
)
