# Audio path architecture

This document describes the classes involved in producing audio and their
responsibilities: global synth, per-part timbre, per-voice state and the
pluggable sound engines.

## Ownership tree

```
Synth                                   global audio engine + transport
├── Voice voices_[20]                   neutral per-voice state + FX1 + sample buffer
├── VoicePool                           "which timbre owns each voice" bookkeeping
└── Timbre timbres_[6]                  one synth part / instrument
    ├── TimbreSharedParams sharedParams_[6]  Arp + FX1 + FX2 + preset name (engine-neutral)
    ├── SynthEngine *engineImpl_        selected engine
    │   └── SynthEngineFactory          per-timbre engine instances
    │       ├── Preenfm3Engine [6]      PFM3 engine
    │       │   ├── Pfm3Preset [6]           flat preset+UI mirror
    │       │   ├── Pfm3TimbreParams [6]     static, indexed by timbre
    │       │   ├── Pfm3TimbreContext [6]    static, indexed by timbre
    │       │   ├── Osc osc_[6] / Env env_[6]
    │       │   └── Pfm3VoiceState [20]      static, indexed by voice slot
    │       └── Dx7SynthEngine [6]     DX7/DX7 engine
    │           ├── program_[6][156] / packedPatch_[6]
    │           ├── Dx7TimbreState [6]      static, indexed by timbre
    │           └── Dx7VoiceState [20]      static, indexed by voice slot
```

The 20 voices form a single global pool shared by all six timbres. A voice is
only ever rendered by the engine of the timbre that currently owns it.

The per-timbre and per-voice engine arrays (`Pfm3TimbreParams`,
`Pfm3TimbreContext`, `Pfm3VoiceState`, `Dx7TimbreState`, `Dx7VoiceState`) are
static and indexed by timbre number or global voice slot, so they are shared by
the four engine instances rather than duplicated per instance.

## Layers

### Global

- **`Synth`** owns the voice array, the four timbres and the `VoicePool`.
  It receives note/MIDI events per timbre, runs the block loop
  (`buildNewSampleBlock`), sums timbre outputs, applies pan/volume and the
  output compressor, and handles MIDI clock and global all-notes-off.
- **`VoicePool`** tracks which timbre claimed each global voice. It provides
  `findFree`, `findOldestReleased`, `findOldestPlaying`, `claim` and
  `release`; it contains no DSP and no allocation policy.
- **`SynthState` / `MixerState`** are the application/mixer state containers:
  UI and storage hooks, MIDI channel routing, per-instrument volume/pan,
  scale-frequency tables and the FX bus.

### Per part

- **`Timbre`** represents one synth part. It selects the engine from
  `SynthEngineFactory` (`engineImpl_`), owns the list of voices it currently uses, performs voice
  allocation, stealing and deferred transfer, handles group note-off/kill for
  its voices, runs the arpeggiator, applies GateFX and FX2, holds
  `TimbreSharedParams`, and dispatches preset load/storage to its engine.

### Per voice

- **`Voice`** is the engine-neutral note container: note, velocity, frequency,
  playing/released/held flags, transfer state (`pendingTimbre_`,
  `transferReady_`), the output `sampleBlock`, and the shared per-voice FX1
  state. It contains no engine DSP state or engine types.

### Engines

- **`SynthEngineFactory`** owns the static per-timbre PFM3 and DX7 engine
  instances and returns the one selected by `SynthEngineType`.
- **`SynthEngine`** is the abstract per-timbre contract: note on/off, block
  rendering, matrix sources, glide, FX matrix preparation, LFO/clock handling,
  voice lifecycle hooks (`killVoice`, `noteOffVoice`, `noteOffVoiceQuick`,
  `startVoiceAfterTransfer`, `completeVoiceTransfer`), persistence (magic +
  blob) and optional editor memory (`getPatchMemory()`, `patchMemoryChanged()`)
  plus the DX7 import (`loadDx7Patch()`). Engines implement only what they use;
  most methods default to no-ops.
- **`Preenfm3Engine`** implements the PFM3 DSP: `Pfm3TimbreParams`,
  oscillators, envelopes, per-voice `Pfm3VoiceState` (matrix, LFOs, envelope
  and oscillator state), note lifecycle, block rendering and PFM3 preset
  serialization.
- **`Dx7SynthEngine`** adapts the vendored DX7/DX7 C code: it owns the
  156-byte program and packed patch, and drives the static `Dx7TimbreState` /
  `Dx7VoiceState` through the `Dx7Engine` adapter, including controller
  handling, note start/key-up, per-voice rendering and DX7 preset
  serialization. It has no unison and no matrix.

### Engine data

- **`Pfm3TimbreParams`** is the actual PFM3 DSP parameter block (operators,
  envelopes, algorithm, matrix rows, LFOs, MIDI curves, ...). `Pfm3Preset`
  remains the flat on-flash/UI layout.
- **`Pfm3TimbreContext`** is a per-timbre resolved view of
  `Pfm3TimbreParams`: pointers to matrix rows, LFO params/phases and
  step-sequencer tables, plus precomputed pan L/R and mixer gain. It is built
  by `initContext()` and refreshed on preset load (`refreshContext()`), so
  binding a voice and rendering do not repeatedly dereference the flat layout.
- **`Pfm3VoiceState`** is all mutable PFM3 state for one global voice slot:
  six `OscState`, six `EnvData`, `Matrix`, LFO objects, pan/mix values,
  modulation indexes, feedback memory, and the note-transition/glide state
  (`newNotePending`, `pendingNote*`, `phase_`, `gliding`, `nextGlidingNote*`,
  `nextMainFrequency`, `noteAlreadyFinished`).
- **`Dx7TimbreState` / `Dx7VoiceState`** are opaque C structs from the
  vendored DX7 code: timbre-level algorithm/controllers and per-voice DX7
  operator/envelope state. They are accessed through `getDx7TimbreState()`
  and `getDx7VoiceState()`.
- **`TimbreSharedParams`** holds engine-independent part data: arpeggiator,
  FX1, FX2 and preset name. It is used by both engines and by storage.

Supporting DSP primitives (`Osc`, `Env`, `Matrix`, `Lfo*`) are implementation
details owned by `Preenfm3Engine`.

## Data flow

### Note on

`Synth::noteOn(timbre, ...)` -> `Timbre::noteOn` -> `engineImpl_->noteOn` ->
`Timbre::allocateVoice` / `reuseVoice` (may steal a voice from another timbre)
-> engine binds/resets its per-voice state and starts the note on `Voice`.

On a non-deferred steal, `Timbre::reuseVoice` calls
`oldTimbre->engineImpl_->killVoice(voice)` before rebinding, so each engine
only resets its own state. PFM3-to-PFM3 steals can be deferred: the voice keeps
its old timbre until its release tail finishes, then completes the transfer.

### Render

`Synth::buildNewSampleBlock` -> for each timbre: arpeggiator tick, `glide`,
`prepareMatrixForNewBlock` (engine hook `prepareBlock`), then `voicesNextBlock`
-> engine renders each voice
into its `sampleBlock` and applies per-voice FX1 -> timbre sums its voices,
applies GateFX and FX2 -> `Synth` mixes the four parts into the three output
buses and compresses.

### Note off / stop

`Synth` and `Timbre` dispatch through the engine interface
(`noteOffVoice`, `noteOffVoiceQuick`, `killVoice`) rather than calling engine
internals directly.

Engine-specific editor changes use
`SynthState::propagateNewEngineParamValue()` → `Synth::newEngineParamValue()` →
`Timbre::engineParamChanged()` → the active engine. PFM3 uses its flat
`paramChanged()` path; DX7 maps the parameter ID to the DX7 program byte and
updates only the affected operator/global state.

## Effects placement

- **FX1**: per voice, in `Voice::fxAfterBlock()`. Shared by both engines; the
  engine passes the matrix-derived filter values (PFM3) or zero (DX7).
- **GateFX / FX2**: per timbre, in `Timbre::gateFx()` / `Timbre::applyTimbreFx()`.
- **Mix, pan, compressor**: global, in `Synth`.

## Persistence

Persistence is engine-owned. `SynthEngine` exposes `getStorageMagic()`,
`getStorageNameOffset()`, `getStorageDefaultName()`, `getStorageBlob()` and
`loadStorageBlob()`. A slot is `ALIGNED_PATCH_SIZE` bytes: engine blob followed
by a 4-byte magic. `TimbreSharedParams` can be included in the blob by engines
that need it. PFM3 serializes the legacy `FlashSynthParams` layout; DX7 stores
the DX7 program, name and shared parameters.

## Separation of concerns

The main boundaries are clear:

- `Synth` handles global/transport/mixing, not per-part DSP.
- `Timbre` handles one part: voice ownership, allocation/steal, arp,
  GateFX/FX2.
- `Voice` is a neutral note container plus shared per-voice FX1.
- `Preenfm3Engine` and `Dx7SynthEngine` own all DSP, parameters and
  per-voice engine state.
- `VoicePool` is pure ownership bookkeeping.
- Persistence is engine-owned behind `SynthEngine`.

Remaining coupling is localized:

The flat `Pfm3Preset` mirror now lives in `Preenfm3Engine`. It is kept
explicitly as an editor/storage cache; the PFM3 DSP state is owned by
`Pfm3TimbreParams`, and `Timbre` only owns the shared Arp/FX/name block.

Overall the separation is well respected at the important boundaries: global
vs part vs voice vs engine, and DSP vs storage vs UI. A new engine can be
implemented almost entirely behind `SynthEngine`. The remaining coupling is
limited to that explicit flat mirror, which is a deliberate UI/serialization
cache. Step 9 (the third engine) is tracked in
`doc/engine_voice_refactor.md`.
