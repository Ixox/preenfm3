# Screen editor separation

The editor UI is split into a common `ScreenEditor`, a generic
`EditorParamPage`, one editor per engine and one editor for pages shared by all
engines:

- `ScreenEditor` — common chrome, page routing and dispatch.
- `EditorParamPage` — generic menu/encoder page mechanics over
  `Pfm3Preset`. Not instantiated directly.
- `EditorPfm3 : EditorParamPage` — PFM3 engine pages.
- `EditorMsfa` — MSFA engine pages.
- `EditorCommon : EditorParamPage` — pages shared by all engines
  (Arp/Fx).

Every external call still goes to `ScreenEditor`.

## Structure

```
ScreenCoordinator
   |
   v
ScreenEditor  (common utilities + page routing)
   |-- EditorPfm3 : EditorParamPage   (PFM3 pages 0–3, 5)
   |-- EditorMsfa                    (MSFA pages 0–1)
   `-- EditorCommon : EditorParamPage (shared Arp/Fx)
```

- `ScreenEditor` remains the only public editor entry point.
- It owns one engine editor per engine plus the shared-page editor.
- `activeEditor()` returns the engine editor when it owns the current page,
  otherwise the shared editor when the engine maps the page through
  `getCommonPage()`.
- `editorForEngine()` always returns the current engine's editor (used for
  algorithm drawing, engine name and `newTimbre`).
- Engine editors implement a small common interface; flat-param mechanics live
  in `EditorParamPage`.
- Arp/Fx is shared; Perf is PFM3-only.

## Page ownership

`EditorEngine` declares its pages explicitly:

- `getEngineMenu(page)` — menu for an engine-owned page, or `nullptr`.
- `getCommonPage(page)` — maps an engine page to a shared page
  (`COMMON_PAGE_ARP_FX`), or `COMMON_PAGE_NONE`.

Current mappings:

| Engine | Engine pages | Shared page |
|--------|--------------|-------------|
| PFM3   | 0 FM, 1 Oper, 2 Matrix, 3 Modul, 5 Perf | 4 → Arp/Fx |
| MSFA   | 0 Global, 1 Oper | 2 → Arp/Fx |

`ScreenEditor::getMainMenu(page)` resolves the engine menu first, then the
shared menu. `activeEditor()` uses the same mapping to pick the editor that
draws and edits the page.

## Common editor (`ScreenEditor`)

Keep here:

- TFT, `SynthState`, current timbre, preset-modified flags.
- Page navigation state (`mainPage`, `editPage`, `editPageSelected_`).
- Common display helpers: header, preset name, popup, CPU/oscilloscope
  refresh.
- Dispatch to the active editor and the engine editor.
- Recovery when the current page is not valid for the current engine: fall
  back to the main menu instead of dereferencing a null menu.

It does not contain PFM3 parameter logic, PFM3 menu tables, PFM3 page
rendering, or PFM3 operator/matrix highlighting.

## `EditorParamPage`

Generic menu/encoder page mechanics over `Pfm3Preset`:

- `encoderTurned()` / `encoderTurnedPfm2()`.
- `displayParamValue()`, `drawEditorButton()`, `drawParameterName()` generic
  paths.
- `newPfm3ParamValue()` generic path (shared Arp/Fx special cases included).
- Page select/toggle, `tempoClick()` value counters, `resetHideParams()`.
- Value formatting (`updateEncoderValueWithoutCursor`).

State: `hideParam_`, `valueChangedCounter_`, `lfoFreqPreviousValue`.

## PFM3 editor (`EditorPfm3`)

- PFM3 menu tables (FM, Oper, Matrix, Modul, Perf).
- Operator / matrix / modulator page checks and rendering.
- Operator, IM and oscillator-mix highlighting.
- PFM3 copy/paste buffers and multiple-operator edition.
- PFM3 oscilloscope backgrounds (shape, LFO, envelope).
- PFM3-specific `newPfm3ParamValue()` (operator rows, algorithm refresh).
- `getCommonPage(4) = COMMON_PAGE_ARP_FX`; Perf (page 5) stays here.

## MSFA editor (`EditorMsfa`)

- MSFA menus and parameter mapping (pages 0–1: Global, Oper).
- MSFA value display and refresh steps.
- MSFA operator/feedback highlighting and copy/paste.
- MSFA program access.
- `getCommonPage(2) = COMMON_PAGE_ARP_FX`.
- Uses public `ScreenEditor` accessors only; no friend declaration.

## Shared editor (`EditorCommon`)

- Owns the Arp/Fx menu tables (`pfm3ButtonArpeggiator`, Fx1/Fx2).
- `getCommonMenu(COMMON_PAGE_ARP_FX)` returns the menu.
- Uses `EditorParamPage` for all page handling; no algorithm, no oscilloscope
  background.
- `getEngineType()` returns `SYNTH_ENGINE_COUNT` so `editorForEngine()` never
  selects it.

## Dispatch model

- `ScreenEditor` receives all calls from `ScreenCoordinator`, `SynthState`,
  encoders and buttons.
- It handles common chrome itself.
- For engine pages it calls `activeEditor()->...`.
- Algorithm drawing, engine name and `newTimbre` go to `editorForEngine()`.
- Engine selection remains per timbre.
- Adding an engine means adding one `EditorEngine` subclass and its menu
  tables; shared pages need no engine-specific code.

## Risks

- `EditorPfm3.cpp` is still large; further splits must not change
  behaviour.
- Page index and preset-modified flags are shared; per-editor counters stay in
  their editor.
- Keep `isInMatrixPage()` engine-aware: MSFA page 2 is Arp/Fx, not the PFM3
  matrix.
- Parameter display and copy/paste behaviour must be preserved.
- RAM/flash are tight; avoid adding per-engine copies of shared buffers.
