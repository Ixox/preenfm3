# Cleaning PFM3 out of MidiDecoder

Status: all phases done (incoming CC and NRPN maps, outgoing feedback and the
patch dump moved to `Preenfm3Engine`; NRPN index translation in `Pfm3Midi`;
`allParameterRows` in `Pfm3ParameterRows`).

`MidiDecoder` decodes MIDI and routes events to timbres, which is generic.
It also owns the PFM3 parameter map: the incoming CC/NRPN tables, the outgoing
CC/NRPN feedback and the PFM3 patch dump. That map belongs to the PFM3
parameter model, so it should live with the PFM3 engine, not in the decoder.

This document describes the remaining PFM3 leaks in `MidiDecoder` and the
target split. It follows the same pattern as the editor split
(`doc/screen_separation.md`) and the engine ownership described in
`doc/engine_voice_refactor.md`.

## 1. Current leaks

All PFM3-specific behaviour still inside `MidiDecoder`:

| Location | Leak |
|---|---|
| `MidiDecoder.cpp:527-759` | CC map (`receives & 0x01`) calls `Synth::setNewValueFromMidi(ROW_*, ENCODER_*)` |
| `MidiDecoder.cpp:653,674` | reads `synthState_->pfm3Preset->engine1.algo` for the "all carrier/modulator" CCs |
| `MidiDecoder.cpp:761-805` | NRPN accumulation, then `decodeNrpn()` |
| `MidiDecoder.cpp:808-912` | `sendCurrentPatchAsNrpns()` dumps `Pfm3Preset` and `lfoSteps1/2` |
| `MidiDecoder.cpp:914-950` | `decodeNrpn()` maps NRPN indexes to `allParameterRows` rows |
| `MidiDecoder.cpp:961-1218` | `newPfm3ParamValue()` maps a PFM3 row/encoder change to an outgoing CC/NRPN |
| `MidiDecoder.cpp:1263-1341` | `getMidiIndexFromMemory()` / `getMemoryIndexFromMidi()` encode the legacy PFM3 NRPN layout |

Secondary leak: `allParameterRows` is defined in `EditorPfm3.cpp:1867` (a UI
file) and declared in `SynthState.h:654`, yet `Synth::setNewValueFromMidi()`
and `MidiDecoder` use it. The PFM3 parameter table is not an UI artefact.

The PFM3 runtime API itself (`Preenfm3Engine::getPfm3Preset()`,
`Timbre::setParamValue()`, `Synth::setNewValueFromMidi()`,
`SynthState::propagateNewPfm3ParamValue*`) may stay; it is the MIDI *mapping*
that moves.

## 2. What stays in MidiDecoder

`MidiDecoder` remains the MIDI device layer and keeps:

- byte parsing, running status, sysex buffering (`newByte()`,
  `newMessageType()`, `newMessageData()`, `analyseSysexBuffer()`);
- channel → timbre routing (`midiEventReceived()`, MPE routing);
- notes, velocity, MPE notes, pitch bend, channel/poly aftertouch, mod wheel,
  breath;
- MIDI clock/start/continue/stop/song position;
- bank select, program change and the `LOAD_PRESET` async action;
- mixer CCs (volume, pan, send) and the `0x7d` mixer-volume sysex;
- global FX CCs (master FX, `CC_MFX_*`);
- hold pedal, all-notes-off, all-sound-off, omni on/off, reset, current
  instrument;
- the sequencer CCs (`CC_SEQ_*` → `Synth::setNewSeqValueFromMidi()`);
- CC transport: `writeMidiCCOut()`, `sendMidiDin5Out()`, `sendMidiUsbOut()`,
  `sendMidiUsbOutIfBufferFull()`, and the blocking "wait for USART" loops.

The shared Arp/FX CCs stay here too, because the Arp/FX page is shared by all
engines (see `doc/screen_separation.md`):

- Arp: `CC_ARP_CLOCK`, `CC_ARP_DIRECTION`, `CC_ARP_OCTAVE`,
  `CC_ARP_PATTERN`, `CC_ARP_DIVISION`, `CC_ARP_DURATION`.
- FX1/FX2: `CC_FILTER_*`, `CC_FILTER2_*`.

Only engine-owned parameter CCs move to the engine.

## 3. Engine MIDI contract

Add four methods to `SynthEngine` (`firmware/src/app/synth/SynthEngine.h`),
next to the existing capability hooks. All default to "not handled", so
`Dx7SynthEngine` is automatically safe and a third engine can start with the
defaults.

```cpp
// A MIDI message that changes one engine parameter.
struct MidiParamMessage {
    bool isNrpn;
    uint8_t cc;        // CC number when isNrpn == false
    uint8_t ccValue;   // 0..127
    uint16_t nrpn;     // NRPN parameter index when isNrpn == true
    uint16_t nrpnValue;
};

```

On `SynthEngine`:

```cpp
// Engine-owned MIDI data (CC, user CC, NRPN, aftertouch, pitch bend, MPE).
// Return true when the event was consumed.
virtual bool midiData(int timbre, const struct MidiData& data) { return false; }

// Map a PFM3 row/encoder change to an outgoing CC/NRPN.
// Return false when the parameter has no MIDI representation.
virtual bool midiPfm3ParamFeedback(int row, int encoder, float value,
                                   struct MidiParamMessage &out) const { return false; }

// Map a generic engine-parameter change (DX7 or a future engine) to an
// outgoing CC/NRPN. `paramId` and `context` are engine-defined.
virtual bool midiParamFeedback(int paramId, int context, float value,
                               struct MidiParamMessage &out) const { return false; }

// Enumerate the patch as NRPN messages (preset name, parameters, step
// sequencer). `index` starts at 0 and is advanced by the caller; return false
// when the patch is exhausted. Keeps the patch size out of the decoder.
virtual bool midiPatchParam(int timbre, int index,
                            struct MidiParamMessage &out) const { return false; }
```

`MidiDecoder` then:

- calls the engine CC hook from the `receives & 0x01` block, after the shared
  Arp/FX/mixer/sequencer handling;
- calls the engine NRPN hook from `decodeNrpn()` before the PFM3 index
  translation;
- delegates `newPfm3ParamValue()` to `midiPfm3ParamFeedback()` and
  `newEngineParamValue()` to the generic `midiParamFeedback()`, then only
  performs the actual send;
- loops on `midiPatchParam()` in `sendCurrentPatchAsNrpns()` and sends the
  returned NRPNs, keeping the 4-CC framing and the USART waits.

The engine has `Timbre*` and `SynthState*` from `init()`, so the PFM3
implementation can write the preset mirror and propagate UI refreshes without
new plumbing:

```cpp
float oldValue = ((float*) getPfm3Preset())[index];
timbre_->setParamValue(index, oldValue, newValue);
synthState_->propagateNewPfm3ParamValueFromExternal(
    timbreNumber_, row, encoder, param, oldValue, newValue);
```

The clamping currently done in `Synth::setNewValueFromMidi()`
(`Synth.cpp:662-691`) moves with the mapping.

## 4. PFM3 implementation

Put the PFM3 map in new files:

| File | Content |
|---|---|
| `Pfm3Midi.h` / `Pfm3Midi.cpp` | `Pfm3Midi` namespace: legacy NRPN index ↔ flat memory index translation |
| `Preenfm3Engine` methods | thin overrides calling `Pfm3MidiMap` |

CMake uses `file(GLOB_RECURSE src/app/*.cpp)`, so new files need no
`CMakeLists.txt` change.

Move `allParameterRows` (definition `EditorPfm3.cpp:1867`, declaration
`SynthState.h:654`) to the PFM3 side, next to `Pfm3Midi.*`. It is the PFM3
parameter metadata used by the engine, the editor and the MIDI map, not by the
generic framework. Keep the `extern` in a PFM3 header and update the editor
include.

Suggested split of the `receives & 0x01` CCs:

| Owner | CCs |
|---|---|
| PFM3 engine | `CC_ALGO`, `CC_IM*`, `CC_MIX*`, `CC_PAN*`, `CC_OSC*_FREQ`, `CC_MATRIXROW*_MUL`, `CC_MATRIX_SOURCE_CC*`, `CC_LFO*_FREQ`, `CC_LFO_ENV2_SILENCE`, `CC_STEPSEQ*_GATE`, `CC_ENV_*`, `CC_LFO*_PHASE`, `CC_LFO*_BIAS`, `CC_LFO*_SHAPE`, `CC_UNISON_*` |
| shared | `CC_ARP_*`, `CC_FILTER_*`, `CC_FILTER2_*`, `CC_MIXER_*`, `CC_SEQ_*`, `CC_MPE_SLIDE_CC74` |

Move the whole CC/NRPN body with its scaling constants verbatim first, then
refactor. The scales (`.1f`, `*.007874f`, `*.015625f`, `*.01f`, …) are wire
compatibility and must not change.

## 5. NRPN compatibility

NRPN indexes are part of the external protocol and must stay stable:

- `getMemoryIndexFromMidi()` / `getMidiIndexFromMemory()` encode the historical
  reordering of `ROW_LFOPHASES`, the LFO envelopes/step sequencers and the
  time/level rows (`MidiDecoder.cpp:1263-1341`). Move them unchanged into the
  PFM3 map; keep `NUMBER_OF_ENCODERS_PFM2` arithmetic.
- Step sequencer NRPNs use `paramMSB` 2 and 3 (`decodeNrpn()`,
  `MidiDecoder.cpp:937-942`) and are PFM3-only (`supportsStepSequencer()`).
- The preset name occupies NRPN indexes 228..239 and uses
  `Timbre::setPresetNameChar()` (`Timbre.cpp:2605`), which writes both the
  shared params and the PFM3 mirror.
- `paramMSB == 127 && paramLSB == 127` triggers `SEND_PATCH_AS_NRPN`. That is
  a device-level request; keep the async action in `MidiDecoder` and have it
  loop on the engine's `midiPatchParam()`.

DX7 timbres must not fall into the PFM3 map. With the virtual defaults this
happens automatically; do not reuse PFM3 row indexes for DX7 parameters.

## 6. Phases

Each phase must build and flash before the next one starts.

1. **Incoming MIDI — done.** Add `midiData()`. Move the engine-owned CCs from
   `controlChange()` into `Preenfm3Engine`. Keep shared Arp/FX/mixer/sequencer
   CCs in the decoder. Remove the two `pfm3Preset->engine1.algo` reads by
   reading `getPfm3Preset()` inside the engine.
2. **Incoming NRPN — done.** Handle `MIDI_DATA_NRPN` in `midiData()`. Move
   `decodeNrpn()`, the NRPN translation helpers and the step-sequencer/
   preset-name mapping into `Pfm3Midi`. Keep NRPN accumulation and the
   `SEND_PATCH_AS_NRPN` action in the decoder.
3. **Outgoing feedback — done.** Add `midiPfm3ParamFeedback()` and the generic
   `midiParamFeedback()`. Reduce `MidiDecoder::newPfm3ParamValue()` and
   `newEngineParamValue()` to: ask the engine for a `MidiParamMessage`, then
   send it with the existing transport, keeping the `lastSentCC` dedupe.
4. **Patch dump — done.** Add `midiPatchParam()` as an index-based
   enumerator; `sendCurrentPatchAsNrpns()` loops until it returns false. The
   engine owns the patch layout; the decoder keeps the 4-CC framing, DIN5/USB
   sending and the USART wait loops.
5. **Move `allParameterRows` — done.** Definition and `extern` now live in
   `Pfm3ParameterRows.*`; the generic `SynthState.h` no longer declares it.

After phase 4, `MidiDecoder` no longer includes `Pfm3Preset.h`, `Pfm3Midi.h`
or `Pfm3ControlChange.h`, and does not reference `allParameterRows`,
`Pfm3PresetLayout`, `ROW_*`/`ENCODER_*` or the PFM3 scaling constants. Only
the PFM3 listener method name remains (`newPfm3ParamValue`).

## 7. Invariants and risks

- Do not move MIDI transport or blocking waits into the engine. The engine is
  the audio/parameter owner, not the MIDI device.
- Do not change NRPN indexes, CC numbers or scaling; external editors depend on
  them.
- Keep the outgoing `lastSentCC` dedupe in `MidiDecoder`
  (`MidiDecoder.cpp:1204-1216`).
- No dynamic allocation. The patch dump stays a streaming callback, not a
  buffer of messages.
- `param == NULL` feedback (step-select changes) must keep working through the
  engine hook.
- The Arp/FX CCs must keep working on DX7 timbres.
- `Synth::setNewValueFromMidi()` is called only from the MIDI path today; after
  the move it can delegate to the engine or be deleted. `Timbre::setParamValue()`
  already writes both the shared block and the PFM3 mirror.

## 8. Test matrix

| Area | PFM3 | DX7 |
|---|---|---|
| CC parameter map | applies | ignored |
| CC Arp/FX | applies | applies |
| CC mixer / sequencer | applies | applies |
| NRPN parameters | applies | ignored |
| NRPN step sequencer | applies | ignored |
| Preset-name NRPN | applies | ignored |
| `SEND_PATCH_AS_NRPN` | PFM3 dump | name only (or no-op) |
| Outgoing CC/NRPN on edit | emitted | not emitted |
| Program change / bank select | unchanged | unchanged |
| MPE notes / pitch bend / AT / mod / breath | unchanged | unchanged |

Hardware check after each phase: load a PFM3 preset, edit from an external CC
controller, send an NRPN dump, request `SEND_PATCH_AS_NRPN`, then switch one
timbre to DX7 and repeat the CC/NRPN edits to confirm they are ignored.
