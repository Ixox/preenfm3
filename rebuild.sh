#!/bin/bash
set -e
cd "$(dirname "$0")"
#cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build --target clean
cmake --build build -j"$(nproc)"
arm-none-eabi-size build/bootloader/preenfm3_bootloader.elf build/firmware/preenfm3_firmware.elf
