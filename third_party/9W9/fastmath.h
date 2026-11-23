/* SPDX-License-Identifier: GPL-3.0-only */
/* X0X single-precision maths, with no libm.
 *
 * The FM-1's FPU is float-only (-mfprev1) and the firmware links no libm, so this
 * is the whole maths library of the DSP. The same code runs in the host build,
 * which is what lets a host render stand in for the device: build the host with
 * -ffp-contract=off (the device has no fused multiply-add) and the two compute
 * the same samples.
 *
 * Accuracy (tests/host/fastmath_test.c checks every bound against libm):
 *   fm_exp2f  rel 3e-7 on [-126, 127]      fm_expf  rel 6e-7 on [-87, 88]
 *   fm_log2f  abs 3e-7 on [0.25, 4], 4e-6 on [1e-30, 1e30]
 *   fm_powf   rel 1e-5 (x > 0)             fm_tanhf abs 3e-7
 *   fm_sinf / fm_cosf abs 1e-6 on [-100, 100] (three-part 2pi reduction)
 *   fm_tanf   rel 1e-6 on (-pi/2, pi/2)   fm_sqrtf rel 2e-7
 *
 * Every function is static inline and branch-light. None may be called with
 * NaN; none returns a denormal for an in-range argument. */
#pragma once
#include <stdint.h>

#define FM_PI 3.14159265358979f
#define FM_TWO_PI 6.28318530717959f
#define FM_LN2 0.693147180559945f
#define FM_LOG2E 1.44269504088896f

typedef union { float f; uint32_t u; int32_t i; } fm_bits_t;

static inline float fm_fabsf(float x) { fm_bits_t b; b.f = x; b.u &= 0x7FFFFFFFu; return b.f; }
static inline float fm_minf(float a, float b) { return a < b ? a : b; }
static inline float fm_maxf(float a, float b) { return a > b ? a : b; }
static inline float fm_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* floor for |x| < 2^31 */
static inline float fm_floorf(float x)
{
    int32_t i = (int32_t)x;
    float f = (float)i;
    return f > x ? f - 1.0f : f;
}

/* 2^n * 2^f for an integer n and |f| <= 0.5 (or a little more) */
static inline float fm_exp2_nf(int32_t n, float f)
{
    fm_bits_t b;
    float p;
    if (n < -126)
        return 0.0f;                 /* flush instead of a denormal */
    p = 1.534580891e-04f;            /* 2^f on [-0.5, 0.5]: degree 6, fitted (tools/fit_fastmath.py) */
    p = p * f + 1.339993158e-03f;
    p = p * f + 9.618488972e-03f;
    p = p * f + 5.550328776e-02f;
    p = p * f + 2.402264689e-01f;
    p = p * f + 6.931472057e-01f;
    p = p * f + 1.0f;
    b.f = p;
    b.i += n << 23;
    return b.f;
}

/* 2^x. Split x = n + f, f in [-0.5, 0.5]; fitted polynomial for 2^f. */
static inline float fm_exp2f(float x)
{
    float fl;
    if (x < -126.0f)
        return 0.0f;
    if (x > 127.0f)
        x = 127.0f;
    fl = fm_floorf(x + 0.5f);
    return fm_exp2_nf((int32_t)fl, x - fl);
}

/* e^x with Cody-Waite reduction: x = n ln2 + r, so |x| * eps never reaches the result */
static inline float fm_expf(float x)
{
    float n;
    if (x < -87.3f)
        return 0.0f;
    if (x > 88.7f)
        x = 88.7f;
    n = fm_floorf(x * FM_LOG2E + 0.5f);
    x = (x - n * 0.693145752f) - n * 1.42860677e-6f;      /* ln2 split: hi has 16 bits */
    return fm_exp2_nf((int32_t)n, x * FM_LOG2E);            /* |x log2e| <= 0.5 */
}

/* log2(x), x > 0 (denormals and 0 return -126 / -127-ish, never NaN) */
static inline float fm_log2f(float x)
{
    fm_bits_t b;
    float m, t, t2, p;
    int32_t e;
    b.f = x;
    if (b.i <= 0)
        return -127.0f;
    e = ((b.i >> 23) & 0xFF) - 127;
    b.i = (b.i & 0x007FFFFF) | 0x3F800000;     /* mantissa in [1, 2) */
    m = b.f;
    if (m > 1.41421356f) {                     /* centre on 1: m in [0.707, 1.414) */
        m *= 0.5f;
        e += 1;
    }
    /* log2(m) = 2/ln2 * atanh(t), t = (m-1)/(m+1), |t| <= 0.1716 */
    t = (m - 1.0f) / (m + 1.0f);
    t2 = t * t;
    p = 3.428278367e-01f;                       /* odd polynomial in t, fitted */
    p = p * t2 + 4.115337810e-01f;
    p = p * t2 + 5.770866525e-01f;
    p = p * t2 + 9.617966483e-01f;
    p = p * t2 + 2.885390082e+00f;
    return (float)e + p * t;
}

static inline float fm_logf(float x) { return fm_log2f(x) * FM_LN2; }

/* x^y for x > 0 (x <= 0 returns 0) */
static inline float fm_powf(float x, float y)
{
    if (x <= 0.0f)
        return 0.0f;
    return fm_exp2f(y * fm_log2f(x));
}

/* 10^(db/20) */
static inline float fm_db2lin(float db) { return fm_exp2f(db * 0.166096405f); }

static inline float fm_tanhf(float x)
{
    float a = fm_fabsf(x), e, r;
    if (a < 0.0004f)
        return x;                                /* tanh x = x - x^3/3: below float resolution here */
    if (a < 0.5f) {                              /* Taylor to x^13: the next term is 4e-8 at 0.5. No exp2, */
        float z = x * x;                         /* no divide: most clippers spend most samples here */
        return x * (1.0f + z * (-0.333333333f + z * (0.133333333f + z * (-0.0539682540f + z * (0.0218694885f +
                    z * (-0.00886323552f + z * 0.00359212803f))))));
    }
    if (a > 9.0f)
        return x > 0.0f ? 1.0f : -1.0f;
    e = fm_exp2f(2.0f * FM_LOG2E * a);
    r = 1.0f - 2.0f / (e + 1.0f);
    return x > 0.0f ? r : -r;
}

/* sin on [-pi, pi]: odd polynomial, degree 13 */
static inline float fm_sin_pi(float x)
{
    float x2 = x * x, p;
    p = 1.345176048e-10f;                      /* degree 13, fitted */
    p = p * x2 - 2.467706523e-08f;
    p = p * x2 + 2.752946668e-06f;
    p = p * x2 - 1.984015614e-04f;
    p = p * x2 + 8.333310417e-03f;
    p = p * x2 - 1.666666459e-01f;
    p = p * x2 + 9.999999946e-01f;
    return p * x;
}

static inline float fm_wrap_pi(float x)                /* to [-pi, pi), 2pi in three parts */
{
    float k = fm_floorf(x * (1.0f / FM_TWO_PI) + 0.5f);
    return ((x - k * 6.28125f) - k * 1.9353071693331003e-3f) - k * 1.0253131561401567e-11f;
}

static inline float fm_sinf(float x) { return fm_sin_pi(fm_wrap_pi(x)); }
static inline float fm_cosf(float x)
{
    float r = fm_wrap_pi(x) + 0.5f * FM_PI;          /* reduce first: adding pi/2 to a big x loses bits */
    if (r >= FM_PI)
        r -= FM_TWO_PI;
    return fm_sin_pi(r);
}
/* tan on (-pi/2, pi/2), the filter-coefficient range: cos(x) = sin(pi/2 - |x|), which keeps
 * its relative accuracy next to the pole where cos is small */
static inline float fm_tanf(float x)
{
    float a = fm_fabsf(x), c = (1.57079637f - a) - 4.37113883e-8f;
    return fm_sin_pi(x) / fm_sin_pi(c);
}

/* sine of a phase in turns (0..1 = one cycle): the cheap oscillator form */
static inline float fm_sin_turns(float ph)
{
    float x = ph - fm_floorf(ph + 0.5f);          /* [-0.5, 0.5) */
    return fm_sin_pi(x * FM_TWO_PI);
}

static inline float fm_sqrtf(float x)
{
    fm_bits_t b;
    float y;
    if (x <= 0.0f)
        return 0.0f;
    b.f = x;
    b.u = 0x5F375A86u - (b.u >> 1);              /* 1/sqrt seed, then Newton x3 */
    y = b.f;
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    return x * y;
}

/* 1 / sqrt(x), x > 0: the seed and two Newton steps (rel 5e-6). For gains, not for values that are
 * shown or compared: fm_sqrtf keeps three. */
static inline float fm_rsqrtf(float x)
{
    fm_bits_t b;
    float y;
    b.f = x;
    b.u = 0x5F375A86u - (b.u >> 1);
    y = b.f;
    y = y * (1.5f - 0.5f * x * y * y);
    return y * (1.5f - 0.5f * x * y * y);
}

/* denormal / tiny-value flush for filter states */
static inline float fm_flush(float x) { return fm_fabsf(x) < 1e-20f ? 0.0f : x; }
