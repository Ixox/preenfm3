# pvig `web-editor-adaptation` vs Ixox `master`

Summary of the changes in [pvig/preenfm3 `web-editor-adaptation`](https://github.com/Ixox/preenfm3/compare/master...pvig:preenfm3:web-editor-adaptation) relative to the original preenFM3 firmware line.

## Comparison

| | |
|---|---|
| Base | `Ixox/preenfm3` `master` (`65cb963`, v1.03-era) |
| Head | `pvig/preenfm3` `web-editor-adaptation` (`d120c09`) |
| Commits | 92 (91 by pvig, 1 by Hans-Henning Klos) |
| Period | 2025-04-27 to 2026-11-15 |
| Diff | 98 files, +17,617 / -9,510 lines |
| Branch version | firmware `v1.06g`, bootloader `1.09` |
| Layout | pre-CMake STM32CubeIDE tree (`firmware/Src`, `lib/`, `bootloader/`) |

> The branch predates this repository's CMake migration and uses the old
> `firmware/Src/...` layout. The current tree uses `firmware/src/app/...` and
> `preen_lib/...`, so paths and many files do not line up directly.

## Executive summary

This branch is a large feature fork of the preenFM3 firmware. It splits
per-voice and per-timbre FX into new modules (`VoiceFx.cpp`, `TimbreFx.cpp`),
overhauls filters and the FX bus, introduces anti-aliased oscillators and 2x
oversampling, expands the arpeggiator/step sequencer, reworks voice allocation
and MIDI/NRPN mapping, and adds numerous DSP/CPU/memory optimizations. It also
ships release binaries and IDE project files.

## Major changes

### 1. FM engine, oscillators and anti-aliasing

#### a. Per-operator start phase (`o1Ph`-`o6Ph`)

- **What:** a per-operator phase offset applied at note-on. The value is stored
  in degrees (0..360) in `OperatorPhaseRowParams::phase`, converted to a
  normalized wavetable position and passed to `Osc::newNote()`.
- **Modulation:** matrix destinations `OSC1_PHASE`..`OSC6_PHASE` add to the
  stored phase; `Voice::applyOperatorStartPhases()` wraps the result modulo 360
  degrees. The UI uses `ROW_OP_PHASE1..6` / `ENCODER_OSC_PHASE`, and the phase is
  also shown in the oscilloscope view.
- **Storage:** serialized in the `FlashSynthParams::phaseOp[6]` tail; older
  patches default to zero.
- **Files:** `Common.h`, `Voice.cpp`, `Voice.h`, `Osc.cpp`, `Osc.h`,
  `SynthState.h`, `PreenFMFileType.h/.cpp`, `FMDisplayEditor.cpp`, `Presets.cpp`.
- **Commits:** `e762669`, `344f8fb`

#### b. Per-operator warp (`o1Wr`-`o6Wr`)

- **What:** asymmetric stretching of the two half-cycles of the wavetable,
  range `-4..+4`. It remaps the wavetable phase before lookup, so positive warp
  stretches the first half-cycle and compresses the second, and vice versa.
- **Implementation:** `Osc::updateWarpWithMatrix()` combines the stored value with
  `OSC1_WARP`..`OSC6_WARP` matrix modulation, clamps NaN and the range, and
  applies a small deadband around zero. Rendering uses the C1-continuous
  `warpPhaseSmooth()` in the interpolating paths and `getWarpedIndexFast()` in
  the decimation paths. Mipmap selection uses `freq * (1 + |warp|)`.
- **UI/MIDI/storage:** `ENCODER_OSC_WARP`, MIDI CC mapping, and storage in
  `OperatorPhaseRowParams::unused1` / `FlashSynthParams::phaseOp[i].warp`;
  patches older than v1.5 default to neutral warp.
- **Files:** `Common.h`, `Osc.h`, `Osc.cpp`, `Voice.cpp`, `Voice.h`,
  `MidiDecoder.h`, `SynthState.h`, `PreenFMFileType.h/.cpp`,
  `FMDisplayEditor.cpp`, `Presets.cpp`.
- **Commits:** `344f8fb`, `2f31082`, `e762669`

#### c. 2x oversampling (`HQx2`)

- **Where:** `Voice::nextBlock()` in `Voice.cpp`. When `oversample2x_` is set,
  the voice runs the algorithm twice per output block (`runAlgoBlock(1, ...)`
  then `runAlgoBlock(2, ...)`) and oscillator phases advance at half rate
  (`phaseRateScale = 0.5f`).
- **Downsampling:** `Voice::decimateHalfBand2x()` applies a 7-tap Kaiser
  half-band FIR (M=3, beta=3.2, 3 MACs/sample/channel) with history in
  `decimHistory_` to return to the normal sample rate.
- **Mode selection:** `FM_DECIMATION_HQ2X` in `updateWaveDecimationMode()`;
  legacy `HQ` maps to `FULL`.
- **Files:** `Voice.cpp`, `Voice.h`, `Osc.h`, `Common.h`, `FMDisplayEditor.cpp`,
  `PreenFMFileType.cpp`, `Presets.cpp`.
- **Commits:** `f38af5d`, `cd19c34`, `ede9e93`

#### d. Windowed oscillator sync — PORTED

- **Ported:** sub-sample-accurate slave reset (`Osc::hardSyncToMaster()` and
  `Osc::getPhaseIncrement()`) for ALG29-32; the amplitude window itself was
  already in the base firmware.
- **Files:** `Osc.h`, `Preenfm3Engine.cpp`.
- **Commit:** `eb4afe9`.

#### e. FM output decimation (1..19-bit)

- **What:** oscillator output can be quantized to 1..19 bits
  (`FM_DECIMATION_1BIT`..`FM_DECIMATION_19BIT`) and optionally sample-and-held
  (D=2) before the envelope. `OscState` keeps the bit depth, scaling factors,
  step phase and held sample; `quantizeWaveSample()` performs the quantization.
- **Modes:** `FM_DECIMATION_FULL` disables decimation, `FM_DECIMATION_HQ2X`
  disables it and enables 2x oversampling, and legacy `HQ` maps to `FULL`.
- **Storage/UI:** `engineDecimation` in `OneSynthParams`; stored as
  `decimation + 1` in flash so zero-filled legacy patches default to `FULL`;
  `ENCODER_ENGINE_DECIMATION`, `ROW_ENGINE_DECIMATION`.
- **Files:** `Common.h`, `Osc.h`, `Osc.cpp`, `Voice.h`, `FMDisplayEditor.cpp`,
  `PreenFMFileType.h/.cpp`, `Presets.cpp`.
- **Commits:** `e762669`, `f38af5d`, `ede9e93`

#### f. Wave mipmap tables

- **What:** band-limited sub-tables for the built-in rich waveforms (saw,
  square, sin-zero, sin-pos, sin-square/S_Q4). `Osc::init()` builds them with
  `buildShapeMipmaps()` into `sawMipmaps`, `squareMipmaps*`, `sinZeroMipmaps`,
  `sinPosMipmaps`, `sq4Mipmaps*`; `waveMipmapTables[shape][0..4]` points at the
  base table and four band-limited levels.
- **Selection:** `getWaveTableMipmap()` uses the base table up to 1200 Hz, then
  levels 1..4 for <=2400, <=4800, <=9600 and above; when warp is active the
  selection frequency is multiplied by `1 + |warp|`.
- **Why:** reduces aliasing for high notes and strong FM.
- **Files:** `Osc.cpp`, `Osc.h`, `waves.c`, `SynthState.h`.
- **Commits:** `f38af5d`, `e18d87d`

#### g. ADAA on saturators, wavefolders and filter wrap — PORTED

- **Ported:** first-order ADAA helpers in `VoiceFx.cpp`, applied to `LPWS`,
  `SAT`, `SIGMOID`, `FOLD`, `WRAP`, `LPXOR`, `LPXOR2`, `ORYX3`, `18DB`,
  `LADDER2`, `TEEBEE` and `SVFLH`; `FxBus` `WARM`/`FUZZ` use the same technique.
- **Files:** `VoiceFx.cpp`, `FxBus.cpp`.
- **Commits:** `f38af5d`, `805d6f7`, `d120c09`.

#### h. `Osc` structure cleanup

- **What:** moved the non-hot `Osc::getNextBlock()` and
  `Osc::getNextBlockWithFeedbackAndEnveloppe()` implementations from `Osc.h` to
  `Osc.cpp`, keeping only the always-inline per-sample helpers in the header.
- **Why:** reduces header bloat, compilation size and code duplication across
  translation units.
- **Files:** `Osc.h` (~246 lines removed), `Osc.cpp` (~245 lines added),
  `Voice.cpp` (small call-site adjustment).
- **Commits:** `8f85b87`

#### i. `sqrtf` optimization — PORTED

- **What:** `hwSqrtf()` uses the Cortex-M7 FPU `vsqrt.f32` instruction when
  `__ARM_FP` is available and falls back to `sqrtf()`. The older
  `fastInvSqrt()` (Quake-style reciprocal square root) remains available.
- **Where:** `FxBus.cpp` ADAA saturation paths (Glue/Fuzz) use `hwSqrtf()` for
  the antiderivative; `Synth.cpp` saturation calls were adjusted to pass the
  timbre; `VoiceFx.cpp` uses `vsqrt.f32` in `tanh4ADAA1_F()` with a `sqrtf()`
  fallback.
- **Why:** avoids a software `sqrtf` call in the per-sample saturation math.
- **Files:** `FxBus.cpp`, `Synth.cpp`, `VoiceFx.cpp`.
- **Commits:** `d120c09`

#### j. `s^2` -> `s_q4` mipmaps

- **What:** the old `s^2` (sine-squared) oscillator shape was replaced by
  `S_Q4` (TX81Z W4) data. `OSC_SHAPE_SIN_SQUARE` is aliased as
  `OSC_SHAPE_S_Q4`, `LFO_SIN_SQUARE` as `LFO_S_Q4`, and the display name changed
  from `"s^2 "` to `"s_q4"`.
- **Mipmaps:** `Osc.cpp` builds `sq4MipmapsL1L2` and `sq4MipmapsL3L4` from
  `sinSquareTable` (now S_Q4 data) and registers them in
  `waveMipmapTables[OSC_SHAPE_SIN_SQUARE][1..4]`.
- **Why:** the TX81Z W4 waveform plus band-limited mipmaps gives less aliasing
  than the old `s^2` table.
- **Files:** `waves.c`, `Osc.cpp`, `SynthState.h`, `FMDisplayEditor.cpp`.
- **Commits:** `e18d87d`

- Related commits: `d120c09`, `8f85b87`, `e18d87d`, `805d6f7`, `f38af5d`,
  `eb4afe9`, `cd19c34`, `ede9e93`, `2f31082`, `344f8fb`, `e762669`.

### 2. Per-voice filters (`VoiceFx`)

#### a. Module split and filter dispatch — PORTED

- **Ported:** filter helpers, `Voice::fxAfterBlock()`, `recomputeBPValues()` and
  `setNewEffectParam()` moved from `Voice.cpp` to `VoiceFx.cpp`; `Voice.cpp`
  keeps the voice state/lifecycle.
- **Files:** `VoiceFx.cpp`, `Voice.cpp`, `Voice.h`, `Common.h`.
- **Commit:** `b2e341b`.

#### b. ZDF SVF and modulation anti-aliasing

- **What:** the state-variable modes use a zero-delay-feedback topology, with
  filter coefficients (`f1`, `f2`, resonance/gain) interpolated per sample over
  the 32-sample block instead of being stepped once per block, which removes
  zipper noise from fast modulation.
- **Transition safety:** filter-type changes are detected synchronously; state
  variables are clamped/zeroed where needed and `a1` is interpolated
  non-recursively to avoid NaN/Inf when switching between heterogeneous modes
  (e.g. `Crsh` -> `BP`).
- **Files:** `VoiceFx.cpp`, `Voice.h`.
- **Commits:** `b2e341b`, `d3ec718`

#### c. New/refined filter personalities

- **`Ap4D`:** true 4-pole dual nested allpass phaser with soft-saturated
  resonant feedback and stereo cross-feedback.
- **TB-303 (`TeeB`):** high-accuracy Pade frequency-to-pole mapping and a
  dynamic accent circuit (C13 “gummy capacitor” + MIDI velocity).
- **`Oryx`/`Oryx2`/`Oryx3`:** tamed resonance peaks with distinct variants.
- **Status:** `TeeB` and `ORYX3` are PORTED; `Ap4D`, `ORYX` and `ORYX2` are
  not.
- **Files:** `VoiceFx.cpp`, `Common.h`, `FMDisplayEditor.cpp`.
- **Commits:** `9d8b310`, `05d5e92`, `17c2676`

#### d. ADAA nonlinear stages — PORTED

- **Ported:** same ADAA work as §1.g; `FxBus` `WARM`/`FUZZ` use per-timbre ADAA
  state.
- **Files:** `VoiceFx.cpp`, `FxBus.cpp`.
- **Commits:** `f38af5d`, `805d6f7`, `d120c09`.

### 3. Timbre effects, FX bus and compressor

#### a. `TimbreFx` split and cleanup — PORTED

- **What:** per-timbre FX code was moved from `Timbre.cpp` to `TimbreFx.cpp`
  (initially `Timbre_fx.cpp`, then renamed). The dispatch covers flange, chorus,
  dimension, doubler, tripler, Bode, wide, delay-crunch, ping-pong, diffuser,
  grain 1/2 and stereo BP; later commits optimized and cleaned the module.
- **Files:** `TimbreFx.cpp` (new), `Timbre.cpp`, `Timbre.h`.
- **Commits:** `2a2916b`, `875db52`, `b53ebed`, `2837e43`, `a63032c`

#### b. Delay-line interpolation — PORTED

- **What:** delay reads use Hermite interpolation
  (`delayInterpolationHermite`, `hermiteInterpolation`) in addition to linear
  interpolation, improving quality on modulated delays.
- **Files:** `TimbreFx.cpp`, `Timbre.h`.
- **Commits:** `cd19c34`, `2837e43`

#### c. FX bus and saturation — PORTED (saturation)

- **Ported:** `FxBus::applyWarmSaturation()` with `PUNCH`, `TUBE`, `WARM`,
  `DRIVE`, `FUZZ` and `OVERDRIVE`; `WARM`/`FUZZ` use per-timbre ADAA state.
  Saturation is selected from the compressor preset; there is no separate
  mixer saturation control.
- **Files:** `FxBus.cpp`, `FxBus.h`, `Synth.cpp`.
- **Commits:** `a837174`, `86b5276`, `9b9fa1d`, `d120c09`.

#### d. Compressor presets — PORTED

- **Ported:** presets `Off`, `Glue`, `Punch`, `Tube`, `Limit`, `Drive`, `Fuzz`
  with matched saturation; `SimpleComp` gained `setKnee()`, `setMakeup()`,
  adaptive release and `compressBlock()`.
- **Files:** `compressor/SimpleComp.cpp`, `SimpleComp.h`,
  `SimpleCompProcess.inl`, `FxBus.cpp`, `Synth.cpp`, `ScreenMixer.cpp`,
  `Common.h`.
- **Commits:** `8137fae`, `aa0fd12`, `5acc683`, `81ce40e`, `43062f5`, `cad3af0`.

### 4. Envelopes, LFOs, modulation and web editor

#### a. Envelope attack/release curves

- **What:** per-envelope attack and release curves (`envCurves1To4`,
  `envCurves5To6`) use `CURVE_TYPE_*` tables; `Env::applyCurves()` applies the
  selected curve to each segment, and user curves `Usr1..Usr6` are supported.
- **Storage:** flash stores `curve + 1` so zero-filled legacy patches decode to
  linear; patches older than v1.5 default to linear.
- **Files:** `Env.cpp`, `Env.h`, `Common.h`, `LfoCurveTables.h`,
  `PreenFMFileType.cpp`, `FMDisplayEditor.cpp`, `Presets.cpp`.
- **Commits:** `94050a9`, `ab33c52`

#### b. Expanded LFOs

- **What:** new shapes (`SawD`, `DExp`, `DLog`, `RExp`, `RLog`, `AD`, `AHD`,
  `SDec`, `Plng`, `Plg2`, `SnSq`, `Sn0`, `Sn+`, `Usr1..Usr6`) and multi-shot
  sync modes (`1Si..8Si`, `1Se..8Se`) independent of `KSyn`; startup delay
  0-4000 ms on negative `Phase`.
- **Files:** `LfoOsc.cpp`, `LfoOsc.h`, `LfoCurveTables.h`, `Common.h`,
  `Env.cpp`, `SynthState.cpp`, `FMDisplayEditor.cpp`.
- **Commits:** `e762669`, `94050a9`, `265dbb1`

#### c. Matrix and MIDI/NRPN refactor

- **What:** new matrix destinations (`note+`, phase/warp, etc.), a MIDI CC/NRPN
  mapping refactor, NRPN 300 -> 324 for envelope curves, version handling, full
  parameter exposure and a patch-dump fix.
- **Files:** `Common.h`, `Matrix.h`, `MidiDecoder.cpp`, `MidiDecoder.h`,
  `Synth.cpp`, `SynthState.cpp`, `PreenFMFileType.cpp`.
- **Commits:** `54ef23e`, `265dbb1`, `ab33c52`, `75087a8`, `8a12b64`

#### d. DX7 import and LFO retrigger

- **What:** improved DX7 SysEx import (operator phase defaults, FM decimation,
  LFO handling) and LFO retrigger modes contributed in `e762669`.
- **Files:** `Dx7Params.cpp`, `DX7SysexFile.cpp`, `MidiDecoder.cpp`,
  `Common.h`, `LfoOsc.cpp`.
- **Commits:** `e762669`

### 5. Arpeggiator improvements

- **What:** `Oct+`, ratchet, strumming, accent masks, Euclidean pattern
  variations, `Arp+`, `ArpVl`, `ArpGt`, `ArpDv`, and preset caching for accent
  masks/ties; implemented in `Timbre` and exposed in `Common.h` /
  `FMDisplayEditor.cpp`.
- **Files:** `Timbre.cpp`, `Timbre.h`, `Common.h`, `FMDisplayEditor.cpp`,
  `SynthState.cpp`, `Presets.cpp`.
- **Commits:** `51fdaaa`, `1892537`, `38ea75b`, `a9b0852`.

### 6. Step sequencer improvements

- **Core:** reworked `LfoStepSeq` with configurable loop length, `Alt2`/`Alt3`
  step actions, LFO and ratchet modes, and per-step ratchet.
- **Step data/MIDI:** step value and mode are split (low nibble = value, high
  nibble = mode); new NRPNs `MSB 5/6` expose per-step mode (`Norm`, `Gld1..3`,
  `Mute`, `Pass`, `Reset`, `Lfo1..3`, `1/32..1/64`, `Alt2`, `Alt3`) and loop
  length (`LSB 16`); patch dump sends them. Old step-gate CCs `60/61` were
  removed.
- **Helpers:** `Synth::setNewStepModeFromMidi()`,
  `Synth::setNewStepSeqLoopLengthFromMidi()`,
  `Timbre::getSeqStepMode()/setSeqStepMode()`.
- **`note+` matrix destination:** `MTX_DEST_NOTE_ADD` shifts the played note and
  applies a scale-aware frequency ratio in `Voice` (`lastNoteAdd`,
  `noteAddScaleRatio`).
- **MIDI/USB:** hardened USB MIDI output buffering for the longer NRPN dumps.
- **Not this:** MIDI clock/start/continue/stop and song position are unchanged
  base behavior, not part of the step-sequencer changes.
- **Files:** `LfoStepSeq.cpp`, `LfoStepSeq.h`, `Common.h`, `MidiDecoder.cpp`,
  `MidiDecoder.h`, `Timbre.cpp`, `Timbre.h`, `Voice.cpp`, `Voice.h`,
  `SynthState.cpp`, `FMDisplayEditor.cpp`.
- **Commits:** `8a12b64`, `e49dac5`, `9b016e4`, `0519bbb`.

### 7. Voice allocation

- **What:** new `midipal/voice_allocator.h` with a note stack,
  `restoreHeldNotes`, bass-protect voice stealing and per-timbre priority
  modes.
- **Files:** `midipal/voice_allocator.h`, `midipal/note_stack.cpp`,
  `midipal/note_stack.h`, `Timbre.cpp`, `Timbre.h`, `Voice.cpp`, `Synth.cpp`,
  `Common.h`.
- **Commits:** `31a1419`, `b29febe`, `79e08d3`, `4f233a8`.

### 8. Build, memory, cleanup and release

#### a. Headless build script

- **What:** new `scripts/build_cli.sh` drives STM32CubeIDE headless builds of
  `lib`, `firmware` and `bootloader`, with `CLEAN_BUILD`, `BUILD_CONFIG`,
  `CUBEIDE_BIN` and `RELEASE_DIR` options.
- **Files:** `scripts/build_cli.sh`.
- **Commits:** `e762669`

#### b. Release bundle and versions

- **What:** release bundle `preenfm3-v1.06-bl1.09` with firmware/bootloader
  binaries and `SHA256SUMS.txt`; firmware `v1.06g`, bootloader `1.09`.
- **Files:** `release/preenfm3-v1.06-bl1.09/*`, `firmware/Inc/version.h`,
  `bootloader/Inc/version.h`.
- **Commits:** `e762669`, `1f2a397`, `e0906cb`

#### c. Memory and CPU

- **What:** `426844e` made lookup tables const (`algoInformation`,
  `algoOpInformation`, `panTable`) so they live in flash instead of DTCMRAM;
  `fd520c1` optimized FxBus/Osc/Voice CPU paths and fixed bugs; `8f85b87`
  reduced compile size by moving `Osc` block functions to `Osc.cpp`.
- **Files:** `Common.cpp`, `Common.h`, `FxBus.h`, `Timbre.cpp`, `Timbre.h`,
  `Voice.cpp`, `Osc.h`, `Osc.cpp`.
- **Commits:** `426844e`, `fd520c1`, `8f85b87`, `d120c09`

#### d. IDE/project cleanup

- **What:** added `.project` files and CubeIDE preferences, removed
  per-subproject `language.settings.xml`, and cleaned notices and README.
- **Files:** `.project`, `.settings/*`, `README.md`, `firmware/README.txt`,
  `doc/README.md`.
- **Commits:** `e762669`, `eac0ecd`, `a6a6695`, `fcfe40c`, `d414981`, `16d2968`

### 9. Other notable changes

#### a. Coherent preset randomizer

- **What:** `SynthState::randomizePreset()` uses coupled parameter groups
  (`Oper`, `EnvT`, `IM`, `Modl`), mode-specific effect/LFO pools, carrier
  normalization and STM32 `HAL_RNG`; randomizer UI values are exposed in
  `fullState.randomizer`.
- **Files:** `SynthState.cpp`, `SynthState.h`, `FMDisplayEditor.cpp`,
  `Common.h`.
- **Commits:** `e762669`

#### b. Patch dump and MIDI robustness

- **What:** fixed patch dump via NRPN, version handling and full parameter
  exposure; additional NRPN mappings for the step sequencer.
- **Files:** `MidiDecoder.cpp`, `MidiDecoder.h`, `Synth.cpp`,
  `PreenFMFileType.cpp`.
- **Commits:** `75087a8`, `54ef23e`, `265dbb1`, `ab33c52`

#### c. Documentation and cleanup

- **What:** README update, notice cleaning and code formatting/cleanup.
- **Files:** `README.md`, `firmware/README.txt`, `doc/README.md`, various
  sources.
- **Commits:** `a6a6695`, `eac0ecd`, `fcfe40c`, `d414981`, `16d2968`

## Merge / porting notes

- The branch uses the pre-CMake STM32CubeIDE layout (`firmware/Src/...`, `lib/`,
  `bootloader/`); the current tree uses CMake and `firmware/src/app/...` plus
  `preen_lib/`. Expect path and file-layout conflicts.
- Biggest conflict hotspots: `Voice.cpp` (~7.2k changed lines), `Timbre.cpp`
  (~3k), `Osc.cpp/.h`, `FxBus.cpp`, `SynthState.cpp`, `MidiDecoder.cpp/.h`,
  `FMDisplayEditor.cpp`, `Common.h`.
- New modules: `VoiceFx.cpp` (ported), `TimbreFx.cpp` (ported),
  `LfoCurveTables.h`, `midipal/voice_allocator.h`.
- Release artifacts (`release/*.zip`, `.bin`), IDE files (`.project`,
  `.settings/`) and the deleted `.settings/language.settings.xml` probably
  should not be merged.
- The branch is based on the old master (`65cb963`), not on the current CMake
  tree; a direct merge is not possible without structural porting.

## Preset storage compatibility

The branch keeps the on-disk patch size at 1024 bytes (`ALIGNED_PATCH_SIZE`) and
the version trailer at `ALIGNED_PATCH_SIZE - 5`. New fields are appended to
`FlashSynthParams` before the trailer, with a `static_assert` that the struct
fits, and `PRESET_CURRENT_VERSION` remains `PRESET_VERSION1`.

Compatibility measures:

- `convertFlashToParams()` handles legacy `pfm3Version` values (`< 1.7`, `< 1.5`,
  `< 1.4`) and defaults the new operator phase/warp, decimation and LFO sync
  modes.
- Old V1 patches load through `convertFlashToParams()`; old V2 patches are
  migrated by copying the legacy prefix and moving `presetName` to its new
  position.
- Mixer slots remain 1024 bytes (`FULL_MIXER_SIZE` unchanged), and each
  per-timbre `FlashSynthParams` is read/written inside those slots.

Caveats:

- The in-memory `OneSynthParams` layout changed (new fields before
  `presetName`), so external tools that cast or read this struct directly (the
  web editor in particular) must be updated.
- Old firmware can still load new patches because the old fields keep their
  offsets and the new tail is ignored, but it will not understand the new
  phase/warp, decimation, LFO sync modes or voice priority.
- The V2 migration assumes the legacy `presetName` was immediately before the
  new operator phase rows, which matches the branch's layout.

## Merged changes

Changes from pvig's branch that have been ported into this repository.

| pvig commit | Change | Status |
|---|---|---|
| `eb4afe9` | Sub-sample-accurate windowed oscillator sync: `Osc::hardSyncToMaster()` + `Osc::getPhaseIncrement()` used in ALG29-32 (`Preenfm3Engine.cpp` / `Osc.h`) | PORTED |
| `8137fae`, `aa0fd12`, `5acc683`, `81ce40e`, `43062f5`, `cad3af0`, `d120c09` | Compressor presets (`Off/Glue/Punch/Tube/Limit/Drive/Fuzz`) + matched `FxBus` saturation (`WARM/PUNCH/TUBE/DRIVE/OVERDRIVE/FUZZ`); `SimpleComp` knee/makeup + `compressBlock()` | PORTED |
| `b2e341b` | `VoiceFx.cpp` split: filter helpers, `Voice::fxAfterBlock()`, `recomputeBPValues()` and `setNewEffectParam()` moved out of `Voice.cpp` | PORTED |
| `2a2916b`, `875db52`, `b53ebed`, `2837e43`, `a63032c` | `TimbreFx.cpp` split and final FX2 dispatch, including `FILTER2_STEREO_BP` | PORTED |
| `cd19c34`, `2837e43` | Hermite delay interpolation in the per-timbre FX path (`delayInterpolationHermite` / `hermiteInterpolation`) | PORTED |
| `f38af5d`, `805d6f7`, `d120c09` | ADAA on `LPWS`, `SAT`, `SIGMOID`, `FOLD`, `WRAP`, `LPXOR`, `LPXOR2`, `ORYX3`, `18DB`, `LADDER2`, `TEEBEE`, `SVFLH` + `FxBus` `WARM`/`FUZZ` | PORTED |
| `d120c09` | `sqrtf` optimization: `hwSqrtf()` (`vsqrt.f32`) in FxBus saturation, `fastInvSqrt()`, and `vsqrt.f32` in `tanh4ADAA1_F()` | PORTED |

## Chronological commit list

| # | Date | Commit | Summary |
|---|------|--------|---------|
| 1 | 2025-04-27 | `a63032c` | bp paraphonic |
| 2 | 2025-04-27 | `238c54c` | filters dc remove |
| 3 | 2025-09-21 | `d414981` | cleaning |
| 4 | 2025-10-03 | `2a2916b` | Timbre_fx split |
| 5 | 2025-10-04 | `2837e43` | hermite tuning |
| 6 | 2026-03-22 | `265dbb1` | version handling in nrpn & expose all params |
| 7 | 2026-03-24 | `ab33c52` | nrpn 300->324 pour env curves |
| 8 | 2026-03-26 | `f3f3da3` | fx2 cleanup & stBp tweak |
| 9 | 2026-03-27 | `d0bffc6` | stBp rework |
| 10 | 2026-03-27 | `9664607` | bode small tweak |
| 11 | 2026-04-03 | `081e11b` | stBp tweak |
| 12 | 2026-04-06 | `748cc53` | smoother StBp |
| 13 | 2026-04-06 | `a9daf4b` | DIFFUSER  blurring, STEREO_BP 2p + 2p harm |
| 14 | 2026-04-06 | `4f45b83` | FILTER2_FLANGE more analog like |
| 15 | 2026-04-09 | `41b93b9` | cross feed for more dimension |
| 16 | 2026-04-09 | `072f8d2` | Bode fx leveling |
| 17 | 2026-04-11 | `71cd75f` | bode level, grains mod smoother |
| 18 | 2026-04-11 | `eac0ecd` | notice cleaning |
| 19 | 2026-06-05 | `e762669` | DX7 import improved, Osc Start Phase, FM Osc descimation, LFO Shapes, LFO Retrigger modes 1..8 (#16) |
| 20 | 2026-06-14 | `344f8fb` | phase int & warp CC |
| 21 | 2026-11-07 | `fb0b2ee` | FILTER2_PINGPONG anti alias |
| 22 | 2026-11-07 | `31a1419` | added notestack for voices handling |
| 23 | 2026-11-07 | `16d2968` | code cleaning |
| 24 | 2026-11-08 | `b29febe` | improved restoreHeldNotes |
| 25 | 2026-11-08 | `fd520c1` | optimisations cpu & bugs fix |
| 26 | 2026-11-08 | `f38af5d` | ADAA, waveMipmapTables, oversamplig x2 in HQ mode |
| 27 | 2026-11-08 | `875db52` | timbreFX optim |
| 28 | 2026-11-09 | `805d6f7` | ADAA sur FILTER_WRAP |
| 29 | 2026-11-09 | `e18d87d` | s^2 -> s_q4 (mipmaps) |
| 30 | 2026-11-11 | `8137fae` | useful compressor presets |
| 31 | 2026-11-11 | `79e08d3` | Bass-protect voice stealing |
| 32 | 2026-11-11 | `54ef23e` | refacto  mapping CC & NRPN |
| 33 | 2026-11-11 | `aa0fd12` | Punch preset |
| 34 | 2026-11-11 | `5acc683` | glue preset |
| 35 | 2026-11-11 | `426844e` | liberation DTCMRAM |
| 36 | 2026-11-11 | `81ce40e` | Drive Preset |
| 37 | 2026-11-11 | `75087a8` | patch dump fix |
| 38 | 2026-11-12 | `4f233a8` | voice priority added in timbre |
| 39 | 2026-11-12 | `94050a9` | modulation env : Attack & Release Curves |
| 40 | 2026-11-12 | `eb4afe9` | windowed sync : lower aliasing |
| 41 | 2026-11-12 | `2f31082` | refactor: enhance phase warping and interpolation in oscillator |
| 42 | 2026-11-12 | `cd19c34` | Osc : HQ 2x, TimbreFx : delayInterpolationHermite |
| 43 | 2026-11-12 | `9571d99` | compilation fix |
| 44 | 2026-11-12 | `1f2a397` | PFM3_PATCH_VERSION 1.6f |
| 45 | 2026-11-12 | `ede9e93` | HQ legacy -> FULL, HQx2 |
| 46 | 2026-11-12 | `b53ebed` | timbreFx cleanup |
| 47 | 2026-11-13 | `8a12b64` | stepseq rework, matrix dest note+ |
| 48 | 2026-11-13 | `0519bbb` | Add LFO and ratchet modes to step sequencer |
| 49 | 2026-11-13 | `a837174` | Refine saturation effects in FxBus: update fuzz and overdrive algorithms for improved sound quality |
| 50 | 2026-11-13 | `86b5276` | Refactor fuzz saturation algorithm for improved sound quality and clarity |
| 51 | 2026-11-14 | `e49dac5` | Enhance step sequencer functionality: add loop length parameter and update related MIDI handling |
| 52 | 2026-11-14 | `9b016e4` | seqtep actions : Alt2 Alt3 |
| 53 | 2026-11-14 | `51fdaaa` | "Arp+ ", "ArpVl", "ArpGt", "ArpDv" |
| 54 | 2026-11-14 | `1892537` | Enhance arpeggiator functionality with accent masks and ratchet effects, euclidian pattern for variations of default presets |
| 55 | 2026-11-14 | `38ea75b` | Enhance arpeggiator functionality: add oct+ and ratchet modes, implement strumming feature, and improve MIDI handling |
| 56 | 2026-11-15 | `9b9fa1d` | Refine warm saturation effect: update description and adjust processing for subtle, transparent soft-knee warmth |
| 57 | 2026-11-15 | `a9b0852` | Optimize arpeggiator performance: implement preset caching for accent masks and ties |
| 58 | 2026-11-15 | `43062f5` | Refine compressor settings and enhance saturation effects: adjust parameters for improved audio processing and dynamic response |
| 59 | 2026-11-15 | `b2e341b` | voice fx refactoring |
| 60 | 2026-11-15 | `17c2676` | FILTER_ORYX rework |
| 61 | 2026-11-15 | `05d5e92` | Enhance TB-303 filter emulation: implement high-accuracy Padé scaling for pole mapping and refine accent circuit dynamics |
| 62 | 2026-11-15 | `fcfe40c` | code formatting |
| 63 | 2026-11-15 | `d3ec718` | filters : modulation aliasing removed |
| 64 | 2026-11-15 | `9d8b310` | Refactor Ap4D : True Dual Nested Allpass Phaser |
| 65 | 2026-11-15 | `a6a6695` | readme |
| 66 | 2026-11-15 | `8f85b87` | Osc structure, reduce compilation size |
| 67 | 2026-11-15 | `cad3af0` | Limit preset |
| 68 | 2026-11-15 | `e0906cb` | PFM3_PATCH_VERSION 1.7f |
| 69 | 2026-11-15 | `d120c09` | sqrtf optimization, adaa on Glue & Fuzz |
