/* SPDX-License-Identifier: GPL-3.0-only */
/* X0X parameter descriptors, shared by every sound engine.
 *
 * Every user-facing sound parameter is a "pot": an integer 0..max (max is 127 for a
 * continuous control, n-1 for an n-way switch), exactly like 9W9's panel. The UI
 * draws and edits them from these descriptors alone, and a project stores the
 * integers, so a saved sound is engine-version independent as long as a pot keeps
 * its meaning. */
#pragma once
#include <stdint.h>

/* X0X's pan: a balance law, pot 0..127, 64 = centre. Centre is 1 on both sides exactly (a centred
 * mix is the mono one, bit for bit); toward a side the other fades along a quarter cosine and the
 * near side stays at 1. cosine: a 5-term even polynomial, |error| < 2e-4 over the quarter turn. */
static inline void x0x_pan_gains(int pot, float *l, float *r)
{
    float q = pot >= 64 ? (float)(pot - 64) / 63.0f : (float)(pot - 64) / 64.0f, x, c;
    x = q * 1.5707963f;
    x *= x;
    c = 1.0f + x * (-0.5f + x * (0.041666667f + x * (-0.0013888889f + x * 0.0000248016f)));
    if (pot <= 0 || pot >= 127)
        c = 0.0f;                                 /* hard over: nothing on the far side, exactly */
    *l = q > 0.0f ? c : 1.0f;
    *r = q < 0.0f ? c : 1.0f;
}

/* a pan gain gliding toward its target over a block of n samples (one pole, ~10 ms): returns the
 * per-sample step for a linear ramp from *cur, and moves *cur to where the block ends. Settled, the
 * step is 0 and *cur is the target exactly */
static inline float x0x_glide_block(float *cur, float tgt, int n)
{
    float c = *cur, e;
    if (c == tgt)
        return 0.0f;
    e = (tgt - c) * (n >= 441 ? 1.0f : (float)n * (1.0f / 441.0f));   /* ~1 - exp(-n / 441) for small n */
    if (tgt - (c + e) < 1e-5f && (c + e) - tgt < 1e-5f)
        e = tgt - c;
    *cur = c + e;
    return e / (float)n;
}

typedef struct {
    const char *name;                 /* <= 6 characters: drawn above a knob */
    uint8_t max;                      /* 127 = continuous; else switch with max+1 positions */
    uint8_t def;                      /* power-on value */
    const char *const *names;         /* switch: max+1 labels (<= 5 chars), else 0 */
} x0x_param_t;
