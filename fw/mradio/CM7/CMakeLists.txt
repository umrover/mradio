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
set(STM32_MCU_FLAGS  "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard " )

#Linker options
set(STM32_LINKER_SCRIPT mradio_CM7.ld)
set(STM32_LINKER_OPTION  )

# Set the project name
set(CMAKE_PROJECT_NAME mradio_CM7)
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

set(HALOW_SUB_BAND "mid" CACHE STRING "HaLow sub-band at boot, low, mid or high")
set(HALOW_SUB_BANDS_ORDER low mid high)
set_property(CACHE HALOW_SUB_BAND PROPERTY STRINGS ${HALOW_SUB_BANDS_ORDER})
list(FIND HALOW_SUB_BANDS_ORDER "${HALOW_SUB_BAND}" HALOW_SUB_BAND_INDEX)
if(HALOW_SUB_BAND_INDEX EQUAL -1)
    message(FATAL_ERROR "HALOW_SUB_BAND must be low, mid or high, got '${HALOW_SUB_BAND}'")
endif()
set(HALOW_BW_MHZ 8 CACHE STRING "HaLow channel width at boot in MHz, 1, 2, 4 or 8")
if(NOT HALOW_BW_MHZ MATCHES "^(1|2|4|8)$")
    message(FATAL_ERROR "HALOW_BW_MHZ must be 1, 2, 4 or 8, got '${HALOW_BW_MHZ}'")
endif()
set(HALOW_TX_POWER_DBM 0 CACHE STRING "HaLow max TX power in dBm, 0 for the regulatory limit")
if(NOT HALOW_TX_POWER_DBM MATCHES "^([0-9]|[12][0-9]|30)$")
    message(FATAL_ERROR "HALOW_TX_POWER_DBM must be 0 to 30, got '${HALOW_TX_POWER_DBM}'")
endif()

set(LINK_TEST OFF CACHE BOOL "Run the HaLow link test, a UDP receiver with 5 s reports, instead of the plain ping")
set(LINK_TEST_FLOOD OFF CACHE BOOL "With LINK_TEST, also flood UDP at the peer once it is up")
set(LINK_TEST_RATE_KBPS 0 CACHE STRING "Flood payload rate in kbps, 0 for as fast as the link takes it")
if(NOT LINK_TEST_RATE_KBPS MATCHES "^[0-9]+$")
    message(FATAL_ERROR "LINK_TEST_RATE_KBPS must be a whole number, got '${LINK_TEST_RATE_KBPS}'")
endif()
message("HaLow ${HALOW_SUB_BAND} sub-band at ${HALOW_BW_MHZ} MHz, link test ${LINK_TEST}, flood ${LINK_TEST_FLOOD}")

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    HALOW_DEFAULT_ROLE_AP=$<STREQUAL:${HALOW_DEFAULT_ROLE},ap>
    HALOW_SUB_BAND_INDEX=${HALOW_SUB_BAND_INDEX}
    HALOW_BW_MHZ=${HALOW_BW_MHZ}
    HALOW_TX_POWER_DBM=${HALOW_TX_POWER_DBM}
    LINK_TEST=$<BOOL:${LINK_TEST}>
    LINK_TEST_FLOOD=$<BOOL:${LINK_TEST_FLOOD}>
    LINK_TEST_RATE_KBPS=${LINK_TEST_RATE_KBPS}U
)

# Enable CMake support for ASM and C languages
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
include("web.cmake")
include("wifi.cmake")

# mradio_CM7.ld INCLUDEs these by name
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-L${CMAKE_CURRENT_SOURCE_DIR}")
foreach(script mradio_CM7.ld wifi.ld eth.ld stm32h745xx_flash_CM7.ld)
    set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${script}")
endforeach()

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









