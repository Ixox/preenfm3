# Shared preenFM3 board/display/input library.
#
# INTERFACE library, like the third_party targets: the sources are compiled
# inside each consumer so every consumer uses its own stm32h7xx_hal_conf.h.
#
# TftAlgo.cpp and platform/{syscalls,sysmem}.c are intentionally not listed:
# each consumer provides its own implementation.

add_library(preen_lib INTERFACE)

target_include_directories(preen_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(preen_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/board/preenfm3lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/RingBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/display/TftDisplay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/Encoders.cpp
)

target_link_libraries(preen_lib INTERFACE
    stm32h7xx_hal
    adafruit_sd
    fatfs
    ili9341
    fonts
)
