cmake_minimum_required(VERSION 3.20)

project(preenfm3 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party)
add_subdirectory(preen_lib)
add_subdirectory(bootloader)
add_subdirectory(firmware)
