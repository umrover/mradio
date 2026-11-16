cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard" )

#Linker options
set(STM32_LINKER_SCRIPT stm32h745xx_flash_CM4.ld)
set(STM32_LINKER_OPTION  )

# Set the project name
set(CMAKE_PROJECT_NAME mradio_CM4)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated
include("mx-generated.cmake")


# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")

# TODO: for demo purposes, runs either ap or station code, remove later
set(HALOW_DEFAULT_ROLE "ap" CACHE STRING "HaLow role at boot, ap or sta")
set_property(CACHE HALOW_DEFAULT_ROLE PROPERTY STRINGS ap sta)
if(NOT HALOW_DEFAULT_ROLE MATCHES "^(ap|sta)$")
    message(FATAL_ERROR "HALOW_DEFAULT_ROLE must be ap or sta, got '${HALOW_DEFAULT_ROLE}'")
endif()
message("HaLow default role: " ${HALOW_DEFAULT_ROLE})

# Add project symbols (macros)
set(LINK_TEST_FLOOD OFF CACHE BOOL "Flood UDP at the peer once it is up")
set(LINK_TEST_PING ON CACHE BOOL "Ping the peer once a second during each window")
set(LINK_TEST_RATE_KBPS 0 CACHE STRING "Flood UDP payload rate in kbps, 0 for as fast as the link takes it")
set(HALOW_TX_POWER_DBM 0 CACHE STRING "Max TX power in dBm, 0 for the regulatory limit")
# SCK is 800 MHz / HALOW_SPI_PLL3_P / the prescaler, 20 MHz by default as before
set(HALOW_SPI_PLL3_P 10 CACHE STRING "PLL3 P divider for the radio SPI kernel clock")
set(HALOW_SPI_PRESCALER LL_SPI_BAUDRATEPRESCALER_DIV4 CACHE STRING "Radio SPI baud rate prescaler")
# each TX block holds one frame until the chip reports its TX status, so this caps frames in flight
set(HALOW_TX_POOL 20 CACHE STRING "Radio TX packet pool size in frames")
set(HEAP_SIZE 131072 CACHE STRING "FreeRTOS heap size in bytes, which holds the radio packet pools")
set(MORSELIB_TUNING "" CACHE STRING "Extra morselib defines for throughput experiments, semicolon separated")
set(HALOW_SPI_PIN_SPEED LL_GPIO_SPEED_FREQ_HIGH CACHE STRING "Radio SCK and MOSI slew rate")

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    HALOW_DEFAULT_ROLE_AP=$<STREQUAL:${HALOW_DEFAULT_ROLE},ap>
    LINK_TEST_FLOOD=$<BOOL:${LINK_TEST_FLOOD}>
    LINK_TEST_PING=$<BOOL:${LINK_TEST_PING}>
    LINK_TEST_RATE_KBPS=${LINK_TEST_RATE_KBPS}
    HALOW_TX_POWER_DBM=${HALOW_TX_POWER_DBM}
    HALOW_SPI_PLL3_P=${HALOW_SPI_PLL3_P}
    HALOW_SPI_PRESCALER=${HALOW_SPI_PRESCALER}
    HALOW_SPI_PIN_SPEED=${HALOW_SPI_PIN_SPEED}
    MMPKTMEM_TX_POOL_N_BLOCKS=${HALOW_TX_POOL}
    configTOTAL_HEAP_SIZE=${HEAP_SIZE}
    ${MORSELIB_TUNING}
)
# cmsis_os2.c, which owns ucHeap, builds in the generated FreeRTOS library rather than the app target
target_compile_definitions(FreeRTOS PRIVATE configTOTAL_HEAP_SIZE=${HEAP_SIZE})

# Enable CMake support for ASM, C, and C++
enable_language(C CXX ASM)

file(GLOB USER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.cpp")

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_SOURCES}
)

include("mmiot.cmake")

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined libraries
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()









