# Third-party libraries used by preenFM3.
#
# The STM32 HAL, CMSIS, USB device, FatFs, Adafruit SD, ILI9341 and Hexter
# targets are INTERFACE libraries: they carry sources and include paths, but
# they are compiled in the consumer target so each consumer can use its own
# stm32h7xx_hal_conf.h / usbd_conf.h / FreeRTOS-style configuration.

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
# STM32 USB Device Library
# ---------------------------------------------------------------------------

add_library(stm32_usb_core INTERFACE)
target_include_directories(stm32_usb_core INTERFACE
    ${TP}/stm32_usb_device/Core/Inc
)
target_sources(stm32_usb_core INTERFACE
    ${TP}/stm32_usb_device/Core/Src/usbd_core.c
    ${TP}/stm32_usb_device/Core/Src/usbd_ctlreq.c
    ${TP}/stm32_usb_device/Core/Src/usbd_ioreq.c
)

add_library(stm32_usb_msc INTERFACE)
target_include_directories(stm32_usb_msc INTERFACE
    ${TP}/stm32_usb_device/Class/MSC/Inc
)
target_sources(stm32_usb_msc INTERFACE
    ${TP}/stm32_usb_device/Class/MSC/Src/usbd_msc.c
    ${TP}/stm32_usb_device/Class/MSC/Src/usbd_msc_bot.c
    ${TP}/stm32_usb_device/Class/MSC/Src/usbd_msc_data.c
    ${TP}/stm32_usb_device/Class/MSC/Src/usbd_msc_scsi.c
)
target_link_libraries(stm32_usb_msc INTERFACE stm32_usb_core)

add_library(stm32_usb_audio INTERFACE)
target_include_directories(stm32_usb_audio INTERFACE
    ${TP}/stm32_usb_device/Class/AUDIO/Inc
)
target_sources(stm32_usb_audio INTERFACE
    ${TP}/stm32_usb_device/Class/AUDIO/Src/usbd_audio.c
)
target_link_libraries(stm32_usb_audio INTERFACE stm32_usb_core)

add_library(stm32_usb_midi INTERFACE)
target_include_directories(stm32_usb_midi INTERFACE
    ${TP}/stm32_usb_device/Class/MIDI/Inc
)
target_sources(stm32_usb_midi INTERFACE
    ${TP}/stm32_usb_device/Class/MIDI/Src/usbd_midi.c
)
target_link_libraries(stm32_usb_midi INTERFACE stm32_usb_core)

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
