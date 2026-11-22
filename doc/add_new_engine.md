# Adding a new engine

This is the short checklist for adding an engine beside PFM3 and DX7.
The examples use **NewEngine**. A complete working third engine lives in
`firmware/src/app/engine/byos/` (see its `README.md`).

An engine has two sides:

- a **sound engine** (`SynthEngine`) that renders voices and owns its params;
- an **editor** (`EditorEngine`) that draws and edits its pages.

## Files to add

| File | Purpose |
|------|---------|
| `firmware/src/app/engine/newengine/NewEngine.h/.cpp` | `NewEngine : public SynthEngine` |
| `firmware/src/app/engine/newengine/EditorNewEngine.h/.cpp` | `EditorNewEngine : public EditorEngine` (or `EditorParamPage`) |
| optional `NewEngineParams.h` / `NewEngineVoiceState.h` | Engine params and per-voice state |

## Sound engine

Implement the core `SynthEngine` methods:

- `init(timbre, synthState)`
- `afterNewParamsLoad()`
- `bindVoice(voice)`
- `noteOn()` / `noteOff()`
- `voicesNextBlock()`
- `prepareBlock()`
- `glide()`
- `gateFx()`
- `setMatrixSource()` / `setMatrixSourceMPE()`
- `resetMatrixDestination()`
- `setMatrixPolyAfterTouch()`
- `noteOnMpe()` / `noteOffMpe()`
- `getStorageMagic()`, `getStorageNameOffset()`, `getStorageDefaultName()`
- `getStorageBlob()` / `loadStorageBlob()`

Most other `SynthEngine` methods have default no-op implementations and can be
ignored until needed (`setParamValue`, `serializePfm3Preset`,
`loadDx7Patch`, MIDI clock, LFO helpers, ...).

Per-voice state: keep one static array indexed by `voice->slot()` and bind it
from `bindVoice()`, like `Pfm3VoiceState` / the DX7 voice state.

The engine owns its own parameter state. `Timbre` only owns
`TimbreSharedParams` (arp, FX1/FX2, preset name); it never touches
engine-specific parameters.

## Arp/Fx (shared)

Arp, FX1 and FX2 are timbre-wide, not engine features. They live in
`TimbreSharedParams` and are edited by the shared `EditorCommon` page.

If the engine should expose them:

- In the editor, return `COMMON_PAGE_ARP_FX` from `getCommonPage(page)` for the
  main page that should host the shared page (see Editor below).
- In `voicesNextBlock()`, after rendering each voice, call
  `voice->fxAfterBlock(matrixFilterFrequency, matrixFilterParam2,
  matrixFilterAmp)`. Use `0.0f, 0.0f, 0.0f` when the engine has no per-voice
  matrix filter modulation (like DX7/BYOS). FX1 keeps per-voice state and must
  run before `Timbre::voicesToTimbre()` sums the voices.
- FX2 and the global reverb/FX bus need nothing: `Synth::buildNewSampleBlock()`
  calls `Timbre::applyTimbreFx()` and `FxBus` for every engine.
- In `getStorageBlob()` / `loadStorageBlob()`, include the `TimbreSharedParams`
  argument (`shared`) in the blob and restore it with
  `Timbre::setSharedParams()`. This keeps Arp/Fx when a preset/mixer is saved
  and loaded, or when the timbre switches engine.

Only call `voice->fxAfterBlock()` if you implement the Arp/Fx page. Without
that page the shared FX type stays `None` for the engine, so the call is
useless work. The `TimbreSharedParams` block should still be stored/restored:
it is shared timbre state and can also come from MIDI or from another engine.

## MIDI

`MidiDecoder` stays the generic MIDI device layer: byte parsing, channel/timbre
routing, notes, MPE, clock, bank/program change, mixer/sequencer, shared
Arp/FX CCs and the DIN5/USB transport. The engine only maps its own parameters.
Everything below is optional; leave the defaults and the engine works with
notes and clock but has no parameter CC/NRPN.

### Receiving

Override `midiData(int timbre, const MidiData& data)` when the engine has its
own MIDI parameter map. `MidiData` carries the event type (CC, user CC, NRPN,
aftertouch, pitch bend, MPE aftertouch/pitch bend/slide), the channel and the
relevant values. Return `true` when the event was consumed.

The default returns `false`, so a new engine ignores parameter MIDI until it
opts in. Shared controls (Arp, FX1/FX2, mixer, sequencer) are handled by
`MidiDecoder` and `Timbre::midiData()`: `Timbre::midiData()` intercepts
`MIDI_DATA_CC` events with `decodeSharedControlChange()` +
`getSharedParamRowEncoder()` using the ids in `SharedParams.h` and applies them
through `setSharedParamFromMidi()`. Only unhandled CCs are forwarded to the
engine. `MIDI_DATA_USER_CC` bypasses the shared decode so user CCs keep
priority.

Notes and clock are generic. Pitch bend, aftertouch, NRPN and MPE are forwarded
to the engine through `midiData()`.

When applying a received CC/NRPN, propagate it with
`SynthState::propagateNewEngineParamValueFromExternal()`. That applies the
change and refreshes the UI, but `MidiDecoder` ignores the external variant, so
the incoming message is not echoed back to the controller. Editor changes use
`propagateNewEngineParamValue()` and do emit feedback through
`midiParamFeedback()`.

There is no shared parameter table to plug into: the PFM3 CC/NRPN map lives
entirely in `Preenfm3Engine`. Define your own CC/NRPN numbers and scaling in the
new engine, and keep them stable once published so external controllers and
editors keep working.

### Emitting

When a parameter changes from the editor, `MidiDecoder` can echo it to the
external controller/DAW. Implement:

- `midiParamFeedback(int paramId, int context, float newValue,
  MidiParamMessage &out) const` — generic engine parameter change. Use the same
  `paramId`/`context` that you pass to
  `SynthState::propagateNewEngineParamValue()`; fill `out` and return `true`.
  Return `false` when the parameter has no CC/NRPN representation.

`MidiParamMessage` carries `isNrpn`, `cc`/`ccValue` (7-bit) or
`nrpn`/`nrpnValue` (14-bit). `MidiDecoder` adds the channel, the
`MIDICONFIG_SENDS` CC/NRPN mode, the `lastSentCC` dedupe and the DIN5/USB
transport, so the engine must not send MIDI itself.

`midiPfm3ParamFeedback(row, encoder, value, out)` is the PFM3-only variant used
by `newPfm3ParamValue()`. A new engine can ignore it; its editor changes arrive
through `newEngineParamValue()` and the generic hook above.

### Patch dump for a PC editor

`midiPatchParam(int timbre, int index, MidiParamMessage &out) const` enumerates
the whole patch as NRPN messages. `MidiDecoder` calls it when a PC editor sends
NRPN MSB=127 / LSB=127 (`SEND_PATCH_AS_NRPN`), starting at `index = 0` and
stopping at the first `false`:

```cpp
Timbre *t = synth->getTimbre(timbre);
int index = 0;
struct MidiParamMessage message;
while (t->midiPatchParam(timbre, index++, message)) {
    sendNrpn(message.nrpn, message.nrpnValue);
}
```

Implement it if the engine should be editable from a PC editor that requests
the whole patch. The enumerator may return as many entries as needed; the
decoder has no fixed patch size. Return the preset name and any extra state
(step sequencer, ...) as additional stable NRPN indexes, like PFM3 does. Keep
those indexes unchanged once published: they are the external protocol. If the
engine has no live NRPN protocol, leave the default (nothing is sent).

This is separate from `getStorageBlob()` / `loadStorageBlob()`, which handle
files and preset slots. `midiPatchParam()` is only for a live NRPN dump to an
editor.

## Editor

Add `EditorNewEngine` and implement the required `EditorEngine` methods:

- `getEngineType()` → `SYNTH_ENGINE_NEWENGINE`
- `getEngineName()` → display name
- `getEngineMenu(page)` → menu for each engine page, or `nullptr`
- `init(parent, synthState, tft)` → must call `parent->insertEditor(this)`
- `drawEditorButton()`
- `drawParameterName()`
- `displayParamValue()`
- `tempoClick()`
- `encoderTurned()`
- `buttonPressed()`
- `refreshOscilloBG()`

Optional hooks:

- `drawAlgorithm()`, `drawPageHeader()`, `preparePage()`. If the engine draws
  its algorithm diagram, put the command-stream tables in an `Editor<Engine>Algo`
  helper class (see `EditorPfm3Algo`, `EditorDx7Algo`) and call
  `tft_->drawAlgoTable(Editor<Engine>Algo::table(algo))`.
- `newEngineParamValue()` for engine-specific param feedback
- `getCommonPage(page)` to expose the shared Arp/Fx page as a main-menu
  button. Return `COMMON_PAGE_ARP_FX` for the page index that should host it
  (for example `1` for the second main button); `ScreenEditor` then uses the
  shared `EditorCommon` editor (`Arp`, `Fx1`, `Fx2`). Only add this if the
  engine also calls `voice->fxAfterBlock()` (see "Arp/Fx (shared)").
- `buttonLongPressed()`, `newTimbre()`

If the engine reuses the flat `Pfm3Preset` layout, derive from
`EditorParamPage` and override only the engine-specific page behavior.
Otherwise derive from `EditorEngine` and own the parameter pages.

`ScreenEditor` routing is automatic: `activeEditor()` uses
`getEngineMenu()` / `getCommonPage()`, and `editorForEngine()` uses
`getEngineType()`.

## Register the engine

1. `Common.h` — add `SYNTH_ENGINE_NEWENGINE` to `enum SynthEngineType`
   **before** `SYNTH_ENGINE_COUNT`. Existing values must not change.
2. `SynthEngineFactory.h/.cpp` — include `NewEngine.h`, add a static
   `NewEngine newEngines_[NUMBER_OF_TIMBRES]`, init it in `init()` and return
   it from `get()` for `SYNTH_ENGINE_NEWENGINE`. The current `get()` falls
   back to PFM3 for unknown types, so add an explicit branch. Add a case to
   `getEngineDisplayName()` (keep it short for the TFT big font, e.g. `"NEW"`).
   If the engine needs one-time DSP tables, add `NewEngine::initGlobal()` and
   call it from `SynthEngineFactory::initDsp()`.
3. `PersistenceMagic.h` — add a `PRESET_NEWENGINE` magic number.
4. `SynthEngineFactory.cpp` — add
   `{ PRESET_NEWENGINE, SYNTH_ENGINE_NEWENGINE }` to `storageMagics[]`.
   `Timbre` and the storage layer then resolve the engine automatically.
5. `ScreenMixer.cpp` — no change: `engineDisplay[]` is built from
   `SynthEngineFactory::getEngineDisplayName()`.
6. `ScreenEditor.cpp` — include `EditorNewEngine.h`, add a static
   `EditorNewEngine newEngineEditorInstance`, and call its `init()` in
   `ScreenEditor::init()`.

CMake globs the firmware sources, so no build-file change is needed.

## Minimal smoke test

- Select NewEngine in the mixer.
- Play notes, check voice allocation and note off.
- Open the editor, check every page and encoder.
- Switch PFM3 ↔ NewEngine on the same timbre.
- Save a preset, reload it, check the engine is restored.
- Check MIDI note/CC/matrix sources.
- If you implemented the MIDI hooks: verify a parameter CC/NRPN changes the
  engine and the matching feedback CC/NRPN is sent. If `midiPatchParam()` is
  implemented, request a patch dump with NRPN 16383 and reload it in the PC
  editor.
