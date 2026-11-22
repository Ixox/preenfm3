# Memory map

Where the firmware puts its data, how each region behaves, and the rules for
placing new buffers. The MPU/cache correctness details (barriers, DMA
coherency) are in [improvements.md](improvements.md); this page is about
placement and budget.

The MCU is an STM32H753 (Cortex-M7, 480 MHz) with 1 MB of regular SRAM split
across the D1/D2/D3 domains, plus 128 KB DTCM and 64 KB ITCM. The firmware is
linked at `0x08020000`, after the 128 KB bootloader, and the linker script is
[firmware/preenfm3_2MB.ld](../firmware/preenfm3_2MB.ld).

All numbers below are from a single LTO build of firmware 1.04. Treat them as
a snapshot; the build prints the current values, and the budget check fails
above 95 % in any RAM region.

## Table of Contents

- [Region overview](#region-overview)
- [How placement is declared](#how-placement-is-declared)
- [Placement rules](#placement-rules)
- [Startup and binary caveats](#startup-and-binary-caveats)
- [Current contents](#current-contents)
- [Compile-time and build-time checks](#compile-time-and-build-time-checks)
- [Remaining tight spots](#remaining-tight-spots)
- [Checking usage](#checking-usage)

---

## Region overview

| Region | Origin | Size | MPU / cache | Startup init | Intended use | Used | % |
|---|---|---|---|---|---|---|---|
| ITCMRAM | `0x00000004` | 63 KB | none configured; never cached | none (zeroed) | hot FM tables, user waveforms, time-critical code | 51,464 B | 79.77 |
| DTCMRAM | `0x20000000` | 128 KB | none configured; never cached | `.data` copied, `.bss` zeroed | hot per-sample state, waveform tables, user buffers, stack | 117,776 B | 89.86 |
| RAM_D1 | `0x24000000` | 512 KB | MPU R1, write-through | none (zeroed) | display buffers, engine state, FX lines | 487,808 B | 93.04 |
| RAM_D2 | `0x30000000` | 128 KB | MPU R2, write-through | none (zeroed) | SAI audio buffers, FX scratch | 121,992 B | 93.07 |
| RAM_D2B | `0x30020000` | 128 KB | MPU R3, write-through | none (zeroed) | CPU-only audio: FX2 delay and diffusers | 72,100 B | 55.01 |
| RAM_D2C | `0x30040000` | 32 KB | MPU R5, non-cacheable, shareable | none (zeroed) | SD/SPI2 DMA and FatFs buffers | 24,144 B | 73.68 |
| RAM_D3 | `0x38000000` | 64 KB | MPU R4, write-back | none (zeroed) | cold data: sequencer, preset mirrors | 48,040 B | 73.30 |
| FLASH | `0x08020000` | 1920 KB | — | — | code, `.rodata`, `.data` load image | 578,902 B | 29.44 |

Total SRAM used is 923,324 B (about 85.5 % of the 1,055 KB available). DTCM is
at 89.86 %, and RAM_D1 and RAM_D2 are around 93 %.

DTCM and ITCM are not described by the MPU: the Cortex-M7 TCMs are accessed
directly and are never cached. That is why hot tables in ITCM gave a large
speed-up compared to the same tables in non-cacheable `.ram_d2c`.

## How placement is declared

By default, every global with an initialiser lands in `.data` and every
zero-initialised global in `.bss`; both are placed in **DTCMRAM**.

Explicit placement uses a section attribute whose name matches an output
section in the linker script:

| Section | Region | Declared as |
|---|---|---|
| `.ram_d1` | RAM_D1 | `RAM_D1_SECTION` (`preen_lib/include/TftDisplay.h`, `firmware/src/app/preenfm3.cpp`) |
| `.ram_d2` | RAM_D2 | `RAM_D2_SECTION` |
| `.ram_d2b` | RAM_D2B | raw `__attribute__((section(".ram_d2b")))` / `RAM_D2B_SECTION` |
| `.ram_d2c` | RAM_D2C | raw `__attribute__((section(".ram_d2c")))` / `RAM_D2C_SECTION` |
| `.ram_d3` | RAM_D3 | `RAM_D3_SECTION` |
| `.instruction_ram` | ITCMRAM | raw `__attribute__((section(".instruction_ram")))` |

The macro definitions are duplicated between `TftDisplay.h` and
`preenfm3.cpp`; keep them in sync.

## Placement rules

1. **DTCM** — data read or written every sample by the CPU, plus the stack.
   Also the home of the six waveform tables and the small user data buffers
   (scales, envelope curves) because both TCMs are zero-wait. Never put
   mutable user data here if ITCM has room.
2. **ITCM** — instruction fetch, the fixed FM lookup tables (`sintab`,
   `exp2tab`, `lut`, the envelope increment table) and the mutable user
   waveform buffers. It is the fastest memory for both code and data.
3. **RAM_D1** — large buffers that are written by the CPU or consumed by the
   display path (`tft` object and its framebuffers, FX delay lines), and
   engine state arrays that are too big for DTCM.
4. **RAM_D2** — audio data close to the SAI DMA and the larger FX buffers.
   Write-through cacheable, so CPU writes reach memory without an explicit
   clean.
5. **RAM_D2B** — CPU-only audio buffers (FX2 delay and diffusers). Write-through
   cacheable; no DMA touches this region.
6. **RAM_D2C** — the SPI2/SD DMA and FatFs buffers. Non-cacheable, so no cache
   maintenance is needed (the safety property from the MPU work). Do not put
   per-sample lookup tables here.
7. **RAM_D3** — cold bulk data that no DMA touches: sequencer steps/actions
   and the editor/storage PFM3 preset mirrors. Write-back caching is fine.
8. **FLASH** — code and constants. Read-only data lives in `.rodata`; the six
   waveform tables also have a `const` copy there as the boot-time load image
   for the DTCM buffers.

## Startup and binary caveats

`startup_stm32h753vitx.s` copies `.data` from its flash load address
(`_sidata`) and zeroes `.bss`, both in DTCMRAM. The custom sections
(`.ram_d1`, `.ram_d2`, `.ram_d2b`, `.ram_d2c`, `.ram_d3`,
`.instruction_ram`) are marked `(NOLOAD)` in the linker script, and the build
also removes them from the flash image (`-R` flags in
`firmware/CMakeLists.txt`). They have **no initialisation path**.

To make their contents deterministic, `ZeroCustomRam()` in
`firmware/src/app/main.c` clears every custom section from the startup code,
after `SystemInit()` and before `__libc_init_array()`. Objects with
constructors are therefore safe: the memory is zero before the constructor
runs. Because the custom sections are cleared, the waveform tables are copied
from their flash load image by `initWaveTables()` at the start of
`Synth::init()`.

Two rules follow:

- A static initialiser on an object placed in a custom section is **silently
  ignored**. Zero-initialise at declaration and fill the data from code at
  runtime (as `msfaInit()` does for the ITCM tables).
- Flash the generated `p3_1_04.bin`, not the ELF: only the binary is known
  to contain exactly the flash load image.

## Current contents

Largest symbols per region, from
`arm-none-eabi-nm --print-size --size-sort` on the LTO ELF.

**DTCM** — 110,888 B

| Bytes | Symbol |
|---|---|
| 22,592 | `Preenfm3Engine::pfm3VoiceStates_` (16 × PFM3 voice state) |
| 15,604 | `synth` (contains `Timbre timbres_[6]`) |
| 8,196 | `sinTable` |
| 8,192 ×2 | `squareTable`, `sawTable` |
| 6,240 | `SynthEngineFactory::pfm3Engines_` (6 × PFM3 engine) |
| 4,096 ×3 | `sinPosTable`, `sinOrZeroTable`, `sinSquareTable` |
| 3,072 | `scalaFrequency` (6 × 128 floats) |
| 1,776 | `sdCard` |
| 1,760 | `synthState` |
| 1,032 ×2 | `hpcd_USB_OTG_FS`, `__malloc_av_` |
| 1,024 | `userEnvCurves` (4 × 64 floats) |
| 768 ×2 | `algoOpInformation`, `EditorPfm3Algo::modulationIndex` |
| 708 | `hUsbDeviceFS` |

**ITCM** — 51,464 B

| Bytes | Symbol |
|---|---|
| 24,576 | `userWaveform` (6 × 1024 floats) |
| 8,192 ×2 | `exp2tab`, `sintab` |
| 6,404 | `Pfm3Env::incTab` |
| 4,100 | MSFA `lut` |

**RAM_D1** — 487,808 B

| Bytes | Symbol |
|---|---|
| 171,440 | `tftBackground` |
| 153,600 | `tftMemory` |
| 60,000 | `tftForeground` |
| 28,676 | `FxBus::delay1Buffer` |
| 23,956 | `FxBus::delay2Buffer` |
| 16,256 | `dx7VoiceStates` |
| 8,192 | `Pfm3Lfo::invTab` |
| 6,732 | `tft` (`FirmwareTftDisplay`) |
| 6,144 | `midiNoteScale` |
| 5,472 | `Preenfm3Engine::pfm3Params_` |
| 1,024 | `ByosEngine::voiceStates_` |
| 1,008 | `pfm3PresetTmp` |
| 512 ×2 | `Porta::rates`, `Porta::rates_glissando` |
| 936 | `Dx7SynthEngine::program_` |

**RAM_D2** — 121,992 B

| Bytes | Symbol |
|---|---|
| 32,000 | `FxBus::predelayBuffer` |
| 27,156 | `FxBus::delay3Buffer` |
| 20,368 | `FxBus::delay4Buffer` |
| 19,200 | `imagePPM` |
| 11,592 | `FxBus::diffuserBuffer2` |
| 4,324 | `FxBus::diffuserBuffer1` |
| 2,440 | `FxBus::inputBuffer3` |
| 1,780 | `FxBus::inputBuffer4` |
| 908 | `FxBus::inputBuffer1` |
| 688 | `FxBus::inputBuffer2` |
| 512 ×3 | `waveform1` … `waveform3` |

**RAM_D2B** — 72,100 B

| Bytes | Symbol |
|---|---|
| 49,152 | `Timbre::delayBuffer` (6 × 2048 floats) |
| 17,104 | `FxBus::diffuserBuffer4` |
| 5,844 | `FxBus::diffuserBuffer3` |

**RAM_D2C** — 24,144 B

| Bytes | Symbol |
|---|---|
| 8,192 | `storageBuffer` |
| 3,840 | `dx7BankAlloc` |
| 1,920 ×4 | `preenFMBankAlloc`, `preenFMMixerAlloc`, `preenFMSequenceAlloc`, `scalaScaleFileAlloc` |
| 1,024 | `lineBuffer` |
| 560 | `USERFatFS` |
| 552 ×4 | `file`, `imageFile`, `mixerFile`, `sequenceFile` |
| 512 | `dummySector` |
| 128 | `dx7PackedPatch` |

**RAM_D3** — 48,040 B

| Bytes | Symbol |
|---|---|
| 24,576 | `stepNotes` |
| 16,384 | `actions` |
| 6,048 | `pfm3Presets_` (6 × `Pfm3Preset` editor mirrors) |
| 1,032 | `backupTimbre_` (ScreenMenu snapshot) |

**FLASH** — the six waveform tables also have a `const` copy in `.rodata`
(36,868 B) as the load image for the DTCM buffers, together with the preset
defaults and the code.

## Compile-time and build-time checks

`firmware/src/app/synth/StaticAsserts.cpp` fails the build if the on-flash
format or the DTCM budget changes silently:

- `sizeof(Pfm3Preset) == 1008`
- `sizeof(FlashSynthParams) == 936`
- the two envelope-curve flash blocks are 16 and 8 bytes
- `presetName`, `envCurves1To4` and `envCurves5To6` offsets inside
  `FlashSynthParams` (720, 896, 912)
- `MAX_NUMBER_OF_VOICES <= 127`
- `sizeof(Timbre) <= 4096` and `sizeof(SynthState) <= 2048`, so a large array
  cannot be re-inlined into a DTCM-resident object

`cmake/check_firmware_memory.cmake` runs as a post-build step and fails the
build when a RAM region is above 95 %. It prints the used size of every
region, so the current budget is visible in every build.

## Remaining tight spots

- **DTCM (89.86 %)** holds the six waveform tables and the hot per-sample
  state; ITCM has ~13 KB free for hot code and small tables.
- **RAM_D1 (93 %)** and **RAM_D2 (93 %)** are the regions to watch. A new
  large buffer must either fit in RAM_D3/DTCM or replace an existing one.
  The post-build check turns a silent overflow into a build failure.
- **`userWaveform` is in ITCM** (24.6 KB). If ITCM is ever needed for code,
  that buffer is the first candidate to move to RAM_D1; user-waveform playback
  would then go through the cache.
- **RAM_D2C is no longer empty**: it holds the non-cacheable SD/SPI2/FatFS DMA
  buffers (24,144 B, 73.68 %). **RAM_D2B** holds CPU-only audio and is
  write-through cacheable.
- **`nullNames`** in `ScreenMixer.cpp` is a zero-length array used as an
  empty name table. It consumes no RAM despite the symbol size reported by
  `nm`.

## Checking usage

```sh
arm-none-eabi-size -A build/firmware/preenfm3_firmware.elf
arm-none-eabi-nm --print-size --size-sort --radix=d build/firmware/preenfm3_firmware.elf
```

The first command gives the per-section sizes (sum them per region to
reproduce the `Used` column above); the second lists symbols by size, which
can be bucketed by address range to find what fills each region. `cmake
--build build` also runs the budget check and prints the per-region summary.

## See also

- [improvements.md](improvements.md) — MPU regions, cache coherency, hardware
  performance items.
- [compilation.md](compilation.md) — build flags, LTO and the toolchain.
- [hardware.md](hardware.md) — MCU, audio and display wiring.
- [what_pfm3preset_is.md](what_pfm3preset_is.md) — how the flat PFM3 preset
  mirror relates to the live engine parameters.