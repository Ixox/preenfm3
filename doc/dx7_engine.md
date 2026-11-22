# DX7 engine — design proposal

> Historical design proposal. The firmware engine is now named **DX7**;
> the vendored Dexed library remains `third_party/msfa/` with its `msfa_*` API.

Status: proposal, not implemented.

This document describes how to add **MSFA** (the Dexed FM engine) as an
alternate per-timbre synthesis engine alongside the existing PFM3 engine. It
is intentionally technical: file names, methods and line numbers refer to the
current tree and are intended as integration anchors, not as stable API.

## Goals

- Keep the current PFM3 engine as the default.
- Allow each of the six timbres to use either:
  - `PFM3` — the existing engine (`Voice`, `Osc`, `Env`, `Matrix`, LFOs).
  - `MSFA` — the Dexed/MSFA DX7 engine (`Dx7Note`, `FmCore`).
- Expose the engine choice from the existing **Voices** mixer button, as a
  third state, without removing the existing “number of voices” and
  “compressor” states.
- Load `Preset > DX7` sysex patches into the MSFA engine automatically.
- Provide an MSFA-specific editor in `ScreenEditor`, with navigation that
  follows the existing six-button / six-encoder model.

## Non-goals (for the first iteration)

- Replacing or rewriting the PFM3 engine.
- Rebuilding all PFM3 UI pages for MSFA.
- Full DX7-compatible sysex *export*.
- Bit-exact Dexed compatibility on the first pass.

---

## 1. Current architecture

### 1.1 Top-level audio path

`Synth` owns six `Timbre` objects and twenty global `Voice` objects:

- `firmware/src/app/synth/Synth.h:158-159`
- `firmware/src/app/synth/Synth.cpp:46-84` (`Synth::init`)

`Synth::buildNewSampleBlock()` (`Synth.cpp:253-473`) is called from the SAI
DMA callbacks (`firmware/src/app/preenfm3.cpp:328-350`). For each active
timbre it calls:

- `timbres_[t].updateArpegiatorInternalClock()` — `Synth.cpp:279`
- `timbres_[t].glide()` — `Synth.cpp:281`
- `timbres_[t].prepareMatrixForNewBlock()` — `Synth.cpp:283`
- `timbres_[t].voicesNextBlock()` — `Synth.cpp:285`
- `timbres_[t].voicesToTimbre(...)` — `Synth.cpp:325`
- `timbres_[t].gateFx()` and `fxAfterBlock()` — `Synth.cpp:326-327`

The per-timbre stereo buffer is `Timbre::sampleBlock_`
(`firmware/src/app/synth/Timbre.h:281`).

### 1.2 Timbre is the natural engine boundary

`Timbre` already owns:

- the parameter block `Pfm3Preset params_` — `Timbre.h:279`;
- six PFM3 `Osc` and six `Env` objects — `Timbre.h:295-308`;
- the global voice pointers `Voice *voices_[MAX_NUMBER_OF_VOICES]` —
  `Timbre.h:290`;
- arpeggiator, per-timbre FX2, MPE/hold state — `Timbre.h:189-347`;
- the render buffer `sampleBlock_` — `Timbre.h:281`.

It also already handles note allocation and voice stealing:
`Timbre::preenNoteOn()` (`Timbre.cpp:360-482`), `Timbre::preenNoteOff()`
(`Timbre.cpp:492-545`), and `Timbre::voicesNextBlock()` (`Timbre.cpp:645-730`).

This makes `Timbre` the right place to select an engine, while `Voice` is the
right place to store per-note engine state.

### 1.3 Where the PFM3 engine is hard-coded

The main coupling points are:

- `Voice::nextBlock()` — `firmware/src/app/synth/Voice.cpp:656-4041`:
  a 3,300-line switch over the 32 PFM3 FM algorithms.
- `Voice::fxAfterBlock()` — `Voice.cpp:4117-7832`: per-voice PFM3 FX1.
- `Timbre::fxAfterBlock()` — `Timbre.cpp:732-2460`: per-timbre PFM3 FX2.
- `Timbre::preenNoteOn()` — `Timbre.cpp:360-482`: PFM3 play modes, unison
  detune/spread.
- `Synth::newParamValue()` — `Synth.cpp:589-682`: PFM3 `ROW_*` dispatch.
- The editor and MIDI NRPN path write directly into the flat
  `Pfm3Preset` array:
  - `ScreenEditor::encoderTurnedPfm2()` — `ScreenEditor.cpp:3843-3998`
  - `ScreenEditor::displayParamValue()` — `ScreenEditor.cpp:3227-3267`
  - `MidiDecoder::decodeNrpn()` — `MidiDecoder.cpp:914-936`

The PFM3 parameter model is row-oriented: `row * NUMBER_OF_ENCODERS_PFM2 +
encoder` indexes `Pfm3Preset`. See `Common.h:149-225` for the `ROW_*`
enum and `Common.h:544-608` for `Pfm3Preset`.

---

## 2. Proposed engine model

### 2.1 Engine identity and storage

Add a small enum in `firmware/src/app/synth/Common.h`:

```cpp
enum SynthEngineType {
    SYNTH_ENGINE_PFM3 = 0,
    SYNTH_ENGINE_MSFA,
    SYNTH_ENGINE_COUNT
};
```

Store it **per timbre** in `MixerInstrumentState`:

- `firmware/src/app/synth/MixerState.h:42-58`

Add `uint8_t engine;` and bump the mixer bank format:

- `MIXER_BANK_VERSION` and `MIXER_BANK_CURRENT_VERSION` — `MixerState.h:24-40`
- `MixerState::getFullState()` — `MixerState.cpp:46-71`
- `MixerState::getFullDefaultState()` — `MixerState.cpp:132-158`
- `MixerState::restoreFullState()` — `MixerState.cpp:192-217`
- add `restoreFullStateVersion7()` next to `restoreFullStateVersion6()`
  (`MixerState.cpp:479-529`)

The engine is a mixer-level per-timbre choice, so saving/loading a mixer bank
preserves it. The engine is **also** stored in each preset: loading a PFM3
preset into an MSFA timbre switches that timbre to PFM3, and loading an MSFA
preset into a PFM3 timbre switches it to MSFA. See
[Persistence](#6-persistence-and-preset-format).

### 2.2 Engine interface options

Three implementation strategies are possible.

| Option | Description | Pros | Cons |
|--------|-------------|------|------|
| A. Branch inside `Timbre` / `Voice` | `if (engine_ == SYNTH_ENGINE_MSFA)` at note/render entry points | Smallest diff, fastest to prototype | Branches spread through hot code; hard to test |
| B. `SynthEngine` interface per Timbre | `Timbre` owns a `Pfm3Engine` or `MsfaEngine` object implementing `noteOn`, `noteOff`, `renderBlock` | Clean boundary, testable, future engines easy | Requires moving PFM3 code behind an adapter |
| C. Separate engine voice arrays | `Timbre` owns an `MsfaVoice` array separate from global `Voice` objects | Keeps PFM3 path untouched | Duplicates voice allocation, more RAM, two voice pools |

**Recommendation: start with A, structure it like B.** Keep the existing
`Timbre`/`Voice` objects, but introduce explicit engine entry points so the
branch is in one place:

```cpp
// Timbre
void setEngine(SynthEngineType engine);
void noteOn(char note, char velocity);       // existing, branches internally
void noteOff(char note);                     // existing
uint8_t voicesNextBlock();                   // existing, branches internally

// New per-engine methods
void pfm3VoicesNextBlock();
void msfaVoicesNextBlock();
void msfaNoteOn(char note, char velocity);
void msfaNoteOff(char note);
void msfaAfterNewParamsLoad();
```

Then a later refactor can replace those methods with a `SynthEngine` object
without changing the rest of `Synth`.

### 2.3 Voice state

The global voice pool stays shared by all six timbres. Each timbre receives a
subset of global voice indices through `Timbre::setVoiceNumber()`
(`Timbre.cpp:302-307`) and `Synth::rebuidVoiceAllTimbre()` (`Synth.cpp:777-791`).
Keep the existing 20 global voices and allow each timbre to select MSFA
independently; do not create a separate MSFA voice pool per timbre.

Yes: extract the PFM3-only per-voice DSP state into a `Pfm3VoiceState` so both
engines have a symmetric state object. Keep the engine-independent note/voice
state in `Voice`:

- shared: `released`, `playing`, `note`, `midiVelocity`, `noteFrequency`,
  `velocity`, glide/pending-note fields, `currentTimbre`, `sampleBlock`,
  `mixerGain`;
- PFM3-only: six `EnvData`, six `OscState`, `Matrix`, LFO objects, modulation
  index caches, mix/pan caches, feedback state, envelope value memories and
  PFM3 FX1 state;
- MSFA-only: `Dx7Note` state, MSFA LFO/pitch-envelope state, 64-sample render
  buffer, controller state.

```cpp
struct Pfm3VoiceState {
    EnvData envState[6];
    OscState oscState[6];
    Matrix matrix;
    LfoOsc lfoOsc[NUMBER_OF_LFO_OSC];
    LfoEnv lfoEnv[NUMBER_OF_LFO_ENV];
    LfoEnv2 lfoEnv2[NUMBER_OF_LFO_ENV2];
    LfoStepSeq lfoStepSeq[NUMBER_OF_LFO_STEP];
    // PFM3 modulation/mix/pan/FX caches
};

struct MsfaVoiceState {
    // Dx7Note / FmOpParams / Env / PitchEnv / Lfo state
    // 64-sample int32_t buffer
};

class Voice {
    // shared note/glide/currentTimbre/sampleBlock state
    // ...

    EngineVoiceKind engineKind_;
    // Illustrative only: see the lifetime note below before using a raw union.
    union EngineVoiceState {
        Pfm3VoiceState pfm3;
        MsfaVoiceState msfa;
        EngineVoiceState() {}
        ~EngineVoiceState() {}
    } engineState_;
};
```

Important: `Pfm3VoiceState` as written is **not trivially constructible**:
`Matrix` has a constructor/destructor, and the LFO classes have virtual
destructors. A plain C++ union containing those members is not safe. Use one
of these designs:

1. **Preferred:** make `EngineVoiceState` a tagged union of raw aligned
   storage and use placement `new` / explicit destructor calls when a voice
   switches engine (`Voice::setEngine()`), exactly like a variant. This also
   works if the vendored `Dx7Note` has non-trivial members.
2. Keep `Matrix` and the LFO objects outside the union as `Voice` members and
   put only trivially-destructible state in `Pfm3VoiceState`.
3. Keep both engine states as ordinary members if RAM allows. This is the
   simplest and safest, but uses more RAM.

Do **not** allocate a separate 20-voice MSFA pool per timbre: that would
multiply RAM by six and duplicate the existing allocation logic.

---

## 3. MSFA integration

### 3.1 Vendoring and build

Vendor a self-contained subset of MSFA under `third_party/msfa/`.

Minimum useful source set from Dexed:

- `fm_core.cc` / `fm_core.h` — 32 algorithms and `FmCore::render`
- `fm_op_kernel.cc` / `fm_op_kernel.h`
- `exp2.cc` / `exp2.h`
- `sin.cc` / `sin.h`
- `freqlut.cc` / `freqlut.h`
- `env.cc` / `env.h`
- `lfo.cc` / `lfo.h`
- `pitchenv.cc` / `pitchenv.h`
- `porta.cc` / `porta.h` (only if portamento is used)
- `dx7note.cc` / `dx7note.h`
- `controllers.h`
- `aligned_buf.h`, `synth.h`

Use Dexed’s `FmCore` for the first implementation. Dexed also provides the
more accurate `EngineMkI` and `EngineOpl` cores; keep `FmCore` behind a small
factory/typedef and add comments in the code explaining where `EngineMkI` and
`EngineOpl` would plug in if more accuracy is needed later.

Dexed’s current `msfa/dx7note.*` also depends on MTS/tuning and the JUCE
debug logger. Prefer one of these:

1. vendor the older Google MSFA sources
   (`google/music-synthesizer-for-android`) which are more self-contained; or
2. vendor Dexed’s files and provide small stubs for `MTSClient`,
   `TuningState`, `DXLOG`, and `TRACE`.

Add a CMake target in `third_party/CMakeLists.txt`, modeled on the existing
`hexter` target at `third_party/CMakeLists.txt:165-174`:

```cmake
add_library(msfa INTERFACE)
target_include_directories(msfa INTERFACE ${TP}/msfa)
target_sources(msfa INTERFACE
    ${TP}/msfa/fm_core.cc
    ${TP}/msfa/fm_op_kernel.cc
    # ...
)
```

Then add `msfa` to `target_link_libraries(preenfm3_firmware PRIVATE ...)` in
`firmware/CMakeLists.txt:72-81`.

Licensing: MSFA is the engine from Google's music-synthesizer-for-android
project and remains Apache-2.0, including the Dexed copy under
`source/msfa` (the files keep their Google/Apache-2.0 headers alongside later
Dexed modifications). Dexed's own plugin/application and patch/Cartridge code
is GPL-3.0. This repository is GPL-3.0. Keep all upstream headers and document
the origin. Apache-2.0 is compatible with GPL-3.0, but the combined firmware
binary is distributed under GPL-3.0.

### 3.2 One-time initialization

MSFA uses static lookup tables initialized at a given sample rate. The
preenFM3 audio sample rate is `PREENFM_FREQUENCY` (`Common.h:39`, 47916 Hz).

Initialize once during firmware startup, before the SAI DMA callbacks start
(around `firmware/src/app/preenfm3.cpp` setup):

```cpp
Freqlut::init(PREENFM_FREQUENCY);
Lfo::init(PREENFM_FREQUENCY);
PitchEnv::init(PREENFM_FREQUENCY);
Env::init_sr(PREENFM_FREQUENCY);
Porta::init_sr(PREENFM_FREQUENCY);   // if portamento is used
Sin::init();                         // if not already initialized
```

Do **not** reinitialize these from the audio interrupt or from a patch load.

### 3.3 Patch format

DX7 patches come in two forms:

- **Packed**: 128 bytes, the on-disk sysex voice format.
- **Unpacked**: 155 bytes, the format expected by MSFA `Dx7Note`; Dexed adds a
  156th operator-switch byte.

Current preenFM3 DX7 flow:

- `DX7SysexFile::dx7LoadPatch()` — `firmware/src/app/storage/DX7SysexFile.cpp:38-45`
  reads 128 bytes from `patchNumber * 128 + 6` into the global
  `dx7PackedPatch` buffer (`DX7SysexFile.cpp:21`).
- `Hexter::loadHexterPatch()` — `third_party/hexter/Hexter.cpp:181-184`
  calls `patchUnpack()` then `voiceSetData()`.
- `Hexter::patchUnpack()` — `Hexter.cpp:186-222` produces the 155-byte
  unpacked layout.
- `Hexter::voiceSetData()` — `Hexter.cpp:310-955` converts the DX7 patch into
  PFM3 `Pfm3Preset`.

For MSFA:

1. Copy the 128 packed bytes out of the global buffer into a timbre-owned
   buffer before another load can overwrite it.
2. Unpack to 155 bytes using either:
   - the existing `Hexter::patchUnpack()` (make it public or extract it), or
   - a port of Dexed’s `Cartridge::unpackProgram()` /
     `Program::pushToParameters()`.
3. Add the operator-switch byte `0x3F` (all operators on) for Dexed’s
   `Dx7Note::init(const uint8_t patch[156], ...)`.

Important: `DX7SysexFile::dx7LoadPatch()` returns `nullptr` on read failure;
check it before unpacking.

### 3.4 Rendering block size

MSFA renders in fixed blocks of `N = 64` samples (`msfa/synth.h`, `LG_N = 6`).
preenFM3 processes `BLOCK_SIZE = 32` samples (`Common.h:29`).

Recommended approach: keep the MSFA block size at 64 and buffer inside
`MsfaVoiceState`:

- On the first preenFM block, call `Dx7Note::compute()` into a 64-sample
  `int32_t` buffer.
- Copy the first 32 samples into `Voice::sampleBlock`.
- On the next preenFM block, copy the remaining 32 samples.
- Refill the 64-sample buffer again on the following block.

Do **not** change `LG_N` to 5 unless the fixed-point tables are validated;
that changes all MSFA scaling and is a much larger risk.

### 3.5 Sample conversion

MSFA works in Q24 fixed point; the PFM3 render path is float. Dexed’s
`PluginProcessor` converts with:

```cpp
val = val >> 4;
clip to 16-bit range;
val = val >> 9;
float out = val / 0x8000f;
```

Port that conversion into `MsfaVoiceState` and write stereo float samples into
the existing `Voice::sampleBlock[BLOCK_SIZE * 2]`.

`Dx7Note::compute()` **adds** into the output buffer. Clear the buffer before
each MSFA block.

### 3.6 Controllers and modulation

MSFA has its own controller model (`msfa/controllers.h`):

- pitch bend, pitch range up/down
- mod wheel, aftertouch, breath, foot
- master tune
- portamento enable/glide
- MPE settings
- `Controllers::refresh()` and `Controllers::core`

Provide one `Controllers` instance per Timbre (or per MSFA engine instance)
and fill it from MIDI events already handled by `MidiDecoder` / `Synth`:

- `Synth::noteOn` / `noteOff` — `Synth.cpp:86-99`
- MIDI clock — `Synth.cpp:113-153`
- hold pedal — `Synth.cpp:155-157`
- MPE note handling — `Timbre::noteOnMPE()` (`Timbre.cpp:329-344`)

For the first iteration, map at least pitch bend, mod wheel, aftertouch,
breath and foot to `Controllers`. Leave PFM3 matrix/LFO out of the MSFA path.

### 3.7 LFO and pitch envelope

MSFA has its own DX7 LFO and pitch envelope:

- `Lfo::reset(patch + 137)` — speed/delay/key-sync/wave
- `lfo.getsample()` / `lfo.getdelay()`
- `PitchEnv::set(...)` / `pitchenv.getsample()`

These are **not** the PFM3 `LfoOsc`, `LfoEnv` or `Matrix` objects. For MSFA
timbres, bypass `Voice::prepareMatrixForNewBlock()` and
`Timbre::prepareMatrixForNewBlock()`; use the MSFA LFO/pitch EG instead. Do
not expose the PFM3 `Matrix` or `Modul` editor pages for MSFA timbres.

If PFM3 matrix modulation of MSFA parameters is desired later, define a small
mapping layer that translates selected matrix destinations into MSFA
controller/parameter changes. Do not try to drive MSFA’s internal LFO from
the PFM3 matrix in the first version.

### 3.8 Effects and mixer path

Keep the existing per-timbre and global output path unchanged:

- `Timbre::gateFx()` — `Timbre.cpp:2531-2559`
- `Timbre::fxAfterBlock()` — `Timbre.cpp:732-2460`
- `Timbre::voicesToTimbre()` — `Timbre.cpp:2490-2529`
- `Synth::buildNewSampleBlock()` — `Synth.cpp:253-473`

MSFA should render into the same per-voice/per-timbre buffers. That means
MSFA timbres still get the existing PFM3 FX1/FX2, pan, volume, compressor and
reverb. Keep the `Arp/Fx` editor page available for MSFA timbres so the
arpeggiator and effects can still be used. The arpeggiator is host-level
(`Timbre::noteOn()` / `Timbre::resetArpeggiator()`) and is independent of the
engine; FX1/FX2 are also after the engine and should remain enabled.

### 3.9 Memory and CPU

Current RAM use from the default build is already tight:

```
DTCMRAM: 89.86%
RAM_D1:  70.17%
RAM_D2:  93.07%
RAM_D2B: 55.01%
```

`Dx7Note` per voice adds six envelopes, six `FmOpParams`, a pitch envelope,
feedback buffers and controller state. With twenty global voices this is
manageable, but must be measured. Avoid a separate MSFA voice array per
timbre. The existing 20 global voices remain the allocation unit.

CPU must be profiled under real load, not only at idle. The reported ~6.4% is
the idle load; a fully loaded PFM3 patch with 16 voices, FX and arpeggiator can
reach roughly 75%. MSFA must be benchmarked with the same worst-case setup,
and the result may require a voice cap, disabling some FX, or deferring MSFA
support on lower-headroom configurations.

### 3.10 Tuning

MSFA uses its standard DX7 tuning (`createStandardTuning()`, 12-TET, A4 =
440 Hz) for every note. It ignores the per-instrument Scala/MTS table
(`MixerState.instrumentState_[t].scaleFrequencies`). The oscilloscope lower-note
frequency therefore uses `diatonicScaleFrequency[note]` rather than the Scala
table.

---

## 4. UI: mixer engine selection

The user-facing entry point is the existing **Voices** mixer button.

Current state definitions:

- `voiceButtonState` — `firmware/src/app/ui/ScreenMixer.cpp:106-109`
- `compButtonState` — `ScreenMixer.cpp:88-91`
- `voiceButton` with `numberOfStates = 2` — `ScreenMixer.cpp:111-115`
- `mixerMenu` — `ScreenMixer.cpp:255-257`

Add a third state:

```cpp
const struct Pfm3MixerButtonState engineButtonState = {
    "Engine", MIXER_VALUE_ENGINE,
    {0, SYNTH_ENGINE_COUNT - 1, SYNTH_ENGINE_COUNT,
     DISPLAY_TYPE_STRINGS, engineDisplay}
};

const struct Pfm3MixerButton voiceButton = {
    "Voices",
    3,
    {&voiceButtonState, &compButtonState, &engineButtonState}
};
```

Required changes:

1. Add `MIXER_VALUE_ENGINE` to `enum MixerValueType` in `Common.h:710-729`.
2. Add the engine name to `SynthEngineFactory::getEngineDisplayName()`.
   `ScreenMixer` builds `engineDisplay[]` from it.
3. Add the per-timbre field to `MixerInstrumentState`
   (`MixerState.h:42-58`).
4. Map it in `ScreenMixer::getValuePointer()` — `ScreenMixer.cpp:278-419`:

   ```cpp
   case MIXER_VALUE_ENGINE:
       valueP = &synthState_->mixerState.instrumentState_[encoder].engine;
       break;
   ```

5. Add display handling in `ScreenMixer::displayMixerValue()` —
   `ScreenMixer.cpp:432-544`. The existing `DISPLAY_TYPE_STRINGS` path is at
   `ScreenMixer.cpp:526-533`.
6. Add edit handling in `ScreenMixer::encoderTurned()` —
   `ScreenMixer.cpp:831-933`. The generic string/int branch already writes
   to the pointer and calls `propagateNewMixerValue()` at line 904.
7. Handle `MIXER_VALUE_ENGINE` in `Synth::newMixerValue()` —
   `Synth.cpp:689-775`, calling `timbres_[timbre].setEngine(...)` and
   reinitializing voices.
8. Persist the field via `MixerState::getFullState()` /
   `getFullDefaultState()` / `restoreFullStateVersion7()`.
9. No drawing change is needed: `TftDisplay::drawButton()` already supports
   three states (`preen_lib/src/display/TftDisplay.cpp:1116-1177`, case 3 at
   lines 1134-1138).

The button state is global (`fullState.buttonState[BUTTONID_MIXER_VOICE]`), but
the engine value itself is per timbre, matching the existing voices/compressor
behavior.

---

## 5. UI: MSFA editor

### 5.1 Mode handling

The current editor assumes PFM3 pages and PFM3 parameter rows. To avoid
breaking it, add an engine-specific mode:

- `enum SynthEditMode` — `firmware/src/app/ui/Menu.h:43-45`
- add `SYNTH_MODE_EDIT_MSFA`

Then update mode dispatch:

- `SynthState::encoderTurned()` — `SynthState.cpp:342-365`
- `SynthState::buttonPressed()` — `SynthState.cpp:467-542`
- `ScreenCoordinator::refreshAllScreenByStep()` — `ScreenCoordinator.cpp:76-98`
- `ScreenCoordinator::newSynthMode()` — `ScreenCoordinator.cpp:174-192`
- `preenfm3.cpp` oscilloscope branch — `preenfm3.cpp:211-225`

Minimal alternative: keep `SYNTH_MODE_EDIT_PFM3` and have `ScreenEditor`
choose `msfaMainMenu` when the current timbre’s engine is MSFA. The separate
mode is cleaner because many editor helpers hard-code PFM3 page indices
(`isInOperatorPage`, `isInMatrixPage`, `isInModulatorPage` —
`ScreenEditor.cpp:4281-4291`, and `checkOperatorNumber` —
`ScreenEditor.cpp:4351-4365`).

### 5.2 Editor menu structure

The editor is data-driven:

- `EditorOneButtonState` — `ScreenEditor.h:28-31`
- `EditorOneButton` — `ScreenEditor.h:33-39`
- `EditorEditMenu` — `ScreenEditor.h:41-45`
- `EditorMainMenu` — `ScreenEditor.h:47-49`
- `mainMenu` — `ScreenEditor.cpp:2988-2995`
- page/state navigation — `ScreenEditor::buttonPressed()` —
  `ScreenEditor.cpp:4104-4164`
- parameter display — `ScreenEditor::refreshEditorByStep()` —
  `ScreenEditor.cpp:3269-3491`

Add a second menu tree, for example `msfaMainMenu`, and select it by engine:

```cpp
const EditorMainMenu *ScreenEditor::currentMainMenu() const {
    if (synthState_->mixerState.instrumentState_[currentTimbre_].engine
            == SYNTH_ENGINE_MSFA) {
        return &msfaMainMenu;
    }
    return &mainMenu;
}
```

Replace direct `mainMenu.editMenu[...]` uses (see `ScreenEditor.cpp:3036`,
`3228`, `3303`, `3808`, `3828`, `4006`, `4036`, `4122`, `4187`, `4229`,
`4312`, `4329`) with `currentMainMenu()->editMenu[...]`.

### 5.3 MSFA parameter model

Do **not** add MSFA parameters to `Pfm3Preset`. It is a flat PFM3 preset
layout indexed by `row * 4 + encoder`, used by the editor, MIDI NRPN and
preset storage. DX7 has roughly 150 parameters and a different structure.

Instead define an MSFA-specific structure, owned by `Timbre`:

```cpp
struct MsfaOperatorParams {
    uint8_t egRate[4];      // R1..R4, 0..99
    uint8_t egLevel[4];     // L1..L4, 0..99
    uint8_t breakPoint;     // 0..99
    uint8_t leftDepth;      // 0..99
    uint8_t rightDepth;     // 0..99
    uint8_t leftCurve;      // 0..3
    uint8_t rightCurve;     // 0..3
    uint8_t rateScaling;    // 0..7
    uint8_t ampModSens;     // 0..3
    uint8_t keyVelSens;     // 0..7
    uint8_t outputLevel;    // 0..99
    uint8_t oscMode;        // 0 = ratio, 1 = fixed
    uint8_t coarse;         // 0..31
    uint8_t fine;           // 0..99
    uint8_t detune;         // 0..14
    bool enabled;
};

struct MsfaProgram {
    uint8_t algorithm;      // 0..31
    uint8_t feedback;       // 0..7
    uint8_t oscKeySync;
    uint8_t transpose;      // 0..48
    uint8_t lfoRate;        // 0..99
    uint8_t lfoDelay;       // 0..99
    uint8_t lfoPitchModDepth;
    uint8_t lfoAmpModDepth;
    uint8_t lfoKeySync;
    uint8_t lfoWaveform;    // 0..5
    uint8_t lfoPitchModSens; // 0..7
    uint8_t pitchEgRate[4];
    uint8_t pitchEgLevel[4];
    MsfaOperatorParams op[6];
    char name[11];
    uint8_t opSwitch;       // 6-bit operator on/off
};
```

The editor can use the existing `RowEncoder {row, encoder}` abstraction by
treating MSFA rows as indexes into a flat `float` view of `MsfaProgram`, or by
adding an explicit `MsfaParameterId` enum. The former reuses more code; the
latter is clearer and less error-prone. Recommend `MsfaParameterId` plus a
small descriptor table (`msfaParameterRows`) that mirrors
`ParameterRowDisplay` (`SynthState.h:338-347`).

`MsfaProgram` above is the decoded parameter view used by the editor. The
canonical data passed to `Dx7Note::init()` and stored in presets is the
156-byte DX7 program (`uint8_t msfaProgram[156]`), with byte 155 holding the
operator on/off bitmask. Keep one conversion routine between the two forms.

### 5.4 DX7 / MSFA parameter list

Global parameters:

| Parameter | Range | Notes |
|---|---:|---|
| Algorithm | 1–32 | DX7 algorithm |
| Feedback | 0–7 | Operator 6 feedback |
| Oscillator key sync | off/on | DX7 “osc key sync” |
| Transpose | 0–48 | DX7 transpose |
| LFO speed | 0–99 | |
| LFO delay | 0–99 | |
| LFO pitch mod depth (PMD) | 0–99 | |
| LFO amp mod depth (AMD) | 0–99 | |
| LFO key sync | off/on | |
| LFO waveform | 0–5 | triangle, saw down, saw up, square, sine, sample & hold |
| LFO pitch mod sensitivity (PMS) | 0–7 | |
| Pitch EG R1–R4 | 0–99 | 4 rates |
| Pitch EG L1–L4 | 0–99 | 4 levels |
| Patch name | 10 chars | |

Per operator (6 operators):

| Parameter | Range | Notes |
|---|---:|---|
| EG rate R1–R4 | 0–99 | |
| EG level L1–L4 | 0–99 | |
| Keyboard break point | 0–99 | or note name |
| Left depth | 0–99 | |
| Right depth | 0–99 | |
| Left curve | 0–3 | -LIN, -EXP, +EXP, +LIN |
| Right curve | 0–3 | -LIN, -EXP, +EXP, +LIN |
| Rate scaling | 0–7 | |
| Amp mod sensitivity | 0–3 | |
| Key velocity sensitivity | 0–7 | |
| Output level | 0–99 | |
| Oscillator mode | ratio/fixed | |
| Coarse frequency | 0–31 | |
| Fine frequency | 0–99 | |
| Detune | 0–14 | |
| Operator enabled | on/off | Dexed extension; DX7 hardware uses algorithm-dependent carriers |

The operator on/off bitmask is not in the original DX7 sysex; Dexed uses
`patch[155]`. Default it to `0x3F` (all on) when loading a DX7 patch unless
the patch already provides the Dexed extension.

### 5.5 MSFA editor navigation

The MSFA editor is implemented in `EditorMsfa`
(`firmware/src/app/ui/EditorMsfa.{h,cpp}`) and owned by
`ScreenEditor`. It uses the same six-button / six-encoder model as the PFM3
editor. The six buttons are drawn as a 2×3 grid. Pages 1–4 (`Global`, `Level`,
`Osc`, `EG`) are MSFA-specific; pages 5–6 reuse the PFM3 `Arp/Fx` and `Perf`
menus.

#### Main menu

| Row | Btn 1 | Btn 2 | Btn 3 |
|-----|-------|-------|-------|
| 1 | Global | Level | Osc |
| 2 | EG | Arp/Fx | Perf |

#### Global (per timbre)

| Row | Btn 1 | Btn 2 | Btn 3 |
|-----|-------|-------|-------|
| 1 | Engine | LFO | Pitch Env |
| 2 | — | — | — |

| Sub-btn | State | Enc 1 | Enc 2 | Enc 3 | Enc 4 | Enc 5 | Enc 6 |
|---------|-------|-------|-------|-------|-------|-------|-------|
| **Engine** | Engine | Algorithm | Feedback | Osc key sync | — | — | — |
| **LFO** | Modulation | AMD | PMD | — | — | — | — |
| | LFO | Speed | Delay | Waveform | Key sync | PMS | — |
| **Pitch Env** | Pitch R | Pitch EG R1 | R2 | R3 | R4 | — | — |
| | Pitch L | Pitch EG L1 | L2 | L3 | L4 | — | — |

#### Level (per operator)

| Row | Btn 1 | Btn 2 | Btn 3 |
|-----|-------|-------|-------|
| 1 | Amp | Scal | — |
| 2 | OP1/2 | OP3/4 | OP5/6 |

| Sub-btn | State | Enc 1 | Enc 2 | Enc 3 | Enc 4 | Enc 5 | Enc 6 |
|---------|-------|-------|-------|-------|-------|-------|-------|
| **Amp** | Amp | Operator enabled | Output level | KVS | AMS | — | — |
| **Scal** | Scal | Break point | Left depth | Right depth | Left curve | Right curve | Rate scaling |

#### Osc (per operator)

| Row | Btn 1 | Btn 2 | Btn 3 |
|-----|-------|-------|-------|
| 1 | Freq | — | — |
| 2 | OP1/2 | OP3/4 | OP5/6 |

| Sub-btn | State | Enc 1 | Enc 2 | Enc 3 | Enc 4 | Enc 5 | Enc 6 |
|---------|-------|-------|-------|-------|-------|-------|-------|
| **Freq** | Freq | Oscillator mode | Coarse | Fine | Detune | — | — |

#### EG (per operator)

| Row | Btn 1 | Btn 2 | Btn 3 |
|-----|-------|-------|-------|
| 1 | Rate | Level | — |
| 2 | OP1/2 | OP3/4 | OP5/6 |

| Sub-btn | State | Enc 1 | Enc 2 | Enc 3 | Enc 4 | Enc 5 | Enc 6 |
|---------|-------|-------|-------|-------|-------|-------|-------|
| **Rate** | Op Rate | EG R1 | R2 | R3 | R4 | — | — |
| **Level** | Op Level | EG L1 | L2 | L3 | L4 | — | — |

Operator selection uses buttons 4–6 (`OP1/2`, `OP3/4`, `OP5/6`) and
`fullState.operatorNumber`, the same mechanism as the PFM3 operator page.
Pages with more than six parameters use additional page states; pressing the
sub-page button cycles through them. Empty encoder slots use `MSFA_NONE`.
`EditorOneButtonState` still drives the state labels and the encoder-to-parameter
mapping (`ScreenEditor.h:28-31`).

### 5.6 Display and visual helpers

The PFM3 editor draws operator waveforms and envelopes:

- `refreshOscillatorOperatorShape()` — `ScreenEditor.cpp:4293-4310`
- `refreshOscillatorOperatorEnvelope()` — `ScreenEditor.cpp:4312-4329`
- `refreshLfoOscillator()` / `refreshLfoEnv()` — `ScreenEditor.cpp:4331-4349`

For MSFA:

- Draw the DX7 algorithm from MSFA’s `FmAlgorithm`/`FmCore` tables instead of
  PFM3’s `algoInformation` (`Common.h:739-773`, `ScreenEditor.cpp:4351-4365`).
  Implemented with the `dx7algo1..32` tables in `EditorDx7Algo`
  (`engine/msfa/`) and the generic `TftAlgo::drawAlgoFromTable()`.
- Draw the DX7 rate/level envelope (four rates, four levels) rather than the
  PFM3 ADSR curves. Implemented as follows:
  - `msfaGetEnvelopeDisplay()` (`MsfaEngine.cpp`) converts one operator’s
    R1..R4 / L1..L4 into an approximate `MsfaEnvelopeDisplay`:
    - Levels are mapped linearly (`L / 99`) so each level only moves its own
      point, matching the DX7’s own envelope display. No normalisation across
      the four stages, so editing one level never rescales the others.
    - Each stage duration depends only on its rate, never on the levels:
      `qrate = (rate * 41) >> 6`, `inc = (4 + (qrate & 3)) << (2 + LG_N + (qrate >> 2))`,
      `samples = fullRange * 2^24 / inc`, `time = samples / 44100`, where
      `fullRange = Env::scaleoutlevel(99) >> 1` in doublings (the DX7 rate is
      defined as the time to sweep the full level range). `inc` is normalised
      to 44.1 kHz, so the result is independent of the real sample rate.
      Rate scaling is ignored (nominal key).
  - `ScreenEditor::refreshOscillatorOperatorEnvelope()` calls the helper
    and passes the four times/levels to `oscilloBgSetEnvelope()`. All four
    stages use `CURVE_TYPE_LIN`, since MSFA envelopes are linear.
  - `ScreenCoordinator::refreshOscilloBG()` shows the envelope whenever an MSFA
    timbre is on the `Oper` page (`mainPage == 1`, any sub-page); other pages
    clear the background.
  - `oscilloBgDrawEnvelope()` (`FirmwareTftDisplay.cpp`) auto-scales the graph
    to the total envelope time. If the total is shorter than 0.2 s the scale
    becomes `total * 5`, so the envelope always occupies about 1/5 of the
    width and stays visible; in that case the scale label is not drawn. The
    other scale steps (1 s, 2 s, then 5 s increments) are unchanged.
  - The approximation ignores operator output level, velocity and keyboard
    level scaling, matching PFM3’s envelope display which is also independent
    of oscillator output level.
- Add new display types if needed in `ParameterDisplayType`
  (`firmware/src/app/ui/ScreenTypes.h:51-64`), for example
  `DISPLAY_TYPE_DX7_RATE`, `DISPLAY_TYPE_DX7_LEVEL`,
  `DISPLAY_TYPE_DX7_CURVE`, `DISPLAY_TYPE_DX7_ALGORITHM`.

---

### 5.7 Live parameter edits

`EditorMsfa` calls `SynthState::propagateNewEngineParamValue()`.
`Synth::newEngineParamValue()` forwards to `Timbre::engineParamChanged()` and
`MsfaSynthEngine::engineParamChanged()`:

- `setMsfaParamValue()` updates the DX7 program byte;
- operator parameters update one operator through
  `Dx7Note::updateOperator()`;
- algorithm, feedback, operator on/off, LFO depths and pitch EG use
  `Dx7Note::updateGlobal()`;
- LFO speed/delay/key sync/waveform reload the timbre LFO with
  `msfaTimbreLoadProgram()`.

`Env::updateParams()` / `PitchEnv::updateParams()` update parameters without
resetting the running envelope. `Dx7Note::update()` is unchanged and remains
the note-on/full-load path. Full `afterNewParamsLoad()` is used for preset
load, engine switch and DX7 import; `patchMemoryChanged()` remains for
operator copy/paste.

## 6. DX7 sysex loading

Current path:

1. `ScreenMenu::buttonPressed()` enters `MENU_PRESET_LOAD_SELECT_DX7_BANK`
   and calls `loadDx7Patch()` — `firmware/src/app/ui/ScreenMenu.cpp:647-651`.
2. `ScreenMenu::encoderTurned()` also loads banks/presets while scrolling —
   `ScreenMenu.cpp:193-206`.
3. `SynthState::loadDx7Patch()` — `SynthState.cpp:385-392`:
   - `propagateBeforeNewParamsLoad(timbre)`
   - `hexter->loadHexterPatch(...)`
   - `propagateAfterNewParamsLoad(timbre)`

Proposed new flow:

```cpp
void SynthState::loadDx7Patch(int timbre, PFM3File const *bank,
                              int patchNumber, Pfm3Preset *params) {
    storeTestNote();
    propagateNoteOff();
    propagateBeforeNewParamsLoad(timbre);

    // 1. Switch this timbre to MSFA before loading.
    mixerState.instrumentState_[timbre].engine = SYNTH_ENGINE_MSFA;

    // 2. Read and copy the packed patch out of the shared global buffer.
    const uint8_t *packed =
        storage->getDX7SysexFile()->dx7LoadPatch(bank, patchNumber);
    if (packed != nullptr) {
        timbres[timbre].loadMsfaPackedPatch(packed);  // copies 128 bytes
    }

    // 3. Keep PFM3 params in sync for display/backward compatibility.
    if (packed != nullptr) {
        hexter->loadHexterPatch(const_cast<uint8_t *>(packed), params);
    }

    propagateAfterNewParamsLoad(timbre);
    restoreTestNote();
}
```

`Timbre::loadMsfaPackedPatch()` should:

- copy 128 bytes into `MsfaProgram`/`msfaPacked_`;
- unpack to 155 bytes (via `Hexter::patchUnpack()` or a Dexed
  `Cartridge::unpackProgram()` port);
- append `0x3F` operator switch byte;
- call `MsfaVoiceState::setProgram()` / `Dx7Note::update()` on all active
  voices;
- reset MSFA LFO / pitch envelope state.

The menu/UI must refresh after the engine changes:
`Synth::newMixerValue()` and `ScreenCoordinator::newMixerEdit()` are the existing
refresh paths (`ScreenCoordinator.cpp:131-143`). Add an explicit
`propagateNewTimbre()` or `propagateNewMixerEdit()` after the engine switch.

Loading a DX7 patch always switches the target timbre to MSFA, even if the
user manually selected PFM3 for that timbre before loading. The mixer/editor
state is refreshed after the switch so the `Voices` engine selection and the
editor pages reflect the new engine.

---

## 7. Persistence and preset format

### 7.1 Engine selection

Store `engine` in `MixerInstrumentState` and bump `MIXER_BANK_VERSION` from 6
to 7. This makes the engine part of a saved mixer.

The engine is also stored in each preset. Loading a PFM3 preset into an MSFA
timbre switches that timbre to PFM3; loading an MSFA preset into a PFM3 timbre
switches it to MSFA. The preset loader must apply the engine tag before
initializing voices.

### 7.2 MSFA patch data

The preset slot is `ALIGNED_PATCH_SIZE` (1024) bytes. PFM3 presets keep the
existing layout: `FlashSynthParams` at offset 0 and the version at
`ALIGNED_PATCH_SIZE - 5` (offset 1019), either `PRESET_VERSION1` or
`PRESET_VERSION2`.

MSFA presets use the same slot with a different payload:

- `PRESET_MSFA = 981344537` at `ALIGNED_PATCH_SIZE - 5`;
- the 156-byte DX7 program at offset 0;
- the 12-byte preset name at offset 160.

There is **no separate `PRESET_VERSION3`**: `PRESET_MSFA` alone identifies the
slot. PFM3 presets are still identified by `PRESET_VERSION1`/`PRESET_VERSION2`
at the same trailing offset, so the two formats are unambiguous.

`PatchBank::loadPatch()` (`PatchBank.cpp:86`) reads the trailing 32-bit version
first. If it is `PRESET_MSFA`, it copies the 156-byte program into the caller
buffer, copies the name into `params->presetName`, sets
`*engine = SYNTH_ENGINE_MSFA`, and returns. Otherwise it follows the existing
PFM3 path and sets `*engine = SYNTH_ENGINE_PFM3`.

`PatchBank::savePatch()` (`PatchBank.cpp:149`) takes the engine and the
156-byte program. For MSFA it zeroes the slot, writes the program at offset 0,
the name at offset 160 and `PRESET_MSFA` at the end. For PFM3 it keeps
`convertParamsToFlash()` and writes `PRESET_CURRENT_VERSION`.

`PatchBank::loadPatchName()` (`PatchBank.cpp:108`) reads the version first; for
`PRESET_MSFA` it reads the name at offset 160, falling back to `"MSFA"` if the
name is empty.

The mixer bank (`MixerBank.cpp`) uses the same per-timbre slot layout:
`saveMixerData()` writes the MSFA program at offset 0, the name at offset 160
and `PRESET_MSFA` at the end for MSFA timbres; `loadMixerData()` detects
`PRESET_MSFA`, restores the program into the timbre's program buffer and the
name into `presetName`, and falls back to `"MSFA"` when the name is empty.
PFM3 timbres still use `convertParamsToFlash()`/`convertFlashToParams()`.

Existing PFM3 banks are unchanged. MSFA mixers/presets saved with the earlier
intermediate layout (program at offset 4) are not compatible and must be
re-saved.

### 7.3 MIDI

See [MIDI](#8-midi) for the engine split between PFM3 and MSFA.

---

## 8. MIDI

`MidiDecoder` is engine-agnostic: it decodes MIDI and forwards events to
`Synth`/`Timbre`, which dispatch to the selected engine. The engine split
therefore happens in the `Timbre`/`Voice` layer, except for the PFM3-specific
CC/NRPN parameter maps and the DX7 sysex handling, which are described below.

### 8.1 Shared MIDI path (both engines)

These features work for PFM3 and MSFA timbres:

- **Notes and velocity** — `MidiDecoder::midiEventReceived()`
  (`MidiDecoder.cpp:284-301`) → `Synth::noteOn/noteOff` → `Timbre::noteOn/noteOff`
  (`Timbre.cpp:361-375`) → `Timbre::engineNoteOn/engineNoteOff`
  (`Timbre.h:107-121`) → `msfaNoteOn/msfaNoteOff` for MSFA.
- **Hold pedal, all-notes-off, all-sound-off** — `Timbre::setHoldPedal()`
  (`Timbre.cpp:616-632`) releases voices through `Voice::noteOff()`, which
  detects an active MSFA voice and calls `msfaNoteOff()` (`Voice.cpp:581-586`).
- **MIDI clock, start, continue, stop, song position** — `MidiDecoder.cpp:76-120`.
  The arpeggiator is host-level (`Timbre::noteOn()` → `arpeggiatorNoteOn`) and
  works with MSFA.
- **Program change and bank select** — `MidiDecoder.cpp:328-341` enqueues a
  `LOAD_PRESET` action; `processAsyncActions()` (`MidiDecoder.cpp:1364-1380`)
  calls `SynthState::loadPresetFromMidi()` (`SynthState.cpp:432-465`):
  - bank 0 → PFM3 preset bank (`loadPreset`, `SynthState.cpp:376-395`); the
    engine comes from the preset;
  - bank 1 → mixer bank (`loadMixer`);
  - banks 2/3/4 → DX7 sysex bank (`loadDx7Patch`, `SynthState.cpp:403-421`),
    which forces the timbre to `SYNTH_ENGINE_MSFA`.
- **MPE notes** — `MidiDecoder.cpp:350-418` → `Timbre::noteOnMPE/noteOffMPE`
  (`Timbre.cpp:377-404`), which branch to MSFA. MPE *per-note modulation*
  (pitch bend, aftertouch, slide) is PFM3-only; see 8.3.
- **Mixer and global CCs** (volume, pan, send, master FX) — engine-independent
  (`MidiDecoder::controlChange()`, `MidiDecoder.cpp:422-524`).
- **Custom `0x7d` mixer-volume sysex** — `analyseSysexBuffer()`
  (`MidiDecoder.cpp:1344-1358`).

### 8.2 MSFA MIDI

MSFA controllers are fed through `Timbre::setMatrixSource()`
(`Timbre.cpp:3272-3276`) → `msfaTimbreSetController()`
(`MsfaEngine.cpp:124-144`).

Currently supported:

| MIDI source | MSFA `Controllers` field |
|---|---|
| Pitch bend | `pitch_cc` |
| Mod wheel | `modwheel_cc` |
| Channel aftertouch | `aftertouch_cc` |
| Breath | `breath_cc` |

All other `SourceEnum` values fall through to `default: return` and are
ignored by the MSFA engine. `Controllers` also has `foot_cc`, portamento,
master tune and MPE fields, but nothing in the current `MidiDecoder` path
fills them; `msfaTimbreInit()` resets them (`MsfaEngine.cpp:80-106`).
`controllers.refresh()` is called after every accepted update.

The MSFA LFO and pitch envelope are driven by the DX7 program
(`msfaTimbreLoadProgram()` → `Lfo::reset(program + 137)`,
`MsfaEngine.cpp:120-122`), not by the PFM3 matrix/LFO.

The MSFA editor writes directly to the DX7 program
(`EditorMsfa::setParamValue()`), so MSFA parameter changes do not go
through `SynthState::setNewValue()` and therefore emit no CC/NRPN.

### 8.3 PFM3-specific MIDI

These paths exist for the PFM3 parameter model and must not be interpreted as
MSFA parameters. On an MSFA timbre the engine MIDI hooks return false, so
PFM3 CCs and NRPNs have no effect.

- **CC parameter map** (`receives & 0x01`) —
  `MidiDecoder::controlChange()` maps shared CCs (Arp/FX/mixer/sequencer) and
  forwards the rest to `Timbre::midiData()` → `Preenfm3Engine::midiData()`:
  algorithm, IM, mix/pan, oscillator frequency, matrix, LFO, envelopes, unison,
  step sequencer.
  `MsfaSynthEngine::midiData()` adds a small global map:

  | CC | MSFA parameter |
  |---|---|
  | 33 | Algorithm |
  | 34 | Feedback |
  | 35 | LFO speed |
  | 36 | LFO delay |
  | 37 | LFO pitch mod depth |
  | 39 | LFO amp mod depth |
  | 111 | LFO key sync |
  | 112 | LFO waveform |
  | 113 | LFO pitch mod sensitivity |

  Values are linearly mapped from 0..127 to the parameter's min..max (key sync
  switches at 64). CC 38 is skipped because it is the NRPN data LSB. The same
  CCs are emitted when a global parameter changes in the editor
  (`MsfaSynthEngine::midiParamFeedback()`). CCs received from MIDI are applied
  with `propagateNewEngineParamValueFromExternal()`, so they update audio/UI
  but are not echoed back.
- **NRPN** (`receives & 0x02`) — `MidiDecoder` accumulates NRPN and forwards
  it to `Timbre::midiData()` with `MIDI_DATA_NRPN` →
  `Preenfm3Engine::midiData()`, which maps the legacy indexes to PFM3
  `allParameterRows`. MSFA ignores them.
  `sendCurrentPatchAsNrpns()` enumerates the patch through
  `midiPatchParam()`; MSFA uses the default (nothing), so it no longer sends
  the PFM3 block.
- **Outgoing CC/NRPN** — `MidiDecoder::newPfm3ParamValue()` delegates the
  row/encoder → CC/NRPN mapping to `Preenfm3Engine::midiPfm3ParamFeedback()`,
  and `newEngineParamValue()` delegates to the generic
  `SynthEngine::midiParamFeedback(paramId, context, ...)`. MSFA does not
  override it, so no MSFA CC/NRPN is emitted yet.
- **MPE per-note modulation and poly aftertouch** —
  `Timbre::setMatrixSourceMPE()` and `Timbre::setMatrixPolyAfterTouch()`
  return early for MSFA (`Timbre.cpp:3283-3310`). MPE notes still play, but
  per-note pitch bend/aftertouch/slide and poly aftertouch do not reach MSFA.
- **DX7 sysex input** — `analyseSysexBuffer()` only handles the custom `0x7d`
  mixer-volume message. `SYSEX_BUFFER_SIZE` is 32 (`MidiDecoder.cpp:42`), so a
  full DX7 sysex dump cannot be received. DX7 patches are loaded from files
  (`DX7SysexFile`) via bank/program change, not from MIDI sysex input.
- **Shared `Arp/Fx` page** — still uses PFM3 parameters, so its CC/NRPN edits
  remain valid for MSFA timbres.

### 8.4 Split summary

| Feature | PFM3 timbre | MSFA timbre |
|---|---|---|
| Notes / velocity / hold / all-notes-off | yes | yes |
| MIDI clock / arpeggiator | yes | yes |
| Program change / bank select | yes (bank 0) | yes (bank 0 MSFA preset, banks 2-4 DX7) |
| Pitch bend / mod wheel / channel AT / breath | PFM3 matrix | MSFA `Controllers` |
| Poly aftertouch / MPE per-note modulation | yes | ignored |
| Engine CC map | full PFM3 | algo, fdbk, 7 global LFO |
| PFM3 NRPN map | yes | no effect |
| Outgoing CC/NRPN | yes | same global CC map |
| Custom `0x7d` mixer-volume sysex | yes | yes |
| DX7 sysex input | not supported | not supported |

### 8.5 Intended enforcement

The split above is the intended behavior. The CC and NRPN maps are enforced:
the engine hooks default to "not handled", so MSFA timbres ignore PFM3
parameter CCs and NRPNs. The PFM3 patch dump is engine-enumerated, so an MSFA
timbre sends nothing. The MSFA CC map currently covers only algorithm,
feedback and the seven global LFO parameters; it can be extended later.

Do not reuse PFM3 row indexes for MSFA parameters; the meaning would be
ambiguous.

### 8.6 DX7 sysex (MSFA)

#### Parameter change

A real DX7 accepts single-parameter changes over sysex. This is the intended
path for editing an MSFA timbre from an external sequencer/editor without
sending a whole voice dump.

#### Message format

```
F0 43 1n pp vv F7
```

- `43` — Yamaha manufacturer ID.
- `1n` — voice parameter change, `n` = MIDI channel 0..15.
- `pp` — DX7 voice parameter number, 0..155.
- `vv` — value, 0..127 (the valid range depends on the parameter).

`MidiDecoder::newByte()` strips the leading `F0`, so
`analyseSysexBuffer()` receives `43 1n pp vv F7` (5 bytes). The current
`SYSEX_BUFFER_SIZE` is 32 (`MidiDecoder.cpp:42`), so a parameter change fits
comfortably.

A full DX7 voice dump (`F0 43 0n 00 01 1B … F7`, 163 bytes) does **not** fit
and is out of scope for this section. Supporting it would require increasing
`SYSEX_BUFFER_SIZE` and adding checksum validation.

#### Parameter mapping

The 156-byte `msfaProgram` buffer (`MSFA_PROGRAM_SIZE`) is already in the DX7
unpacked voice layout:

| Range | Content |
|---|---|
| 0..20 | OP6 parameters |
| 21..41 | OP5 |
| 42..62 | OP4 |
| 63..83 | OP3 |
| 84..104 | OP2 |
| 105..125 | OP1 |
| 126..129 | pitch EG rates |
| 130..133 | pitch EG levels |
| 134 | algorithm |
| 135 | feedback |
| 136 | oscillator key sync |
| 137..143 | LFO |
| 144..154 | name |
| 155 | operator on/off bitmask (Dexed extension) |

Therefore the parameter change is a direct write:

```cpp
if (pp <= 155) {
    program[pp] = vv;
}
```

No conversion is needed. `MsfaParamId`/`msfaParams` are display-oriented and
must not be used as the sysex parameter number.

#### Channel routing

Reuse the same rules as `MidiDecoder::midiEventReceived()`
(`MidiDecoder.cpp:213-269`):

- global channel → all timbres;
- current channel → `currentTimbre`;
- otherwise each timbre whose `omniOn[t]` is true, whose
  `midiChannel == 0`, or whose `midiChannel - 1 == n`.

Build a 6-bit timbre mask and apply the change only to timbres with
`Timbre::getEngine() == SYNTH_ENGINE_MSFA`. Add a small helper, for example
`uint8_t MidiDecoder::getTimbresForChannel(uint8_t channel)`, so the routing
is not duplicated.

#### Deferred apply

`analyseSysexBuffer()` runs in the MIDI receive path.
`Timbre::msfaAfterNewParamsLoad()` (`Timbre.cpp:925-935`) loops over all
voices and resets the MSFA LFO, so do not run it inline in the sysex handler.

Recommended flow:

1. In `analyseSysexBuffer()`, validate the message and enqueue an async
   action instead of applying it immediately:
   ```cpp
   AsyncAction action;
   action.fullBytes = 0;
   action.action.actionType = MSFA_PARAM_CHANGE;
   action.action.timbre = timbreMask;
   action.action.param1 = pp;
   action.action.param2 = vv;
   asyncActions.insert(action);
   ```
2. Add `MSFA_PARAM_CHANGE` to `enum ActionType`
   (`MidiDecoder.h:156-159`). `AsyncActionDetail`
   (`MidiDecoder.h:161-170`) already has `timbre` and `param1..param6`.
3. Handle it in `MidiDecoder::processAsyncActions()`
   (`MidiDecoder.cpp:1364-1380`):
   ```cpp
   case MSFA_PARAM_CHANGE:
       for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
           if ((asyncAction.action.timbre & (1 << t)) == 0) {
               continue;
           }
           Timbre *timb = synth->getTimbre(t);
           if (timb->getEngine() != SYNTH_ENGINE_MSFA) {
               continue;
           }
           uint8_t *program = timb->getMsfaProgramMutable();
           uint8_t param = asyncAction.action.param1;
           uint8_t value = asyncAction.action.param2;
           if (param > 155 || program[param] == value) {
               continue;
           }
           program[param] = value;
           timb->msfaAfterNewParamsLoad();
           // mark the preset modified and refresh the editor (see below)
       }
       break;
   ```

#### Preset modified and editor refresh

`ScreenEditor::markPresetModified()` (`ScreenEditor.cpp:3053`) is a
UI-side method, so `MidiDecoder` cannot call it directly. Add a small
`SynthState` notification (for example a `presetModified` listener callback,
or a `SynthState::markPresetModified(timbre)` helper) and call it from
`processAsyncActions()`.

For the editor display, add a method such as
`EditorMsfa::refreshParam(uint8_t dx7Param)` that:

- scans the current page's `rowEncoder[]`;
- finds the encoder showing the changed parameter;
- calls `displayParamValue(encoder, COLOR_YELLOW)` and sets
  `valueChangedCounter_[encoder] = 4`, matching the encoder-turn highlight.

Route the notification through `SynthState`/`ScreenCoordinator` rather than calling
the editor from `MidiDecoder`. A full page refresh through `refreshStatus` is
also possible but heavier and may reset page state.

#### Highlighting

The changed DX7 parameter can reuse the existing MSFA highlight logic in
`EditorMsfa::newParamValue()`:

- `pp` 0..125 → operator parameter; the operator is
  `6 - (pp / 21)` (OP6 for 0..20, …, OP1 for 105..125), so highlight that
  operator box;
- `pp` 126..133 → pitch EG (global, no operator highlight);
- `pp` 134 → algorithm; redraw the DX7 algorithm;
- `pp` 135 → feedback; highlight the feedback loop;
- `pp` 136..143 → global/LFO (no diagram highlight);
- `pp` 144..154 → name;
- `pp` 155 → operator on/off; highlight the operator corresponding to the
  changed bit.

A `dx7ParamToMsfaParam(pp)` helper (or a reverse lookup from byte offset to
`MsfaParamId`) keeps this mapping in one place.

#### Limits

- DX7 **function parameters** (master tune, poly/mono, pitch bend range,
  etc.) are not part of the 156-byte voice program and are not handled by
  this path.
- **Voice bulk dumps** are handled separately in 8.7.
- Parameter-change messages carry no checksum, so none is validated.
- Changes write the same `msfaProgram` buffer used by preset/mixer save, so
  they are persisted without extra work.

### 8.7 Full DX7 voice dump (MSFA)

A DX7 can also send a complete voice over sysex, either as a single voice or
as a 32-voice bank. This is the sysex equivalent of loading a patch from a
file; it writes the whole 156-byte MSFA program instead of one parameter.

#### Message formats

Single voice:

```
F0 43 0n 00 01 1B <155 bytes> <checksum> F7
```

32 voices:

```
F0 43 0n 09 20 00 <4096 bytes> <checksum> F7
```

- `0n` — voice dump, `n` = MIDI channel 0..15.
- The 155-byte payload is the **unpacked** DX7 voice: exactly bytes 0..154
  of `msfaProgram` (OP6..OP1, pitch EG, algorithm, feedback, LFO, name).
- `<checksum>` — 7-bit checksum over the data: `(-sum(data)) & 0x7F`.
- The single-voice message is 163 bytes total; the 32-voice bank is 4104
  bytes total.

#### Why the current handler cannot receive it

`SYSEX_BUFFER_SIZE` is 32 (`MidiDecoder.cpp:42`), so the single-voice dump
(163 bytes) and the bank dump (4104 bytes) are both truncated. The existing
`analyseSysexBuffer()` also only understands the custom `0x7d` mixer sysex.

#### Single-voice dump

This is the recommended first step.

1. **Buffer.** Increase `SYSEX_BUFFER_SIZE` to at least 163 (for example 192),
   or add a dedicated `msfaSysexBuffer[163]` and route Yamaha `0n` messages
   there. Keep the 32-byte buffer for the `0x7d` mixer sysex if RAM matters.
   RAM impact is small (about 163 bytes).
2. **Validate.** In `analyseSysexBuffer()`:
   - `sysexBuffer[0] == 0x43` and `(sysexBuffer[1] & 0xF0) == 0x00`;
   - `size == 162` after `F0` was stripped (`newByte()` stores `F7` too);
   - the checksum at `sysexBuffer[160]` equals
     `(-sum(sysexBuffer[5..159])) & 0x7F`;
   - reject the message if any check fails, without touching the program.
3. **Payload.** The 155 data bytes start at `sysexBuffer[5]`. They are
   already unpacked, so no `Hexter::patchUnpack()` is needed (unlike the
   packed 128-byte patches read by `DX7SysexFile`). Copy them into a static
   `msfaSysexProgram[156]` and set `msfaSysexProgram[155] = 0x3F` (all
   operators on); the plain DX7 dump does not carry the Dexed operator
   switch byte.
4. **Deferred apply.** The `AsyncAction` union is 64 bits and cannot carry
   156 bytes. Enqueue an `MSFA_VOICE_DUMP` action carrying only the timbre
   mask, and use the static `msfaSysexProgram` as the payload. If back-to-back
   dumps must be supported, use a small ring of program buffers or apply the
   dump synchronously.
5. **Apply.** In `processAsyncActions()`, for each MSFA timbre in the mask:
   - `memcpy(timbre->getMsfaProgramMutable(), msfaSysexProgram, 156)`;
   - `timbre->msfaAfterNewParamsLoad()` (`Timbre.cpp:925-935`);
   - mark the preset modified and refresh the editor (same plumbing as 8.6).
6. **Routing.** Use the same channel rules as the parameter change (8.6).

#### 32-voice bank dump

The bank dump is 4104 bytes and contains 32 packed 128-byte voices. It is
optional: the firmware already loads DX7 banks from files via
`DX7SysexFile::dx7LoadPatch()` / `Hexter::patchUnpack()`, so the usual way to
load a bank is `Preset > DX7`, not sysex.

If sysex bank loading is required:

- increase the buffer to 4104 bytes (or stream the data instead of buffering
  it); this is a noticeable RAM cost;
- validate the 4096-byte payload and its checksum;
- unpack each 128-byte voice with the existing `Hexter::patchUnpack()` and
  store the selected voice, or add a 32-voice in-memory cartridge;
- decide whether a bank dump replaces all 32 slots or only the current one.

#### Limits

- Function parameters (master tune, poly/mono, pitch bend range, etc.) are not
  part of a voice dump.
- A malformed size or checksum must be rejected without modifying the current
  program.
- The plain DX7 dump has no Dexed operator on/off byte; default it to `0x3F`.

---

## 9. Implementation phases

1. **Vendor MSFA and build it.**
   - Add `third_party/msfa` and the CMake target.
   - Add one-time initialization.
   - Verify it links into the firmware with no RAM/flash surprises.

2. **Add engine selection.**
   - `SynthEngineType`, `MixerInstrumentState::engine`.
   - Mixer bank version 7 and preset engine tag.
   - Third state in the Voices mixer button.
   - `Timbre::setEngine()` and voice reinitialization.

3. **Add MSFA patch storage and loading.**
   - Per-timbre packed/unpacked buffers.
   - Packed-to-unpacked conversion.
   - `Timbre::loadMsfaPackedPatch()`.

4. **Add MSFA rendering.**
   - `MsfaVoiceState` in the global `Voice`.
   - noteOn/noteOff branch in `Timbre`.
   - 64-to-32 block buffering and Q24-to-float conversion.
   - Reuse the existing FX/mixer path, keeping `Arp/Fx` available.

5. **Switch DX7 sysex load to MSFA.**
   - Update `SynthState::loadDx7Patch()`.
   - Refresh mixer/editor UI and persist the engine choice.

6. **Add MSFA editor.**
   - `msfaMainMenu`, `msfaParameterRows`, `MsfaParameterId`.
   - Engine-aware menu selection in `ScreenEditor`.
   - DX7 algorithm/envelope drawing.

7. **Add MSFA preset persistence.** *(done)*
   - `PRESET_MSFA` marker at `ALIGNED_PATCH_SIZE - 5` (no `PRESET_VERSION3`).
   - Full 156-byte MSFA program at offset 0, name at offset 160.
   - Same slot layout in mixer banks, per timbre.
   - Load/save round trips and legacy PFM3 banks verified.

8. **Profile and tune.**
   - CPU under worst-case load with 6×MSFA and 20 voices.
   - RAM with MSFA state in all voices.
   - Confirm FX1/FX2 and `Arp/Fx` remain usable with MSFA.

---

## 10. Test plan

- Build both engines and confirm the default PFM3 firmware still builds and
  boots unchanged.
- Load the same DX7 sysex patch in two timbres: one via Hexter/PFM3 and one via
  MSFA, and compare pitch, envelopes and algorithm behavior.
- Test all six timbres with mixed engines and full voice allocation.
- Test voice stealing, mono/poly/unison, hold pedal, MPE and MIDI clock with
  MSFA timbres.
- Save/load mixer banks and presets, including engine selection and MSFA patch
  data.
- Check CPU and RAM with `preenfm3_firmware.map` and the on-screen CPU meter.
- Verify the editor navigation on all MSFA pages, including operator
  selection and page-state cycling.

---

## 11. Decisions

1. **Engine persistence:** the engine is stored in both the mixer bank and
   each preset. Loading a PFM3 preset into an MSFA timbre switches it to PFM3;
   loading an MSFA preset into a PFM3 timbre switches it to MSFA.
2. **DX7 load behavior:** `Preset > DX7` always switches the target timbre to
   MSFA.
3. **MSFA variant:** use `FmCore` first, with code comments describing where
   `EngineMkI` and `EngineOpl` would be plugged in.
4. **PFM3 features for MSFA:** do not expose the PFM3 `Matrix` or `Modul`
   pages. Keep the `Arp/Fx` page so the arpeggiator, FX1 and FX2 remain usable
   with MSFA timbres.
5. **Preset format:** store the full 156-byte MSFA program at offset 0 and
   mark MSFA presets with `PRESET_MSFA = 981344537` at
   `ALIGNED_PATCH_SIZE - 5`; the name is stored at offset 160. PFM3 presets
   keep their existing `PRESET_VERSION1`/`PRESET_VERSION2` layout.
6. **Voice model:** keep the existing 20 global voices and allow MSFA per
   timbre. No separate MSFA voice pool.
7. **Operator on/off:** support the Dexed operator on/off bitmask.
8. **License:** MSFA is the engine from Google's
   music-synthesizer-for-android project and is Apache-2.0, including the
   Dexed `source/msfa` copy (the files keep their Google/Apache-2.0 headers
   with later Dexed modifications). Dexed's own plugin/application and
   patch/Cartridge code is GPL-3.0. Apache-2.0 is compatible with this
   project's GPL-3.0; keep the upstream headers and notices, and distribute
   the combined firmware binary under GPL-3.0.
9. **MIDI split:** notes, clock, hold pedal, MPE notes, program/bank change,
   mixer/global CCs and the custom `0x7d` sysex are shared by both engines.
   PFM3-specific CC/NRPN parameter maps, MPE per-note modulation, poly
   aftertouch and DX7 sysex input are not applied to MSFA timbres. MSFA
   receives pitch bend, mod wheel, channel aftertouch and breath through
   `msfaTimbreSetController()`; a dedicated MSFA CC/NRPN map can be added
   later. See [MIDI](#8-midi).

## TODO

- Should loading a DX7 (MSFA) preset reset the arpeggiator and FX?
- Should the arpeggiator and FX be saved with the preset?
