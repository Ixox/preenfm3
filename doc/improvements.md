# Improvements

Known issues and suggested improvements for the preenFM3 firmware, covering
STM32H7 cache/MPU correctness, the display pipeline, audio performance and
broader project-level work.

## Table of Contents

- [MPU and cache coherency](#mpu-and-cache-coherency)
  - [Current setup](#current-setup)
  - [Problem 1 — D-cache enabled before the MPU is configured](#problem-1--d-cache-enabled-before-the-mpu-is-configured)
  - [Problem 2 — `.ram_d2b` is write-back but used for SPI2 DMA to the SD card](#problem-2--ram_d2b-is-write-back-but-used-for-spi2-dma-to-the-sd-card)
  - [Problem 3 — No `__DSB` / `__DMB` barriers around DMA](#problem-3--no-__dsb--__dmb-barriers-around-dma)
  - [Problem 4 — Display path has no barrier between DMA2D and SPI DMA](#problem-4--display-path-has-no-barrier-between-dma2d-and-spi-dma)
  - [Problem 5 — `S=1` is not a substitute for cache maintenance](#problem-5--s1-is-not-a-substitute-for-cache-maintenance)
  - [Problem 6 — `RAM_D2C` has no MPU region](#problem-6--ram_d2c-has-no-mpu-region)
- [Display and ScreenCoordinator](#display-and-fmdisplay3)
  - [Problem 1 — `TFT_WAITCYCLE` and oscilloscope background share action type 14](#problem-1--tft_waitcycle-and-oscilloscope-background-share-action-type-14)
  - [Problem 2 — action ring buffer can overflow silently](#problem-2--action-ring-buffer-can-overflow-silently)
  - [Problem 3 — `endRefreshStatus_` can survive a mode change](#problem-3--endrefreshstatus_-can-survive-a-mode-change)
  - [Problem 4 — waveform selector off-by-one](#problem-4--waveform-selector-off-by-one)
  - [Problem 5 — dirty bit is cleared before SPI DMA completes](#problem-5--dirty-bit-is-cleared-before-spi-dma-completes)
  - [Problem 6 — refresh steps are hard-coded magic numbers](#problem-6--refresh-steps-are-hard-coded-magic-numbers)
  - [Problem 7 — SPI1 clock may exceed the ILI9341 rating](#problem-7--spi1-clock-may-exceed-the-ili9341-rating)
  - [Problem 8 — duplicate TFT pin definitions](#problem-8--duplicate-tft-pin-definitions)
  - [Display suggested order of work](#display-suggested-order-of-work)
- [Audio performance](#audio-performance)
  - [Audio path budget](#audio-path-budget)
  - [1. PFM3 — wave-table reload per sample](#1-pfm3--wave-table-reload-per-sample)
  - [2. Synth — hardware RNG in the audio callback](#2-synth--hardware-rng-in-the-audio-callback)
  - [3. I-cache pressure in the render switches](#3-i-cache-pressure-in-the-render-switches)
  - [4. Skip FX setup when the effect is off](#4-skip-fx-setup-when-the-effect-is-off)
  - [5. DX7 — fixed-point kernel](#5-dx7--fixed-point-kernel)
  - [6. Mixing loops](#6-mixing-loops)
  - [7. Already-good behaviour](#7-already-good-behaviour)
  - [Audio performance suggested order of work](#audio-performance-suggested-order-of-work)
  - [Also noticed (correctness, not performance)](#also-noticed-correctness-not-performance)
- [MPU suggested order of work](#mpu-suggested-order-of-work)
- [Broader project improvements](#broader-project-improvements)

---

## MPU and cache coherency

Status: problems 1–6 below are fixed. The MPU is configured before the caches,
`RAM_D2B` and `RAM_D2C` are non-cacheable, `RAM_D3` is not shareable, and
`__DSB()` barriers surround the SAI, DMA2D and SD DMA transfers.

The STM32H753 has a Cortex-M7 with separate I-cache and D-cache. DMA masters (SAI, SPI, DMA2D) are **not coherent** with the CPU cache. The firmware therefore has to either:

- place DMA buffers in non-cacheable memory, or
- clean the D-cache before DMA reads from a buffer, and invalidate it after DMA writes to a buffer.

The current MPU setup uses cacheable memory for all DMA buffers and does not perform cache maintenance. This creates several correctness risks.

### Current setup

- `HAL_Init()`, then `MPU_RegionConfig()`, then `SCB_EnableICache()` /
  `SCB_EnableDCache()` in `firmware/src/app/main.c`.
- `MPU_RegionConfig()` is defined in `firmware/src/app/main.c`.
- Five MPU regions are configured:

| Region | Range | Attributes | Policy |
|--------|-------|------------|--------|
| 1 | `0x24000000` + 512 KB | TEX0, C=1, B=0, S=0 | Normal, write-through |
| 2 | `0x30000000` + 128 KB | TEX0, C=1, B=0, S=0 | Normal, write-through |
| 3 | `0x30020000` + 128 KB | TEX1, C=0, B=0, S=1 | Normal, non-cacheable |
| 4 | `0x38000000` + 64 KB | TEX0, C=1, B=1, S=0 | Normal, write-back, write-allocate |
| 5 | `0x30040000` + 32 KB | TEX1, C=0, B=0, S=1 | Normal, non-cacheable (`RAM_D2C`) |

The linker sections map as follows (`firmware/preenfm3_2MB.ld:161-179`):

- `.ram_d1` → Region 1: display buffers, LFO table, some FX buffers
- `.ram_d2` → Region 2: DAC `waveform1/2/3`, PPM image, FX buffers
- `.ram_d2b` → Region 3: FatFS/SD/file/sequence buffers
- `.ram_d3` → Region 4: sequencer data

---

### Problem 1 — D-cache enabled before the MPU is configured

**Severity: high**

`SCB_EnableDCache()` runs at `firmware/Src/main.c:119`, but `MPU_RegionConfig()` does not run until `firmware/Src/main.c:164`. Several peripherals are also initialized in between:

- `MX_DMA_Init()`
- `MX_SAI1_Init()`, `MX_SAI2_Init()`
- `MX_SPI1_Init()`, `MX_SPI2_Init()`
- `MX_FATFS_Init()`
- `MX_USB_DEVICE_Init()`

Until the MPU is enabled, the default memory map applies. Writes made in that window may populate the D-cache with attributes that differ from the final MPU attributes. Changing MPU attributes later does not clean or invalidate existing cache lines.

**Recommendation**

Configure the MPU **before** enabling the D-cache:

1. `HAL_MPU_Disable()`
2. configure all MPU regions
3. `HAL_MPU_Enable(...)`
4. `SCB_EnableICache()` / `SCB_EnableDCache()`

If the MPU must be reconfigured after caches are already on, clean and invalidate the D-cache around the reconfiguration.

---

### Problem 2 — `.ram_d2b` is write-back but used for SPI2 DMA to the SD card

**Severity: critical**

Region 3 is configured with `IsBufferable = MPU_ACCESS_BUFFERABLE` and `IsCacheable = MPU_ACCESS_CACHEABLE`, which means **write-back, write-allocate**, not write-through. The comment in `firmware/Src/main.c:732` says “Configure the MPU attributes as WT for RADM_D2B”, but the actual bits configure WBWA.

`.ram_d2b` contains buffers used by FatFS and the SD card, for example:

- `lib/Src/fatfs.c:24` — `FATFS USERFatFS`
- `lib/Src/adafruit_802_sd.c:97` — `dummySector[512]`
- `firmware/Src/filesystem/*.cpp` — file, bank and sequence buffers

The DMA SD path is:

- `lib/Src/sd_diskio.c:144-149` — `SD_read_DMA()` calls `PFM3_SD_ReadBlocks(..., true)`
- `lib/Src/sd_diskio.c:191-196` — `SD_write_DMA()` calls `PFM3_SD_WriteBlocks(..., true)`
- `lib/Src/adafruit_802_sd.c:1453-1456` — `SD_IO_WriteReadData_DMA()` calls `BSP_SPI_SendRecv_DMA()`
- `lib/Src/stm32h7xx_nucleo_bus.c:215-221` — `BSP_SPI_SendRecv_DMA()` calls `HAL_SPI_TransmitReceive_DMA(&sd_spi2, ...)`

There is no cache maintenance around these transfers:

- no `SCB_CleanDCache_by_Addr()` before SPI2 TX
- no `SCB_InvalidateDCache_by_Addr()` after SPI2 RX

Consequences:

- **Write path:** FatFS writes a sector into `.ram_d2b`; the data may still be dirty in the D-cache when SPI2 DMA reads it. The card can receive stale data.
- **Read path:** SPI2 DMA writes a sector into `.ram_d2b`; the CPU may later read a stale cached line instead of the newly received data. File-system corruption is possible.

This is the most serious cache/MPU issue in the firmware.

**Recommendation**

Either:

1. Mark `.ram_d2b` as non-cacheable:
   - `IsCacheable = MPU_ACCESS_NOT_CACHEABLE`
   - `IsBufferable = MPU_ACCESS_NOT_BUFFERABLE`
   - `IsShareable = MPU_ACCESS_SHAREABLE`
   - `TypeExtField = MPU_TEX_LEVEL1` (normal, non-cacheable)

or:

2. Keep it cacheable and add maintenance around every SPI2 DMA transfer:
   - before TX: `SCB_CleanDCache_by_Addr()` then `__DSB()`
   - after RX: `SCB_InvalidateDCache_by_Addr()` then `__DSB()`
   - align buffers and lengths to 32-byte cache lines

For a simple, low-throughput SD card path, making `.ram_d2b` non-cacheable is usually the safer choice.

---

### Problem 3 — No `__DSB` / `__DMB` barriers around DMA

**Severity: high**

A project-wide search finds no application-level `__DSB()` or `__DMB()` around DMA starts or completions. The HAL uses barriers internally in a few places, but the application does not.

Even for write-through memory, software must ensure that CPU stores are complete before a DMA master is allowed to read the buffer. The normal sequence is:

```c
/* before DMA TX */
__DSB();

/* after DMA RX, before CPU reads */
SCB_InvalidateDCache_by_Addr(...);
__DSB();
```

The SAI DAC path is a good example. `HAL_SAI_TxCpltCallback()` and `HAL_SAI_TxHalfCpltCallback()` in `firmware/Src/preenfm3.cpp:328-349` call `synth.buildNewSampleBlock()` to fill the next half of `waveform1/2/3`, but there is no barrier after the fill. The buffers are write-through, so the data usually reaches SRAM, but completion is not formally guaranteed before the DMA consumes it.

**Recommendation**

Add explicit barriers:

- after filling a DMA TX buffer and before the DMA can consume it
- before starting a DMA transfer
- after a DMA RX transfer completes, before the CPU reads the buffer
- around cache clean/invalidate operations

For the SAI path, a `__DSB()` after `buildNewSampleBlock()` in both SAI callbacks would make the ordering explicit.

---

### Problem 4 — Display path has no barrier between DMA2D and SPI DMA

**Severity: medium**

The display buffers are in `.ram_d1` (Region 1, write-through):

```cpp
// lib/Src/TftDisplay.cpp:33-35
RAM_D1_SECTION uint8_t  tftForeground[60000];
RAM_D1_SECTION uint16_t tftBackground[42860 * 2];
RAM_D1_SECTION uint16_t tftMemory[240 * 320];
```

The display pipeline is:

1. CPU writes glyphs/background data into `tftForeground` and `tftBackground`.
2. DMA2D reads those buffers and writes composed pixels into `tftMemory`.
3. SPI1 DMA reads `tftMemory` and sends it to the ILI9341.

`TftDisplay::pushToTft()` (`lib/Src/TftDisplay.cpp:641-675`) starts `HAL_SPI_Transmit_DMA()` after DMA2D has produced the frame data. There is no explicit `__DSB()` between the DMA2D operation and the SPI DMA start.

Because the regions are write-through, the data is normally visible in SRAM, but the ordering is not formally guaranteed. In addition, DMA2D writes bypass the CPU cache; if the CPU ever reads `tftMemory`, it could see stale cached data.

**Recommendation**

- Add `__DSB()` after DMA2D completion and before starting SPI1 DMA.
- Consider making `tftMemory` non-cacheable, since it is primarily a DMA2D/SPI DMA buffer.
- If the CPU never reads `tftMemory`, document that invariant; otherwise invalidate the relevant cache lines before CPU reads.

---

### Problem 5 — `S=1` is not a substitute for cache maintenance

**Severity: medium**

Regions 3 and 4 are marked `IsShareable = MPU_ACCESS_SHAREABLE`. The `S` bit describes sharing between bus masters; it does not by itself provide hardware cache coherency between the Cortex-M7 and DMA.

The `C` and `B` bits still select a cacheable write-back policy for Region 3. Relying on `S=1` to make DMA safe is fragile and implementation-dependent. The firmware should explicitly choose one of:

- non-cacheable memory for DMA buffers, or
- cacheable memory plus clean/invalidate maintenance

**Recommendation**

Make the intent explicit in the MPU configuration and add the corresponding maintenance. Do not rely on the shareable attribute for DMA coherency.

---

### Problem 6 — `RAM_D2C` has no MPU region

**Severity: low**

`firmware/preenfm3_2MB.ld:47` defines:

```ld
RAM_D2C (xrw) : ORIGIN = 0x30040000, LENGTH = 32K
```

but there is no `.ram_d2c` output section and no MPU region for `0x30040000`. It is currently unused, so this is not an active bug. If a future DMA buffer is placed there, it will use the default memory map and may be cacheable without maintenance.

**Recommendation**

Either remove `RAM_D2C` from the linker script or add an explicit MPU region and linker section for it.

---

## Display and ScreenCoordinator

Status: problems 1, 3, 4 and 5 below are fixed. Problem 2 (ring buffer
overflow protection) was reverted: the current behaviour works and the
`isFull()` check is not wanted on the insertion path. Problem 7 (SPI1 clock)
is **accepted**: 32 MHz works and must not be changed. Problem 6 (named
refresh-step constants) is low severity and not planned.

### Problem 1 — `TFT_WAITCYCLE` and oscilloscope background share action type 14

**Severity: high**

`TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM` is defined as an alias of `TFT_STANDARD_LAST_ACTION` (`firmware/Src/hardware/FirmwareTftDisplay.h:36-38`), which is the same numeric value as `TFT_WAITCYCLE` (`lib/Inc/TftDisplay.h:40-57`, value 14).

`FirmwareTftDisplay::additionalActions()` handles that action type after the standard switch. However, `TftDisplay::tic()` also has a `TFT_WAITCYCLE` case (`lib/Src/TftDisplay.cpp:625-631`) that decrements `currentAction.param2`. The oscilloscope background action constructors create `TFTAction newAction;` and never initialize `param2` (`FirmwareTftDisplay.cpp:150-190`).

Consequences:

- `param2` is read uninitialized by the wait-cycle case.
- If the uninitialized value is 1, `TFT_WAITCYCLE` clears `actionType` to 0 before `additionalActions()` runs, so the oscilloscope background action is silently dropped.

**Recommendation**

- Give the background action a distinct action type. Value 15 is currently free as an action type because `TFT_DRAW_ENVELOPPE` and `TFT_DRAW_LFO` are `param1` selectors, not action types.
- Initialize every `TFTAction` as `TFTAction newAction{};`.
- Alternatively, handle firmware-specific actions before the standard switch, or widen `actionType` to 5 bits and assign separate values.

---

### Problem 2 — action ring buffer can overflow silently

**Severity: high**

The action queue is `RingBuffer<TFTAction, 128>`. `RingBuffer::insert()` (`lib/Inc/RingBuffer.h:36-45`) always writes to the buffer and advances `tail`; it never checks `isFull()`. `isFull()` exists but is not used.

`refreshAllScreenByStep()` callers throttle at `< 100` pending actions (`firmware/Src/preenfm3.cpp:205-207`), but a single refresh step can push several actions, and not every queue producer goes through the same throttle. If the queue fills, the tail wraps and overwrites actions that have not been processed yet.

**Recommendation**

- Make `RingBuffer::insert()` return a success flag and check `isFull()`.
- Drop, assert, or log on overflow instead of silently corrupting the queue.
- Throttle using the actual remaining capacity, not a hard-coded 100.
- Consider increasing `TFTACTION_BUFFER_SIZE` if UI bursts regularly approach the limit.

---

### Problem 3 — `endRefreshStatus_` can survive a mode change

**Severity: medium**

`ScreenCoordinator::newSynthMode()` sets `refreshStatus_ = 21` but does not reset `endRefreshStatus_` (`firmware/Src/hardware/ScreenCoordinator.cpp:174-192`). `setRefreshStatus()` also only writes `refreshStatus_` (`ScreenCoordinator.h:89-91`).

Partial refreshes use `endRefreshStatus_` as a stop step. For example, `newTimbre()` sets `refreshStatus_ = 19`, `endRefreshStatus_ = 14` for the mixer. If the user changes mode before that partial refresh completes, the new full refresh can stop at the stale `endRefreshStatus_` value.

**Recommendation**

- Reset `endRefreshStatus_ = 0` in `newSynthMode()` and in `setRefreshStatus()`.
- Or replace the two integers with a small `RefreshRange { start, end }` object that is always replaced atomically.

---

### Problem 4 — waveform selector off-by-one

**Severity: medium**

In `FirmwareTftDisplay::additionalActions()` the operator-waveform branch checks:

```cpp
if (currentAction.param1 <= TFT_NUMBER_OF_WAVEFORM_EXT) {
    oscilloBgDrawOperatorShape(waveForm[currentAction.param1].waveForms, ...);
}
```

`waveForm` is declared as `WaveFormExt waveForm[TFT_NUMBER_OF_WAVEFORM_EXT]` (`FirmwareTftDisplay.h`), so valid indices are 0–13. The `<=` test allows `param1 == 14`, which reads `waveForm[14]` out of bounds.

**Recommendation**

Use `< TFT_NUMBER_OF_WAVEFORM_EXT` and add an explicit range check/assert for `param1`.

---

### Problem 5 — dirty bit is cleared before SPI DMA completes

**Severity: medium**

`TftDisplay::pushToTft()` clears the part's dirty bit immediately after `HAL_SPI_Transmit_DMA()` returns `HAL_OK` (`lib/Src/TftDisplay.cpp:663-664`). The bit is not restored if the transfer later fails or is aborted. `HAL_SPI_ErrorCallback()` only handles `sd_spi2` (`firmware/Src/stm32h7xx_it.c:376-382`); there is no SPI1/TFT error recovery.

If a transfer fails after the bit is cleared, that screen part may never be refreshed again.

**Recommendation**

- Clear the dirty bit in `pushToTftFinished()` after successful completion.
- On SPI error, restore the bit or mark the display for a full refresh.
- Add an SPI1 error path in `HAL_SPI_ErrorCallback()`.

---

### Problem 6 — refresh steps are hard-coded magic numbers

**Severity: low**

`refreshStatus_` uses values such as 21, 20, 19–14, 12–7 and 6–1, with a `switch` in each screen class and an implicit contract with `endRefreshStatus_`. Missing numbers are silent no-ops, and the same numeric ranges are repeated across `ScreenMixer`, `ScreenEditor`, `ScreenMenu` and `ScreenSequencer`.

This makes it easy to add a drawing step without updating the countdown start, the partial-refresh range, or the stop condition.

**Recommendation**

- Define named step constants or a per-screen `enum` instead of raw integers.
- Consider a table of step functions, or at least a comment block documenting each screen's step map.
- Keep `start`, `end`, and the set of valid steps in one place.

---

### Problem 7 — SPI1 clock may exceed the ILI9341 rating

**Severity: medium**

`hspi1.Init.BaudRatePrescaler` is `/2` (`firmware/Src/main.c:485`) and the SPI1/2/3 kernel clock is selected from PLL2P (`main.c:259-281`). With PLL2P = 64 MHz, SCK is approximately 32 MHz.

The ILI9341 datasheet typically specifies around 10 MHz for write and 6.67 MHz for read. The display may work in practice, but 32 MHz is well above the nominal limit, and `ILI9341_ReadPowerMode()` uses the same SPI bus for reads.

**Recommendation**

- Accepted as-is: 32 MHz works reliably on the tested panels. **Do not change
  the SPI1 clock.**

---

### Problem 8 — duplicate TFT pin definitions

**Severity: low**

Two headers define TFT pins differently:

- `firmware/Inc/main.h:72-77`: `TFT_CS=PE11`, `TFT_DC=PE12`, `TFT_RESET=PE13`
- `lib/Inc/preenfm3_pins.h:45-50`: `TFT_DC=PD8`, `TFT_RESET=PD9`, `TFT_CS=PD10`

The display driver includes `preenfm3_pins.h`, so the `main.h` aliases are stale and misleading. They can also conflict with the TIM1_CH2 backlight pin on PE11.

**Recommendation**

Remove the unused TFT aliases from `main.h`, or add a comment that the authoritative display pin mapping is in `preenfm3_pins.h`.

---

### Display suggested order of work

1. ~~Fix the action type 14 collision and initialize `TFTAction` fields.~~ Done.
2. Ring buffer overflow protection: reverted. Current behaviour works; the
   `isFull()` check must not slow the insertion path.
3. ~~Reset `endRefreshStatus_` on mode changes.~~ Done.
4. ~~Move dirty-bit clearing to DMA completion and add SPI1 error recovery.~~ Done.
5. ~~Fix the `waveForm` off-by-one.~~ Done.
6. SPI1 clock: accepted at 32 MHz; do not change.
7. Replace hard-coded refresh step numbers with named constants. Not planned
   (low severity).

---

## Audio performance

Notes from a walk through the sample-generation path. These are **candidate
optimisations to measure**, not confirmed bottlenecks. The DWT cycle counter is
already available (`SHOW_CPU_USAGE` in `firmware/src/platform/clock/dwt.h`) and
`Synth::buildNewSampleBlock()` is wrapped with `CYCLE_MEASURE_START/END`, so the
existing CPU indicator can validate any change.

### Audio path budget

- Sample rate: `PREENFM_FREQUENCY` = 47,916 Hz; `BLOCK_SIZE` = 32.
- One block every ~668 µs, i.e. ~1,497 blocks/s.
- At 480 MHz that is ~320,000 CPU cycles per block.
- Worst case per block: 6 timbres × 16 shared voices × up to 6 operators ×
  32 samples = 3,072 oscillator samples, plus envelopes, FX1 (per voice),
  FX2 (per timbre), compressor, FX bus and output conversion.

The PFM3 engine is float DSP; the DX7 engine is 32-bit fixed point.

### 1. PFM3 — wave-table reload per sample

**Priority: high. Implemented.** `Osc::getWaveTable()` returns the
`WaveTable*` for the current shape. `Voice::nextBlock()` hoists one pointer per
oscillator once per block and passes it to `Osc::getNextSample()`, so the
per-sample loop no longer indexes `waveTables[]`. This adds only six pointers
to the `nextBlock()` stack frame and **no persistent RAM** (`OscState` and
`Voice` sizes are unchanged), keeping DTCMRAM at 96.65%.

Measured on hardware: 10 notes with FX went from 45.1% to 41.4% CPU. Flash
grew by ~11 KB because the smaller `getNextSample()` is inlined more
aggressively (call overhead removed), which likely contributes to the gain.

### 2. Synth — hardware RNG in the audio callback

**Priority: medium.** `Synth::buildNewSampleBlock()` calls
`HAL_RNG_GenerateRandomNumber()` to seed the first two noise samples, then uses
a 32-bit LCG for the other 30. The HAL call polls the RNG ready bit and can
block for several microseconds inside the audio interrupt.

Suggested action: generate all 32 noise values with the existing LCG (or seed
the LCG from the RNG once, outside the audio path). This removes a
variable-latency hardware call from the block callback.

### 3. I-cache pressure in the render switches

**Priority: medium, measure first.** `Voice::nextBlock()` is a very large
function with a per-algorithm `switch` and one per-sample loop per algorithm.
`Voice::fxAfterBlock()` and `Timbre::fxAfterBlock()` are also large switches.
The STM32H7 I-cache is 16 KB. With six timbres using different algorithms and
FX, the hot code can exceed it and cause I-cache misses in the audio ISR.

Suggested action: measure with the DWT counter; if it matters, keep the
per-sample loops compact and factor shared code rather than adding more
algorithm-specific inline code.

### 4. Skip FX setup when the effect is off

**Priority: low.** `Voice::fxAfterBlock()` (`FILTER_OFF`) and
`Timbre::fxAfterBlock()` (`FILTER2_OFF`) compute matrix destinations and
smoothed parameters before the `switch`, even when the effect is off.

Suggested action: early-return when `effect1.type == FILTER_OFF` /
`effect2.type == FILTER2_OFF`, keeping the delay tails running only for
non-off effects. This saves a few matrix reads and multiplies per voice/timbre
per block.

### 5. DX7 — fixed-point kernel

**Priority: low/medium, only if DX7 is the hot engine.**
`FmOpKernel::compute()` / `compute_fb()` (`third_party/msfa/fm_op_kernel.cc`)
do `((int64_t)y * gain) >> 24` per sample, and `Sin::lookup()`
(`third_party/msfa/sin.h`) does another 64-bit multiply for delta
interpolation. On Cortex-M7 `SMULL` is cheap, so this is unlikely to dominate,
but if profiling shows the DX7 kernel at the top, consider a 32-bit path
(`__SMMUL`/`__SMULWB`) or a non-delta sine LUT. `SIN_DELTA` doubles the LUT
(8 KB) to save a subtraction; measure the trade-off.

`Env::getsample()` is already block-rate (once per block per operator), and
`dx7VoiceRender()` computes 64 samples at a time and outputs 32 per block.
Both are reasonable.

### 6. Mixing loops

**Priority: low.** `Timbre::voicesToTimbre()` and the per-timbre output dispatch
in `Synth::buildNewSampleBlock()` are simple scalar loops. They are already
branch-hoisted and benefit from `-funroll-loops`; the Cortex-M7 has no NEON, so
`-fno-tree-vectorize` is appropriate. Revisit only if profiling shows them.

### 7. Already-good behaviour

- `updateAllModulationIndexes()` and `updateAllMixOscsAndPans()` early-return
  after the operators used by the current algorithm.
- `prepareMatrixForNewBlock()` skips matrix work for non-playing voices.
- `Voice::emptyBuffer()` only clears a voice buffer once.
- `calculateFrequencyWithMatrix()` and the envelope advances are block-rate,
  not per sample.
- `Timbre::pfm3VoicesNextBlock()` sums unison voices into the first voice
  buffer, so `voicesToTimbre()` only copies one buffer in unison mode.

### Audio performance suggested order of work

1. ~~Hoist the `WaveTable` out of the per-sample loop.~~ Done: passed as a
   per-block pointer, no persistent RAM.
2. Remove the `HAL_RNG_GenerateRandomNumber()` call from the audio block
   callback.
3. Add early returns for `FILTER_OFF` / `FILTER2_OFF`.
4. Measure I-cache pressure before restructuring `Voice::nextBlock()`.
5. Only optimise the DX7 fixed-point kernel if it is the hot engine.

### Also noticed (correctness, not performance)

`Synth::buildNewSampleBlock()` uses `abs()` on a float
(`abs(smoothVolume_[timbre] - volume) < .0005f`). If `abs()` resolves to the
integer overload, the difference is truncated before the comparison. Prefer
`fabsf()`.

---

## MPU suggested order of work

1. ~~Fix Region 3: make `.ram_d2b` non-cacheable, or add clean/invalidate around SPI2 DMA.~~ Done: `RAM_D2B` is non-cacheable.
2. ~~Move MPU configuration before `SCB_EnableDCache()`.~~ Done.
3. ~~Add `__DSB()` barriers around SAI, DMA2D and SPI DMA transfers.~~ Done.
4. Cache policy for `tftMemory` and the display buffers: `.ram_d1` is
   write-through; `tftMemory` is written by DMA2D and read by SPI DMA, and the
   CPU does not read it. `pushToTft()` waits for DMA2D and issues a `__DSB()`
   before starting SPI DMA. No further change needed.
5. Align DMA buffers to 32-byte cache lines: not required with the current
   policy (non-cacheable `RAM_D2B`/`RAM_D2C`, write-through `RAM_D1`/`RAM_D2`).
6. Add a comment for every DMA buffer stating its cache policy: the MPU region
   comments cover this; add per-buffer notes when touching those buffers.

---

## Broader project improvements

Additional project-level ideas, orthogonal to the (now fixed) MPU and display
items above. These are candidates, not confirmed priorities.

1. **Add CI and host-side tests.** There is no `.github/` and no test target.
   Build firmware and bootloader on every push, and add host tests for pure
   logic: `Pfm3FlashFormat` round-trip and `pfm3Version` migration, DX7 patch
   unpacking (`Hexter`), algorithm tables and preset layout. This is the
   highest-leverage change: the `envCurves1To4` out-of-bounds access and the
   `Env`/`Lfo` ODR problems were only caught because LTO happened to warn.

2. **Enable stricter warnings and a curated `-Werror`.** Only `-Wall` is
   enabled. Add `-Wextra`, `-Wodr` (with LTO), `-Wstringop-overflow`,
   `-Wshadow` and `-Wdouble-promotion` for the float DSP. Fix the resulting
   set and enforce a clean subset in CI so the flags cannot regress.

3. **Define and enforce a memory-placement policy.** Done: see
   [memory_map.md](memory_map.md). Cold preset mirrors moved out of DTCM, the
   built-in waveform tables moved to DTCM (with a flash load image), the user
   waveform buffers moved to ITCM, `tft` moved to `.ram_d1`, and the
   custom RAM sections are zeroed before the constructors. Enforcement lives
   in `firmware/src/app/synth/StaticAsserts.cpp` (on-flash layout and DTCM
   budgets) and `cmake/check_firmware_memory.cmake` (95 % post-build limit).
   Raising `MAX_NUMBER_OF_VOICES` is now an ordinary budget decision; DTCM is
   at 75 %, so the limiting regions are RAM_D1/RAM_D2.

4. **Build a preset-compatibility harness.** Keep golden PFM3/DX7/BYX preset
   blobs and run them through `pfm3ParamsToFlash()` / `pfm3FlashToParams()` in
   both directions, including the `pfm3Version` migration path. The
   `FlashSynthParams` size/offset asserts now exist in
   `firmware/src/app/synth/StaticAsserts.cpp`; the round-trip tests remain.

5. **Keep a repeatable CPU benchmark.** The DWT counter already exists
   (`SHOW_CPU_USAGE`). Add a fixed benchmark set (for example a 10-note DX7
   patch and a 16-note PFM3 patch) and record the results in the repository.
   The ITCM and LTO experiments were only meaningful because they could be
   measured the same way.

6. **Split the largest translation units.** `Preenfm3Engine.cpp` (~5.5k
   lines), `Voice.cpp` (~4k), `EditorPfm3.cpp` (~3.4k) and `Timbre.cpp`
   (~3.2k) slow down builds and complicate review. Extract parameter
   pages/display code, voice-pool/mixer helpers and engine-specific parameter
   handling.

7. **Close out the remaining audio-path list.** Remove
   `HAL_RNG_GenerateRandomNumber()` from `Synth::buildNewSampleBlock()`, add
   the `FILTER_OFF` / `FILTER2_OFF` early returns, and fix the `abs()` →
   `fabsf()` correctness issue. Measure each with the benchmark from item 5.

8. **Run measured DX7 hot-path experiments.** The per-block `exp()` in
   `Dx7Note::compute()` (amp modulation), the virtual `FmCore::render()` call
   and the 64-bit `Sin::lookup()` interpolation are candidates. The earlier
   micro-optimisations were rolled back because the gain was small; revisit
   only with a benchmark and a listen test.

9. **Make the build reproducible.** Pin the GCC 16.2 toolchain in CI or a
   devcontainer, export `compile_commands.json`, add a `clang-tidy`
   configuration, and document why `-fno-tree-vectorize` and LTO are required
   (GCC 16 issues) so they are not removed later.

10. **Unify the storage boundary.** The firmware has
    `firmware/src/app/storage/`, while the bootloader calls FatFs and the SD
    BSP directly. Either extract a small shared storage layer (or document the
    boundary), and validate file sizes/CRC before copying firmware or preset
    data.

11. **Harden the bootloader.** Verify the application CRC/hash and size
    before jumping, add a recovery path for a corrupted application, and keep
    the SD/FatFs error handling explicit. This is separate from the flash-write
    logic.

12. **Clean up the documentation and third-party isolation.** Update the stale
    16-voice references (`doc/audio_path_architecture.md`,
    `doc/preenfm3_engine.md`, `doc/dx7_engine.md` and this file), and consider
    wrapping the vendored MSFA code in `namespace msfa` so an `Env`/`Lfo`-style
    ODR collision cannot reappear.
