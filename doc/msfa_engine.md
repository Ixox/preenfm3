# MSFA engine — design proposal

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
- Provide an MSFA-specific editor in `FMDisplayEditor`, with navigation that
  follows the existing six-button / six-encoder model.

## Non-goals (for the first iteration)

- Replacing or rewriting the PFM3 engine.
- Rebuilding all PFM3 UI pages for MSFA.
- Full DX7-compatible sysex *export*.
- Bit-exact Dexed compatibility on the first pass.

---

## 1. Current architecture

### 1.1 Top-level audio path

`Synth` owns six `Timbre` objects and sixteen global `Voice` objects:

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

- the parameter block `OneSynthParams params_` — `Timbre.h:279`;
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
  `OneSynthParams` array:
  - `FMDisplayEditor::encoderTurnedPfm2()` — `FMDisplayEditor.cpp:3843-3998`
  - `FMDisplayEditor::displayParamValue()` — `FMDisplayEditor.cpp:3227-3267`
  - `MidiDecoder::decodeNrpn()` — `MidiDecoder.cpp:914-936`

The PFM3 parameter model is row-oriented: `row * NUMBER_OF_ENCODERS_PFM2 +
encoder` indexes `OneSynthParams`. See `Common.h:149-225` for the `ROW_*`
enum and `Common.h:544-608` for `OneSynthParams`.

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
Keep the existing 16 global voices and allow each timbre to select MSFA
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

Do **not** allocate a separate 16-voice MSFA pool per timbre: that would
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
  PFM3 `OneSynthParams`.

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
DTCMRAM: 95.85%
RAM_D1:  86.21%
RAM_D2:  93.07%
RAM_D2B: 73.43%
```

`Dx7Note` per voice adds six envelopes, six `FmOpParams`, a pitch envelope,
feedback buffers and controller state. With sixteen global voices this is
manageable, but must be measured. Avoid a separate MSFA voice array per
timbre. The existing 16 global voices remain the allocation unit.

CPU must be profiled under real load, not only at idle. The reported ~6.4% is
the idle load; a fully loaded PFM3 patch with 16 voices, FX and arpeggiator can
reach roughly 75%. MSFA must be benchmarked with the same worst-case setup,
and the result may require a voice cap, disabling some FX, or deferring MSFA
support on lower-headroom configurations.

---

## 4. UI: mixer engine selection

The user-facing entry point is the existing **Voices** mixer button.

Current state definitions:

- `voiceButtonState` — `firmware/src/app/ui/FMDisplayMixer.cpp:106-109`
- `compButtonState` — `FMDisplayMixer.cpp:88-91`
- `voiceButton` with `numberOfStates = 2` — `FMDisplayMixer.cpp:111-115`
- `mixerMenu` — `FMDisplayMixer.cpp:255-257`

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
2. Add `engineDisplay[]` (`"PFM3"`, `"MSFA"`) near the other display arrays
   in `FMDisplayMixer.cpp:31-62`.
3. Add the per-timbre field to `MixerInstrumentState`
   (`MixerState.h:42-58`).
4. Map it in `FMDisplayMixer::getValuePointer()` — `FMDisplayMixer.cpp:278-419`:

   ```cpp
   case MIXER_VALUE_ENGINE:
       valueP = &synthState_->mixerState.instrumentState_[encoder].engine;
       break;
   ```

5. Add display handling in `FMDisplayMixer::displayMixerValue()` —
   `FMDisplayMixer.cpp:432-544`. The existing `DISPLAY_TYPE_STRINGS` path is at
   `FMDisplayMixer.cpp:526-533`.
6. Add edit handling in `FMDisplayMixer::encoderTurned()` —
   `FMDisplayMixer.cpp:831-933`. The generic string/int branch already writes
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
- `FMDisplay3::refreshAllScreenByStep()` — `FMDisplay3.cpp:76-98`
- `FMDisplay3::newSynthMode()` — `FMDisplay3.cpp:174-192`
- `preenfm3.cpp` oscilloscope branch — `preenfm3.cpp:211-225`

Minimal alternative: keep `SYNTH_MODE_EDIT_PFM3` and have `FMDisplayEditor`
choose `msfaMainMenu` when the current timbre’s engine is MSFA. The separate
mode is cleaner because many editor helpers hard-code PFM3 page indices
(`isInOperatorPage`, `isInMatrixPage`, `isInModulatorPage` —
`FMDisplayEditor.cpp:4281-4291`, and `checkOperatorNumber` —
`FMDisplayEditor.cpp:4351-4365`).

### 5.2 Editor menu structure

The editor is data-driven:

- `Pfm3OneButtonState` — `FMDisplayEditor.h:28-31`
- `Pfm3OneButton` — `FMDisplayEditor.h:33-39`
- `Pfm3EditMenu` — `FMDisplayEditor.h:41-45`
- `PfmMainMenu` — `FMDisplayEditor.h:47-49`
- `mainMenu` — `FMDisplayEditor.cpp:2988-2995`
- page/state navigation — `FMDisplayEditor::buttonPressed()` —
  `FMDisplayEditor.cpp:4104-4164`
- parameter display — `FMDisplayEditor::refreshEditorByStep()` —
  `FMDisplayEditor.cpp:3269-3491`

Add a second menu tree, for example `msfaMainMenu`, and select it by engine:

```cpp
const PfmMainMenu *FMDisplayEditor::currentMainMenu() const {
    if (synthState_->mixerState.instrumentState_[currentTimbre_].engine
            == SYNTH_ENGINE_MSFA) {
        return &msfaMainMenu;
    }
    return &mainMenu;
}
```

Replace direct `mainMenu.editMenu[...]` uses (see `FMDisplayEditor.cpp:3036`,
`3228`, `3303`, `3808`, `3828`, `4006`, `4036`, `4122`, `4187`, `4229`,
`4312`, `4329`) with `currentMainMenu()->editMenu[...]`.

### 5.3 MSFA parameter model

Do **not** add MSFA parameters to `OneSynthParams`. It is a flat PFM3 preset
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

### 5.5 Suggested MSFA editor navigation

The PFM3 editor has six main pages (buttons 1–6) and **six encoders per
page**. Each page state maps all six encoders through
`RowEncoder rowEncoder[6]` (`FMDisplayEditor.h:28-31`). Reuse that model with
six MSFA pages:

| Main page (button) | Page states (button presses) | Six encoders |
|---|---|---|
| `MSFA` | 0: global; 1: LFO | Global: Algorithm, Feedback, Transpose, Osc key sync, LFO speed, LFO delay. LFO: PMD, AMD, LFO key sync, LFO waveform, PMS, spare |
| `Oper` | 0: oscillator; 1: level/mod | Oscillator: Coarse, Fine, Detune, Output level, Osc mode, Rate scaling. Level/mod: KVS, AMS, Break point, Left depth, Right depth, spare |
| `EG` | 0: operator rates; 1: operator levels; 2: pitch rates; 3: pitch levels | Rates: R1–R4 plus two spare. Levels: L1–L4 plus two spare. Pitch rates: R1–R4 plus two spare. Pitch levels: L1–L4 plus two spare |
| `Scale` | 0: levels; 1: curves; 2: switches | Levels: Break point, Left depth, Right depth, Rate scaling, two spare. Curves: Left curve, Right curve, KVS, AMS, two spare. Switches: operator enabled plus five spare |
| `Arp/Fx` | reuse existing `pfm3ArpFilterMenu` (`FMDisplayEditor.cpp:2978-2985`) | Existing arpeggiator, FX1 and FX2 pages |
| `Perf` | reuse existing `pfm3PerfMenu` (`FMDisplayEditor.cpp:2972-2976`) | Existing performance page |

Operator selection:

- Reuse `fullState.operatorNumber` (`Menu.h:181`) and the existing
  `Oper` page pattern (`operatorEditMenu`, `FMDisplayEditor.cpp:2901-2910`).
- On `Oper`, `EG` and `Scale` pages, buttons 4 and 6 can change the selected
  operator (as `FMDisplayEditor::buttonPressed()` does for the PFM3 operator
  page at `FMDisplayEditor.cpp:4127-4144`).
- Button 5 can toggle the selected operator on/off (the Dexed extension is
  supported), or be unused to match the existing “nothing” button.

Unused encoder slots in a state should use the MSFA equivalent of
`ROW_NONE` / `ENCODER_NONE`. If a page needs more than six parameters, add
another page state rather than trying to exceed the six-encoder layout.

The existing `Pfm3OneButtonState` mechanism supports multiple states per page;
a page cycles through them by pressing its button
(`FMDisplayEditor::buttonPressed()` — `FMDisplayEditor.cpp:4145-4149`).

### 5.6 Display and visual helpers

The PFM3 editor draws operator waveforms and envelopes:

- `refreshOscillatorOperatorShape()` — `FMDisplayEditor.cpp:4293-4310`
- `refreshOscillatorOperatorEnvelope()` — `FMDisplayEditor.cpp:4312-4329`
- `refreshLfoOscillator()` / `refreshLfoEnv()` — `FMDisplayEditor.cpp:4331-4349`

For MSFA:

- Draw the DX7 algorithm from MSFA’s `FmAlgorithm`/`FmCore` tables instead of
  PFM3’s `algoInformation` (`Common.h:739-773`, `FMDisplayEditor.cpp:4351-4365`).
- Draw the DX7 rate/level envelope (four rates, four levels) rather than the
  PFM3 ADSR curves.
- Add new display types if needed in `ParameterDisplayType`
  (`firmware/src/app/ui/FMDisplay.h:51-64`), for example
  `DISPLAY_TYPE_DX7_RATE`, `DISPLAY_TYPE_DX7_LEVEL`,
  `DISPLAY_TYPE_DX7_CURVE`, `DISPLAY_TYPE_DX7_ALGORITHM`.

---

## 6. DX7 sysex loading

Current path:

1. `FMDisplayMenu::buttonPressed()` enters `MENU_PRESET_LOAD_SELECT_DX7_BANK`
   and calls `loadDx7Patch()` — `firmware/src/app/ui/FMDisplayMenu.cpp:647-651`.
2. `FMDisplayMenu::encoderTurned()` also loads banks/presets while scrolling —
   `FMDisplayMenu.cpp:193-206`.
3. `SynthState::loadDx7Patch()` — `SynthState.cpp:385-392`:
   - `propagateBeforeNewParamsLoad(timbre)`
   - `hexter->loadHexterPatch(...)`
   - `propagateAfterNewParamsLoad(timbre)`

Proposed new flow:

```cpp
void SynthState::loadDx7Patch(int timbre, PFM3File const *bank,
                              int patchNumber, OneSynthParams *params) {
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
`Synth::newMixerValue()` and `FMDisplay3::newMixerEdit()` are the existing
refresh paths (`FMDisplay3.cpp:131-143`). Add an explicit
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

The current preset format stores `OneSynthParams` in a 1024-byte
`ALIGNED_PATCH_SIZE` slot, with a 4-byte version at
`ALIGNED_PATCH_SIZE - 5` (`PreenFMFileType.h:144-148`,
`PatchBank.cpp:86-106` and `PatchBank.cpp:149-161`).

There is **no magic number at offset 0 today**. Offset 0 is the first field of
`FlashSynthParams` (`engine1`), and the version is read from
`ALIGNED_PATCH_SIZE - 5` (offset 1019). `PRESET_VERSION1` is `0` and
`PRESET_VERSION2` is the random-looking `292928062`, both stored at the end of
the slot, not at the beginning.

Store the full 156-byte MSFA program in the preset. PFM3 presets keep the
existing layout unchanged; only MSFA presets use offset 0 for a type marker:

- add `PRESET_VERSION3` to `enum PRESET_VERSION` in `PatchBank.h:24-30`;
- define a random 32-bit `PRESET_MSFA = 2504912210` (`0x954DED52`) in
  `PatchBank.h`, using the same idea as `PRESET_VERSION2` so random SD-card
  data is unlikely to match;
- for an MSFA preset, write `PRESET_MSFA` at offset 0, followed by the
  156-byte MSFA program, and `PRESET_VERSION3` at `ALIGNED_PATCH_SIZE - 5`;
- PFM3 presets keep offset 0 as `engine1` and use `PRESET_VERSION1` or
  `PRESET_VERSION2` at the end. No `PRESET_PFM3` marker is needed.

Update `PatchBank::loadPatch()` (`PatchBank.cpp:86-106`),
`PatchBank::savePatch()` (`PatchBank.cpp:149-161`) and
`PatchBank::loadPatchName()` (`PatchBank.cpp:108-147`) to read the version
first: if it is `PRESET_VERSION3`, read the magic at offset 0 and expect
`PRESET_MSFA`; otherwise treat the slot as a legacy PFM3 preset.

This makes MSFA presets unambiguous while keeping existing PFM3 banks and the
PFM3 payload layout unchanged.

### 7.3 MIDI NRPN

`MidiDecoder::decodeNrpn()` (`MidiDecoder.cpp:914-936`) maps NRPN indexes to
PFM3 `allParameterRows`. For MSFA timbres, **ignore PFM3 NRPN edits** while
the timbre uses the MSFA engine. Do not reuse PFM3 row indexes for MSFA
parameters; the meaning would be ambiguous. A dedicated MSFA NRPN map can be
added later if required.

---

## 8. Implementation phases

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
   - Engine-aware menu selection in `FMDisplayEditor`.
   - DX7 algorithm/envelope drawing.

7. **Add MSFA preset persistence.**
   - `PRESET_VERSION3`, random `PRESET_MSFA` magic at offset 0.
   - Full 156-byte MSFA program in the preset.
   - Test load/save round trips and legacy PFM3 banks.

8. **Profile and tune.**
   - CPU under worst-case load with 6×MSFA and 16 voices.
   - RAM with MSFA state in all voices.
   - Confirm FX1/FX2 and `Arp/Fx` remain usable with MSFA.

---

## 9. Test plan

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

## 10. Decisions

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
5. **Preset format:** store the full 156-byte MSFA program in the preset and
   mark MSFA presets with a random 32-bit `PRESET_MSFA = 0x954DED52` at
   offset 0, using `PRESET_VERSION3`. PFM3 presets keep their existing
   `PRESET_VERSION1`/`PRESET_VERSION2` layout; no new PFM3 magic is needed.
6. **Voice model:** keep the existing 16 global voices and allow MSFA per
   timbre. No separate MSFA voice pool.
7. **Operator on/off:** support the Dexed operator on/off bitmask.
8. **License:** MSFA is the engine from Google's
   music-synthesizer-for-android project and is Apache-2.0, including the
   Dexed `source/msfa` copy (the files keep their Google/Apache-2.0 headers
   with later Dexed modifications). Dexed's own plugin/application and
   patch/Cartridge code is GPL-3.0. Apache-2.0 is compatible with this
   project's GPL-3.0; keep the upstream headers and notices, and distribute
   the combined firmware binary under GPL-3.0.