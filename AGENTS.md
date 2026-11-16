# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project

preenFM3 firmware for an STM32H753 (Cortex-M7, 480 MHz). C11 / C++11.
- `firmware/` — main synth firmware
- `preen_lib/` — shared display, input, board, storage library
- `bootloader/` — standalone bootloader
- `third_party/` — vendored HAL, CMSIS, FatFs, display, MSFA
- `hardware/` — PCB and mechanical files
- `doc/` — hardware, compilation and design notes

Default sound engine is **PFM3**. The MSFA (Dexed DX7) engine is an additional
engine, opt-in per timbre.

## Build and flash

```sh
cmake --build build -j"$(nproc)"
st-flash --reset write build/firmware/p3_1_04.bin 0x08020000
```

- `./rebuild.sh` does a clean rebuild and prints the size of both ELFs.
- If flashing fails with `Failed to enter SWD mode`, simply retry.
- "build and flash" means run both commands without asking for confirmation.

## Verifying changes

- Trust only `cmake --build` output. The editor/LSP reports false errors
  because the ARM include paths are not configured: `stm32h7xx_hal.h` /
  `Common.h` not found, unknown `uint8_t`, `fatfs/integer.h` typedef
  redefinition, incomplete types. Ignore them.
- After a successful build, check the memory report; DTCMRAM and RAM_D1 are
  tight (both > 85% used).
- Flashing is the normal way to verify hardware-facing changes.

## Working style

- When you are unsure whether something matters (for example a code-size
  increase, a compiler warning, or a possible performance regression), stop and
  ask the user. Do not spend a lot of time investigating unless they confirm it
  is worth investigating.
- If the user asks a question, do not change code. Answer the question and wait
  for an explicit "Go" before making any code change.

## Code conventions

- Prefer minimal, targeted changes. Do not refactor or over-engineer.
- Match the surrounding style; avoid adding comments unless they explain
  non-obvious hardware behaviour. Keep comments short and clear.
- No dynamic allocation in the audio path.
- The 16 global voices are shared by all engines; do not add a separate voice
  pool per engine.
- `TFTAction.actionType` is a plain `uint8_t` (not a bitfield).
- `TftAlgo.cpp` exists twice: `preen_lib/src/display/TftAlgo.cpp` (real) and
  `bootloader/src/TftAlgo.cpp` (dummy). Every new `TftAlgo` method needs a
  bootloader stub or the bootloader will fail to link.
- TFT big font is fixed-width: parameter/button names longer than ~7
  characters collide. Keep names short.

## MSFA engine (opt-in)

- Keep PFM3 as the default engine; MSFA is selected per timbre.
- Do not add MSFA parameters to `Pfm3Preset`.
- MSFA has no unison mode.
- DX7 program layout (156 bytes): operator block `(5 - operatorNumber) * 21`;
  rates at `[0..3]`, levels at `[4..7]`; global data from `[126]`; algorithm
  `[134]`; feedback `[135]`; operator on/off mask `[155]`.
- Only DX7 algorithms 4 and 6 have multi-operator feedback loops; all others
  are self-feedback.
- MSFA operators are all sine; only the global LFO has a waveform.
- The DX7 envelope display maps levels linearly and derives each stage
  duration from its rate only (see `msfaGetEnvelopeDisplay()`).
- Vendored MSFA files stay Apache-2.0 and must keep their upstream headers.

## Documentation

- `README.md` — project overview, build and flashing.
- `doc/folder_structure.md` — repository layout and where code lives.
- `doc/hardware.md` — MCU, audio, display, SD, buttons and MIDI reference.
- `doc/compilation.md` — build details and CMake migration.
- `doc/improvements.md` — known issues and planned work.
- Keep `doc/msfa_engine.md` up to date when changing MSFA behaviour.
- Keep `doc/dx7_algo_drawing.md` in sync with algorithm-diagram changes.

## Git

- Do not create commits unless explicitly asked.
- Never amend or force-push.
- Do not commit `build/`, binaries, or editor artefacts.
- Do not commit secrets or keys.

## Domain notes

- Sample rate is `PREENFM_FREQUENCY` = 47916 Hz (`Common.h`).
