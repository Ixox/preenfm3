# Drawing the DX7 algorithm on the preenFM3 TFT

This note records how the PFM3 algorithm diagram is drawn and what is needed
to draw the 32 DX7 (MSFA) algorithms the same way.

## 1. PFM3 drawing pipeline

The algorithm diagram is not drawn directly to the TFT. It is rendered into an
80×100 off-screen buffer and then copied by DMA2D.

```
FMDisplayEditor::refreshEditorByStep()
        -> tft_->drawAlgo(algo)
             TftDisplay::drawAlgo()                preen_lib/src/display/TftDisplay.cpp:1044
             inserts TFT_DRAW_ALGO action
        -> TftDisplay action queue
             case TFT_DRAW_ALGO                    preen_lib/src/display/TftDisplay.cpp:475
             clear bgAlgo (80x100, DMA2D)
             tftAlgo->drawAlgo(param1)             preen_lib/src/display/TftAlgo.cpp:867
             merge foreground (highlight) buffer
```

Buffers:
- background: `uint16_t bgAlgo[80 * 100]` (RGB565), `TftDisplay.cpp:261`
- foreground: `uint8_t fgAlgo[80 * 100]` (mask), `TftDisplay.cpp:226`

The diagram area is 80 px wide and 100 px high.

## 2. TftAlgo command-stream format

Each algorithm is a byte stream (`preen_lib/include/TftAlgo.h:32`):

```c
enum {
    OPERATOR = 1,   // OPERATOR, op, position
    IM,             // IM, im, sourceOp, destOp
    IMSYNC,         // IMSYNC, im, sourceOp, destOp   (purple line)
    MIX,            // MIX, op                        (marks carrier)
    OPERATORSYNC,   // OPERATORSYNC, op, position
    END,
};
```

Semantics (`preen_lib/src/display/TftAlgo.cpp`):

| Command | Meaning |
|---------|---------|
| `MIX, op` | Marks operator `op` as a carrier. Must appear before its `OPERATOR`. Carriers are drawn cyan. |
| `OPERATOR, op, pos` | Draws the operator box at grid position `pos`; stores `operatorPosition_[op-1] = pos`. Modulators are blue. |
| `OPERATORSYNC, op, pos` | Same as `OPERATOR` but marks it purple. |
| `IM, im, src, dst` | Red line from `src` to `dst`. If `src == dst`, draws the feedback loop. |
| `IMSYNC, im, src, dst` | Same but purple. |

Operator numbers in the stream are **1-based**. `drawOperator()` uses
`operatorPosition_[op-1]`, while `highlightOperator()` uses
`operatorPosition_[op]` and is therefore called with **0-based** operator
numbers (`TftAlgo.cpp:774`, `:807`, `:835`).

### Grid

```c
#define GETX1(o) (((o-1) % 3)*26 + 6)
#define GETY1(o) (((o-1)/3)*25 + 2)
```

So positions `1..12` map to a 3-column × 4-row grid. Operator boxes are 16×16.
Max x = 74, max y = 93, which fits the 80×100 buffer.

### Table

The PFM3 tables are `algo1..algo32` with `allAlgos[32]`
(`TftAlgo.cpp:621`). `drawAlgo()` selects `allAlgos[algo % 32]`. These are the
**PFM3** algorithms, not the DX7 ones.

Example (`algo1`, `TftAlgo.cpp`):

```c
const uint8_t algo1[] = {
    MIX, 1,
    OPERATOR, 1, 11,
    OPERATOR, 2, 4,
    OPERATOR, 3, 3,
    IM, 1, 2, 1,
    IM, 2, 3, 1,
    IM, 3, 3, 2,
    IM, 6, 3, 3,
    END,
};
```

## 3. DX7 algorithm data

The DX7 engine already contains the connection table in
`third_party/msfa/fm_core.cc:29`:

```cpp
const FmAlgorithm FmCore::algorithms[32] = {
    { { 0xc1, 0x11, 0x11, 0x14, 0x01, 0x14 } }, // 1
    ...
    { { 0xc4, 0x04, 0x04, 0x04, 0x04, 0x04 } }, // 32
};
```

`FmAlgorithm` is six bytes, one per operator (`third_party/msfa/fm_core.h:42`).
Flags (`fm_core.h:32`):

| Flag | Value | Meaning |
|------|-------|---------|
| `OUT_BUS_ONE` | `1<<0` | Output to internal bus 1 |
| `OUT_BUS_TWO` | `1<<1` | Output to internal bus 2 |
| `OUT_BUS_ADD` | `1<<2` | Add to the destination bus / final output |
| `IN_BUS_ONE` | `1<<4` | Read modulation from bus 1 |
| `IN_BUS_TWO` | `1<<5` | Read modulation from bus 2 |
| `FB_IN` | `1<<6` | Feedback input |
| `FB_OUT` | `1<<7` | Feedback output |

Carrier test: `FmCore::isCarrier()` (`fm_core.cc:138`) returns true when
`OUT_BUS_ADD` is set. `n_out()` counts `(ops[i] & 7) == OUT_BUS_ADD`.

Feedback: `(flags & 0xc0) == 0xc0` on an operator (see `FmCore::render()`,
`fm_core.cc:114`).

### Operator numbering

`FmCore::render()` iterates `op = 0..5` in that order. For DX7 algorithm 1:

- `op 0` = 0xc1 → feedback, output bus 1
- `op 1` = 0x11 → in bus 1, out bus 1
- `op 2` = 0x11 → in bus 1, out bus 1
- `op 3` = 0x14 → in bus 1, out 0 + add  (carrier)
- `op 4` = 0x01 → out bus 1
- `op 5` = 0x14 → in bus 1, out 0 + add  (carrier)

This is DX7 algorithm 1: `6→5→4→3` and `2→1`, feedback on OP6. Therefore:

```
FmCore op index i  ==  DX7 operator (6 - i)
DX7 operator n     ==  FmCore op index (6 - n)
```

So `op 0` is DX7 OP6 and `op 5` is DX7 OP1. This matches the program layout
used by the editor: DX7 OP1 lives in program block 5, OP6 in block 0
(`block = 5 - operatorNumber`).

### Deriving the diagram from `FmAlgorithm`

For each op `i` with `outbus = flags & 3`:

- `outbus != 0`: op `i` is a modulator on that bus. Every op `k` with
  `inbus = (flags >> 4) & 3 == outbus` is modulated by it → draw `IM` from
  `i` to `k`.
- `outbus == 0` and `OUT_BUS_ADD` set: op `i` is a carrier → `MIX`.
- `(flags & 0xc0) == 0xc0`: op `i` has feedback → `IM` with `src == dst`.

This gives connections, carriers and feedback without a second table. What it
does **not** give is screen positions, which is the missing piece.

## 4. What is needed

1. **A DX7 layout table** `dx7Algos[32]` in the same command-stream format,
   using DX7 operator numbers `1..6` (so the printed numbers and
   `highlightOperator(operatorNumber)` line up directly). Positions are the
   only hand-authored part; connections/carriers/feedback can be derived from
   `FmCore::algorithms` as described above (or baked into the table).

2. **A DX7 draw path in `TftAlgo`**
   (`preen_lib/src/display/TftAlgo.{h,cpp}`):
   - add `dx7Algos[32]`
   - add `drawDx7Algo(int algo)` or parameterise `drawAlgo()` with a table
     pointer; the body (`drawOperator`, `drawIM`, `drawMix`, `drawNumber`) is
     reusable unchanged.

3. **Display plumbing** (`preen_lib/src/display/TftDisplay.{h,cpp}`):
   - add `TftDisplay::drawDx7Algo(int)`.
   - either a new action type (`TFT_DRAW_DX7_ALGO`) or a flag/offset on
     `TFT_DRAW_ALGO` (`TftDisplay.cpp:475`), because drawing is deferred
     through the action queue.

4. **Call sites**:
   - `FMDisplayMsfaEditor::refreshEditorByStep()` (Engine page) should draw
     the DX7 algorithm instead of clearing, using `program[134]` (0..31).
   - `FMDisplayEditor::refreshEditorByStep()` currently skips `drawAlgo` for
     MSFA (`case 10` and `case 21`); those should call the DX7 variant on the
     main edit screen.

5. **Highlight mapping**: the MSFA editor's `operatorNumber` is `0..5` for
   DX7 OP1..OP6, which is exactly what `highlightOperator()` expects, provided
   the DX7 table stores positions with DX7 operator numbers `1..6`
   (`operatorPosition_[op-1]`).

6. **Feedback/IM colour**: use `IM` for modulator→carrier and `IM` with
   `src == dst` for feedback. `IMSYNC` (purple) is a PFM3-specific sync
   concept; DX7 has no equivalent.

## 5. Reusing PFM3 layouts (DX7 ↔ PFM3 equivalents)

DX7 and PFM3 algorithms are organised very similarly. For the following DX7
algorithms the operator grid positions can be copied directly from the PFM3
`algoN` table; only the connections/carriers/feedback need to come from the
DX7 data.

| DX7 algorithm(s) | PFM3 layout |
|------------------|-------------|
| 1, 2 | `algo10` |
| 3, 4 | `algo11` |
| 7, 8, 9 | `algo13` |
| 10, 11 | `algo14` |
| 12, 13 | `algo15` |
| 14, 15 | `algo16` |
| 16, 17 | `algo17` |
| 18 | `algo18` |

Method for each DX7 algorithm:

1. Copy the `OPERATOR, op, pos` pairs from the equivalent PFM3 `algoN`; these
   give the grid positions.
2. Rebuild `MIX` (carriers) from `FmCore::isCarrier()`.
3. Rebuild `IM` from the bus connections in `FmCore::algorithms`.
4. Add `IM, n, op, op` for the DX7 feedback operator.

Example: DX7 algorithms 1 and 2 both use the `algo10` positions:

```
OP1=10, OP2=7, OP3=12, OP4=9, OP5=6, OP6=3
```

which lays out as (column, row):

```
OP2 (0,2)          OP6 (2,0)
                   OP5 (2,1)
                   OP4 (2,2)
OP1 (0,3)          OP3 (2,3)
```

DX7 1 puts feedback on OP6, DX7 2 on OP2; everything else is identical.
The remaining DX7 algorithms (5, 6, 19–32) have no direct PFM3 layout and
need hand-authored positions (or an auto-layout pass).

## 6. Suggested implementation order

1. Add `dx7Algos[32]` and `drawDx7Algo()` to `TftAlgo`.
2. Add `TftDisplay::drawDx7Algo()` + action handling.
3. Call it from the MSFA Engine page (`program[134]`) and the main edit
   screen.
4. Verify operator highlight on the Oper page.
5. Optionally derive the tables at build time from `FmCore::algorithms` to
   avoid transcription errors.

## 7. Reference: DX7 algorithm 1 decoded

```
op0 (DX7 OP6): 0xc1  FB_IN|FB_OUT, out bus 1
op1 (DX7 OP5): 0x11  in bus 1, out bus 1
op2 (DX7 OP4): 0x11  in bus 1, out bus 1
op3 (DX7 OP3): 0x14  in bus 1, out final + add  -> carrier
op4 (DX7 OP2): 0x01  out bus 1
op5 (DX7 OP1): 0x14  in bus 1, out final + add  -> carrier
```

Equivalent command stream:

Using the `algo10` positions:

```c
MIX, 3,
MIX, 1,
OPERATOR, 6, 3,
OPERATOR, 5, 6,
OPERATOR, 4, 9,
OPERATOR, 3, 12,
OPERATOR, 2, 7,
OPERATOR, 1, 10,
IM, 1, 6, 5,
IM, 2, 5, 4,
IM, 3, 4, 3,
IM, 4, 2, 1,
IM, 5, 6, 6,   // feedback (DX7 algo 1; algo 2 uses OP2 instead)
END,
```
