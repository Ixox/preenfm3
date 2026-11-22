# PreenFM3 NRPN specification — v2

Status: **proposal**, clean-slate rewrite. Backward compatibility with the
current NRPN map is explicitly **not** a goal.

This protocol targets the **PFM3 engine only**; other engines are out of scope.

This document defines a single, self-describing NRPN address space for the
PreenFM3, used by the web editor (and any other editor) for:

- parameter modification;
- full or partial preset dumps;
- preset name;
- capabilities and protocol version discovery;
- editor session control;
- mixer and global settings.

CC (control change) mapping is a separate concern and is not covered here.

---

## 1. Why a rewrite

The current NRPN layer mixes three different address spaces and several ad-hoc
ranges:

- the legacy PreenFM2 map (`paramMSB < 2`);
- special ranges: `MSB=1` preset name, `MSB=2` envelope curves / step values,
  `MSB=4` timbre/MFX, `MSB=5/6` (unused), `MSB=127` dump + capabilities;
- three different indices for the same parameter: legacy `nrpnBase/nrpnStride`,
  flat preset index, and editor "memory index";
- per-path value transforms (`Float100`, `Float127`, `Bipolar`, `ImTimes10`,
  `OscFrequency`, `EnvAttack`, ...).

Every new feature therefore needs a special case. This specification replaces
all of it with:

- **one MSB per block**;
- **one LSB formula** inside a block;
- **one value encoding**;
- **one reserved command domain** for protocol commands.

---

## 2. Transport

Standard NRPN, four CC messages:

| CC  | Meaning              | Range   |
|-----|----------------------|---------|
| 99  | parameter MSB        | 0..127  |
| 98  | parameter LSB        | 0..127  |
| 6   | value MSB            | 0..127  |
| 38  | value LSB            | 0..127  |

- Parameter address: `(MSB << 7) | LSB` → 14 bits (0..16383).
- Value: `(valueMSB << 7) | valueLSB` → 14 bits (0..16383).

> **Note:** NRPN MSB is a 7-bit value. `MSB = 128` is not representable. The
> editor/command domain is therefore **MSB 127**.

Messages are sent on the timbre MIDI channel. NRPN handling must be enabled by
the synth's existing "receives NRPN" configuration.

---

## 3. Address map

`MSB` identifies the block. `LSB` identifies the parameter inside the block.

| MSB     | Domain                                                        |
|---------|---------------------------------------------------------------|
| 0       | Engine1                                                       |
| 1       | Engine2                                                       |
| 2       | Mod (IM / feedback)                                           |
| 3       | OscMix                                                        |
| 4       | Osc                                                           |
| 5       | EnvTime                                                       |
| 6       | EnvLevel                                                      |
| 7       | EnvCurve                                                      |
| 8       | Matrix                                                        |
| 9       | Performance                                                   |
| 10      | LfoOsc                                                        |
| 11      | LfoPhase                                                      |
| 12      | LfoEnv1                                                       |
| 13      | LfoEnv2                                                       |
| 14      | LfoSeq                                                        |
| 15      | MidiCurve                                                     |
| 16      | StepSeq                                                       |
| 17      | Arp1                                                          |
| 18      | Arp2                                                          |
| 19      | ArpPattern                                                    |
| 20      | Effect1                                                       |
| 21      | Effect2                                                       |
| 22..98  | **reserved for future blocks**                                |
| 99      | **PresetName**                                                |
| 100     | **Mixer** (per timbre)                                        |
| 101     | **Global**                                                    |
| 102..126| reserved                                                      |
| 127     | **Control / Editor protocol**                                 |

Block ids are append-only once released. Reserved ids must not be reused for a
different meaning.

---

## 4. Parameter addressing inside a block

For data blocks (0..21, 100, 101):

```
LSB = element * fieldCount + field
```

- `element`: repeated instance (operator, matrix row, LFO, sequencer, timbre…).
- `field`: parameter inside one element.
- `fieldCount`: number of fields per element, from the block descriptor.

`PresetName` (99) is special: `LSB = character index 0..11`.

Current field counts:

| Block | elements (`count`) | `fieldCount` | max LSB |
|-------|-------------------:|-------------:|--------:|
| Engine1 | 1 | 4 | 3 |
| Engine2 | 1 | 4 | 3 |
| Mod | 6 | 2 | 11 |
| OscMix | 6 | 2 | 11 |
| Osc | 6 | 4 | 23 |
| EnvTime | 6 | 4 | 23 |
| EnvLevel | 6 | 4 | 23 |
| EnvCurve | 6 | 4 | 23 |
| Matrix | 12 | 4 | 47 |
| Performance | 1 | 4 | 3 |
| LfoOsc | 3 | 4 | 11 |
| LfoPhase | 1 | 3 | 2 |
| LfoEnv1 | 1 | 4 | 3 |
| LfoEnv2 | 1 | 4 | 3 |
| LfoSeq | 2 | 2 | 3 |
| MidiCurve | 2 | 3 | 5 |
| StepSeq | 2 | 16 | 31 |
| Arp1 | 1 | 4 | 3 |
| Arp2 | 1 | 9 | 8 |
| ArpPattern | 4 | 32 | 127 |
| Effect1 | 1 | 4 | 3 |
| Effect2 | 1 | 4 | 3 |
| Mixer | 1 | 13 | 12 |
| Global | 1 | 13 | 12 |

`ArpPattern` uses the full LSB range (0..127): 4 patterns × 16 steps × 2
fields. All other blocks fit below 64.

All data blocks except `Global` (101) are scoped to the timbre of the MIDI
channel. `PresetName` (99) and `Mixer` (100) are per-timbre; `Global` (101) is
not.

### Examples

| Parameter | MSB | LSB |
|-----------|----:|----:|
| Operator 3 frequency | 4 (Osc) | `2*4 + 2 = 10` |
| Operator 1 attack time | 5 (EnvTime) | `0*4 + 0 = 0` |
| Matrix row 5 Dest1 | 8 (Matrix) | `4*4 + 2 = 18` |
| Preset name character 7 | 99 | `7` |
| Mixer volume (timbre = channel) | 100 | `0` |
| Global currentChannel | 101 | `0` |

---

## 5. Value encoding

One canonical encoding, independent of parameter type. No per-path transforms.

### Continuous parameters (float)

```
raw = round((x - min) / (max - min) * 16383)
x   = min + raw / 16383 * (max - min)
```

After decoding, the receiver quantizes to the parameter's declared `step`.

### Discrete parameters (enum, integer, step)

```
raw = native index / integer value
```

### Character

```
raw = ASCII code
```

### Bitfields

- `raw = bitfield value` for bitfields up to 14 bits. v2 exposes no wider
  bitfield: the arp pattern is exposed per step (§6).

Parameter metadata (type, `min`, `max`, `step`, decimals) is **not** sent with
every message. It comes from a versioned table generated from the firmware
parameter table (`Pfm3ParamTable.cpp`). The protocol version is checked at
connect time (§7.2).

---

## 6. ArpPattern layout

v2 replaces the legacy 16-bit arp mask with one state and one action per step;
the firmware must store this per-step data. Pattern lengths and the Euclidean
seed live in `Arp2` (block 18, fields 4..8).

`ArpPattern` (MSB 19) is the only block that uses the full LSB range:

```
LSB = pattern * 32 + step * 2 + field
```

- `pattern`: 0..3 (`Usr1`..`Usr4`)
- `step`: 0..15
- `field`: 0 = state, 1 = action

State values: `0 = Off`, `1 = On`, `2 = Accent`.
Action values: `0 = Norm`, `1 = Tie`, `2 = Oct+`, `3 = Rt*2`, `4 = Chrd`, `5 = Slap`.

Max LSB = `3*32 + 15*2 + 1 = 127`.

---

## 7. Control / editor domain (MSB = 127)

`LSB` selects the command or response. `value` is the argument.

### 7.1 Commands and responses

Commands are sent **to** the synth (editor → synth), LSB `0..63`.
Responses are sent **by** the synth (synth → editor), LSB `64..127`.

#### Commands (0..63)

| LSB | Command | Value | Expected response |
|----:|---------|-------|-------------------|
| 0 | REQUEST_PRESET_DUMP | scope: 0 = full state for the timbre, >0 reserved | `DUMP_BEGIN` … params … `DUMP_END` |
| 1 | REQUEST_BLOCK_DUMP | block id | params of that block … `DUMP_END` |
| 2 | REQUEST_PARAM | parameter address `(block << 7) \| lsb` | one `[block, lsb]` parameter write |
| 3 | REQUEST_CAPABILITIES | 0 | the five `CAP_*` responses below |
| 4..63 | reserved | | |

#### Responses (64..127)

| LSB | Response | Value |
|----:|----------|-------|
| 64 | DUMP_BEGIN | protocol version |
| 65 | DUMP_END | number of parameters sent |
| 66 | ERROR | error code (§11) |
| 67 | CAP_PROTOCOL_VERSION | 1 |
| 68 | CAP_FEATURE_BITS_LO | bits 0..13 |
| 69 | CAP_FEATURE_BITS_HI | bits 14..27 |
| 70 | CAP_MAX_VOICES | |
| 71 | CAP_MAX_TIMBRES | |
| 72..127 | reserved | |

### 7.2 Capabilities

Requested with `[127, 3]`. The synth replies with exactly these five responses,
in this order:

1. `CAP_PROTOCOL_VERSION`
2. `CAP_FEATURE_BITS_LO`
3. `CAP_FEATURE_BITS_HI`
4. `CAP_MAX_VOICES`
5. `CAP_MAX_TIMBRES`

### 7.3 Feature bits

The following list is valid for `CAP_PROTOCOL_VERSION = 1`.

`CAP_FEATURE_BITS_LO` (bits 0..13) and `CAP_FEATURE_BITS_HI` (bits 14..27):

| Bit | Feature                          |
|----:|----------------------------------|
| 0   | reserved                         |
| 1   | operator phase                   |
| 2   | operator warp                    |
| 3   | engine decimation                |
| 4   | voice priority                   |
| 5   | step-sequencer modes             |
| 6   | extended arp                     |
| 7   | extended matrix destinations     |
| 8   | LFO envelope curves              |
| 9..27 | reserved                       |

A feature bit means "this firmware supports the corresponding parameters". It
does not imply anything about the block layout, which is fixed by this
specification.

---

## 8. Dump protocol

The synth streams the whole requested dump.

```
Editor → [127, 0]  value = scope
Synth  → [127, 64] DUMP_BEGIN (protocol version)
Synth  → [block, lsb] value        (repeated, paced)
Synth  → [127, 65] DUMP_END (count)
```

### 8.1 Scope

`scope` is `0` only; values `> 0` are reserved.

| Scope | Content |
|------:|---------|
| 0 | preset (blocks 0..21) + name (99) + mixer for the addressed timbre (100) + global (101) |

The request must be sent on a **timbre channel** (the timbre is the channel).
On the global channel the synth replies with `ERROR` code `2` (unknown command).

### 8.2 Block order

Blocks are streamed in ascending MSB order: 0..21, then `PresetName` (99),
then `Mixer` for the addressed timbre (100), then `Global` (101).

### 8.3 Targeted reads

```
Editor → [127, 1] value = block id       → synth streams that block, then DUMP_END
Editor → [127, 2] value = param address  → synth replies with that single param
```

### 8.4 Modification

Normal writes to `[block, lsb]` are applied immediately.

### 8.5 Notifications (synth → editor)

When the hardware UI changes a parameter, the synth sends the normal
`[block, lsb]` NRPN. There is no separate notification command.

### 8.6 Implementation notes

- The current firmware emits the dump in a blocking loop (~1.1 s at DIN5
  speed). The v2 implementation should pace it and allow cancellation.
- Editor → synth bulk writes must be paced; there is no ACK. The synth input
  ring is small (64 bytes).

---

## 9. Preset name (MSB = 99)

- `LSB = 0..11`, one character per message.
- Value = printable ASCII (32..126).
- Names are 12 characters; index 12 is NUL. Unused positions are space padded.
- Writing character 11 triggers the "name complete" propagation, so the synth
  can update its UI and MIDI metadata.

Example: `A` (65) at index 0 → `[99, 0] = 65`.

---

## 10. Mixer (100) and Global (101)

### 10.1 Mixer — MSB 100

`LSB = field` (0..12). The timbre is the MIDI channel; there is no timbre index
in the address.

| Field | Parameter |
|------:|-----------|
| 0 | out |
| 1 | compressorType |
| 2 | midiChannel |
| 3 | firstNote |
| 4 | lastNote |
| 5 | shiftNote |
| 6 | numberOfVoices |
| 7 | scalaEnable |
| 8 | scalaMapping |
| 9 | scaleScaleNumber |
| 10 | volume |
| 11 | pan |
| 12 | send |

### 10.2 Global — MSB 101

`LSB = field` (0..12). Global state is not tied to a timbre.

| Field | Parameter |
|------:|-----------|
| 0 | currentChannel |
| 1 | globalChannel |
| 2 | midiThru |
| 3 | tuning |
| 4 | levelMeterWhere |
| 5 | reverbPreset |
| 6 | reverbOutput |
| 7 | reverbLevel |
| 8 | userCC1 |
| 9 | userCC2 |
| 10 | userCC3 |
| 11 | userCC4 |
| 12 | MPE_inst1 |

`scope 0` includes the mixer for the addressed timbre and the global state.
String fields (`mixName`, `scalaScaleFileName`) are out of scope for v2.

---

## 11. Errors

`[127, 66]` (ERROR response) with value:

| Code | Meaning |
|-----:|---------|
| 0 | unknown parameter |
| 1 | value out of range |
| 2 | unknown command |
| 3 | unsupported block |

On a dump, an error stops the dump and is reported with `ERROR`.

---

## 12. What is removed

The following are no longer part of the NRPN protocol:

- the legacy PreenFM2 map (`paramMSB < 2`);
- `MSB = 1`, `LSB = 100..111` preset name;
- `MSB = 2`, `LSB = 44..67` operator envelope curves;
- `MSB = 2/3`, `LSB < 16` step-sequencer values;
- `MSB = 4`, `LSB = 0..3` timbre/MFX values;
- `MSB = 5/6` unused ranges;
- `[127, 126]` / `[127, 127]` dump and capability requests (replaced by §7);
- all `MidiTransform` paths for NRPN values.

The legacy `nrpnBase` / `nrpnStride` fields of `ParamBlock` are no longer
needed. Only `count` and `fieldCount` remain meaningful for NRPN addressing.

---

## 13. Implementation notes

### 13.1 Firmware

`preenfm3` is the primary target. `preenfm3-pvig` (legacy `firmware/Src/...`
model, no `Pfm3Param`) will implement v2 later; the spec is the target for both.

1. Add `Pfm3Nrpn.h` with block ids, control command ids, feature bits, and
   value encode/decode helpers.
2. Replace the `midiNrpnData()` dispatch with a router:
   - `MSB <= 24` and `MSB == 99..101` → block/element/field → `Pfm3Param` /
     `CommonParamId` / mixer / global;
   - `MSB == 127` → control command handler.
3. Replace `midiPatchParam()` with a block/parameter iterator for the dump,
   including `EnvCurve`, `ArpPattern` and `StepSeq`.
4. Add per-step arp storage (state/action) and the Arp2 lengths/seed.
5. Keep the CC mapping untouched (separate concern).
6. Remove the legacy NRPN index tables once the editor uses v2.
7. Pace the dump and allow cancellation; the current loop blocks the main loop.

### 13.2 Web editor

1. Add `src/midi/nrpnSpec.ts` with the same block table, or generate it from
   the firmware table to avoid duplication.
2. Replace `NRPN_COMMANDS` and the ad-hoc `sendLfo…`, filter, curve, and step
   helpers with a single `sendParam(block, element, field, value)`.
3. Rewrite `requestPatchDump` / the parser around `DUMP_BEGIN … DUMP_END`.
4. On connect, request capabilities and validate `CAP_PROTOCOL_VERSION`.

### 13.3 Single source of truth

The parameter metadata (type, min, max, step, names, native enum order) should
be generated from `Pfm3ParamTable.cpp` into a shared artifact consumed by the
editor. The NRPN spec fixes only addressing, encoding and protocol; it does not
duplicate the parameter list.

---

## 14. Worked examples

### 14.1 Set operator 3 frequency to 1.00

- Block: `Osc = 4`, element `2`, field `2`, `fieldCount = 4` → `LSB = 10`.
- Range `0..16` → `raw = round(1/16 * 16383) = 1024`.
- `valueMSB = 1024 >> 7 = 8`, `valueLSB = 1024 & 0x7F = 0`.

```
CC 99 = 4
CC 98 = 10
CC 6  = 8
CC 38 = 0
```

### 14.2 Set preset name character 0 to 'A'

```
CC 99 = 99
CC 98 = 0
CC 6  = 0
CC 38 = 65
```

### 14.3 Request a full preset dump

```
CC 99 = 127
CC 98 = 0
CC 6  = 0
CC 38 = 0
```

The synth replies with `[127, 64]`, the name and blocks, then `[127, 65]`.

---

## 15. Test vectors

| Action | Expected NRPN |
|--------|---------------|
| Operator 3 frequency = 1.00 | `[4, 10] = 1024` |
| Operator 3 frequency = 16.00 | `[4, 10] = 16383` |
| Matrix row 5 Dest1 = 0 | `[8, 18] = 0` |
| Preset name char 0 = 'A' | `[99, 0] = 65` |
| Mixer volume (timbre = channel) | `[100, 0]` |
| Global currentChannel | `[101, 0]` |
| ArpPattern Usr1 step 0 state = On | `[19, 0] = 1` |
| ArpPattern Usr1 step 0 action = Tie | `[19, 1] = 1` |
| ArpPattern Usr4 step 15 action = Slap | `[19, 127] = 5` |
| Arp2 Usr1 length = 8 | `[18, 4] = 8` |
| Arp2 Euclidean seed = 3 | `[18, 8] = 3` |
| Request preset dump | `[127, 0] = 0` |
| Request capabilities | `[127, 3] = 0` |

---

## 16. Reserved ranges

- MSB `22..98`: future data blocks.
- MSB `102..126`: reserved.
- Control command LSB `4..63`: reserved.
- Control response LSB `72..127`: reserved.
- Feature bits `9..27`: reserved.

Reserved values must be ignored by receivers and must not be reused.

---

## 17. Change log

- **v2** — clean-slate block-based address space; one MSB per block; preset name
  at 99, mixer at 100, global at 101; command domain at 127 (4 commands,
  8 responses, `CAP_` capability responses); unified 14-bit value encoding;
  scope 0 dump = preset + per-timbre mixer + global; per-step arp pattern;
  Arp2 lengths/seed.
- **v1** — legacy PreenFM2 NRPN map plus ad-hoc editor ranges (superseded).
