# MSFA — DX7 engine

The MSFA engine is preenFM3's Yamaha DX7 synthesis engine. It is selected per
timbre in the mixer; PFM3 remains the default engine.

It uses Dexed's vendored subset of **MSFA** (Music Synthesizer for Android) and
its `FmCore` implementation of the 32 DX7 algorithms. All operators are sine;
only the global LFO has a waveform. DX7 patches are 156-byte programs and are
stored per timbre.

## Upstream

- **Dexed** — <https://github.com/asb2m10/dexed> (`source/msfa`)
- **MSFA** — Google's Music Synthesizer for Android:
  <https://github.com/google/music-synthesizer-for-android>

The vendored engine, its local changes and the build stubs are described in
`third_party/msfa/README.md`.

## Licence

Apache-2.0 for the MSFA files (Copyright 2012 Google Inc.; later Dexed
modifications by Pascal Gauthier and contributors). The combined preenFM3
firmware is distributed under GPL-3.0.

## Files

- `Dx7SynthEngine.h/.cpp` — `SynthEngine` adapter and voice lifecycle.
- `Dx7Engine.h/.cpp` — `FmCore` wrapper and per-voice DX7 state.
- `Dx7Params.h/.cpp` — 156-byte DX7 program layout and parameter mapping.
- `Dx7ControlChange.h` — DX7 control-change mapping.
- `EditorDx7.h/.cpp`, `EditorDx7Algo.h/.cpp` — editor pages and algorithm
  display.
