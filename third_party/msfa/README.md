# MSFA (Dexed DX7 engine)

Vendored subset of Dexed's `source/msfa` engine, used by preenFM3 as the
alternate per-timbre DX7 synthesis engine.

- Upstream: <https://github.com/asb2m10/dexed>, `source/msfa`
- Vendored from commit: `fea2f0f0f2c397799f59b27eb11c038cd34b22c5`
- License: Apache-2.0 (see `LICENSE`). The original files are Copyright
  2012 Google Inc.; later Dexed modifications are Copyright Pascal Gauthier
  and contributors.
- The combined preenFM3 firmware is distributed under GPL-3.0.

## Vendored files

`aligned_buf.h`, `controllers.h`, `dx7note.{cc,h}`, `env.{cc,h}`,
`exp2.{cc,h}`, `fm_core.{cc,h}`, `fm_op_kernel.{cc,h}`, `freqlut.{cc,h}`,
`lfo.{cc,h}`, `module.h`, `pitchenv.{cc,h}`, `porta.{cpp,h}`, `sin.{cc,h}`,
`synth.h`, `tuning.h`.

Dexed's `tuning.cc` is not vendored: it depends on JUCE and the Surge Tuning
Library. Standard 12-TET tuning is provided by `platform_stubs.cc` instead.

## Local changes to upstream files

- `controllers.h`, `env.cc`: include `"debugger/Logger.h"` instead of
  `"../debugger/Logger.h"` so the stub logger lives inside this directory.
- `dx7note.cc`: guard `<iostream>` behind `VERBOSE`; it is not used in the
  normal build.
- `fm_core.cc`: the `FmCore::render` definition uses `int32_t` for its last
  parameter to match the declaration in `fm_core.h`. Upstream relies on
  `int` and `int32_t` being the same type, which is not true for the
  arm-none-eabi toolchain (`int32_t` is `long int` there).
- `freqlut.cc`, `sin.cc`, `exp2.cc`, `porta.cpp`: the lookup tables are placed
  in the `.ram_d2c` section. They are filled by `msfaInit()` before use, so
  they do not need to be in `.bss`/DTCMRAM. This uses the otherwise-unused
  `RAM_D2C` bank and keeps DTCMRAM within budget.

Everything else is unmodified upstream code.

## Stubs and glue

- `debugger/Logger.h`: no-op `DXLOG`/`TRACE`.
- `libMTSClient.h` + `platform_stubs.cc`: no-op MTS-ESP client. The standard
  `TuningState` path is used.
- `Tunings.h`: empty stand-in for the Surge Tuning Library type referenced by
  `tuning.h`.
- `platform_stubs.cc`: implements `createStandardTuning()` with the DX7
  standard tuning table.
- `msfa_init.{h,cc}`: one-time lookup-table initialization for the preenFM3
  sample rate. Called from `preenfm3Init()` before the SAI DMA callbacks
  start.

## Engine core

The first implementation uses Dexed's `FmCore` (32 DX7 algorithms). Dexed also
provides the more accurate `EngineMkI` and `EngineOpl` cores; those can be
plugged in later behind the same `FmCore`-style interface if more accuracy is
needed.
