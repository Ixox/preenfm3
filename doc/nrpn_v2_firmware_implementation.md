# NRPN v2 — firmware implementation plan

Status: **firmware complete** — `ParamDesc::step` (§2), `Pfm3Nrpn.h` (§3), the
async response refactor (§4), mixer/global (§5), preset name (§6), the v2 dump
iterator (§7) and the v1-NRPN cleanup (§8) are done. Remaining: hardware
verification (§9) and the deferred web editor (§10).

Companion document: [`nrpn_spec_v2.md`](nrpn_spec_v2.md) defines the protocol.
This document defines how `preenfm3` implements it. `preenfm3-pvig` is a later
target; it has no `Pfm3Param` and keeps its own model.

---

## 1. Locked decisions

These were agreed before writing code and must not be re-litigated casually:

1. **No version bump.** `CAP_PROTOCOL_VERSION` stays `1`. The preset / flash
   format does **not** change.
2. **Do not advertise what is not implemented.** `MIDI_SUPPORTED_FEATURES`
   stays `0` until a feature is actually wired; the editor gates its UI on the
   capability bits.
3. **Blocking dump loop** for v1. The current pacing / 64-byte USART ring
   busy-wait is kept. Cancellation is deferred.
4. **Dump requires a timbre channel.** The timbre is the MIDI channel; there is
   no timbre index in the address. On the global channel a preset dump request
   answers `ERROR` code `2` (unknown command).
5. **ArpPattern and Arp2 extensions are deferred.** Keep the legacy 16-bit
   `arp_pattern_t` mask and Arp2's four fields. Feature bit 6 stays `0`.
   `[19, …]` and `[18, 4..8]` answer `ERROR` code `3` (unsupported block).
6. **Web editor work is deferred** until the firmware is done (see §8).
7. **Step is a real per-parameter value** and lives in `ParamDesc`, not in
   `ParamUiDesc` (see §2).

---

## 2. First step — `ParamDesc::step`

### 2.1 Why

The v2 decoder must quantize a value to the parameter's real step. Today the
step is derived from the UI value count:

```
EditorParamPage.cpp:410
    inc = (maxValue - min) / (ui.values - 1)
EditorParamPage.cpp:729
    info.incValue = (info.max - info.min) / (ui.values - 1)
```

That couples the MIDI codec to a UI count and cannot express e.g. `1/12`
without relying on a magic `nv` (`Freq` uses `nv = 193` for `0..16`).

`ParamUiDesc::values` is only read by:

- `EditorParamPage.cpp:410` (step derivation),
- `EditorParamPage.cpp:425` (the `values < 1.5f` guard),
- `EditorParamPage.cpp:728` (`info.nameCount`, written but never read),
- `Preenfm3EngineMidi.cpp:52` and `Timbre.cpp:840` (enum name lookup, loop over
  `valueOrder`).

So for float / integer / BPM rows, `values` exists solely to derive the step.

### 2.2 Decisions

- Keep `nv` / `ui.values` **as-is**; just add an explicit `step`. Minimal diff,
  no change to any other reader.
- Quantize **only** in the future `decode` (v2), never in `pfm3SetParam`.
- Write the oscillator `Freq` step as `1.0f / 12.0f` (one semitone).

### 2.3 Changes

**`firmware/src/app/engine/pfm3/Pfm3Param.h:148-153`**

```cpp
struct ParamDesc {
    ParamAccess access;
    uint16_t fieldOffset;
    float min;
    float max;
    float step;  // 0 = raw (char / bitfield / step index), no quantization
};
```

`ParamDesc` is only aggregate-initialized in `Pfm3ParamTable.cpp`; no other
initializer needs updating.

**`firmware/src/app/engine/pfm3/Pfm3ParamTable.cpp:192-239`**

```cpp
#define PARAM_FLOAT(n, mn, mx, st, off, dt, nv)
#define PARAM_INTEGER(n, mn, mx, st, off, nv)
#define PARAM_STEP_BPM(n, mn, mx, st, off, nv)
```

Each writes `st` into the `ParamDesc` position. The other macros set a
constant:

- `PARAM_ENUM`, `PARAM_ENUM_ORDERED` → `1.0f` (the value is an index);
- `PARAM_ARP_PATTERN`, `PARAM_CHAR`, `PARAM_STEP` → `0.0f` (raw).

**`firmware/src/app/ui/EditorParamPage.cpp`**

- `:410` → `float inc = desc.step;`
  Keep the `maxValue` override above (`:407-409`) and the LFO slow-down below
  (`:433-436`).
- `:425-427` guard unchanged.
- `:729-730` → `info.incValue = desc.step; info.step = desc.step;`

`pfm3SetParam` / `commonSetParam` are untouched.

### 2.4 Row map

48 rows gain an explicit step argument. `nv` stays as it is.

Step `0.01f` — 40 rows:

- `:254` Spread, `:255` Detune, `:256` pfm3Version
- `:258` IM, `:259` Mod Velo (0.01 — not the Engine1 `Velo`)
- `:261` Mix, `:262` Pan, `:268` FTun
- `:270-273` envelope times, `:275-278` envelope levels
- `:287` Matrix `Mult`
- `:293-296` performance `p1..p4`
- `:300` LFO Bias, `:301` LFO KSyn
- `:303-305` LFO phase
- `:307-310` LFO env1, `:312-314` LFO env2
- `:318` LFO Seq Gate
- `:357,358,359` Effect1 param1/param2/Gain, `:362,363,364` Effect2

Step `1.0f` — 6 rows:

- `:249` Engine1 Velo, `:251` Engine1 Speed
- `:317` LFO Seq Bpm (`PARAM_STEP_BPM`)
- `:321` MidiCurve Break, `:345` Arp1 BPM, `:347` Arp1 Octave

Step `0.1f` — 1 row:

- `:299` LFO Osc `Freq`. The effective step today is `0.1` because of the
  `LFO_FREQ_MAX + 0.9f` override (`maxValue = 100.8`, `values = 1009`,
  `100.8 / 1008 = 0.1`). The runtime override stays, so `desc.step = 0.1f`
  matches it exactly.

Step `1.0f / 12.0f` — 1 row:

- `:267` Osc `Freq` (`0..16`, `nv = 193` → 192 intervals → 1 semitone).

All these steps divide their range exactly, so behaviour is identical to the
current `(max - min) / (values - 1)` computation:

| Param | range | step | intervals | exact |
|---|---|---|---|---|
| IM | 0..16 | 0.01 | 1600 | yes |
| Mult | -10..24 | 0.01 | 3400 | yes |
| FTun | -9..9 | 0.01 | 1800 | yes |
| Freq | 0..16 | 1/12 | 192 | yes |
| KSyn | -0.01..16 | 0.01 | 1601 | yes |
| LFO Freq | 0..99.9 | 0.1 | 999 | yes |

### 2.5 Dead fields

`EditorParamInfo::incValue` (`EditorEngine.h:43`), `::step` and `::nameCount`
are written in `getParamInfo()` but never read. They are left in place — not
part of this refactor.

### 2.6 Cost and verification

`ParamFullDescription` grows by 4 bytes; 101 descriptors ≈ +404 bytes of
`.rodata` (flash is at 29.88 %, negligible).

```
cmake --build build -j"$(nproc)"
```

Check the memory report. Then confirm in the UI that encoder steps are
unchanged for a float param, `Freq`, `Velo` and LFO `Freq`.

---

## 3. Phase 0 — `Pfm3Nrpn.h`

New file `firmware/src/app/engine/pfm3/Pfm3Nrpn.h` (header-only; the CMake
build uses `GLOB_RECURSE … CONFIGURE_DEPENDS`, `firmware/CMakeLists.txt:5`, so
a new `.cpp` needs no edit either).

Contents:

- Block ids and reserved ranges (`0..16` engine = `Pfm3Block`, `17..21`
  common = `CommonBlock`, `22..98` reserved, `99` name, `100` mixer, `101`
  global, `102..126` reserved, `127` control).
- Control command ids `0..3`, response ids `64..71`, error codes `0..3`.
- `CAP_PROTOCOL_VERSION = 1`, the capability constants (`CAP_MAX_VOICES = 20`,
  `CAP_MAX_TIMBRES = 6`).
- Feature bits: `MIDI_SUPPORTED_FEATURES` = `0` for now.
- Value codec:

```cpp
// float: linear over [min, max]; other access kinds: raw
uint16_t encode(const ParamDesc& d, float v);
float    decode(const ParamDesc& d, uint16_t raw);
```

- Address resolver:

```cpp
struct Address { Target target; uint16_t id; uint8_t element; uint8_t field; uint8_t error; bool ok() const; };
Address resolve(uint8_t msb, uint8_t lsb);
```

`resolve()` splits `LSB` into `element = lsb / fieldCount` and
`field = lsb % fieldCount` using the `ParamBlock` metadata, and returns
`ERROR_UNKNOWN_PARAMETER` when the element is out of range or
`ERROR_UNSUPPORTED_BLOCK` when the MSB is not an engine/common block.

`encode`/`decode` use 14 bits. For `ParamAccess::Float`:
`raw = round((v - min) / (max - min) * 16383)` and
`v = min + raw * (max - min) / 16383`, then snapped to `step` when
`step > 0` (`round((v - min) / step) * step + min`, then clamped to
`[min, max]`). For `U8` / `Char` / `U16` / `Bitfield`, `raw` is the raw value.

---

## 4. Phase 1 — input router

### 4.1 `Preenfm3EngineMidi.cpp`

Replace `midiNrpnData()` (`:230`) with a router:

| MSB | Action |
|----:|--------|
| `0..16` | engine block → `applyEngineParam(Pfm3Param, value)` |
| `17..21` | common block → `applyCommonParam(CommonParamId, value)` |
| `99` | preset name char (LSB `0..11`) — **§6** |
| `100` | mixer field (LSB = field) — **§5** |
| `101` | global field (LSB = field) — **§5** |
| `127` | **not ours** — handled in `MidiDecoder` before the router |
| else | `ERROR_UNSUPPORTED_BLOCK` |

Implemented helpers (`Preenfm3EngineMidi.cpp`):

- `applyEngineParam(Pfm3Param id, float v)`: `StepSeq` has `flatBase = 0xFFFF`
  (`Pfm3ParamTable.cpp:397`) and is written through `setStepValueFromMidi`;
  every other engine block has a flat index and goes through
  `setPfm3ValueFromMidi(flat, v)`, which writes the preset mirror and
  propagates. That propagation reaches `Timbre::setParamValue` and updates the
  live `pfm3Params()` (`Synth.h:77` → `Synth.cpp:572` → `Timbre.cpp:895`).
- `applyCommonParam(CommonParamId id, float v)`: flat-indexed common blocks use
  `setPfm3ValueFromMidi`; `ArpPattern` has no flat index and is written with
  `commonSetParam` on `timbre_->sharedParams()` + `syncSharedParams` +
  propagation.

### 4.2 `MidiDecoder.cpp`

Keep the four-CC NRPN parser. Split data writes from control writes:

- data (`MSB != 127`) → forward it to the engine; the engine resolves and
  applies it, and the ISR sends `ERROR` on a resolution failure (see §4.3);
- control (`MSB == 127`) → enqueue an async response action (see §4.3); the
  engine decodes the command from the main loop.

When several timbres share a MIDI channel (omni, or the global channel which
maps to all six), `controlChange()` is called once per timbre. A
`bool ownsChannel` parameter (`tk == 0` from the per-timbre loop, `true` for
the MPE path) marks the timbre that owns the channel: only that one applies the
NRPN or emits a control/error reply. The other timbres still parse the four CC
bytes so their `currentNrpn_` state stays in sync, then discard the message.

Phase 1 implemented `nrpnControl()` for commands `0..3` directly in
`MidiDecoder`; §4.3 replaces it.

### 4.3 Async NRPN responses (audio-ISR constraint)

The firmware parses MIDI inside the audio DMA callback
(`HAL_SAI_TxCpltCallback` → `preenfm3DecodeMidiIn()` → `newByte()`,
`preenfm3.cpp:317-326`). `controlChange()` therefore runs in the audio ISR.
The dump used to busy-wait on `usartBufferOut` per message, and `usartBufferOut` is only
`RingBuffer<uint8_t, 64>` (`preenfm3.cpp:38`) with no overflow check
(`RingBuffer.h:33-40`). A dump, or a five-message capability burst, must not
run there.

**All four control commands are therefore handled from the main loop** through
the existing async queue (`processAsyncActions()`, `MidiDecoder.cpp:852`, called
from `preenfm3Loop()`, `preenfm3.cpp:161-165`). Data writes (MSB != 127) stay
in the ISR: they mutate the engine state the audio callback is using.

Async action (replaces `SEND_PATCH_AS_NRPN`); one type, raw control NRPN:

| Field | Value |
|---|---|
| `actionType` | `SEND_NRPN_RESPONSE` |
| `timbre` | target timbre |
| `param1` | channel `0..15` |
| `param2` | address MSB (`127`) |
| `param3` | address LSB (command) |
| `param4` | value MSB |
| `param5` | value LSB |

**ISR path** (`controlChange`, after the `ownsChannel` gate,
`MidiDecoder.cpp:622`):

- `paramMSB == 127` → enqueue the action. No send.
- otherwise → `timbre->midiData(MIDI_DATA_NRPN, …)`. On failure send the single
  `[127,66] code` immediately (12 bytes, fits the ring; this matches the
  pre-existing synchronous capability reply).

Error propagation: add `virtual uint8_t nrpnError() const` to `SynthEngine`
(default `0xFF` = none) and a `Timbre` forwarder. `Preenfm3Engine::midiNrpnData`
clears it on entry and stores `0`/`1`/`3` on failure.

**Main loop** (`processAsyncActions`):

```cpp
case SEND_NRPN_RESPONSE:
    timbre->startNrpnResponse(channel, address, value);
    while ((message = timbre->getNextNrpn()) != nullptr) {
        if (message->isNrpn) {
            sendNrpn(channel, message->nrpn, message->nrpnValue);
            sendMidiDin5Out();
            sendMidiUsbOutIfBufferFull();
            while (usartBufferOut.getCount() > 0) {
            }
        }
    }
    sendMidiUsbOut();
    break;
```

The pacing loop is the previous `sendCurrentPatchAsNrpns()` body.

**Engine API** (`SynthEngine.h` / `Timbre.h` / `Preenfm3EngineMidi.cpp`):

```cpp
virtual void startNrpnResponse(uint8_t channel, uint16_t address, uint16_t value) {}  // main loop
virtual const struct MidiParamMessage* getNextNrpn() { return nullptr; }     // main loop
virtual uint8_t nrpnError() const { return 0xFF; }                           // ISR
```

- `startNrpnResponse` decodes `[127, cmd]`, validates, then fills a small
  immediate queue (error / capabilities / single parameter) or starts the dump
  iterator.
- `getNextNrpn` drains the queue, then the dump iterator.
- Only the main loop calls `startNrpnResponse`/`getNextNrpn`; `midiData`
  (ISR) only enqueues, so there is no shared response state with the ISR.

**Deleted in this change:**

- `midiPatchParam` (interface + PFM3 implementation).
- `sendCurrentPatchAsNrpns`.
- `sendCapabilities`, `sendError`, `nrpnControl` and their declarations.
- `#include "Pfm3Nrpn.h"` from `MidiDecoder.cpp`, the `SEND_PATCH_AS_NRPN`
  action type and async case.

Commands 0/1 are implemented by the §7 dump iterator; command 2
(`REQUEST_PARAM`) works through `resolve()` + `encode()`; command 3
(`REQUEST_CAPABILITIES`) returns the five `CAP_*`. No v1 dump code is kept.

---

## 5. Phase 2 — mixer and global

`[100, field]` and `[101, field]` with `LSB = field`, no timbre index.

Mixer fields `0..12` (`MixerInstrumentState`, `MixerState.h:42-59`):

`out, compressorType, midiChannel, firstNote, lastNote, shiftNote,
numberOfVoices, scalaEnable, scalaMapping, scaleScaleNumber, volume, pan,
send`.

Global fields `0..12` (`MixerState`, `MixerState.h:71-83`):

`currentChannel, globalChannel, midiThru, tuning, levelMeterWhere,
reverbPreset, reverbOutput, reverbLevel, userCC1..4, MPE_inst1`.

Implementing the value domains (also used by the §7 dump):

| Field | Kind | Range / step |
|---|---|---|
| out | raw | 0..8 |
| compressorType | raw | 0..6 |
| midiChannel, currentChannel, globalChannel | raw | 0..16 |
| firstNote, lastNote | raw | 0..127 |
| shiftNote | float | -63..64, step 1 |
| numberOfVoices | raw | 0..`MAX_NUMBER_OF_VOICES` |
| scalaEnable, midiThru | raw | 0..1 |
| scalaMapping | raw | 0..`SCALA_MAPPING_NUMBER_OF_OPTIONS - 1` |
| scaleScaleNumber | raw | 0..`NUMBEROFSCALASCALEFILES - 1` |
| volume | float | 0..4, step 0.01 |
| pan | float | -63..63, step 1 |
| send, reverbLevel | float | 0..1, step 0.01 |
| tuning | float | 420..460, step 0.1 |
| levelMeterWhere | raw | 0..2 |
| reverbPreset | raw | 0..18 |
| reverbOutput | raw | 0..2 |
| userCC1..4 | raw | 0..127 |
| MPE_inst1 | raw | 0..5 |

Implemented in `Preenfm3EngineMidi.cpp`:

- `mixerNrpnFields` / `globalNrpnFields` describe each field with
  `ParamAccess`/min/max/step and feed `Pfm3Nrpn::encode`/`decode`.
- `Pfm3Nrpn::resolve` maps MSB `99`/`100`/`101` to `PresetName`/`Mixer`/`Global`
  (LSB out of range → `ERROR_UNKNOWN_PARAMETER`; other MSBs → `ERROR 3`).
- `applyMixerField`/`applyGlobalField` write `synthState_->mixerState_` and
  propagate through `propagateNewMixerValueFromExternal`, so `Synth` and
  `ScreenMixer` side effects run while `MidiDecoder` stays silent (no echo).
  Global sub-indices follow the mixer UI mapping (`GLOBAL_SETTINGS_1`
  0=MPE,1=globalChannel,2=currentChannel,3=midiThru,4=tuning,5=levelMeterWhere;
  `GLOBAL_SETTINGS_2` 0..3=userCC; `GLOBAL_SETTINGS_3`
  0=reverbPreset,1=reverbLevel,2=reverbOutput).
- `REQUEST_PARAM` reads blocks `99`/`100`/`101` as well (name char, mixer,
  global). A global write is applied once because the decoder's `ownsChannel`
  gate leaves a single timbre.

The value encodings defined here are also required by the §7 dump for blocks
`100`/`101`.

---

## 6. Phase 3 — preset name

- `[99, 0..11]` read/write the 12 preset-name chars.
- Remove the old `MSB = 1`, `LSB = 100..111` path.
- Propagate the name on the 12th char.

The preset-name descriptors already exist (`Pfm3ParamTable.cpp:366-377`,
`PARAM_CHAR`, step `0`).

Implemented: `applyPresetNameChar` clamps the value to printable ASCII
(`32..126`), calls `Timbre::setPresetNameChar` (which also syncs
`sharedParams()`), and on index `11` calls `propagateNewPresetName` for the UI.
`REQUEST_PARAM` on `[99, i]` returns the raw character. The legacy
`MSB = 1`, `LSB = 100..111` path is gone.

---

## 7. Phase 4 — v2 dump iterator

Implemented in `Preenfm3EngineMidi.cpp` (`startDump`, `nextDumpMessage`,
`dumpParamValue`, `dumpBlockSize`). No v1 dump code is kept.

`startNrpnResponse` (command 0/1) starts the iterator; `getNextNrpn` streams it
from the main loop with the §4.3 pacing:

- command 0 (`REQUEST_PRESET_DUMP`, scope 0, timbre channel only): emit
  `[127,64]` (DUMP_BEGIN, value = protocol version), then the parameters of
  blocks `0..18`, `20`, `21`, `99` (name), `100` (mixer for the addressed
  timbre), `101` (global) in ascending MSB order, then `[127,65]` (DUMP_END,
  value = number of parameters sent). Block `19` (ArpPattern) is excluded until
  the per-step layout lands; `Arp2` fields `4..8` are deferred too, so `[19,…]`
  and `[18,4..8]` answer `ERROR 3`.
- command 1 (`REQUEST_BLOCK_DUMP`, value = block id): emit that block's
  parameters, then `DUMP_END` (count). Per spec §8.3 there is **no**
  `DUMP_BEGIN` for a block dump. `isDumpBlockSupported` rejects block `19` and
  `22..98`/`102..126` with `ERROR 3`.
- The iterator returns one `MidiParamMessage` per `getNextNrpn()` call; pacing
  stays in the main-loop drain.
- Value source: `pfm3GetParam(pfm3Params(), id)` / `commonGetParam(shared, id)`
  for blocks `0..21` (`StepSeq` through `getSeqStepValue`), the preset name for
  `99`, `MixerState` for `100`/`101`; encode with `Pfm3Nrpn::encode` and the
  encodings from §5.
- On the global channel command 0 answers `ERROR 2`; scope `> 0` answers
  `ERROR 1`.

---

## 8. Phase 5 — cleanup (done)

Removed by §4/§4.3 (do not re-add):

- the legacy `paramMSB < 2` map;
- `[127,126]` / `[127,127]`;
- `paramMSB 4/5/6`;
- `midiPatchParam`, `sendCurrentPatchAsNrpns`, `SEND_PATCH_AS_NRPN`.

Removed by this cleanup:

- `Pfm3Midi` (both files), the `nrpnBase`/`nrpnStride` block fields,
  `pfm3Nrpn`/`commonNrpn`, `nrpnToParam`/`pfm3NrpnToParam`/`ParamRef`;
- `ParamMidiDesc` / `ParamFullDescription::midi` and
  `MidiTransform::Float100`;
- `MIDI_CAPABILITY_NRPN_COUNT` and `MIDI_MAX_LFO_SHAPE`.

NRPN feedback (engine parameters and the mixer/global four values) now uses the
v2 `[block, lsb]` map via `Pfm3Nrpn::encode`. **CC feedback is kept unchanged**
(`MIDICONFIG_SENDS == 1`, `midiSends = {"None","CC","NRPN"}`).

---

## 9. Phase 6 — verification

1. `cmake --build build -j"$(nproc)"`, check the memory report (DTCMRAM and
   RAM_D1 are tight).
2. Flash and exercise the §15 vectors from `nrpn_spec_v2.md` with a MIDI
   monitor.
3. Round-trip check: `EnvCurve`, `Effect2`, `StepSeq` and the preset name must
   survive write → read and a full dump.
4. Capability request on the global channel returns exactly five responses,
   streamed from the main loop (not the audio ISR).
5. Preset dump on a timbre channel returns BEGIN / params / END with a
   matching count; block dump returns params / END (no BEGIN); block 19
   returns `ERROR 3`; preset dump on the global channel returns `ERROR 2`.
6. Invalid data write returns a single `[127,66]` error and no other message.
7. `REQUEST_PARAM` returns one `[block, lsb]` write; unknown `[127,cmd]`
   returns `ERROR 2`.
8. `[99, 0..11]` writes/reads the preset name; index 11 refreshes the UI.
9. `[100, field]` and `[101, field]` write/read mixer and global values with
   the §5 domains; out-of-range LSB returns `ERROR 0`; `pan`/`shiftNote` snap
   to integers.

---

## 10. Web editor (deferred)

Tracked, not started:

- add `src/midi/nrpnSpec.ts`;
- replace `NRPN_COMMANDS` and the ad-hoc `sendLfo…` / filter / curve / step
  helpers with `sendParam(block, element, field, value)`;
- rewrite `requestPatchDump` / the parser around `DUMP_BEGIN … DUMP_END`;
- request capabilities on connect;
- mixer / global UI via `[100]` / `[101]`;
- preset name via `[99]`;
- remove the legacy helpers and `Pfm3Midi` indices.