# Third-party libraries used by preenFM3.
#
# The STM32 HAL, CMSIS, TinyUSB, FatFs, Adafruit SD, ILI9341 and Hexter
# targets are INTERFACE libraries: they carry sources and include paths, but
# they are compiled in the consumer target so each consumer can use its own
# stm32h7xx_hal_conf.h / tusb_config.h / FreeRTOS-style configuration.

set(TP ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# STM32H7 HAL + CMSIS
# ---------------------------------------------------------------------------

add_library(stm32h7xx_cmsis INTERFACE)
target_include_directories(stm32h7xx_cmsis INTERFACE
    ${TP}/stm32h7xx/cmsis/Include
    ${TP}/stm32h7xx/cmsis/Device/ST/STM32H7xx/Include
)

file(GLOB STM32H7XX_HAL_SOURCES CONFIGURE_DEPENDS
    ${TP}/stm32h7xx/hal/Src/*.c
)

add_library(stm32h7xx_hal INTERFACE)
target_include_directories(stm32h7xx_hal INTERFACE
    ${TP}/stm32h7xx/hal/Inc
    ${TP}/stm32h7xx/hal/Inc/Legacy
)
target_sources(stm32h7xx_hal INTERFACE ${STM32H7XX_HAL_SOURCES})
target_link_libraries(stm32h7xx_hal INTERFACE stm32h7xx_cmsis)

# ---------------------------------------------------------------------------
# TinyUSB (trimmed: src/ + LICENSE only)
# ---------------------------------------------------------------------------

add_library(tinyusb INTERFACE)
target_include_directories(tinyusb INTERFACE
    ${TP}/tinyusb/src
    ${TP}/stm32h7xx/cmsis/Device/ST/STM32H7xx/Include
)
target_sources(tinyusb INTERFACE
    ${TP}/tinyusb/src/tusb.c
    ${TP}/tinyusb/src/common/tusb_fifo.c
    ${TP}/tinyusb/src/device/usbd.c
    ${TP}/tinyusb/src/class/audio/audio_device.c
    ${TP}/tinyusb/src/class/midi/midi_device.c
    ${TP}/tinyusb/src/class/msc/msc_device.c
    ${TP}/tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TP}/tinyusb/src/portable/synopsys/dwc2/dwc2_common.c
)

# ---------------------------------------------------------------------------
# FatFs + Adafruit SD BSP
# ---------------------------------------------------------------------------

add_library(adafruit_sd INTERFACE)
target_include_directories(adafruit_sd INTERFACE
    ${TP}/adafruit_sd
    ${TP}/stm32h7xx/hal/Inc
    ${TP}/stm32h7xx/hal/Inc/Legacy
    ${TP}/stm32h7xx/cmsis/Include
    ${TP}/stm32h7xx/cmsis/Device/ST/STM32H7xx/Include
)
target_sources(adafruit_sd INTERFACE
    ${TP}/adafruit_sd/adafruit_802_sd.c
    ${TP}/adafruit_sd/stm32h7xx_nucleo_bus.c
)

add_library(fatfs INTERFACE)
target_include_directories(fatfs INTERFACE
    ${TP}/fatfs
    ${TP}/adafruit_sd
    ${TP}/stm32h7xx/hal/Inc
    ${TP}/stm32h7xx/hal/Inc/Legacy
    ${TP}/stm32h7xx/cmsis/Include
    ${TP}/stm32h7xx/cmsis/Device/ST/STM32H7xx/Include
)
target_sources(fatfs INTERFACE
    ${TP}/fatfs/ff.c
    ${TP}/fatfs/ff_gen_drv.c
    ${TP}/fatfs/diskio.c
    ${TP}/fatfs/sd_diskio.c
    ${TP}/fatfs/fatfs.c
    ${TP}/fatfs/syscall.c
)
target_link_libraries(fatfs INTERFACE adafruit_sd)

# ---------------------------------------------------------------------------
# ILI9341
# ---------------------------------------------------------------------------

add_library(ili9341 INTERFACE)
target_include_directories(ili9341 INTERFACE
    ${TP}/ili9341
    ${TP}/stm32h7xx/hal/Inc
    ${TP}/stm32h7xx/hal/Inc/Legacy
    ${TP}/stm32h7xx/cmsis/Include
    ${TP}/stm32h7xx/cmsis/Device/ST/STM32H7xx/Include
    ${PROJECT_SOURCE_DIR}/preen_lib/include
)
target_sources(ili9341 INTERFACE
    ${TP}/ili9341/ili9341.c
)

# ---------------------------------------------------------------------------
# Fonts
# ---------------------------------------------------------------------------

add_library(fonts STATIC
    ${TP}/fonts/fonts.c
)
target_include_directories(fonts PUBLIC
    ${TP}/fonts
)

# ---------------------------------------------------------------------------
# Mutable Instruments MIDIpal helpers
# ---------------------------------------------------------------------------

add_library(mutable_midipal STATIC
    ${TP}/mutable/midipal/event_scheduler.cpp
    ${TP}/mutable/midipal/note_stack.cpp
)
target_include_directories(mutable_midipal PUBLIC
    ${TP}/mutable/midipal
)

# Only linked by the firmware. Compile it with the same flags as the firmware
# instead of the CMake default (which has no optimization at all).
target_compile_options(mutable_midipal PRIVATE
    -Ofast
    -fno-strict-aliasing
    -fno-tree-vectorize
    -funroll-loops
)

# ---------------------------------------------------------------------------
# Hexter DX7 patch loader
# ---------------------------------------------------------------------------

add_library(hexter INTERFACE)
target_include_directories(hexter INTERFACE
    ${TP}/hexter
    ${PROJECT_SOURCE_DIR}/firmware/src/app/synth
    ${PROJECT_SOURCE_DIR}/firmware/include
    ${PROJECT_SOURCE_DIR}/preen_lib/include
)
target_sources(hexter INTERFACE
    ${TP}/hexter/Hexter.cpp
)

# ---------------------------------------------------------------------------
# MSFA (Dexed DX7 engine)
# ---------------------------------------------------------------------------

add_library(msfa INTERFACE)
target_include_directories(msfa INTERFACE
    ${TP}/msfa
)
target_sources(msfa INTERFACE
    ${TP}/msfa/dx7note.cc
    ${TP}/msfa/env.cc
    ${TP}/msfa/exp2.cc
    ${TP}/msfa/fm_core.cc
    ${TP}/msfa/fm_op_kernel.cc
    ${TP}/msfa/freqlut.cc
    ${TP}/msfa/lfo.cc
    ${TP}/msfa/pitchenv.cc
    ${TP}/msfa/porta.cpp
    ${TP}/msfa/sin.cc
    ${TP}/msfa/platform_stubs.cc
    ${TP}/msfa/msfa_init.cc
)

# ---------------------------------------------------------------------------
# 9W9 TR-909 drum engine (X0X port)
# ---------------------------------------------------------------------------

add_library(drum909 INTERFACE)
target_include_directories(drum909 INTERFACE
    ${TP}/9W9
)
target_sources(drum909 INTERFACE
    ${TP}/9W9/drum909.c
)
