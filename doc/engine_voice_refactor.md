# Engine and voice architecture refactor

Status: design / not implemented.

This is the single migration plan for two refactorings that meet in the same
code:

1. **Engine separation** — `Synth` becomes the host; `Preenfm3` and `Msfa`
   become independent sound engines behind a `SynthEngine` interface.
2. **Dynamic voice allocation** — the static per-timbre voice reservation is
   replaced by one global 16-voice pool that timbres borrow on demand.

They are planned together because both rewrite the same seams:
`Timbre::preenNoteOn/Off`, `Timbre::voicesNextBlock`, `Timbre::voicesToTimbre`,
`Voice` binding, and the meaning of `numberOfVoices_` / `voiceNumber_`.

## 1. Goals

- Keep 16 global voices and 6 timbres.
- Stop permanently reserving voices per timbre; let any timbre borrow any free
  global voice.
- Keep PFM3 as the default engine and MSFA as an opt-in per-timbre engine.
- Make a third engine a matter of implementing the engine interface and being
  registered.
- Keep the audio path real-time safe: no dynamic allocation, no per-sample
  virtual dispatch.
- Preserve poly, mono, unison, MPE, arpeggiator, voice stealing, FX and the
  existing mixer/preset model.
- Keep the PFM3 preset layout compatible; MSFA keeps its own program blob.

## 2. Current architecture

### 2.1 Ownership

`Synth` owns the global arrays and the shared services:

- `Voice voices_[MAX_NUMBER_OF_VOICES]`
- `Timbre timbres_[NUMBER_OF_TIMBRES]`
- note routing, MIDI, presets, mixer, CPU meter, oscilloscope.

`Timbre` is a per-timbre hybrid. `Voice` is also a hybrid.

### 2.2 Static voice reservation

`Synth::rebuidVoiceAllTimbre()` walks the timbres and assigns contiguous global
voices according to `MixerInstrumentState::numberOfVoices`:

```cpp
int voiceNumber = 0;
for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
    int nv = mixerState.instrumentState_[t].numberOfVoices;
    for (int v = 0; v < nv; v++) {
        timbres_[t].setVoiceNumber(v, voiceNumber++);
    }
    for (int v = nv; v < MAX_NUMBER_OF_VOICES; v++) {
        timbres_[t].setVoiceNumber(v, -1);
    }
}
```

`Timbre::setVoiceNumber()` stores the mapping and calls
`Voice::setCurrentTimbre(this)`. A timbre only scans its own reserved slots, and
the sum of all per-timbre counts cannot exceed 16.

### 2.3 Engine dispatch

`Timbre` is the per-timbre dispatcher:

- `engineNoteOn()` / `engineNoteOff()` choose `preenNoteOn/Off()` or
  `msfaNoteOn/Off()`.
- `voicesNextBlock()` chooses `pfm3VoicesNextBlock()` or
  `msfaVoicesNextBlock()`.
- `afterNewParamsLoad()` chooses `pfm3AfterNewParamsLoad()` or
  `msfaAfterNewParamsLoad()`.
- `setEngine()` switches `engine_`.
- `prepareMatrixForNewBlock()`, `glide()`, `resetMatrixDestination()`,
  `setMatrixSource*()`, `setMatrixPolyAfterTouch()`, `fxAfterBlock()` and the
  MPE note paths branch on MSFA.

`Voice` is not engine-agnostic either: it contains both PFM3 DSP and MSFA state,
and some `Voice` methods test `currentTimbre->getEngine()`.

### 2.4 `Timbre` inventory

PFM3-only state:

- `Osc osc1_..osc6_`
- `Env env1_..env6_`
- `lfoUSed_`
- `recomputeNext_`
- `currentGate_`
- `unisonPhase[]` (static)
- `lowerNoteFrequency` (`lowerNote_` / `lowerNoteReleased_` are shared with
  MSFA)

PFM3-only methods:

- `preenNoteOn()`, `preenNoteOnUpdateMatrix()`, `preenNoteOff()`
- `prepareMatrixForNewBlock()`, `glide()`
- `pfm3VoicesNextBlock()`, `pfm3AfterNewParamsLoad()`
- `lfoValueChange()`
- `updateMidiNoteScale()`
- `resetMatrixDestination()`, `setMatrixSource()`, `setMatrixSourceMPE()`,
  `setMatrixPolyAfterTouch()`
- `verifyLfoUsed()`, `isLfoUsed()`
- `isUnisonMode()`
- `midiClockContinue/Start/SongPositionStep()` (they forward to PFM3 `Voice`
  LFO clocking)
- `gateFx()` and the PFM3 branch of `fxAfterBlock()`
- the PFM3 branches of `noteOnMPE()` / `noteOffMPE()`
- the unison paths in `preenNoteOn()` / `preenNoteOff()` / `voicesToTimbre()`

MSFA-only state and methods:

- `msfaState_`, `msfaPackedPatch_[]`, `msfaProgram_[]`
- `loadMsfaPackedPatch()`, `loadMsfaProgram()`, `getMsfaProgram*()`
- `msfaNoteOn()`, `msfaNoteOff()`, `msfaVoicesNextBlock()`,
  `msfaAfterNewParamsLoad()`

Shared (stays with the timbre host):

- `voices_`, `voiceNumber_`, `numberOfVoices_`, `numberOfVoiceInverse_`,
  `holdPedal_`, `lastPlayedNote_`
- `sampleBlock_`, `sbMax_`, `mixerGain_`, `mixerState_`, `timbreNumber_`,
  `mpeSetting_`
- arpeggiator and clock fields, MIDI note stack
- FX state: delay buffers, filters, granulator
- shared parameters: `engineArp*`, `effect1/2`, preset name

Important: the PFM3 `Lfo*` and `Matrix` instances live in `Voice`
(`Voice::lfoOsc[]`, `lfoEnv[]`, `lfoEnv2[]`, `lfoStepSeq[]`, `matrix`), not in
`Timbre`. `Timbre` only has the PFM3 `Osc`/`Env` objects and the matrix plumbing
methods.

### 2.5 `Voice` inventory

PFM3-only:

- `matrix`
- `lfoOsc[]`, `lfoEnv[]`, `lfoEnv2[]`, `lfoStepSeq[]`
- oscillator/envelope state and the PFM3 `nextBlock()` DSP
- `updateAllMixOscsAndPans()`, `updateAllModulationIndexes()`
- `prepareMatrixForNewBlock()`, `fxAfterBlock()` PFM3 part

MSFA-only:

- `msfaState_`, `msfaNoteOn()`, `msfaNoteOff()`, `msfaNextBlock()`,
  `msfaUpdateProgram()`

Shared:

- note/velocity/glide/steal state, `currentTimbre`, output buffer, effect state.

## 3. Target architecture

### 3.1 Host and engines

```
Synth (host)
  ├── VoicePool (16 global voices)
  ├── shared services: MIDI, arpeggiator, mixer, FX, presets, CPU meter
  └── Timbre[6]
        ├── SynthEngine* engine  (Preenfm3Engine or MsfaEngine or future)
        ├── shared timbre state (mixer, FX, arp, active voice list)
        └── engine-owned timbre context
```

- `Synth` owns the 16 `Voice` objects and the `VoicePool`.
- `Timbre` owns one `SynthEngine` instance and the shared timbre state.
- The allocator is engine-agnostic.
- The engine owns its per-timbre context and its per-voice state.
- Engine selection remains a property of the timbre.

### 3.2 `SynthEngine` interface

The interface is called per note or per block, never per sample. Sketch:

```cpp
class SynthEngine {
public:
    virtual ~SynthEngine() {}

    virtual void init(Timbre *timbre) = 0;
    virtual void afterNewParamsLoad() = 0;

    virtual void bindVoice(Voice *voice) = 0;
    virtual void unbindVoice(Voice *voice) = 0;

    virtual void noteOn(Voice *voice, char note, char velocity) = 0;
    virtual void noteOff(Voice *voice, char note) = 0;
    virtual void killVoice(Voice *voice) = 0;
    virtual void noteOffQuick(Voice *voice) = 0;

    virtual void renderVoice(Voice *voice) = 0;
    virtual void renderTimbre(Timbre *timbre) = 0;

    virtual void setController(SourceEnum source, float value) = 0;
    virtual void setControllerMPE(int voiceIndex, SourceEnum source, float value) = 0;

    virtual const void *context() const = 0;
};
```

`Preenfm3Engine` owns the PFM3 DSP objects (`Osc`, `Env`, `Lfo*`, `Matrix`
pointers/context, PFM3 per-voice state) and implements the PFM3 render path.
`MsfaSynthEngine` implements the interface as a wrapper around the existing
`MsfaEngine.*` free-function API; `MsfaEngine.*` stays the low-level MSFA API.

The virtual calls are per note and per voice per block, not per sample.

### 3.3 Engine-owned timbre context

Each engine owns its per-timbre context, recomputed when parameters change:

```cpp
struct Pfm3TimbreContext {
    MatrixRowParams *matrixRows;
    LfoParams *lfoOscParams[3];
    float *lfoPhases[3];
    EnvelopeLfoParams *lfoEnv;
    Envelope2LfoParams *lfoEnv2;
    StepSequencerParams *lfoStepSeqParams[2];
    StepSequencerSteps *lfoStepSeqSteps[2];
    const AlgoInformation *algoInfo;
    const int *algoOpInfo;
    int numberOfIMs;
    int numberOfMix;
    float panLeft[6];
    float panRight[6];
    float mixerGain;
};
```

`MsfaTimbreContext` holds the MSFA program, operator on/off mask, algorithm and
feedback, and any precomputed values.

The context is invalidated/recomputed from `afterNewParamsLoad()`, `setNewValue()`
and the relevant parameter changes. It must not be rebuilt per voice.

### 3.4 Typed per-voice state

Per-voice engine state is split into separate types, not a union:

```cpp
struct Pfm3VoiceState {
    Matrix matrix;
    LfoOsc lfoOsc[NUMBER_OF_LFO_OSC];
    LfoEnv lfoEnv[NUMBER_OF_LFO_ENV];
    LfoEnv2 lfoEnv2[NUMBER_OF_LFO_ENV2];
    LfoStepSeq lfoStepSeq[NUMBER_OF_LFO_STEP];
    OscState oscState[6];
    EnvData envState[6];
};

struct MsfaVoiceState {
    // Dx7Note + MSFA per-voice state
};
```

Implementation note: `Pfm3VoiceState` lives in `Pfm3VoiceState.h` and is a
member of `Voice` (`pfm3State_`). The per-voice filter/effect state stays in
`Voice` because `fxAfterBlock()` is shared with MSFA. `Voice::engine` is not
added yet; binding goes through `currentTimbre` until step 4.

Storage differs by engine because of RAM:

- `Pfm3VoiceState` stays inline in `Voice` (DTCMRAM), as the PFM3 DSP state is
  today.
- `MsfaVoiceState` stays in the existing dedicated RAM_D1 arrays
  (`msfaVoiceStates`, see `MsfaEngine.h`). It is large and must not be inlined
  into `Voice`.

`Voice` therefore holds a typed pointer/index, not both full states:

```cpp
class Voice {
    // shared
    Timbre *currentTimbre;
    SynthEngine *engine;
    Pfm3VoiceState pfm3State;      // inline, as today
    MsfaVoiceState *msfaState_;    // pointer into the RAM_D1 array, as today
    // note/glide/steal state, output buffer
};
```

The engine only touches its own typed state. The two states do not overlap and
are not aliased.

### 3.5 Voice binding

Binding a voice to a timbre means:

1. store `currentTimbre` and `engine`;
2. point the engine at its typed voice state;
3. bind the engine's timbre context (matrix row pointer, LFO params/phase
   pointers, pan/gain snapshot);
4. reset per-voice state (filters, oscillator/envelope/LFO state).

`Voice::setCurrentTimbre()` is replaced by a `bindTimbre()` that delegates to the
engine's `bindVoice()`. The expensive work is precomputed in the engine context,
so binding is cheap enough for the note-on path.

### 3.6 `VoicePool` and active lists

`VoicePool` is owned by `Synth` and shared by all timbres:

```cpp
class VoicePool {
public:
    int allocate(uint8_t timbre, uint8_t note, uint8_t velocity);
    void release(int voiceIndex);
    void releaseAll(uint8_t timbre);
    int owner(int voiceIndex) const;
private:
    int8_t owner_[MAX_NUMBER_OF_VOICES];   // -1 = free
    uint32_t startIndex_[MAX_NUMBER_OF_VOICES];
};
```

Each `Timbre` keeps:

- `activeVoices_[MAX_NUMBER_OF_VOICES]` — global indices currently owned;
- `activeVoiceCount_`;
- `maxVoices_` — configured per-timbre maximum from the mixer.

`voiceNumber_` is replaced by `activeVoices_` to make the new meaning explicit.
`numberOfVoices_` is split into `maxVoices_` (configured maximum) and
`activeVoiceCount_` (currently playing).

`Synth::rebuidVoiceAllTimbre()`, `Synth::getFreeVoice()` and
`Synth::getNumberOfFreeVoicesForThisTimbre()` are removed.

### 3.7 Mixer semantics

Option A: `numberOfVoices` is a per-timbre maximum, not a reservation.

- Every timbre can be set 0..16 independently; the sum of maxima may exceed 16.
- Only the global active pool is limited to 16.
- A timbre playing alone with max 16 can use all 16 voices.
- When other timbres play, they share the pool and steal as needed.
- The mixer UI stops subtracting the other timbres' counts.
- Lowering the maximum below the active count kills the excess voices.
- No minimum guaranteed voice count is reserved.

Old mixer banks load with their `numberOfVoices` values reinterpreted as maxima.
The preset format does not need to change; bump the mixer bank version only if a
migration must distinguish the old semantics.

### 3.8 Stealing and deferred rebinding

Stealing order:

1. same-note retrigger within the timbre;
2. free voice in the global pool (`owner == -1`);
3. idle voice in the global pool (owned by any timbre but not playing);
4. oldest released voice in the global pool;
5. oldest playing voice owned by **another** timbre (only while below this
   timbre's maximum, so the note grows the timbre);
6. oldest playing voice owned by the same timbre;
7. oldest playing voice in the global pool.

Steps 2–5 only run while the timbre is below its `maxVoices_`, so stealing
never raises the active count above the configured maximum. Step 5 comes before
step 6 so a timbre can still grow to its maximum when every voice is held by
other timbres; at the maximum it reuses its own oldest playing voice first.

Idle voices (envelope finished, still owned by a timbre) are reclaimable
directly: the old timbre drops them from its active list and the pool reassigns
them. Released and playing voices come after idle ones so silent voices are
reused before a tail is truncated or a held note is cut.

Cross-timbre steal must use the anti-click path:

- do not call `bindTimbre()` at steal time;
- keep the voice in the old timbre's active list;
- call `Voice::noteOnWithoutPop()` so the quick release uses the old envelopes;
- store the target timbre in `Voice::pendingTimbre_`;
- let the old timbre keep rendering until the quick release finishes;
- do not update the new note's matrix sources before the quick release finishes.

When the quick release finishes, `endNoteOrBeginNextOne()` marks the voice
transfer-ready instead of starting the note. `Synth::processPendingTransfers()`
runs at the end of the audio block, after the mixing loop, and for each ready
voice drops it from the old timbre, claims it for the new one, binds the new
engine/context, writes the matrix sources and starts the pending note. The note
then renders from the next block. Transferring inside `endNoteOrBeginNextOne()`
would render the voice twice in the same Synth block when the target timbre has
not rendered yet, and would mutate the old timbre's list during iteration.

Pending voices are not yet in the target's active list, so `allocateVoice()`
counts them when checking `maxVoices_` (by scanning the pool). Idle voices are
reused immediately (no tail to protect). MSFA targets and cross-engine steals
keep the immediate path because MSFA has no quick-release mechanism.

If ownership or binding moves before the quick release completes, the old tail
is cut off or decays through the new timbre's envelopes (click / wrong sound).

### 3.9 MPE

MPE stays instrument-1 only.

- Replace `voiceNumber_[channel - 1]` with an explicit `mpeChannelVoice_[16]`
  map from member channel to global voice index.
- Allocate on the first note for a channel and keep the mapping until
  note-off/channel release; clear/update it if the voice is stolen.
- The editable Mixer > MIDI value is the last member channel `X`, clamped to
  `[1, min(16, 1 + maxVoices_)]`.
- For the first dynamic-allocator implementation, derive
  `X = min(16, 1 + maxVoices_)`; persisting an editable `X`
  (`MixerInstrumentState::mpeLastChannel`) is deferred to a later mixer-bank
  version.
- Ignore member channels above `X` for note-on/off, pitch bend, aftertouch and
  CC74.
- Lowering `X` kills voices above it immediately with `Voice::killNow()`.

### 3.10 Unison

`PLAY_MODE_UNISON` remains the mode; the width is `min(maxVoices_, 7)`.

- `allocateUnison()` claims exactly that width atomically; active voices above
  it are trimmed first.
- If the full width cannot be claimed, skip the note instead of playing a
  narrower group.
- `numberOfVoiceInverse_` is `1 / width` while the group is active.
- `voicesNextBlock()` accumulates the active group into voice 0;
  `voicesToTimbre()` copies only voice 0.
- Lowering `maxVoices_` below the active group kills the excess voices.

No new `Unis*` play-mode values and no preset change; the width comes from the
mixer `numberOfVoices` setting, capped at 7.

## 4. What moves where

PFM3-specific, to `Preenfm3Engine`:

- `Timbre::osc1_..osc6_`, `Timbre::env1_..env6_`.
- PFM3 per-voice DSP: `Voice::matrix`, `lfoOsc[]`, `lfoEnv[]`, `lfoEnv2[]`,
  `lfoStepSeq[]`, oscillator/envelope state, `nextBlock()` PFM3 path,
  `fxAfterBlock()` PFM3 part, `updateAllMixOscsAndPans()`,
  `updateAllModulationIndexes()`.
- `Timbre::preenNoteOn()` / `preenNoteOnUpdateMatrix()` / `preenNoteOff()`,
  `prepareMatrixForNewBlock()`, `glide()`, `pfm3VoicesNextBlock()`,
  `pfm3AfterNewParamsLoad()`.
- `Timbre::lfoValueChange()`, `updateMidiNoteScale()`,
  `resetMatrixDestination()`, `setMatrixSource()`, `setMatrixSourceMPE()`,
  `setMatrixPolyAfterTouch()`, `verifyLfoUsed()`, `isLfoUsed()`.
- `Timbre::isUnisonMode()`, `unisonPhase[]`, and the unison paths in
  `preenNoteOn()` / `preenNoteOff()` / `voicesToTimbre()`.
- `Timbre::midiClockContinue/Start/SongPositionStep()`.
- `Timbre::gateFx()` and the PFM3 branch of `fxAfterBlock()`.
- The PFM3 branches of `Timbre::noteOnMPE()` / `noteOffMPE()`.
- PFM3-only `Timbre` state: `lfoUSed_`, `recomputeNext_`, `currentGate_`,
  `lowerNoteFrequency`.
- The PFM3-specific part of the parameter block.

MSFA-specific, to `MsfaEngine`:

- `MsfaEngine.*`, `Voice::msfa*`, `Timbre::msfa*`, `msfaProgram_`,
  `msfaPackedPatch_`, MSFA init currently done in `Synth::init()`.
- MSFA program/patch loading and the MSFA editor data.

Shared, stays in `Synth`/`Timbre`:

- 16-voice allocation and stealing, note on/off routing, hold pedal.
- Arpeggiator, MIDI clock, sequencer interaction.
- Per-timbre mixer state, volume/pan, compressor, FX1, FX2, FX bus.
- MIDI, presets, CPU meter, oscilloscope note info.
- `Timbre::lowerNote_` / `lowerNoteReleased_` (shared with MSFA);
  `lowerNoteFrequency` is PFM3-only.
- Shared parameters: arp, FX, preset name.
- PFM3 parameters: `performance1` (exposed as the Perf1..Perf4 matrix
  sources), `engine1/2`, `engineIm*`, `engineMix*`, oscillators, envelopes,
  matrix rows, LFOs, step sequencers and MIDI note curves.

## 5. Parameters, presets and UI

- Keep the PFM3 preset byte layout compatible.
- During steps 1–5, `Timbre::params_` stays where it is; `Preenfm3Engine` holds
  a reference/pointer to it. The parameter split happens in step 6.
- Split the parameter block conceptually into shared timbre parameters (arp, FX,
  preset name) and PFM3 DSP parameters owned by `Preenfm3Engine` (including
  `performance1`, which feeds the Perf1..Perf4 matrix sources).
- MSFA keeps its own program blob.
- Engine-specific parameters live with the engine, not in the shared block.
- A generic engine-editor interface already exists (`EngineEditor`); the engine
  separation does not change the editor split, only where parameters live.
- MIDI/NRPN paths that are currently PFM3-only should be routed by engine or
  documented as PFM3-only.

## 6. Migration order

The two refactorings are interleaved so `Voice` binding is written once.

1. **Introduce the `SynthEngine` interface and `Preenfm3Engine` wrapper.**
   Keep the current static allocation and the current `Voice` code. No
   behaviour change.

2. **Add engine-owned timbre contexts and typed per-voice state.**
   `Preenfm3Engine` owns `Pfm3TimbreContext`; `Voice` gets `Pfm3VoiceState` and
   `MsfaVoiceState`; `Voice::setCurrentTimbre()` becomes `bindTimbre()` through
   the engine. Verify no audio change with static allocation.

3. **Add `VoicePool` and active lists.**
   Implement the global pool and `maxVoices_` / `activeVoices_`. Update
   `preenNoteOn/Off`, `msfaNoteOn/Off`, render loops, MPE, unison and the mixer
   value path. Remove `rebuidVoiceAllTimbre()`, `getFreeVoice()` and
   `getNumberOfFreeVoicesForThisTimbre()`.

4. **Put both engines' state behind the interface.**
   First move the PFM3 timbre DSP objects (`Timbre::osc1_..osc6_`,
   `Timbre::env1_..env6_`) and their initialization into `Preenfm3Engine`.
   Then move MSFA state and methods into `MsfaEngine`, remove the MSFA branches
   from `Timbre`/`Voice`, and use the same allocator/binding path.

5. **Remove the static reservation and engine branches.**
   Delete the fixed `voiceNumber_` mapping and the remaining
   `SYNTH_ENGINE_*` branches in the render/note paths. No compatibility mode or
   fallback.

6. **Move parameters, persistence and editor data behind the interface.**
   Keep the PFM3 preset layout compatible; route engine-specific parameter
   access through the engine.

   - **6a — Classify parameters.** Shared (`Timbre`): `engineArp1/2`,
     `engineArpUserPatterns`, `effect1`, `effect2`, `presetName`. PFM3
     (`Preenfm3Engine`): `engine1/2`, `engineIm1..3`, `engineMix1..3`,
     `osc1..6`, `env1..6` time/level/curve, `matrixRowState1..12`,
     `lfoOsc1..3`, `lfoPhases`, `lfoEnv1/2`, `lfoSeq1/2`, `lfoSteps1/2`,
     `midiNote1/2Curve`, and `performance1` (the Perf1..Perf4 matrix
     sources). MSFA keeps its own `program_[156]` blob.

   - **6b — Split the runtime storage.** Add `TimbreSharedParams` in `Timbre`
     and `Pfm3TimbreParams` in `Preenfm3Engine`. Keep `OneSynthParams` as the
     on-flash layout **and** as the in-memory UI/serialization mirror: the
     editor writes it directly, and `convertParamsToFlash/FromFlash`,
     `backupParams`, `copyNewPreset` and the preset constants all operate on
     the flat buffer. Add `serialize()` / `deserialize()` and
     `getParamValue(flatIndex)` / `setParamValue(flatIndex, value)`.

   - **6c — Route editor and MIDI access.** The editor and MIDI keep reading
     and writing the flat mirror. `setParamValue()` updates the mirror and the
     real owner (`TimbreSharedParams` or `Pfm3TimbreParams`). Shared rows
     (Arp/Fx) stay on `Timbre`; PFM3 rows go to `Preenfm3Engine`. The MSFA
     editor keeps writing its own program blob. Ignore the PFM3 CC/NRPN maps
     for MSFA timbres.

   - **6d — Update persistence.** `PatchBank`, `MixerBank` and `SynthState`
     keep using `OneSynthParams`; load calls `deserialize()`, save calls
     `serialize()`. No PFM3 flash-format change; the MSFA preset/mixer layout
     is unchanged.

   - **6e — Update the PFM3 engine context.** `initOscEnv()` and
     `refreshContext()` read `Pfm3TimbreParams`.
     `Timbre::afterNewParamsLoad()` resets shared and engine state.

   - **6f — Test.** PFM3 preset/mixer round-trip, randomizer, editor on both
     engines, MIDI CC/NRPN, MSFA patch load, cross-engine switching.

   The flat mirror is kept during step 6 to avoid rewriting all flat accesses;
   it is a UI/serialization cache, not the DSP owner.

7. **Move the per-voice DSP behind the engine.** `Preenfm3Engine::renderVoice()`
   owns the PFM3 sample calculation (formerly `Voice::nextBlock()`) and
   `updateAllMixOscsAndPans()`; `MsfaSynthEngine::renderVoice()` owns the MSFA
   path (formerly `Voice::msfaNextBlock()`). `Voice::fxAfterBlock()` stays
   shared: FX1 is per voice but common to both engines.

8. **Add a third engine to validate the structure.**

## 7. Test plan

- 6 timbres playing at once with more than 16 simultaneous notes.
- One timbre holds all 16 voices; another timbre must be able to steal the
  oldest voice. No permanent starvation.
- Several timbres set to max 16; only the active count stays at or below 16.
- No minimum reservation: a timbre may be fully stolen; no voice leak.
- Old mixer banks load with `numberOfVoices` as maxima.
- Mixed engines: PFM3 and MSFA timbres competing for the same 16 voices.
- Poly, mono, unison, glide, hold pedal.
- Unison width equals `min(maxVoices_, 7)` (up to 7); insufficient voices follow the
  steal/skip policy; `numberOfVoiceInverse_` matches the width.
- Lowering `maxVoices_` while a unison note plays kills the excess voices.
- Unison width persistence through mixer bank save/load.
- MPE channels, note-off, channel reuse, and editing the last-member-channel
  limit `X` while MPE is active.
- Raising/lowering `X`: channels above `X` ignored for note-on/off, pitch bend,
  aftertouch and CC74; `X` persists.
- Lowering `X` while notes are active above `X` kills those voices.
- Changing `maxVoices_` while MPE notes are active: clamp `X`, rebuild/clamp
  the channel map, no orphaned notes or two channels on one voice.
- Arpeggiator patterns, latch and hold.
- Voice stealing: same timbre, cross-timbre, released voices, playing voices.
- Cross-timbre steal ordering: the quick release is rendered entirely by the
  old timbre; ownership/binding changes only in
  `Synth::processPendingTransfers()` at block end.
- No clicks/pops and no wrong-timbre tail when a voice is rebound.
- Note-off during the deferred quick release for both the old and pending note.
- Mixer save/load, preset load, boot defaults.
- Long soak test with random note storms across all six timbres.
- Both engines on every editor page (see `doc/screen_separation.md`).

## 8. Risks

- Shared 16-voice pool and voice stealing.
- Real-time audio path: no per-sample virtual dispatch, no allocation.
- `Voice` RAM: keep the two typed states lean; DTCMRAM and RAM_D1 are tight.
- Preset compatibility, especially the PFM3 parameter layout.
- MSFA/PFM3 header name collisions (`Env`, `Lfo`); keep MSFA in its own
  translation units and use opaque state across the boundary.
- UI and MIDI paths that currently branch on the engine.
- Cross-timbre steal ordering must be exact; a mistake produces clicks or a
  wrong-timbre tail.

## 9. Decisions

1. Engine context is engine-owned (`Pfm3TimbreContext` / `MsfaTimbreContext`),
   not a generic `Timbre` struct.
2. Per-voice engine state is split into separate types
   (`Pfm3VoiceState`, `MsfaVoiceState`), not a union.
3. Mixer semantics: Option A — `numberOfVoices` is a per-timbre maximum,
   0..16 independently; the sum of maxima may exceed 16.
4. Stealing order: same-note retrigger > global free > global idle > oldest
   globally released > oldest playing other timbre (while below max) > oldest
   playing same timbre > oldest playing globally.
5. Unison width is `min(maxVoices_, 7)`; skip the note if the full width cannot
   be claimed.
6. No minimum guaranteed voice count.
7. `VoicePool` is a separate class owned by `Synth`.
8. Remove `rebuidVoiceAllTimbre()` and the fixed `voiceNumber_` mapping
   directly; no compatibility mode, fallback or development flag.
9. Cross-timbre steal keeps the old timbre for the quick release; rebinding and
   ownership transfer happen at the end of the audio block in
   `Synth::processPendingTransfers()`.
10. MPE `X` is the editable last member channel, clamped to
    `min(16, 1 + maxVoices_)`; lowering `X` kills voices above it immediately.
11. `MsfaSynthEngine` is a class wrapper over the existing `MsfaEngine.*`
    free-function API; `Preenfm3Engine` is new.
12. Per-block orchestration uses engine hooks
    (`prepareBlock` / `renderVoices` / `postRender`); `voicesToTimbre` and FX
    stay shared; `gateFx` moves to `Preenfm3Engine` (MSFA no-op).
13. Persisting the editable MPE last-member channel is deferred; derive
    `X = min(16, 1 + maxVoices_)` initially.
14. `Timbre` owns one `SynthEngine` instance; selection is per timbre.
15. `Timbre::params_` stays in `Timbre` during steps 1–5; the PFM3 parameter
    split happens in step 6.
16. MSFA note-on/off uses the same `VoicePool` from step 3.
17. Keep a flat `OneSynthParams` mirror in `Timbre` for the editor, storage and
    MIDI; the DSP owners are `TimbreSharedParams` / `Pfm3TimbreParams`.