# preenFM3 bootloader (USB MSC + SD firmware update).

set(BOOTLOADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(preenfm3_bootloader
    ${BOOTLOADER_DIR}/src/main.c
    ${BOOTLOADER_DIR}/src/bootloader.cpp
    ${BOOTLOADER_DIR}/src/FirmwareFile.cpp
    ${BOOTLOADER_DIR}/src/TftAlgo.cpp
    ${BOOTLOADER_DIR}/src/flash_if.c
    ${BOOTLOADER_DIR}/src/stm32h7xx_hal_msp.c
    ${BOOTLOADER_DIR}/src/stm32h7xx_hal_msp_bootloader.c
    ${BOOTLOADER_DIR}/src/stm32h7xx_it.c
    ${BOOTLOADER_DIR}/src/system_stm32h7xx.c
    ${BOOTLOADER_DIR}/src/syscalls.c
    ${BOOTLOADER_DIR}/src/sysmem.c
    ${BOOTLOADER_DIR}/src/usbd_storage_if.c
    ${BOOTLOADER_DIR}/src/platform/usb/App/usb_device.c
    ${BOOTLOADER_DIR}/src/platform/usb/App/usbd_desc.c
    ${BOOTLOADER_DIR}/src/platform/usb/Target/usbd_conf.c
    ${BOOTLOADER_DIR}/src/startup/startup_stm32h753vitx.s
)

# Own include directories must come before the linked INTERFACE directories so
# that bootloader/include/stm32h7xx_hal_conf.h wins over the one in preen_lib.
target_include_directories(preenfm3_bootloader PRIVATE
    ${BOOTLOADER_DIR}/include
    ${BOOTLOADER_DIR}/src/platform/usb/App
    ${BOOTLOADER_DIR}/src/platform/usb/Target
)

target_compile_definitions(preenfm3_bootloader PRIVATE
    USE_HAL_DRIVER
    STM32H753xx
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(preenfm3_bootloader PRIVATE DEBUG)
endif()

target_compile_options(preenfm3_bootloader PRIVATE
    # Size matters for the bootloader, not speed. Keep -Os, but disable the
    # GCC 16 auto-vectorizer that miscompiles the main firmware.
    -Os
    -g3
    -Wall
    -fdata-sections
    -ffunction-sections
    -fno-tree-vectorize
)

target_link_libraries(preenfm3_bootloader PRIVATE
    preen_lib
    stm32_usb_core
    stm32_usb_msc
    m
)

target_link_options(preenfm3_bootloader PRIVATE
    -T${BOOTLOADER_DIR}/STM32H753VITX_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/preenfm3_bootloader.map
    -Wl,--print-memory-usage
)

set_target_properties(preenfm3_bootloader PROPERTIES
    SUFFIX ".elf"
    OUTPUT_NAME "preenfm3_bootloader"
)

# Release version, e.g. "1.09" -> p3_boot_1_09.bin (same naming as scripts/create_object*.sh)
file(READ ${BOOTLOADER_DIR}/include/version.h BOOTLOADER_VERSION_HEADER)
string(REGEX MATCH "PFM3_BOOTLOADER_VERSION[ \t]+\"([0-9.]+)\"" _match "${BOOTLOADER_VERSION_HEADER}")
if(_match)
    set(BOOTLOADER_VERSION "${CMAKE_MATCH_1}")
else()
    set(BOOTLOADER_VERSION "unknown")
endif()
string(REPLACE "." "_" BOOTLOADER_VERSION_FILE "${BOOTLOADER_VERSION}")
message(STATUS "preenFM3 bootloader version: ${BOOTLOADER_VERSION}")

# The bootloader is flashed at 0x08000000; the firmware starts at 0x08020000.
# Exclude the RAM-only sections from the binary, as scripts/create_object*.sh do.
set(BOOTLOADER_OBJCOPY_REMOVE
    -R .ram_d2b
    -R .ram_d2
    -R .ram_d1
    -R .ram_d3
    -R .instruction_ram
)

add_custom_command(TARGET preenfm3_bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${BOOTLOADER_OBJCOPY_REMOVE} -O binary
        $<TARGET_FILE:preenfm3_bootloader>
        ${CMAKE_CURRENT_BINARY_DIR}/preenfm3_bootloader.bin
    COMMAND ${CMAKE_OBJCOPY} ${BOOTLOADER_OBJCOPY_REMOVE} -O binary
        $<TARGET_FILE:preenfm3_bootloader>
        ${CMAKE_CURRENT_BINARY_DIR}/p3_boot_${BOOTLOADER_VERSION_FILE}.bin
    COMMAND ${CMAKE_OBJCOPY} ${BOOTLOADER_OBJCOPY_REMOVE} -O ihex
        $<TARGET_FILE:preenfm3_bootloader>
        ${CMAKE_CURRENT_BINARY_DIR}/preenfm3_bootloader.hex
    COMMENT "Generating bootloader .bin/.hex (version ${BOOTLOADER_VERSION})"
)
