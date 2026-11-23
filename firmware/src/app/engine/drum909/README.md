# 9W9 — TR-909 drum engine

The 9W9 engine is preenFM3's TR-909 drum machine. One carrier voice per timbre
(`getMaxVoices() == 1`) plays the whole kit: the 909 mixes its eleven voices
internally, and `Timbre::voicesToTimbre()` sums the carrier block.

The bass drum, snare, toms, rimshot and clap are circuit-modelled (oscillators,
envelopes, filters, diode shaping); the hi-hat, ride and crash are mu-law
samples. The DSP runs at 44.1 kHz and is resampled to `PREENFM_FREQUENCY`
(47916 Hz) in `voicesNextBlock()`.

## Upstream

- **9W9** — TR-909 emulation by athousanddetails:
  <https://github.com/athousanddetails/schwung-9W9>
- The vendored DSP is the bare-metal C port from **X0X**:
  <https://github.com/charlesvestal/fm1-x0x> (`firmware/src/dsp/`)
- 9W9 grew out of **ER-99** by Matthew Cieplak.

The vendored sources, samples and generator live in `third_party/9W9/`.

## Licence

GPL-3.0. The vendored files keep their upstream SPDX headers; the preenFM3
firmware is distributed under GPL-3.0 as well.

## Files

- `Drum909Engine.h/.cpp` — `SynthEngine` adapter: GM note map, carrier voice,
  44.1 → 47916 resampler.
- `Drum909Params.h` — per-drum pot storage (`pots[DR_NUM + 1][DR_MAX_PARAMS]`).
- `EditorDrum909.h/.cpp` — six-page editor built from `drum909_param()`.
