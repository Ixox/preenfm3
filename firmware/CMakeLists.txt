# preenFM3 firmware.

set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE FIRMWARE_SOURCES CONFIGURE_DEPENDS
    ${FIRMWARE_DIR}/src/app/*.c
    ${FIRMWARE_DIR}/src/app/*.cpp
    ${FIRMWARE_DIR}/src/platform/*.c
    ${FIRMWARE_DIR}/src/platform/*.cpp
)

add_executable(preenfm3_firmware
    ${FIRMWARE_SOURCES}
    ${FIRMWARE_DIR}/src/platform/startup/startup_stm32h753vitx.s
    ${PROJECT_SOURCE_DIR}/preen_lib/src/display/TftAlgo.cpp
)

# Own include directories must come before the linked INTERFACE directories so
# that firmware/src/platform/cubemx/stm32h7xx_hal_conf.h wins over preen_lib's.
target_include_directories(preenfm3_firmware PRIVATE
    ${FIRMWARE_DIR}/include
    ${FIRMWARE_DIR}/src/app
    ${FIRMWARE_DIR}/src/app/midi
    ${FIRMWARE_DIR}/src/app/midi_controller
    ${FIRMWARE_DIR}/src/app/storage
    ${FIRMWARE_DIR}/src/app/synth
    ${FIRMWARE_DIR}/src/app/synth/SimpleEffect
    ${FIRMWARE_DIR}/src/app/ui
    ${FIRMWARE_DIR}/src/platform/clock
    ${FIRMWARE_DIR}/src/platform/cubemx
)

target_compile_definitions(preenfm3_firmware PRIVATE
    USE_HAL_DRIVER
    STM32H753xx
)

set(PRE_ENFM3_FW_OPTIMIZATION "" CACHE STRING
    "Override firmware optimization flags (semicolon-separated, e.g. -O2;-fno-strict-aliasing)")

if(PRE_ENFM3_FW_OPTIMIZATION)
    target_compile_options(preenfm3_firmware PRIVATE ${PRE_ENFM3_FW_OPTIMIZATION})
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(preenfm3_firmware PRIVATE DEBUG)
    target_compile_options(preenfm3_firmware PRIVATE -Og -g3)
else()
    # GCC 16's auto-vectorizer miscompiles this firmware (it hangs during
    # init). -Ofast without vectorization, plus loop unrolling, is the
    # fastest verified configuration (6.4% CPU load vs 6.7% for the old
    # GCC 7.3 CubeIDE build).
    target_compile_options(preenfm3_firmware PRIVATE
        -Ofast
        -g0
        -fno-strict-aliasing
        -fno-tree-vectorize
        -funroll-loops
    )
endif()

target_compile_options(preenfm3_firmware PRIVATE
    -Wall
    -fdata-sections
    -ffunction-sections
)

# Optional suffix for experiment artifacts, e.g. -DPRE_ENFM3_FW_VARIANT=O2
# produces p3_O2.bin. When unset, the artifact is named after the firmware
# version from firmware/include/version.h (e.g. p3_1_04.bin).
set(PRE_ENFM3_FW_VARIANT "" CACHE STRING
    "Optional suffix for the p3_<variant>.bin artifact instead of the version name")

target_link_libraries(preenfm3_firmware PRIVATE
    preen_lib
    stm32_usb_core
    stm32_usb_audio
    stm32_usb_midi
    hexter
    msfa
    mutable_midipal
    fonts
    m
)

target_link_options(preenfm3_firmware PRIVATE
    -T${FIRMWARE_DIR}/preenfm3_2MB.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/preenfm3_firmware.map
    -Wl,--print-memory-usage
)

set_target_properties(preenfm3_firmware PROPERTIES
    SUFFIX ".elf"
    OUTPUT_NAME "preenfm3_firmware"
)

# Release version, e.g. "v1.03" -> p3_1_03.bin (same naming as scripts/create_object*.sh)
file(READ ${FIRMWARE_DIR}/include/version.h FIRMWARE_VERSION_HEADER)
string(REGEX MATCH "PFM3_FIRMWARE_VERSION[ \t]+\"v?([0-9.]+)\"" _match "${FIRMWARE_VERSION_HEADER}")
if(_match)
    set(FIRMWARE_VERSION "${CMAKE_MATCH_1}")
else()
    set(FIRMWARE_VERSION "unknown")
endif()
string(REPLACE "." "_" FIRMWARE_VERSION_FILE "${FIRMWARE_VERSION}")
message(STATUS "preenFM3 firmware version: ${FIRMWARE_VERSION}")

if(PRE_ENFM3_FW_VARIANT)
    set(FIRMWARE_BIN_NAME "p3_${PRE_ENFM3_FW_VARIANT}.bin")
else()
    set(FIRMWARE_BIN_NAME "p3_${FIRMWARE_VERSION_FILE}.bin")
endif()
message(STATUS "preenFM3 firmware artifact: ${FIRMWARE_BIN_NAME}")

# The firmware is flashed at 0x08020000 (after the 128 KB bootloader).
# Exclude the RAM/ITCM sections from the binary, as scripts/create_object*.sh do.
set(FIRMWARE_OBJCOPY_REMOVE
    -R .ram_d2b
    -R .ram_d2c
    -R .ram_d2
    -R .ram_d1
    -R .ram_d3
    -R .instruction_ram
)

add_custom_command(TARGET preenfm3_firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${FIRMWARE_OBJCOPY_REMOVE} -O binary
        $<TARGET_FILE:preenfm3_firmware>
        ${CMAKE_CURRENT_BINARY_DIR}/preenfm3_firmware.bin
    COMMAND ${CMAKE_OBJCOPY} ${FIRMWARE_OBJCOPY_REMOVE} -O binary
        $<TARGET_FILE:preenfm3_firmware>
        ${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BIN_NAME}
    COMMAND ${CMAKE_OBJCOPY} ${FIRMWARE_OBJCOPY_REMOVE} -O ihex
        $<TARGET_FILE:preenfm3_firmware>
        ${CMAKE_CURRENT_BINARY_DIR}/preenfm3_firmware.hex
    COMMENT "Generating firmware .bin/.hex (version ${FIRMWARE_VERSION}, variant ${PRE_ENFM3_FW_VARIANT})"
)
