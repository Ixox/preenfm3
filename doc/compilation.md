# Compilation — preenFM3

This document describes how the preenFM3 firmware is configured and built, based on the STM32CubeIDE project files in `firmware/`.

## Table of Contents

- [Build System](#build-system)
- [Build Configurations](#build-configurations)
- [MCU and Toolchain](#mcu-and-toolchain)
- [Source Folders](#source-folders)
  - [Source exclusions](#source-exclusions)
- [Include Paths](#include-paths)
  - [Application](#application)
  - [preenfm3lib](#preenfm3lib)
  - [STM32 HAL / CMSIS](#stm32-hal--cmsis)
  - [Middleware](#middleware)
  - [Display / SD](#display--sd)
  - [C++ extra include](#c-extra-include)
- [Defines](#defines)
  - [DebugLQFP144](#debuglqfp144)
  - [Release](#release)
- [Linker](#linker)
- [How to Build](#how-to-build)
  - [STM32CubeIDE](#stm32cubeide)
  - [Headless CubeIDE](#headless-cubeide)
- [VS Code Notes](#vs-code-notes)
- [Migrating to CMake](#migrating-to-cmake)
  - [What CMake must reproduce](#what-cmake-must-reproduce)
  - [Proposed source layout](#proposed-source-layout)
  - [Work items](#work-items)
  - [CubeMX-generated CMake](#cubemx-generated-cmake)
  - [Migration order](#migration-order)
- [GCC 16 optimization experiments](#gcc-16-optimization-experiments)
- [Caveats](#caveats)

## Build System

Historically, the repository relied on STM32CubeIDE/Eclipse project metadata. A CMake build has since been added at the repository root; the CubeIDE files described below remain as reference. See [GCC 16 optimization experiments](#gcc-16-optimization-experiments) for the firmware flags and results.

The CubeIDE metadata consists of:

| File | Purpose |
|------|---------|
| `firmware/.project` | Eclipse project descriptor: project name, builders, natures, linked resources |
| `firmware/.cproject` | CDT managed-build configuration: MCU, toolchain, source folders, include paths, defines, optimization, linker script |
| `firmware/.mxproject` | STM32CubeMX bookkeeping: generated file lists and previous configuration |

The compiler does not read `.mxproject`. STM32CubeIDE generates the actual makefiles inside the build directory (`Debug/`, `Release/`, etc.) at build time; those generated files are not tracked.

`firmware/` is not the only CubeIDE project:

- `lib/.cproject` builds `preenfm3lib`
- `bootloader/.cproject` builds the bootloader
- `firmware/.cproject` builds the main firmware and links `preenfm3lib`

Build order is therefore normally **lib → firmware**, with the bootloader built separately.

## Build Configurations

`firmware/.cproject` contains two configurations:

| Configuration | Artifact | Optimization | Debug level |
|---------------|----------|--------------|-------------|
| `DebugLQFP144` | `${ProjName}.elf` | `-Og` | `-g3` |
| `Release` | `${ProjName}.elf` | `-Ofast` | `-g0` |

Only the LQFP144 build configuration is present. The source code can detect other board variants at runtime, but there is no LQFP100 or LQFP176 build configuration in `.cproject`.

The `Release` configuration still has `buildType=debug` in its `buildProperties`; the effective differences are the optimization and debug levels.

## MCU and Toolchain

| Setting | Value |
|---------|-------|
| MCU | `STM32H753VITx` |
| Board | `genericBoard` |
| CPU ID | `0` |
| Core ID | `0` |
| FPU | `fpv5-d16` |
| Float ABI | hard |
| Toolchain | GNU Tools for STM32 |
| Internal toolchain version | `7-2018-q2-update` |
| Compiler prefix | `arm-none-eabi-` |

## Source Folders

The firmware project includes these source folders:

- `Drivers`
- `Middlewares`
- `Src`
- `Startup`

There is **no `lib` source folder** in `firmware/.cproject`; `preenfm3lib` is linked as a library instead.

### Source exclusions

Some files are deliberately excluded from the firmware project because they are built in `preenfm3lib` or are not compiled directly:

| Folder | Excluded files |
|--------|----------------|
| `Drivers` | `ili9341/ili9341.c`, `ili9341/fonts.c`, `SD/adafruit_802_sd.c`, `SD/stm32h7xx_nucleo_bus.c` |
| `Middlewares` | `Third_Party/FatFs/src/ff_gen_drv.c`, `Third_Party/FatFs/src/ff.c`, `Third_Party/FatFs/src/diskio.c`, `Third_Party/FatFs/src/option/syscall.c` |
| `Src` | `SimpleEffect/SimpleGateProcess.inl`, `SimpleEffect/SimpleLimitProcess.inl`, `SimpleEffect/SimpleCompProcess.inl`, `hardware/Encoders.cpp`, `hardware/TftDisplay.cpp`, `utils/RingBuffer.cpp` |
| `Startup` | none |

This means the firmware project alone is not self-contained: `preenfm3lib` must be built first.

## Include Paths

The C compiler include path list contains these groups. Many paths are duplicated as both `${workspace_loc:/preenfm3/...}` and relative paths.

### Application

- `${workspace_loc:/preenfm3/Inc}`
- `${workspace_loc:/preenfm3/Src/hardware}`
- `${workspace_loc:/preenfm3/Src/synth}`
- `${workspace_loc:/preenfm3/Src/midi}`
- `${workspace_loc:/preenfm3/Src/midipal}`
- `${workspace_loc:/preenfm3/Src/filesystem}`
- `${workspace_loc:/preenfm3/Src/utils}`
- `../`
- `../Inc`
- `Inc`

### preenfm3lib

- `${workspace_loc:/preenfm3lib/Inc}`
- `${workspace_loc:/preenfm3lib/FATFS/App}`
- `${workspace_loc:/preenfm3lib/FATFS/Target}`
- `${workspace_loc:/preenfm3lib/Middlewares/Third_Party/FatFs/src}`

### STM32 HAL / CMSIS

- `${workspace_loc:/preenfm3/Drivers/STM32H7xx_HAL_Driver/Inc}`
- `${workspace_loc:/preenfm3/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy}`
- `${workspace_loc:/preenfm3/Drivers/CMSIS/Include}`
- `${workspace_loc:/preenfm3/Drivers/CMSIS/Device/ST/STM32H7xx/Include}`
- Relative equivalents such as `Drivers/STM32H7xx_HAL_Driver/Inc`, `Drivers/CMSIS/Include`, `../Drivers/...`

### Middleware

- `${workspace_loc:/preenfm3/Middlewares/ST/STM32_USB_Device_Library/Core/Inc}`
- `${workspace_loc:/preenfm3/Middlewares/ST/STM32_USB_Device_Library/Class/AUDIO/Inc}`
- `${workspace_loc:/preenfm3/Middlewares/ST/STM32_USB_Device_Library/Class/MIDI/Inc}`
- `${workspace_loc:/preenfm3/Middlewares/Third_Party/FatFs/src}`
- Relative equivalents such as `Middlewares/ST/STM32_USB_Device_Library/Core/Inc` and `../Middlewares/...`

### Display / SD

- `${workspace_loc:/preenfm3/Drivers/ili9341}`
- `${workspace_loc:/preenfm3/Drivers/SD}`

### C++ extra include

The C++ compiler has the same list plus:

- `${workspace_loc:/${ProjName}/Src/SimpleEffect}`

## Defines

### DebugLQFP144

- `USE_HAL_DRIVER`
- `LQFP144`
- `STM32H753xx`
- `DEBUG`

### Release

- `USE_HAL_DRIVER`
- `LQFP144`
- `STM32H753xx`

## Linker

| Linker | Script |
|--------|--------|
| C linker | `STM32H753VITX_FLASH.ld` |
| C++ linker | `preenfm3_2MB.ld` |

The project also links:

- `preenfm3lib`

Note the mismatch: the C linker and C++ linker reference different linker scripts. The C++ linker script is the 2 MB firmware layout used with the bootloader.

## How to Build

### STM32CubeIDE

1. Import/open `lib/`, `firmware/`, and `bootloader/` as CubeIDE projects.
2. Build `lib` first.
3. Build `firmware` with `DebugLQFP144` or `Release`.
4. Build `bootloader` separately if needed.

### Headless CubeIDE

A typical headless build uses the CDT managed-build application, for example:

```bash
stm32cubeide \
  -nosplash \
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
  -data /path/to/workspace \
  -import /path/to/lib \
  -import /path/to/firmware \
  -build preenfm3/DebugLQFP144
```

The exact project names and workspace paths must match your local CubeIDE workspace.

## VS Code Notes

The current official **STM32CubeIDE for Visual Studio Code** extension imports CMake projects, not `.cproject` projects. This repository now has a complete CMake build at the root, which the extension can use.

For VS Code, the practical options are:

1. Configure and build with CMake (see [GCC 16 optimization experiments](#gcc-16-optimization-experiments)) and debug the resulting `.elf` with **Cortex-Debug**.
2. Build with STM32CubeIDE (GUI or headless) and debug the resulting `.elf` with **Cortex-Debug**.
3. Use a third-party extension that can import `.cproject` files, such as `stm32-for-vscode`.

## Migrating to CMake

A CMake build is feasible, but it is a **medium-sized migration**, not a simple rename. The `.cproject` files contain most of the information needed; the difficult parts are the source lists, exclusions, linker scripts and the separate `preenfm3lib` project.

### What CMake must reproduce

| Item | Required value |
|------|----------------|
| Toolchain | `arm-none-eabi-gcc` / `arm-none-eabi-g++` |
| CPU | `-mcpu=cortex-m7 -mthumb` |
| FPU | `-mfpu=fpv5-d16 -mfloat-abi=hard` |
| MCU define | `STM32H753xx` |
| Board define | `LQFP144` |
| HAL define | `USE_HAL_DRIVER` |
| Debug define | `DEBUG` for `DebugLQFP144` only |
| Firmware linker script | `preenfm3_2MB.ld` |
| Bootloader linker script | `STM32H753VITX_FLASH.ld` |
| Library | `preenfm3lib` as a static library |
| Firmware startup | `firmware/Startup/startup_stm32h753vitx.s` |
| Debug flags | `-Og -g3` |
| Release flags | `-Ofast -g0` |

### Proposed source layout

```text
CMakeLists.txt
cmake/
  arm-none-eabi.cmake
  cpu_flags.cmake

third_party/
  CMakeLists.txt
  stm32h7xx/
    hal/
    cmsis/
  stm32_usb_device/
  fatfs/
  adafruit_sd/
  ili9341/
  fonts/
  mutable/
    midipal/
    arpeggiator/
  hexter/

lib/                      # shared preenfm3lib, used by firmware + bootloader
  CMakeLists.txt
  src/
    display/              # TftDisplay, TftAlgo
    input/                # Encoders
    board/                # preenfm3lib.c, preenfm3_pins.h
    storage/              # FatFs/SD wrappers
  include/                # public headers for firmware + bootloader

firmware/
  CMakeLists.txt
  src/
    app/
      main.c
      synth/
      midi/
      ui/                 # FMDisplay*, Menu
      storage/            # filesystem/
      midi_controller/    # alternate controller UI
    platform/
      cubemx/             # CubeMX-generated init/USB glue
      clock/
  include/
  tests/                  # optional, but useful for host-side logic

bootloader/
  CMakeLists.txt
  src/
  include/
```

Top-level `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(preenfm3 LANGUAGES C CXX ASM)

add_subdirectory(third_party)
add_subdirectory(lib)
add_subdirectory(firmware)
add_subdirectory(bootloader)
```

`cmake/arm-none-eabi.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)
```

Target outline:

```cmake
# lib/CMakeLists.txt
add_library(preenfm3lib STATIC
  # explicit list of lib/src files
)
target_link_libraries(preenfm3lib PUBLIC
  adafruit_sd
  fatfs
  ili9341
  fonts
)

# firmware/CMakeLists.txt
add_executable(preenfm3.elf
  # firmware/src + firmware/Startup
)
target_compile_definitions(preenfm3.elf PRIVATE
  USE_HAL_DRIVER
  STM32H753xx
  LQFP144
)
target_include_directories(preenfm3.elf PRIVATE
  firmware/include
  firmware/src/app/synth
  firmware/src/app/midi
  lib/include
)
target_link_options(preenfm3.elf PRIVATE
  -T${CMAKE_SOURCE_DIR}/firmware/preenfm3_2MB.ld
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/preenfm3.map
)
target_link_libraries(preenfm3.elf PRIVATE preenfm3lib)

# bootloader/CMakeLists.txt
add_executable(bootloader.elf
  # bootloader/src + bootloader/Startup
)
target_link_options(bootloader.elf PRIVATE
  -T${CMAKE_SOURCE_DIR}/bootloader/STM32H753VITX_FLASH.ld
  -Wl,--gc-sections
)
target_link_libraries(bootloader.elf PRIVATE preenfm3lib)
```

### Work items

1. Create an `arm-none-eabi` CMake toolchain file.
2. Create `third_party/CMakeLists.txt` targets for HAL/CMSIS, USB, FatFs, Adafruit SD, ILI9341, fonts, Mutable and Hexter.
3. Port `lib/.cproject` into `lib/CMakeLists.txt` and build the shared `preenfm3lib` target.
4. Port `firmware/.cproject` into `firmware/CMakeLists.txt`; link `preenfm3lib`.
5. Port `bootloader/.cproject` into `bootloader/CMakeLists.txt`; link the same `preenfm3lib`.
6. Recreate the source exclusions explicitly; do not simply glob entire directories.
7. Reproduce include paths per target instead of using one global include list.
8. Reproduce `USE_HAL_DRIVER`, `STM32H753xx`, `LQFP144` and `DEBUG`.
9. Use the correct linker script per target; resolve the current C/C++ linker mismatch.
10. Copy exact compiler/linker flags from the generated CubeIDE makefiles instead of guessing.
11. Add post-build `.bin`/`.hex` generation if required.
12. Add CMake presets for `DebugLQFP144` and `Release`.
13. Compare the resulting `.map` files and binary sizes against the CubeIDE build before switching.

### CubeMX-generated CMake

STM32CubeMX can generate a CMake project when `Toolchain/IDE` is set to `CMake`, but:

- It regenerates CubeMX-managed files and may overwrite changes outside `USER CODE` guards.
- It will not automatically include all hand-written `firmware/Src` files.
- It will not create the `preenfm3lib` or bootloader targets.
- The generated CMake is best used as a reference, not as the complete build.

### Migration order

1. Create `third_party/` and CMake targets for vendor code.
2. Rebuild `lib/` as shared `preenfm3lib` with `lib/CMakeLists.txt`; link third-party targets.
3. Add `firmware/CMakeLists.txt`, link `preenfm3lib`, and compare `.map`/size against CubeIDE.
4. Add `bootloader/CMakeLists.txt`, link `preenfm3lib`, and verify its linker script and startup.
5. Move CubeMX-generated files into `firmware/src/platform/cubemx/`.
6. Move firmware-only code into `firmware/src/app/` domain folders.
7. Move Adafruit SD, FatFs, ILI9341, fonts, Mutable and Hexter into `third_party/`.
8. Extract the Mutable arpeggiator from `Timbre.cpp`.
9. Update include paths and remove broad `lib/Inc` includes.
10. Add CMake presets and VS Code/Cortex-Debug configurations.
11. Stop maintaining `.cproject` only after both firmware and bootloader build reliably.

The biggest risks are missing source files, compiling files that CubeIDE intentionally excludes, choosing the wrong linker script, and CubeMX overwriting generated code.

## GCC 16 optimization experiments

The CMake build is implemented and uses **GCC 16.2.0**. The last working CubeIDE firmware (`p3_1_03.ok.bin`) was built with **GCC 7.3.1** (GNU Tools for STM32 `7-2018-q2-update`). Several optimization levels make the GCC 16 firmware hang during `SystemClock_Config()`/`preenfm3Init()` (black screen, no timer tick), so each configuration below was flashed to hardware and checked.

CPU % is the load reported by the firmware. The CubeIDE reference is included for comparison.

| Variant | Optimization flags (GCC 16 unless noted) | Boots? | CPU % |
|---------|-------------------------------------------|--------|-------|
| CubeIDE reference | `-Ofast` with GCC 7.3.1 | yes | 6.7% |
| `og` | `-Og -g3` (Debug) | yes | 9.6% |
| `O1` | `-O1 -fno-strict-aliasing` | yes | 9.0% |
| `O2` | `-O2 -fno-strict-aliasing -fno-tree-vectorize -fwrapv` | yes | 8.5% |
| `O3` | `-O3 -fno-strict-aliasing -fno-tree-vectorize` | yes | 8.2% |
| `Of` | `-Ofast -fno-strict-aliasing -fno-tree-vectorize` | yes | 7.3% |
| `ON` | `-Ofast -fno-tree-vectorize` (strict aliasing on) | yes | 6.8% |
| `OL` | `Of` + LTO (`CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`) | yes | 6.7% |
| `OU` | `Of` + `-funroll-loops` | yes | **6.4% (default)** |
| `UL` | `OU` + LTO | yes | 6.4% |
| — | `-Ofast` | **hangs** | — |
| — | `-O2 -fno-strict-aliasing` | **hangs** | — |
| — | `-Ofast -fwrapv` | **hangs** | — |
| — | `-O3 -fwrapv` | **hangs** | — |

Conclusions:

- GCC 16's **auto-vectorizer** causes the hangs. `-fno-tree-vectorize` lets `-O2`, `-O3` and `-Ofast` boot.
- `-fwrapv` alone does **not** fix it; this is not only a signed-overflow problem.
- `-Ofast` with vectorization disabled **and `-funroll-loops`** is the fastest verified configuration: **6.4%**, better than the GCC 7.3 CubeIDE build (6.7%). LTO alone (6.7%) and strict aliasing on (6.8%) were slower.
- Adding LTO on top of unroll (`UL`) also measured **6.4%**, so `OU` remains the default: same speed with a smaller image (524 KB vs 658 KB) and no LTO complexity.
- The C++ ABI flags must match CubeIDE: `-fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit`. Without them, global constructors call `__aeabi_atexit()` during `__libc_init_array()`, which can hang/crash before `main()`.

### Bootloader and static libraries

- The bootloader keeps the size-oriented `-Os`, plus `-fno-tree-vectorize` to avoid the same GCC 16 vectorizer hang. It is 55,100 B of 128 KB.
- The `mutable_midipal` static library is linked only by the firmware and is compiled with the firmware's `-Ofast -fno-strict-aliasing -fno-tree-vectorize -funroll-loops`, instead of CMake's default (no optimization).

By default the firmware artifact is named `p3_<version>.bin`, using the version from `firmware/include/version.h` with dots replaced by underscores (for example `v1.03` becomes `p3_1_03.bin`). The optional `PRE_ENFM3_FW_VARIANT` suffix is only for experiment builds:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DPRE_ENFM3_FW_OPTIMIZATION="-O2;-fno-strict-aliasing;-fno-tree-vectorize" \
  -DPRE_ENFM3_FW_VARIANT=O2
cmake --build build -j"$(nproc)" --target preenfm3_firmware
```

The default `Release` build uses:

```cmake
-Ofast -g0 -fno-strict-aliasing -fno-tree-vectorize -funroll-loops
```

and produces `build/firmware/p3_<version>.bin` (plus `preenfm3_firmware.bin`/`.hex`). With the current header (`v1.04`) that is `p3_1_04.bin`. The firmware is flashed at `0x08020000`; the bootloader at `0x08000000`.

## Caveats

- `.cproject` is not a standalone build script; it is Eclipse CDT metadata.
- The actual makefiles are generated at build time and are not tracked.
- `preenfm3lib` must be built before `firmware`.
- The C and C++ linker scripts differ.
- The toolchain version is old (`7-2018-q2-update`).
- Include paths are duplicated and mix workspace-absolute and relative entries.
- Only `DebugLQFP144` and `Release` configurations exist.
- `.mxproject` contains machine-specific absolute paths and should not be hand-edited.
