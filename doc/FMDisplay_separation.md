# FMDisplay editor separation

High-level plan for splitting the editor UI into a common `FMDisplayEditor`
plus per-engine editor classes:

- `FMDisplayEditor` — common editor utilities, shared pages, dispatch.
- `FMDisplayPfm3Editor` — PFM3-specific editor code.
- `FMDisplayMsfaEditor` — MSFA-specific editor code.

Every external call still goes to `FMDisplayEditor`. It handles the shared
parts itself and forwards engine-specific work to the active engine editor.

## Current state

- `FMDisplayEditor` contains both common editor code and all PFM3-specific
  code (menus, parameter handling, page rendering, operator/matrix/IM
  highlighting, LFO/oscilloscope refresh, copy/paste buffers).
- `FMDisplayMsfaEditor` already exists as a separate class, but it depends on
  `FMDisplayEditor` through a friend declaration and a `parent_` pointer.
- `FMDisplayEditor` already owns one `FMDisplayMsfaEditor` instance and
  delegates to it behind many scattered `isMsfaTimbre()` checks.
- Shared pages (Arp/Fx, Perf) currently still go through the PFM3 parameter
  path because they use `OneSynthParams`.
- Menu structs are named `Pfm3EditMenu` / `Pfm3OneButton` even though MSFA
  uses them too.

## Target structure

```
FMDisplay3
   |
   v
FMDisplayEditor  (common utilities + shared pages + dispatch)
   |-- FMDisplayPfm3Editor  (PFM3 pages and params)
   `-- FMDisplayMsfaEditor  (MSFA pages and params)
```

- `FMDisplayEditor` remains the only public editor entry point.
- It owns one editor instance per engine.
- `activeEditor()` selects the editor from the current timbre's engine.
- Engine editors implement a small common interface.
- Shared pages are handled by `FMDisplayEditor`, not by an engine editor.

## Common editor (`FMDisplayEditor`)

Keep here:

- TFT, `SynthState`, current timbre, preset-modified flags.
- Page navigation state (`mainPage`, `editPage`, `editPageSelected_`).
- Value-change and highlight counters, `hideParam_`, multiple-edition state.
- Common display helpers: header, preset name, popup, value formatting,
  button drawing, CPU/oscilloscope refresh.
- Shared pages: Arp/Fx and Perf, plus the buttons that switch between them.
- Dispatch to the active engine editor.
- Common encoder/button entry points called by `FMDisplay3` / listeners.

It should not contain PFM3 parameter logic, PFM3 menu tables, PFM3 page
rendering, or PFM3 operator/matrix highlighting.

## Engine editor interface

A small interface, called per note, page or block only — never per sample:

- `init(parent, synthState, tft)`
- `getEditMenu(page)`
- `refreshEditorByStep(...)`
- `displayParamValue(encoder, color)`
- `newParamValue(...)`
- `encoderTurned(...)`
- `encoderTurnedWhileButtonPressed(...)`
- `buttonPressed(button)`
- `buttonLongPressed(instrument, button)`
- `tempoClick()`
- `refreshOscilloscope*()` hooks (shape / envelope / LFO)
- preset/parameter accessors needed by the common editor.

## PFM3 editor (`FMDisplayPfm3Editor`)

Move from `FMDisplayEditor`:

- PFM3 menu tables and button/page state.
- `encoderTurnedPfm3()` / `encoderTurnedPfm2()`.
- PFM3 `newParamValue()`, `displayParamValue()`, and PFM3 refresh steps.
- Operator / matrix / modulator page checks.
- Operator and IM highlighting.
- PFM3 copy/paste buffers.
- PFM3 oscilloscope backgrounds (shape, LFO, envelope).

## MSFA editor (`FMDisplayMsfaEditor`)

Already separate; make it implement the same interface:

- MSFA menus and parameter mapping.
- MSFA value display and refresh steps.
- MSFA operator/feedback highlighting and copy/paste.
- MSFA program access.
- Remove the need for `friend class FMDisplayMsfaEditor`; expose only the
  common utilities it needs from `FMDisplayEditor`.

## Shared pages

- Arp/Fx and Perf are shared between engines and still use `OneSynthParams`.
- Keep them in `FMDisplayEditor` (or a small shared helper owned by it).
- `FMDisplayEditor` decides whether the current page is shared or belongs to
  the active engine editor.
- Rename the `Pfm3EditMenu` / `Pfm3OneButton` structs to generic names if they
  are kept shared.

## Dispatch model

- `FMDisplayEditor` receives all calls from `FMDisplay3`, `SynthState`,
  encoders and buttons, as today.
- It handles shared pages and common chrome itself.
- For engine pages it calls `activeEditor()->...`.
- `activeEditor()` uses the current timbre's engine type, so there is one
  place that knows PFM3 vs MSFA vs a future engine.
- The many `isMsfaTimbre()` branches in `FMDisplayEditor.cpp` are replaced by
  this single dispatch.
- Engine selection remains per timbre.

## Migration order

1. Define the engine-editor interface and make `FMDisplayMsfaEditor` implement
   it.
2. Extract the PFM3-specific code from `FMDisplayEditor` into
   `FMDisplayPfm3Editor`.
3. Replace the scattered `isMsfaTimbre()` branches with `activeEditor()`
   dispatch.
4. Move the shared Arp/Fx and Perf pages fully into the common editor.
5. Remove the friend/back-pointer coupling between the editors; use accessors.
6. Test both engines on every editor page after each step.

## Risks

- `FMDisplayEditor.cpp` is large; the split must not change behaviour.
- Shared state (page index, highlight counters, preset-modified flags) must
  stay consistent across editors.
- Shared pages must not accidentally become engine-specific.
- Parameter display and copy/paste behaviour must be preserved.
- RAM/flash are tight; avoid adding per-engine copies of shared buffers.
