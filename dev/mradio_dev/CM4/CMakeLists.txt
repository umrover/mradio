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
set(STM32_LINKER_SCRIPT stm32h755xx_flash_CM4.ld)
set(STM32_LINKER_OPTION  )

# Set the project name
set(CMAKE_PROJECT_NAME mradio_dev_CM4)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated
include("mx-generated.cmake")


# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")

# Add project symbols (macros)
set(LINK_TEST_FLOOD OFF CACHE BOOL "Flood UDP at the peer once it is up")
set(LINK_TEST_PING ON CACHE BOOL "Ping the peer once a second during each window")
set(LINK_TEST_RATE_KBPS 0 CACHE STRING "Flood UDP payload rate in kbps, 0 for as fast as the link takes it")
set(HALOW_TX_POWER_DBM 0 CACHE STRING "Max TX power in dBm, 0 for the regulatory limit")

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    LINK_TEST_FLOOD=$<BOOL:${LINK_TEST_FLOOD}>
    LINK_TEST_PING=$<BOOL:${LINK_TEST_PING}>
    LINK_TEST_RATE_KBPS=${LINK_TEST_RATE_KBPS}
    HALOW_TX_POWER_DBM=${HALOW_TX_POWER_DBM}
    LINK_TEST_RX_HOOK=mmint_5145db60
)

# Enable CMake support for ASM, C, and C++
enable_language(C CXX ASM)

file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Common/Src/*.cpp"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Common/Inc"
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
