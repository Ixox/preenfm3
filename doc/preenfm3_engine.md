# PFM3 engine

This document explains how the PFM3 sound engine works and what the editor
controls do. It is written for musicians and patch creators, not for developers.

## Overview

The preenFM3 has **6 timbres**. Each timbre runs one engine: **PFM3** (default)
or **DX7** (DX7-compatible). This document covers PFM3.

PFM3 is a 6-operator FM synthesizer:

- 6 sine operators, each with its own envelope,
- 6 envelopes (time, level, curve),
- a 12-row modulation matrix,
- 3 LFOs, 2 free envelope LFOs, 2 step sequencers,
- 2 effects (FX1, FX2),
- a post-FX amplitude gate,
- 20 voices shared by all timbres (a voice belongs to one timbre at a time).

Audio is computed per block at 47,916 Hz. Notes, matrix values and LFOs are
updated per block, not per sample.

## Signal path

```
MIDI note on/off
      |
      v
voice allocation (mono / poly / unison, glide)
      |
      v
6 operators (sine + envelope)  <-- algorithm decides who modulates whom
      |
      v
algorithm mix -> per-operator volume/pan
      |
      v
FX1 (per voice)
      |
      v
voices summed into the timbre mix
      |
      v
GateFX (per timbre) -> FX2 (per timbre) -> timbre volume/pan -> main mix
```

FX1 runs inside each voice before the voices are mixed. GateFX and FX2 run on
the summed timbre mix, in that order. The matrix, LFOs and step sequencers
modulate operator frequency, modulation index, mix, pan and FX parameters.

## FM synthesis in one minute

An **operator** is a sine oscillator with an envelope. It can play two roles:

- **Carrier**: its output is audible.
- **Modulator**: its output is not heard directly; it modulates another
  operator's phase, which creates new harmonics.

The **modulation index (IM)** sets how strongly a modulator affects its target.
Low IM gives a nearly pure sine; higher IM gives a brighter, more complex
spectrum. The **envelope** shapes the operator's amplitude over time (attack,
decay, sustain, release), so FM sounds can evolve.

An **algorithm** is the wiring: which operators modulate which, and which ones
are carriers. The Engine page shows the current algorithm as a diagram.

### Example: Algorithm 1

Algorithm 1 uses three operators: **op1 is the carrier**, and **op2 and op3 are
modulators**. You hear op1; op2 and op3 shape its timbre. The Engine page draws
the exact routing, and the Modulation pages set IM2/IM3 (the modulation depth of
the two modulators).

A simple recipe:

1. Start from Algorithm 1.
2. Give op1 a slow attack and a long release.
3. Set IM2 low and IM3 higher.
4. Modulate IM2 with an envelope or LFO in the matrix.
5. Add a second carrier later (another algorithm) if you want a fuller sound.

## The front panel

While editing, the screen is laid out as follows:

```
+---------------------------------------------------------------+
| Instrument number                              Patch name     |
+---------------------------------------------------------------+
| Currently edited section (e.g. "Oscillator")                  |
+---------------------------------------------------------------+
|       Param 1       |       Param 2       |       Param 3      |
|       Param 4       |       Param 5       |       Param 6      |
+---------------------------------------------------------------+
|                                     |                         |
|           Oscilloscope              |         FM algo         |
|              (2/3)                  |          (1/3)          |
+---------------------------------------------------------------+
|       Button 1      |       Button 2      |       Button 3     |
|       Button 4      |       Button 5      |       Button 6     |
+---------------------------------------------------------------+
```

- The **six encoders** under the screen edit the six parameters, arranged in two
  rows of three.
- The **six buttons** below the screen perform the actions printed on their
  labels, arranged in two rows of three. Their labels change with the current
  page.
- The left two thirds of the display show the **oscilloscope**. It is replaced
  by the envelope, LFO or oscillator-shape display while the corresponding page
  is open.
- The right third always shows the **FM algorithm** diagram.

### Navigation

- **Buttons 1..6** select a page or an operator, depending on the page.
- **Menu** opens the global menu (Preset, Mixer, Seq, SD, Settings).
- **Mixer**, **Edit** and **Sequencer** switch between the main screens.
- **Prev/Next instrument** (short press) change the edited timbre.
- **Prev + Next instrument** at the same time resets the audio engine. Use it if
  a note is stuck.

### Button shortcuts

- **Menu + turn an encoder**: apply the same change to all operators of the same
  type. On envelope pages, all carriers or all modulators are updated together;
  on the IM pages, IM or IM velocity is applied to all operators; on the
  oscillator mix/pan pages, all carriers are updated.
- **Prev + turn an encoder**: reset the parameter to its default preset value.

### Copy and paste

Copy/paste works on the operator, envelope, matrix-row and LFO pages.

- **Long press Prev**: copy the current item. The screen shows `Copy Osc`,
  `Copy Env`, `Copy Mtx` or `Copy LFO`.
- **Long press Next**: paste it into the current item. The screen shows
  `Paste Osc`, `Paste Env`, `Paste Mtx` or `Paste LFO`.
- Paste is only allowed on a compatible page; otherwise the screen shows
  `Cannot`.
- For envelopes, the copy stores the source operator number; the paste copies
  time, level and curve to the current operator.
- For matrix rows, the whole row (source, multiplier, destinations) is copied.

## Editor pages

The editor is organised by the six first-level buttons: **FM**, **Oper**,
**Matrix**, **Modul**, **Arp/Fx** and **Perf**. Under each button, the pages
show six encoders; the encoder order is listed below.

### FM

- **Engine** — two pages:
  - *Engine*:
    - **Algorithm**: operator routing (1..32), with the diagram on the right.
    - **Velocity**: how note velocity affects the sound.
    - **Play mode**: poly, mono or unison.
  - *Mono*:
    - **Glide**: glide type (mono/unison).
    - **Speed**: glide speed (mono/unison).
    - **Spread**: stereo spread (unison only).
    - **Detune**: voice detune (unison only).
- **IM** — two pages:
  - *Modulation Index*: IM1..IM6, the FM depth of each operator.
  - *Velocity*: velocity sensitivity of IM1..IM6.
- **Mix** — two pages:
  - *Op Mixer 1*: mix and pan for op1..op3.
  - *Op Mixer 2*: mix and pan for op4..op6.

### Oper

- **Shape** — one page (*Oscillator*):
  - **Shape**: operator waveform.
  - **FType**: frequency type (ratio or fixed).
  - **Freq**: frequency ratio or fixed frequency.
  - **FTune**: fine tuning.
- **Env1** — two pages:
  - *Env Time*: attack, decay, sustain and release times.
  - *Env Curve*: attack, decay, sustain and release curves.
- **Env2** — one page (*Env Level*):
  - attack, decay, sustain and release levels.
- **Operator select**: use the second row of action buttons (buttons 4–6) to
  select the operator; each button toggles between a pair (1/2, 3/4, 5/6).

### Matrix

- **Matrix 1..12** — one page per row:
  - **Source**: what drives the row (LFO1..3, Free Env1/2, Seq1/2, ...).
  - **Mul**: multiplier (`Mtx1..Mtx4` chain rows).
  - **Dest 1** / **Dest 2**: what the row modulates.
- **Row select**: use the six action buttons to select the matrix row.

### Modul

- **LFOs** — three pages (LFO 1..3):
  - **Shape**: Sin, Saw, Tria, Squa, Rand, Brwn, Wndr, Flow.
  - **Sync**: LFO sync (off/MIDI clock/...).
  - **Freq**: LFO frequency.
  - **KSyn**: key sync/ramp.
  - **Bias**: offset added to the LFO.
  - **Phase**: starting phase of the LFO (0..1). Offsets the LFOs relative to
    each other; also offsets the phase when synced to MIDI clock.
- **Envs** — two pages:
  - *Free Env 1*: Attk, Deca, Sust, Rele (ADSR).
  - *Free Env 2*: Sile, Attk, Deca, Loop (loop off, loop silence, loop attack).
- **Steps** — two pages (Step Seq 1/2):
  - **Sync**: step sync source.
  - **BPM**: step-sequencer tempo.
  - **Gate**: gate length of each step.
  - **Step**: select the step (0..15).
  - **Value**: value of the selected step (0..15).
- **Midi** — two pages (Midi note 1/2):
  - **Before**: curve before the break note.
  - **Break note**: the pivot note.
  - **After**: curve after the break note.

All modulators (LFO1..3, Free Env1/2, Seq1/2) are matrix sources.

### Arp/Fx

- **Arp** — two pages:
  - *Arpeggiator 1*: Clock, BPM, Direction, Octave, Division, Duration.
  - *Arpeggiator 2*: Pattern, Latch, and three user-pattern steps.
- **Fx1** — one page (*Voice FX*):
  - **Type**, **Param 1**, **Param 2**, **Param 3**.
  - Applied per voice, before the timbre mix.
- **Fx2** — one page (*Paraphonic FX*):
  - **Type**, **Param 1**, **Param 2**, **Param 3**.
  - Applied once per timbre, after GateFX.

### Perf

- **Performance** — one page:
  - **p1..p4**: four performance controls, routed by the patch creator via the
    matrix (`Perf1..Perf4`).
  - **FX1 param 1/2**: the two main FX1 parameters for quick live tweaking.
  - The performance values are stored in the preset (`performance1`).

## GateFX

The gate is a **post-FX amplitude gate** applied to the whole timbre output. It
is not one of the FX1/FX2 effect types.

It is controlled only through the matrix:

1. Open a Matrix row.
2. Set **Dest 1** or **Dest 2** to **Gate**.
3. Choose a **Source** (LFO, step sequencer, envelope, velocity, ...).
4. Set **Mul** to the desired depth.

At render time the engine reads the `MAIN_GATE` destination from the last
played voice and applies it to the summed timbre mix, **before FX2**. This makes
rhythmic gating, tremolo and envelope-controlled gating possible without using
an FX slot.

## Presets and mixers

A preset/mixer slot is 1024 bytes:

- the engine writes its blob (parameters, name, and shared data if needed),
- the storage layer writes a 4-byte magic at the end of the slot,
- on load, the magic selects the engine; an unknown magic is ignored and the
  current preset is kept.

PFM3 uses the legacy `FlashSynthParams` layout, so existing PFM3 banks remain
compatible. DX7 uses its program blob plus the shared timbre parameters.

## Tips

- Start with a simple algorithm and one carrier; add modulators one by one.
- Use envelopes on IM for movement; use the matrix for slow LFO modulation.
- `Mul = 0` disables a matrix row; use it as a safe "off" switch.
- Use the Gate destination for rhythmic gating instead of an FX slot.
- Copy/paste operators, envelopes and matrix rows to build patches faster.
- The Mixer screen controls how the timbre sits in the mix; the preset stores
  the sound, the mixer stores the mix.
