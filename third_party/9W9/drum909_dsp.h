/* SPDX-License-Identifier: GPL-3.0-only */
/* Building blocks of the 909 drum engine and the FX bus, ported from 9W9
 * (er99 webaudio.h / er99_circuit.h, GPL-3.0) to the FM-1: float only, no libm,
 * no double. Shared by drum909.c and fxbus.c (all static inline).
 *
 * What changed against 9W9 and why, so a reader can check the numbers:
 *  - Envelopes (Web Audio exponentialRampToValueAtTime): 9W9 evaluates
 *    v0 * (v1/v0)^(t/T) with powf on every sample. Here the ramp is a
 *    recursion v *= r, re-anchored to the closed form (one fm_exp2f) at every
 *    block start, so the drift is bounded by a block (256 multiplies) and the
 *    end point / end sample are exactly 9W9's.
 *  - Oscillator phase: 9W9 keeps a double; here a uint32 turn (2^32 = one cycle),
 *    which is exact modulo the increment's float rounding.
 *  - Sample read position: 9W9 keeps a double; here 32.32 fixed point, which is
 *    exact for every playback rate the pots produce (identical positions).
 *  - tanh: a 1025-point table with linear interpolation (abs error < 6e-6),
 *    generated offline; used for the diode rounding and every saturator.
 *  - Biquad coefficients: cos(w0) as 1 - 2 sin^2(w0/2), which keeps the
 *    low-frequency filters (20 Hz DC blocker) as accurate as libm's cosf. */
#pragma once
#include <stdint.h>
#include "fastmath.h"
#include "x0x_drum_tables.h"

#if defined(__GNUC__) || defined(__clang__)
#define D9_INLINE static inline __attribute__((always_inline))
#define D9_NOINLINE static __attribute__((noinline, unused))   /* not every includer uses every helper */
#define D9_UNLIKELY(x) __builtin_expect(!!(x), 0)   /* the branch's other side falls through */
#define D9_MAY_ALIAS __attribute__((may_alias))      /* a type whose accesses may alias any object */
#else
#define D9_INLINE static inline
#define D9_NOINLINE static
#define D9_UNLIKELY(x) (x)
#define D9_MAY_ALIAS
#endif
/* Loop unrolling, for the loop that follows: by n, completely, or not at all. Clang's pragmas; other
 * compilers unroll as they choose. On the FM-1's core a taken branch costs about 8 cycles. */
#if defined(__clang__)
#define D9_PRAGMA(x) _Pragma(#x)
#define D9_UNROLL(n) D9_PRAGMA(clang loop unroll_count(n))
#define D9_UNROLL_ALL D9_PRAGMA(clang loop unroll(full))
#define D9_NO_UNROLL D9_PRAGMA(clang loop unroll(disable))
#else
#define D9_UNROLL(n)
#define D9_UNROLL_ALL
#define D9_NO_UNROLL
#endif

#define D9_SR 44100.0f
#define D9_MS (0.001f * D9_SR)                 /* samples per ms, as 9W9's `ms` */
#define D9_MIN_EXP 1.0e-9f
#define D9_PHASE_PER_HZ 97391.548752834467f   /* 2^32 / 44100 */

/* ---- exp(x) to about an ulp for |x| < 0.35 (envelope and detector rates) ---- */
static inline float d9_exp_small(float x)
{
    if (x < -0.35f || x > 0.35f)
        return fm_expf(x);
    float p = 1.0f + x * (1.0f / 8.0f);
    p = 1.0f + x * (1.0f / 7.0f) * p;
    p = 1.0f + x * (1.0f / 6.0f) * p;
    p = 1.0f + x * (1.0f / 5.0f) * p;
    p = 1.0f + x * (1.0f / 4.0f) * p;
    p = 1.0f + x * (1.0f / 3.0f) * p;
    p = 1.0f + x * 0.5f * p;
    return 1.0f + x * p;
}

/* ceil for 0 <= x < 2^31 */
static inline int32_t d9_ceil(float x)
{
    int32_t i = (int32_t)x;
    return (float)i < x ? i + 1 : i;
}

/* ===================================================================== */
/* Envelope: Web Audio AudioParam with setValueAtTime and                 */
/* exponentialRampToValueAtTime (the only two automation calls 9W9 makes) */
/* ===================================================================== */
typedef struct {
    float v;        /* value the next tick returns           */
    float cur;      /* value the last tick returned (9W9's p->value; a new
                       ramp starts from it)                  */
    float r;        /* per-sample ratio while ramping        */
    float v0, v1;   /* ramp start / end                      */
    float l2r;      /* log2(v1 / v0)                         */
    float t1;       /* ramp length in samples (9W9's t1)     */
    int32_t k;      /* ramp length in ticks before v1        */
    int32_t left;   /* ticks of ramp left (0 = constant)     */
} d9_env_t;

static inline void d9_env_set(d9_env_t *e, float v)
{
    e->v = v;
    e->cur = v;
    e->v1 = v;
    e->left = 0;
}

/* exponentialRampToValueAtTime(v1, now + t1 samples). Tick k (k = 0 the first
 * tick after this call) returns v0 * (v1/v0)^(k/t1) while k + 1 < t1, then v1.
 * Out of line: it runs a few times per note, never per sample. */
D9_NOINLINE void d9_env_exp(d9_env_t *e, float v1, float t1)
{
    float v0 = e->cur < D9_MIN_EXP ? D9_MIN_EXP : e->cur;
    if (v1 < D9_MIN_EXP)
        v1 = D9_MIN_EXP;
    if (!(t1 > 0.0f))
        t1 = 1.0f;
    e->v0 = v0;
    e->v1 = v1;
    e->t1 = t1;
    e->k = d9_ceil(t1) - 1;
    e->left = e->k;
    if (e->k <= 0) {
        e->left = 0;
        e->v = v1;
        e->cur = v0;
        return;
    }
    e->l2r = fm_log2f(v1 / v0);
    e->r = d9_exp_small(e->l2r * FM_LN2 / t1);
    e->v = v0;
    e->cur = v0;
}

static inline float d9_env_tick(d9_env_t *e)
{
    const float o = e->v;
    e->cur = o;
    if (e->left) {
        if (--e->left)
            e->v *= e->r;
        else
            e->v = e->v1;
    }
    return o;
}

/* back onto the closed form; once per block */
static inline void d9_env_anchor(d9_env_t *e)
{
    if (e->left)
        e->v = e->v0 * fm_exp2f(e->l2r * ((float)(e->k - e->left) / e->t1));
}

/* ===================================================================== */
/* Biquad (Web Audio / RBJ), direct form 1 as 9W9                         */
/* ===================================================================== */
enum { D9_LP = 0, D9_HP, D9_BP };

typedef struct {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
} d9_biquad_t;

static inline void d9_biquad_set(d9_biquad_t *f, int type, float freq, float q)
{
    float fc = freq / (D9_SR * 0.5f);
    if (fc >= 1.0f) fc = 0.9999f;
    if (fc <= 0.0f) fc = 0.0001f;
    const float w0 = FM_PI * fc;
    const float sh = fm_sinf(0.5f * w0);
    const float cosw0 = 1.0f - 2.0f * sh * sh;
    const float sinw0 = fm_sinf(w0);
    if (q < 0.0001f) q = 0.0001f;
    const float alpha = sinw0 / (2.0f * q);
    float b0, b1, b2;
    const float a0 = 1.0f + alpha, a1 = -2.0f * cosw0, a2 = 1.0f - alpha;
    if (type == D9_LP) {
        b0 = (1.0f - cosw0) * 0.5f; b1 = 1.0f - cosw0; b2 = (1.0f - cosw0) * 0.5f;
    } else if (type == D9_HP) {
        b0 = (1.0f + cosw0) * 0.5f; b1 = -(1.0f + cosw0); b2 = (1.0f + cosw0) * 0.5f;
    } else {
        b0 = alpha; b1 = 0.0f; b2 = -alpha;
    }
    const float inv = 1.0f / a0;
    f->b0 = b0 * inv; f->b1 = b1 * inv; f->b2 = b2 * inv;
    f->a1 = a1 * inv; f->a2 = a2 * inv;
}

static inline void d9_biquad_reset(d9_biquad_t *f) { f->x1 = f->x2 = f->y1 = f->y2 = 0.0f; }

static inline float d9_biquad_tick(d9_biquad_t *f, float in)
{
    const float y = f->b0 * in + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1; f->x1 = in;
    f->y2 = f->y1; f->y1 = y;
    return y;
}

/* ===================================================================== */
/* Oscillator: Web Audio triangle on a uint32 phase                       */
/* ===================================================================== */
static inline uint32_t d9_inc(float hz) { return (uint32_t)(hz * D9_PHASE_PER_HZ); }

/* 4p - 1 rising over the first half, 3 - 4p falling over the second */
static inline float d9_tri(uint32_t ph)
{
    const uint32_t u = ph ^ (uint32_t)((int32_t)ph >> 31);
    return (float)(int32_t)u * (1.0f / 1073741824.0f) - 1.0f;
}

/* ===================================================================== */
/* tanh from the table                                                    */
/* ===================================================================== */
static inline float d9_tanh(float u)
{
    const float a = fm_fabsf(u) * X0X_TANH_SCALE;
    float t;
    if (a >= (float)X0X_TANH_N) {
        t = 1.0f;
    } else {
        const int32_t i = (int32_t)a;
        const float fr = a - (float)i;
        t = x0x_tanh_tab[i] + (x0x_tanh_tab[i + 1] - x0x_tanh_tab[i]) * fr;
    }
    return u < 0.0f ? -t : t;
}

/* ===================================================================== */
/* The voice / master distortion stage (er99_shape_st), 7 types           */
/* ===================================================================== */
typedef struct {
    int32_t type;
    float k;       /* drive, floored at 0.01            */
    float g;       /* diode: 1/tanh(k); others: 1/k when k < 1 else 1 */
    float c1, c2;  /* type-specific constants (see prep) */
} d9_shape_t;

static inline void d9_shape_prep(d9_shape_t *s, float drive, int type)
{
    const float k = drive > 0.01f ? drive : 0.01f;
    s->type = type;
    s->k = k;
    s->g = k < 1.0f ? 1.0f / k : 1.0f;
    s->c1 = s->c2 = 0.0f;
    switch (type) {
    case 0: s->g = 1.0f / d9_tanh(k); break;
    case 2: s->c1 = 0.08f * k; s->c2 = k < 1.0f ? k : 1.0f; break;   /* SAT: m */
    case 3: {                                                       /* BFZ: m */
        float m = (k - 0.85f) / 1.15f;
        s->c1 = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
        s->c2 = k * 2.5f;
        break;
    }
    case 6:                                                          /* crush */
        s->c1 = 1.5f + 9.0f / k;
        s->c2 = k < 1.0f ? 1.0f : 1.0f + (k - 1.0f) * 1.7f;
        s->g = 1.0f / s->c1;
        break;
    default: break;
    }
}

/* types 1..6, out of line (the switch is big; the default type is inline below) */
D9_NOINLINE float d9_shape_other(const d9_shape_t *s, float x, float *st)
{
    const float k = s->k;
    switch (s->type) {
    case 1: {   /* asymmetric soft clip */
        const float v = (d9_tanh(x * k + 0.35f) - 0.33638f) * 0.958f;
        return v * s->g;
    }
    case 5: {   /* wavefolder */
        float v = x * k;
        for (int i = 0; i < 3; ++i) {
            if (v > 1.0f) v = 2.0f - v;
            if (v < -1.0f) v = -2.0f - v;
        }
        float body = x * k;
        if (body > 1.0f) body = 1.0f;
        if (body < -1.0f) body = -1.0f;
        v = 0.62f * v + 0.38f * body;
        return v * s->g;
    }
    case 6: {   /* bitcrush: quantise + decimate */
        float q = fm_floorf(x * s->c1 + 0.5f) * s->g;
        st[1] += 1.0f;
        if (st[1] >= s->c2) { st[1] -= s->c2; st[0] = q; }
        return st[0];
    }
    case 2: {   /* SAT */
        const float u = x * k + s->c1 * x * x;
        const float wet = u / (1.0f + fm_fabsf(u));
        const float m = s->c2;
        const float v = (1.0f - 0.65f * m) * x * m + 0.65f * m * wet * 1.35f;
        return v * s->g;
    }
    case 3: {   /* BFZ */
        const float u = x * s->c2 + 0.22f;
        const float wet = (u / (1.0f + fm_fabsf(u)) - 0.18033f) * 1.05f;
        const float m = s->c1;
        return (1.0f - m) * x + m * wet;
    }
    case 4: {   /* PDIST */
        float u = x * k + 0.12f;
        if (u > 1.0f) u = 1.0f;
        if (u < -1.0f) u = -1.0f;
        const float y0 = 0.12f - (0.12f * 0.12f * 0.12f) / 3.0f;
        return ((u - u * u * u * (1.0f / 3.0f)) - y0) * (1.5f / 1.479f);
    }
    default:    /* 0: the 909's diode pair */
        return d9_tanh(k * x) * s->g;
    }
}

D9_INLINE float d9_shape(const d9_shape_t *s, float x, float *st)
{
    if (s->type == 0)                       /* Diode: every voice's default */
        return d9_tanh(s->k * x) * s->g;
    return d9_shape_other(s, x, st);
}

/* ===================================================================== */
/* White noise: xorshift32, uniform [-1, 1) (9W9's wa_noise)              */
/* ===================================================================== */
static inline float d9_noise(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return (float)x * (1.0f / 2147483648.0f) - 1.0f;
}
