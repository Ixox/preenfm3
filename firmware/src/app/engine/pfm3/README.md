# PFM3 — native preenFM3 engine

PFM3 is the original sound engine of the preenFM3 and the default engine for
every timbre. It is the FM synthesizer the instrument was designed around; DX7
(MSFA), BYOS and 9W9 are additional engines selected per timbre in the mixer.

PFM3 is a 6-operator FM synthesizer:

- 6 sine operators, each with its own envelope;
- 6 envelopes (time, level, curve);
- a 12-row modulation matrix;
- 3 LFOs, 2 envelope LFOs and 2 step sequencers;
- FX1 per voice, then GateFX and FX2 per timbre;
- 20 voices shared by all six timbres (a voice belongs to one timbre at a
  time);
- 47,916 Hz, computed per block (32 frames), not per sample.

The engine adapter is `Preenfm3Engine`, the editor is `EditorPfm3`, and the
operator/envelope/LFO/matrix/FX code lives in this directory.

## Project

- preenFM3: <https://github.com/Ixox/preenfm3>
- Wiki: <https://github.com/Ixox/preenfm3/wiki>
- Licence: GPL-3.0 (see the repository `LICENSE`). Copyright Xavier Hosxe and
  contributors.

## Files

- `Preenfm3Engine.h/.cpp`, `Preenfm3EngineMidi.cpp` — `SynthEngine` adapter,
  voice lifecycle, MIDI and NRPN.
- `Pfm3Params.h`, `Pfm3Param.h`, `Pfm3ParamTable.cpp`, `Pfm3ParamUiData.h` —
  parameter layout, descriptors and UI data.
- `Pfm3Preset.h`, `Pfm3PresetIO.h/.cpp`, `Pfm3FlashFormat.h/.cpp`,
  `Pfm3StorageBlob.h/.cpp` — preset model, serialization and flash storage.
- `Pfm3VoiceState.h` — per-voice DSP state.
- `Osc.h/.cpp`, `Env.h/.cpp`, `Lfo.h/.cpp`, `LfoEnv.h/.cpp`,
  `LfoEnv2.h/.cpp`, `LfoOsc.h/.cpp`, `LfoStepSeq.h/.cpp`,
  `Matrix.h/.cpp` — the DSP.
- `EditorPfm3.h/.cpp`, `EditorPfm3Algo.h/.cpp` — editor pages and algorithm
  display.
- `Presets.cpp` — factory presets.
