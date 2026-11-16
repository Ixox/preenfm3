# Better structure: Synth hosting sound engines

High-level plan for separating the sample-generating engine code from the
shared synth host. No implementation details here; the goal is to agree on the
shape of the refactoring first.

## Goal

- `Synth` becomes a host: shared voice pool, note routing, arpeggiator, mixer,
  FX, MIDI, presets, CPU meter.
- `Preenfm3` becomes one sound engine.
- `Msfa` becomes another sound engine.
- Both are independent of each other.
- A third engine only has to implement the engine interface and be registered.

## Current coupling

- `Synth` owns both `Timbre timbres_[6]` and `Voice voices_[16]`; the timbre and
  voice objects mix PFM3 and MSFA code.
- `Synth` includes `MsfaEngine.h` and calls `msfaGetVoiceState()` /
  `Voice::setMsfaState()` during init.
- `Synth::buildNewSampleBlock()` calls PFM3-specific
  `Timbre::prepareMatrixForNewBlock()` and `Timbre::glide()` for every timbre.
- `Timbre` is a hybrid: PFM3 `Osc`/`Env`/`Lfo`/`Matrix`, MSFA state and program,
  plus shared voice allocation, arpeggiator and FX.
- `Voice` is a hybrid: PFM3 DSP (`nextBlock`, `fxAfterBlock`, envelopes,
  matrix, oscillators) and MSFA state (`msfaState_`, `msfaNoteOn`,
  `msfaNoteOff`, `msfaNextBlock`, `msfaUpdateProgram`).
- `Timbre::engineNoteOn()` / `engineNoteOff()` / `voicesNextBlock()` branch on
  `SYNTH_ENGINE_PFM3` vs `SYNTH_ENGINE_MSFA`.
- `Timbre::init()` initializes both engines unconditionally.
- `Voice::noteOn()` / `noteOff()` / `killNow()` / `nextBlock()` contain MSFA
  checks and resets.
- `Timbre` keeps MSFA program/packed-patch arrays as static members.
- UI and persistence know about PFM3 vs MSFA (`isMsfaTimbre()`,
  `FMDisplayMsfaEditor`, preset layout).
- `OneSynthParams` is PFM3-specific but is also the shared timbre parameter
  block.

## Target structure

- `Synth` owns the shared 16-voice pool and the shared services.
- `Timbre` becomes a thin per-timbre host: one engine instance, mixer/FX state,
  arpeggiator, and an engine-specific preset blob.
- A common engine interface (working name `SynthEngine`) with at least:
  note on/off, block render, reset, program load, per-voice state hooks,
  parameter access.
- `Preenfm3Engine`: owns `Osc`, `Env`, `Lfo*`, `Matrix`, the algorithm render
  and the PFM3 per-voice state.
- `MsfaEngine`: owns the DX7 note/envelope/LFO/FmCore state and program.
- The 16 voices stay global. Each engine gets per-voice state keyed by the
  global voice index (or the shared voice holds an engine-specific slot).
- `Synth::buildNewSampleBlock()` talks only to the engine interface; no
  `SYNTH_ENGINE_*` branches.

## What moves where

PFM3-specific, to `Preenfm3Engine`:

- `Osc`, `Env`, `LfoOsc`, `LfoEnv`, `LfoEnv2`, `LfoStepSeq`, `Matrix`.
- `Voice::nextBlock()` PFM3 DSP, `fxAfterBlock()` PFM3 part,
  `updateAllMixOscsAndPans()`, `updateAllModulationIndexes()`.
- `Timbre::preenNoteOn()` / `preenNoteOff()`, `prepareMatrixForNewBlock()`,
  `glide()`, `pfm3VoicesNextBlock()`, `pfm3AfterNewParamsLoad()`.
- The PFM3-specific part of `OneSynthParams`.

MSFA-specific, to `MsfaEngine`:

- `MsfaEngine.*`, `Voice::msfa*`, `Timbre::msfa*`, `msfaProgram_`,
  `msfaPackedPatch_`, MSFA init currently done in `Synth::init()`.
- MSFA program/patch loading and the MSFA editor data.

Shared, stays in `Synth`/`Timbre`:

- 16-voice allocation and stealing, note on/off routing, hold pedal.
- Arpeggiator, MIDI clock, sequencer interaction.
- Per-timbre mixer state, volume/pan, compressor, FX1, FX2, FX bus.
- MIDI, presets, CPU meter, oscilloscope note info.

## Interface shape (high level)

- One engine instance per timbre, owned by `Synth` or `Timbre`.
- Calls are per note or per block, never per sample.
- Engines render into the existing shared voice/timbre buffers.
- No dynamic allocation in the audio path.
- Engine selection is a property of the timbre.

## Parameters, presets and UI

- Engine type per timbre in the mixer/preset data.
- Engine-specific parameters live with the engine, not in `OneSynthParams`.
- Keep the existing PFM3 preset layout compatible; MSFA keeps its own program
  blob.
- Define a generic engine-editor interface. `FMDisplayEditor` delegates to the
  active engine editor; `FMDisplayMsfaEditor` becomes one implementation.
- MIDI/NRPN paths that are currently PFM3-only should be routed by engine or
  explicitly documented as PFM3-only.

## Build and dependencies

- New files: engine interface, `Preenfm3Engine.*`; `MsfaEngine.*` already
  exists.
- Keep MSFA in its own translation units to avoid the `Env`/`Lfo` name
  collisions with PFM3.
- Use forward declarations/opaque state across the boundary.
- Watch RAM: DTCMRAM and RAM_D1 are already tight.

## Suggested migration order

1. Introduce the engine interface and a `Preenfm3Engine` wrapper around the
   current PFM3 code; no behaviour change.
2. Move PFM3 fields and methods out of `Timbre`/`Voice` into the wrapper.
3. Put MSFA behind the same interface; remove MSFA checks from
   `Voice`/`Timbre`.
4. Make `Synth` own engine instances and render through the interface; remove
   PFM3-specific calls from `Synth`.
5. Move engine-specific parameters, persistence and editor UI behind the
   interface.
6. Add a third engine to validate the structure.

## Risks

- Shared 16-voice pool and voice stealing.
- Real-time audio path (no per-sample virtual dispatch, no allocation).
- Preset compatibility.
- RAM/flash budget.
- MSFA/PFM3 header name collisions.
- UI and MIDI paths that currently branch on the engine.
