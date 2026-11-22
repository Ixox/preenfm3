# Folder structure

Overview of the repository layout: what each top-level folder contains and
where the main code lives. For build details see [compilation.md](compilation.md);
for hardware see [hardware.md](hardware.md).

## Top level

| Path | Contents |
|------|----------|
| `firmware/` | Main synth firmware (PFM3 + DX7 engines, UI, MIDI, storage) |
| `preen_lib/` | Shared board/display/input/storage library, linked by firmware and bootloader |
| `bootloader/` | Standalone USB-MSC / SD-card firmware update bootloader |
| `third_party/` | Vendored HAL, CMSIS, USB, FatFs, SD, display, fonts, MSFA, MIDI helpers |
| `hardware/` | PCB, control board and 3D case design files |
| `doc/` | Documentation (hardware, build, design notes) |
| `cmake/` | CMake toolchain file |
| `scripts/` | Helper shell scripts (CubeIDE artefacts, rebuilds) |
| `build/` | CMake build output (generated, not committed) |

Root files:

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Top-level build; adds `third_party`, `preen_lib`, `bootloader`, `firmware` |
| `rebuild.sh` | Clean rebuild and prints the size of both ELFs |
| `README.md` | Project overview, build and flashing |
| `AGENTS.md` | Instructions for AI coding agents |
| `LICENSE` | GPL-3.0 |

## `firmware/`

Main synth application. CMake globs `src/app/**` and `src/platform/**`.

| Path | Contents |
|------|----------|
| `include/` | `preenfm3.h`, `version.h` (firmware version string) |
| `src/app/engine/` | PFM3, DX7/MSFA and BYOS engines and their editors |
| `src/app/synth/` | Shared synth host, voice/timbre, FX bus and compressor |
| `src/app/ui/` | Display screens and the firmware TFT extensions |
| `src/app/midi/` | MIDI decoder and sequencer |
| `src/app/storage/` | Presets, mixer banks, sequences, config, DX7 sysex, user waves |
| `src/platform/clock/` | DWT cycle counter / CPU-usage helper (`dwt.h`) |
| `src/platform/cubemx/` | CubeMX glue: `main.h`, HAL MSP, interrupts, USB, startup |
| `src/platform/startup/` | `startup_stm32h753vitx.s` |
| `preenfm3_2MB.ld` | Firmware linker script (flash at `0x08020000`) |
| `preenfm3.ioc` | CubeMX project file (reference) |
| `CMakeLists.txt` | Firmware target, include paths and optimisation flags |

### `firmware/src/app/engine/`

| File(s) | Contents |
|---------|----------|
| `pfm3/` | PFM3 engine: `Preenfm3Engine`, `Pfm3*`, `Osc`, `Env`, `Lfo*`, `Matrix`, `Presets`, `EditorPfm3`, `EditorPfm3Algo` |
| `msfa/` | DX7 (Dexed/MSFA) engine: `Dx7Engine`, `Dx7SynthEngine`, `Dx7Params`, `Dx7ControlChange`, `EditorDx7`, `EditorDx7Algo` |
| `byos/` | Example third engine: `ByosEngine`, `EditorByos`, params and README |

### `firmware/src/app/synth/`

| File(s) | Contents |
|---------|----------|
| `Synth.*` | Top-level render: `buildNewSampleBlock()`, timbre mix, output dispatch |
| `Timbre.*` | Per-timbre voice allocation, arpeggiator, FX2, engine glue |
| `Voice.*` | Per-voice host and voice state |
| `VoicePool.*` | Voice pool and allocation |
| `MixerState.*`, `SynthState.*`, `Common.*` | Shared state, presets and common constants |
| `waves.*` | Wavetable data used by the oscillators |
| `FxBus.*` | Reverb / FX bus |
| `compressor/` | Compressor, simple envelope/gain helpers |
| `SynthEngine.*`, `SynthEngineFactory.*`, `TimbreParams.h` | Engine interface, factory and shared storage contract |
| `MidiControlChange.h`, `SharedParams.h`, `PersistenceMagic.h` | Shared MIDI/param/persistence helpers |
| `SynthParamListener.*`, `SynthMenuListener.*`, `SynthStateAware.*` | Listener interfaces |

### `firmware/src/app/ui/`

| File(s) | Contents |
|---------|----------|
| `ScreenCoordinator.*` | Screen coordinator and refresh state machine |
| `ScreenMixer.*` | Mixer screen |
| `ScreenEditor.*` | Common editor entry point, page routing and dispatch |
| `EditorEngine.*` | Editor interface and shared page/menu helpers |
| `EditorParamPage.*` | Generic menu/encoder page mechanics over `Pfm3Preset` |
| `EditorCommon.*` | Shared Arp/Fx page |
| `ScreenMenu.*` | Menu/file browser |
| `ScreenSequencer.*` | Sequencer screen |
| `FirmwareTftDisplay.*` | Oscilloscope background / firmware-specific TFT actions |
| `ScreenTypes.h`, `Menu.*`, `VisualInfo.h` | Shared UI types |

### `firmware/src/app/midi/`

| Path | Contents |
|------|----------|
| `midi/MidiDecoder.*` | DIN/USB MIDI parser, channel routing, MPE, sysex, async actions |
| `midi/Sequencer.*` | Step sequencer and playback |

### `firmware/src/app/storage/`

| File(s) | Contents |
|---------|----------|
| `Storage.*`, `PreenFMFileType.*`, `FileSystemUtils.*` | File-system abstraction |
| `PatchBank.*`, `MixerBank.*`, `SequenceBank.*`, `ConfigurationFile.*` | Preset / mix / sequence / config files |
| `DX7SysexFile.*` | DX7 single-voice and 32-voice sysex import |
| `UserWaveform.*`, `UserEnvCurve.*`, `ScalaFile.*` | User waves, envelopes, Scala tuning |
| `PPMImage.*` | Screenshot export |

## `preen_lib/`

Shared library linked by both the firmware and the bootloader.

| Path | Contents |
|------|----------|
| `include/` | Public headers: `TftDisplay.h`, `TftAlgo.h`, `Encoders.h`, `RingBuffer.h`, `preenfm3lib.h`, `preenfm3_pins.h`, `stm32h7xx_hal_conf.h` |
| `src/board/` | `preenfm3lib.c` — board ID, GPIO and SD init |
| `src/core/` | `RingBuffer.cpp` |
| `src/display/` | `TftDisplay.cpp`, `TftAlgo.cpp` (generic algorithm raster primitives; engine tables live in `engine/*/Editor*Algo.*`) |
| `src/input/` | `Encoders.cpp` — HC165 buttons/encoders |
| `src/platform/` | `syscalls.c`, `sysmem.c` |
| `src/storage/` | Reserved (storage lives in `firmware/src/app/storage`) |

Note: `bootloader/src/TftAlgo.cpp` is a dummy stub. Every new `TftAlgo`
method must also be stubbed there or the bootloader fails to link.

## `bootloader/`

Standalone bootloader at `0x08000000` (firmware starts at `0x08020000`).

| Path | Contents |
|------|----------|
| `include/` | `bootloader.h`, `FirmwareFile.h`, `flash_if.h`, `version.h`, HAL/USB headers |
| `src/main.c`, `src/bootloader.cpp` | Entry point and update flow |
| `src/FirmwareFile.cpp` | Firmware file handling |
| `src/flash_if.c` | Internal flash programming |
| `src/usbd_storage_if.c` | USB mass-storage export of the SD card |
| `src/TftAlgo.cpp` | Dummy `TftAlgo` stub |
| `src/platform/usb/` | USB device core/descriptors/configuration |
| `src/startup/` | `startup_stm32h753vitx.s` |
| `STM32H753VITX_FLASH.ld` | Bootloader linker script |

## `third_party/`

Vendored dependencies. Keep upstream headers/licences; see `third_party/NOTICE`.

| Path | Contents |
|------|----------|
| `stm32h7xx/` | STM32H7 HAL and CMSIS |
| `stm32_usb_device/` | ST USB device stack (core, audio, MIDI, MSC) |
| `adafruit_sd/` | SD card SPI driver |
| `fatfs/` | FatFs filesystem |
| `ili9341/` | ILI9341 display driver |
| `fonts/` | Font tables |
| `hexter/` | DX7 sysex unpacking used by DX7 |
| `msfa/` | Dexed/MSFA DX7 engine (Apache-2.0) |
| `mutable/` | Mutable Instruments MIDI-pal helpers (note stack, event scheduler) |

## `hardware/`

| Path | Contents |
|------|----------|
| `controlboard/`, `controlboard_SMD/` | Control-board PCB designs (through-hole and SMD variants) |
| `3DCase/`, `3DCaseB/`, `3DCasePCB/` | 3D-printed case designs |
| `lib/` | KiCad footprint library |

## `doc/`

| File | Contents |
|------|----------|
| `folder_structure.md` | This document |
| `hardware.md` | MCU, audio, display, SD, buttons and MIDI reference |
| `compilation.md` | Build details and CMake migration |
| `improvements.md` | Known issues and planned work |
| `dx7_engine.md` | DX7 engine integration |
| `dx7_algo_drawing.md` | DX7 algorithm diagram drawing |
| `engine_voice_refactor.md` | Engine separation and dynamic voice allocation plan |
| `stlink.md` | ST-LINK setup and troubleshooting |
| `pfm3_qs.pdf` | Quick-start guide |
| `README.md` | Short user-oriented intro |

## Build targets

| Target | Sources | Output |
|--------|---------|--------|
| `preenfm3_firmware` | `firmware/src/app/**`, `firmware/src/platform/**`, `preen_lib` | `build/firmware/p3_<version>.bin` |
| `preenfm3_bootloader` | `bootloader/src/**`, `preen_lib` | `build/bootloader/p3_boot_<version>.bin` |

Both targets link `preen_lib`. The firmware additionally links the STM32 USB
audio/MIDI classes, `hexter`, `msfa`, `mutable_midipal` and `fonts`; the
bootloader links the USB MSC class.

## Where to add code

- **New PFM3 parameter**: `Pfm3Params.h` / `SynthState.h`, then the editor UI in
  `ScreenEditor.cpp`, and the DSP use in `Voice.cpp` / `Timbre.cpp`.
- **New DX7 parameter**: `EditorDx7.*` and the DX7 program layout
  (see `doc/dx7_engine.md`).
- **New screen/page**: add a `Screen*` class and wire it in `ScreenCoordinator`.
- **New file type**: add a class under `firmware/src/app/storage/` and register
  it in `Storage`.
- **New TFT drawing action**: `preen_lib/include/TftDisplay.h`,
  `preen_lib/src/display/TftDisplay.cpp` and the bootloader stub in
  `bootloader/src/TftAlgo.cpp` if it touches `TftAlgo`.
