# BYOS — Bring Your Own Synth

BYOS is the third engine example, next to PFM3 and MSFA. It is deliberately
small: two additive oscillators, one linear AR envelope, seven parameters, a
minimal editor and a simple MIDI CC map.

It is meant to show how little an engine needs to provide, and which files
outside the engine/editor folder must be touched to register it.

## Files in this folder

| File | Content |
|---|---|
| `ByosParams.h` | Parameter ids, `ByosParams`, MIDI CC ids |
| `ByosEngine.h/.cpp` | `ByosEngine : SynthEngine`, 2 × PFM3 `Osc`, AR envelope, persistence, CC |
| `EditorByos.h/.cpp` | `EditorByos : EditorEngine`, one `Engine` page with `OP`/`ENV` |
| `README.md` | This file |

CMake globs `firmware/src/app/*.cpp` recursively, so no build-file change is
needed.

## Signal flow

```
MIDI note → ByosEngine::noteOn
    allocate a global voice (Timbre::allocateVoice)
    start 2 Osc (PFM3 wave tables) at the same note frequency
    start a linear attack

ByosEngine::voicesNextBlock (per block)
    per voice:
        envelope attack / sustain / release
        op1 = Osc1.getNextSample(...)
        op2 = Osc2.getNextSample(...)
        out = (op1 * (1 - mix) + op2 * mix) * env * velocity
        write the same value to L and R

Timbre::voicesToTimbre mixes the voice blocks
```

The engine uses the PFM3 `Osc`/`OscillatorParams` and `waveTables`, so all
PFM3 oscillator shapes are available (`sin`, `saw`, `square`, `sin²`,
`sin-zero`, `sin-pos`, `rand`, `off`, `User1..6`).

## Parameters

| Id | Name | Range | Default | UI |
|---|---|---|---|---|
| `BYOS_PARAM_WAVE` | operator waveform | `0..OSC_SHAPE_LAST-1` | op1 `SQUARE`, op2 `SAW` | OP row 1/2, encoder 0 |
| `BYOS_PARAM_MULTIPLIER` | frequency multiplier | `0.25..4.0`, step `0.25` | op1 `1.0`, op2 `1.5` | OP row 1/2, encoder 1 |
| `BYOS_PARAM_MIX` | `Mix 12` op1 ↔ op2 mix | `0..1`, step `0.05` | `0.5` | OP page, encoder 3 |
| `BYOS_PARAM_ATTACK` | attack time | `0..5 s` | `0.01 s` | ENV page, encoder 0 |
| `BYOS_PARAM_RELEASE` | release time | `0..5 s` | `0.3 s` | ENV page, encoder 1 |

`mix = 0` is op1 only, `mix = 1` is op2 only. The envelope is linear:
0→1 on attack, current→0 on release. Velocity scales the amplitude linearly.

## MIDI CC

| CC | Parameter |
|---|---|
| 46 | op1 waveform |
| 47 | op1 multiplier |
| 48 | op2 waveform |
| 49 | op2 multiplier |
| 50 | mix |
| 51 | attack |
| 52 | release |

These are PFM3-specific CCs; they do not conflict at runtime because CC
dispatch is per active engine. CC values are mapped linearly to each range;
multipliers are quantized to the 16 steps `0.25..4.0`. The same CCs are sent
back when a parameter changes in the editor (`midiParamFeedback`). Incoming
CCs are applied with `propagateNewEngineParamValueFromExternal`, so they update
audio and UI but are not echoed.

There is no NRPN map and no `midiPatchParam()` patch dump.

## Editor

`EditorByos` registers one main page, `Engine`:

- `OP` — row 1 is op1 (`Wave`, `Mult`, `Mix 12`), row 2 (encoders 4–6) is op2
  (`Wave`, `Mult`). `Mix 12` is the single global crossfade.
- `ENV` — `Attack`, `Release`.

It uses the shared `EditorOneButton`/`EditorEditMenu` structures and the
`EditorEngine` helpers (`currentEditMenu()`, `currentPage()`,
`rowEncoderFor()`, `getEncoderX()`). The engine parameters are read through
`Timbre::getPatchMemory()` cast to `ByosParams*`.

## Persistence

- Magic: `PRESET_BYOS` = `0x42594F53` (`"BYOS"`).
- Name offset: `sizeof(ByosParams)` (28).
- Blob layout:

```
[0..28)   ByosParams
[28..44)  12-byte preset name + padding
[44..)    TimbreSharedParams (Arp, FX1, FX2, preset name)
```

`ByosEngine::getStorageBlob()` / `loadStorageBlob()` implement it.
`afterNewParamsLoad()` and `patchMemoryChanged()` re-apply the parameters.

## Files changed outside this folder

| File | Change |
|---|---|
| `firmware/src/app/synth/Common.h` | add `SYNTH_ENGINE_BYOS` before `SYNTH_ENGINE_COUNT` |
| `firmware/src/app/synth/PersistenceMagic.h` | add `PRESET_BYOS` |
| `firmware/src/app/synth/SynthEngineFactory.h/.cpp` | include `BYOS/ByosEngine.h`, add `byosEngines_`, init/get, storage magic, display name |
| `firmware/src/app/ui/ScreenMixer.cpp` | add the BYOS entry to `engineDisplay[]` |
| `firmware/src/app/ui/ScreenEditor.cpp` | include `BYOS/EditorByos.h`, add the static editor and call `init()` |
| `firmware/src/app/synth/Timbre.h` | `friend class ByosEngine` (voice allocation, hold pedal, voice list) |
| `firmware/src/app/synth/Voice.h` | `friend class ByosEngine` (voice state and sample block) |
| `doc/folder_structure.md` | list this folder |

`ScreenMixer` gets the name from
`SynthEngineFactory::getEngineDisplayName()`, so it does not contain any BYOS
string.

## Cloning BYOS for a real engine

1. Copy this folder and rename `Byos*` to your engine name.
2. Implement the required `SynthEngine` methods (`init`, `afterNewParamsLoad`,
   `bindVoice`, `noteOn`, `noteOff`, `voicesNextBlock`, `prepareBlock`,
   `glide`, `gateFx`, `setMatrixSource*`, `resetMatrixDestination`,
   `setMatrixPolyAfterTouch`, `noteOnMpe`, `noteOffMpe`, persistence).
3. Implement `EditorEngine` pages, or derive from `EditorParamPage` if the
   engine reuses the flat `Pfm3Preset` layout.
4. Apply the outside changes listed above.
5. Build, flash, and check the mixer, editor pages, CC and preset save/load.

## Limitations

- No NRPN/patch dump for a PC editor.
- No matrix, LFO, unison, mono, glide or per-operator output level.
- Shared FX1/FX2 and Arp still run through `Timbre`/`Voice` as for the other
  engines; BYOS does not expose them in its editor.
- Uses the shared 16-voice pool; voice allocation is the generic `Timbre`
  allocator.
