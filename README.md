# preenFM3

preenFM3 is an open-source FM synthesizer built around an STM32H7 microcontroller.

This repository contains the firmware, the shared hardware library, the bootloader and the hardware design files.

Binaries are available in the [Releases](https://github.com/Ixox/preenfm3/releases) section.  
More user documentation is available in the [Wiki](https://github.com/Ixox/preenfm3/wiki).

## Overview

The firmware runs the FM synth engine, the display UI, MIDI input/output, SD card storage and the bootloader.

The project is split into:

| Folder | Contents |
|--------|----------|
| `firmware/` | Main synth firmware |
| `preen_lib/` | Shared display, input, board and storage library |
| `third_party/` | Vendored HAL, CMSIS, USB, FatFs, SD, display and helper libraries |
| `bootloader/` | Standalone bootloader |
| `hardware/` | PCB and mechanical files |
| `doc/` | Hardware, compilation and improvement notes |

## Hardware at a glance

| Subsystem | Details |
|-----------|---------|
| MCU | STM32H753VITx, Cortex-M7, 480 MHz |
| Audio output | 3 × CS4344 stereo DACs, 6 analog outputs |
| Audio transport | SAI1/SAI2 in I2S mode, DMA, 48 kHz |
| Display | ILI9341, 240 × 320 RGB565, SPI1 + DMA2D |
| Storage | microSD card in SPI mode, SPI2 + FatFs |
| Controls | 6 encoders and 12/18 buttons through a 74HC165 chain |
| MIDI | DIN MIDI on USART1 and USB MIDI over OTG FS |
| Memory | RAM_D1 for display, RAM_D2 for DAC/FX, RAM_D2B for FatFs, RAM_D3 for sequencer |

The CPU runs at its rated 480 MHz; it is not overclocked. The ILI9341 SPI bus runs at approximately 32 MHz, above the panel's nominal rating.

See [doc/hardware.md](doc/hardware.md) for the full hardware documentation.

## Building

The project builds with **CMake** at the repository root. The legacy STM32CubeIDE project files remain in the tree as reference.

The main firmware build configurations are:

| Configuration | Optimization | Debug |
|---------------|--------------|-------|
| `Debug` | `-Og` | `-g3` |
| `Release` (default) | `-Ofast -fno-strict-aliasing -fno-tree-vectorize -funroll-loops` | `-g0` |

Common build settings:

- Toolchain: GNU Tools for STM32 (`arm-none-eabi-`)
- MCU: `STM32H753VITx`
- Defines: `USE_HAL_DRIVER`, `STM32H753xx`, `LQFP144`
- Firmware linker script: `firmware/preenfm3_2MB.ld`
- Bootloader linker script: `bootloader/STM32H753VITX_FLASH.ld`

### CMake

Requirements:

- CMake 3.20 or newer
- `arm-none-eabi-gcc`
- `arm-none-eabi-newlib`

Configure and build:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build
```

This builds both `preenfm3_firmware` and `preenfm3_bootloader`. The default `Release` build produces `build/firmware/p3_<version>.bin` and `build/bootloader/p3_boot_<version>.bin`, along with the `.elf`, `.bin` and `.hex` artifacts. The firmware version comes from `firmware/include/version.h` (currently `v1.04`, so `p3_1_04.bin`); the bootloader version comes from `bootloader/include/version.h`.

### Flashing

There are two separate operations: flashing the **bootloader** (only needed once, or to upgrade it) and flashing the **firmware**.

To boot into the bootloader, hold down the **MENU** button while turning the preenFM3 on.

#### Bootloader (first-time / never-flashed board)

The bootloader lives at `0x08000000` and must be installed before the firmware can be updated normally. On a board that has never been flashed, put the MCU in DFU mode with the switch on the back of the PCB, then flash the bootloader:

```bash
dfu-util -a 0 -d 0x0483:0xdf11 \
  -D build/bootloader/p3_boot_1_10.bin -s 0x08000000
```

Alternatively, use ST-LINK through the 4-pin header on the back of the PCB; see [doc/stlink.md](doc/stlink.md).

Once the bootloader has been flashed, you can enter DFU mode from the bootloader itself, or keep using the ST-LINK 4-pin header.

#### Firmware (normal update)

The preferred method is the SD card:

1. Put `build/firmware/p3_<version>.bin` (for example `p3_1_04.bin`) on the SD card.
2. Boot the bootloader and flash the file from the SD card.

If the SD card is not accessible, the bootloader can expose it to a computer as a USB mass-storage device, so the firmware `.bin` can be copied without removing the card.

Firmware updates can also be done over DFU from the bootloader:

```bash
dfu-util -a 0 -d 0x0483:0xdf11 \
  -D build/firmware/p3_1_04.bin -s 0x08020000
```

Finally, the firmware can be written directly with ST-LINK through the 4-pin header on the back of the PCB:

```bash
st-flash --reset write build/firmware/p3_1_04.bin 0x08020000
```

See [doc/stlink.md](doc/stlink.md) for ST-LINK setup and troubleshooting.

### Project status

The source tree has been reorganized into `firmware/`, `preen_lib/`, `third_party/` and `bootloader/`. The CMake build now produces the full firmware and bootloader; the CubeIDE `.cproject` files are no longer the primary build.

See [doc/compilation.md](doc/compilation.md) for detailed build information and the CMake migration plan.

## Documentation

- [Hardware documentation](doc/hardware.md)
- [Compilation and CMake migration](doc/compilation.md)
- [Known improvements](doc/improvements.md)
- [Project Wiki](https://github.com/Ixox/preenfm3/wiki)

## License

The firmware is distributed under the GNU General Public License v3. See the source file headers for details.
