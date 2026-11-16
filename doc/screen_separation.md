# Screen editor separation

High-level plan for splitting the editor UI into a common `ScreenEditor`
plus per-engine editor classes:

- `ScreenEditor` — common editor utilities and dispatch.
- `ScreenPfm3Editor` — PFM3-specific editor code.
- `ScreenMsfaEditor` — MSFA-specific editor code.

Every external call still goes to `ScreenEditor`. It handles common chrome,
page navigation and dispatch. The shared Arp/Fx page is hosted by
`ScreenPfm3Editor`; MSFA page 2 redirects there.

## Status

Step 1 done:

- `EngineEditor` interface added in `ScreenEditor.h`.
- `ScreenMsfaEditor` implements it.
- `ScreenPfm3Editor` added and implements it.
- `ScreenEditor` dispatches `newParamValue`, `displayParamValue`,
  `refreshEditorByStep` and `tempoClick` to the active editor.

Step 2 done:

- `encoderTurned*`, `button*`, `refreshOscillator*` and `checkOperatorNumber`
  dispatch through the engine-editor interface.
- The PFM3 menu/parameter tables, encoder/button logic, copy/paste buffers
  and oscilloscope backgrounds now live in `ScreenPfm3Editor`.
- `ScreenEditor` keeps the common chrome, page navigation and dispatch.
- `currentEditMenu`/`mainMenuName` select the menu through
  `pfm3Editor_->getEditMenu()`.
- `getEditPageMultiplier()` was removed: it had no callers.

Next: replace the remaining `isMsfaTimbre()` branches with a single
`activeEditor()` dispatch. Keep the MSFA page 2 → PFM3 Arp/Fx redirect:
Arp/Fx is a shared parameter page whose UI lives in `ScreenPfm3Editor`.

## Current state

- `ScreenEditor` is the common entry point: common chrome, page navigation,
  preset-modified flags and dispatch.
- `ScreenPfm3Editor` owns the PFM3 menu/parameter tables, page rendering,
  operator/matrix/IM highlighting, LFO/oscilloscope refresh and copy/paste
  buffers.
- `ScreenMsfaEditor` owns MSFA pages 0–1 (Global, Oper). It uses public
  `ScreenEditor` accessors only; the friend declaration is gone.
- MSFA page 2 (Arp/Fx) is intentionally routed to `ScreenPfm3Editor`, because
  the shared Arp/Fx page is implemented on the PFM3 `OneSynthParams` path.
- Perf is PFM3-only (matrix P1–P4) and is not offered for MSFA timbres.
- `EditorEditMenu` / `EditorOneButton` are shared by both editors.

## Target structure

```
ScreenCoordinator
   |
   v
ScreenEditor  (common utilities + dispatch)
   |-- ScreenPfm3Editor  (PFM3 pages, params, shared Arp/Fx)
   `-- ScreenMsfaEditor  (MSFA pages and params)
```

- `ScreenEditor` remains the only public editor entry point.
- It owns one editor instance per engine.
- `activeEditor()` selects the editor from the current timbre's engine.
- Engine editors implement a small common interface.
- Arp/Fx is hosted by `ScreenPfm3Editor` for both engines; Perf is PFM3-only.

## Common editor (`ScreenEditor`)

Keep here:

- TFT, `SynthState`, current timbre, preset-modified flags.
- Page navigation state (`mainPage`, `editPage`, `editPageSelected_`).
- Common display helpers: header, preset name, popup, value formatting,
  button drawing, CPU/oscilloscope refresh.
- No shared-page implementation; it only dispatches the page to the owning
  editor.
- Dispatch to the active engine editor.
- Common encoder/button entry points called by `ScreenCoordinator` / listeners.

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

## PFM3 editor (`ScreenPfm3Editor`)

Implemented:

- PFM3 menu tables and button/page state.
- `encoderTurned()` / `encoderTurnedPfm2()`.
- PFM3 `newParamValue()`, `displayParamValue()`, and PFM3 refresh steps.
- Operator / matrix / modulator page checks.
- Operator and IM highlighting.
- PFM3 copy/paste buffers.
- PFM3 oscilloscope backgrounds (shape, LFO, envelope).
- Shared Arp/Fx page (MSFA page 2 redirects here).
- Perf page (PFM3 only; matrix P1–P4).

## MSFA editor (`ScreenMsfaEditor`)

Implemented:

- MSFA menus and parameter mapping (pages 0–1: Global, Oper).
- MSFA value display and refresh steps.
- MSFA operator/feedback highlighting and copy/paste.
- MSFA program access.
- MSFA page 2 (Arp/Fx) redirects to `ScreenPfm3Editor`.
- Uses public `ScreenEditor` accessors only; no friend declaration.

## Shared pages

- Arp/Fx is shared between engines and still uses `OneSynthParams`.
- Its menu tables and page handling live in `ScreenPfm3Editor`; MSFA page 2
  redirects to that editor instead of duplicating the page.
- Perf is PFM3-only: it controls matrix P1–P4 parameters, which MSFA does not
  have.
- `EditorEditMenu` / `EditorOneButton` are shared by both editors.

## Dispatch model

- `ScreenEditor` receives all calls from `ScreenCoordinator`, `SynthState`,
  encoders and buttons, as today.
- It handles common chrome itself.
- For engine pages it calls `activeEditor()->...`.
- MSFA page 2 (Arp/Fx) is dispatched to `ScreenPfm3Editor` deliberately.
- `activeEditor()` uses the current timbre's engine type, so there is one
  place that knows PFM3 vs MSFA vs a future engine.
- The many `isMsfaTimbre()` branches in `ScreenEditor.cpp` are replaced by
  this single dispatch.
- Engine selection remains per timbre.

## Migration order

1. Done: engine-editor interface.
2. Done: extract PFM3 code into `ScreenPfm3Editor`.
3. Next: replace the remaining `isMsfaTimbre()` branches with `activeEditor()`
   dispatch.
4. Keep Arp/Fx hosted in `ScreenPfm3Editor` (MSFA page 2 redirects to it);
   keep Perf PFM3-only.
5. Done: friend/back-pointer removed; public accessors only.
6. Test both engines on every editor page after each step.

## Risks

- `ScreenPfm3Editor.cpp` is large; further splits must not change behaviour.
- Page index and preset-modified flags are shared; per-editor counters stay in
  their editor.
- Arp/Fx is intentionally hosted in the PFM3 editor; avoid duplicating it per
  engine.
- Keep `isInMatrixPage()` engine-aware: MSFA page 2 is Arp/Fx, not the PFM3
  matrix.
- Parameter display and copy/paste behaviour must be preserved.
- RAM/flash are tight; avoid adding per-engine copies of shared buffers.
