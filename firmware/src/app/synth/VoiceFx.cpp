/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>

#include "SynthState.h"
#include "Timbre.h"
#include "Voice.h"
#include "waves.h"

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wsequence-point"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#if defined(__GNUC__) && !defined(DEBUG)
#pragma GCC push_options
#pragma GCC optimize("Ofast", "fast-math")
#endif

// for bitwise manipulations
#define FLOAT2SHORT 32768.f
#define SHORT2FLOAT 1. / 32768.f

#define RATIOINV 1. / 131072.f

#define SVFRANGE      1.23f
#define SVFOFFSET     0.151f
#define SVFGAINOFFSET 0.3f

#define LP2OFFSET -0.045f
#define min(a, b) ((a) < (b) ? (a) : (b))

extern float noise[32];

#define filterWindowMin 0.01f
#define filterWindowMax 0.99f

inline float expf_fast(float a) {
    // https://github.com/ekmett/approximate/blob/master/cbits/fast.c
    union {
        float f;
        int x;
    } u;
    u.x = (int)(12102203 * a + 1064866805);
    return u.f;
}
inline float exp_pole(float p) {
    const float h = p * 0.25f;
    const float h2 = h * h * 0.083333336f;
    const float hp = 0.5f * h;
    float e = (1.0f + hp + h2) / (1.0f - hp + h2);
    e *= e;
    return e * e;
}
inline float fastInvSqrtVoice(float x) {
    float xhalf = 0.5f * x;
    union {
        float f;
        uint32_t i;
    } u;
    u.f = x;
    u.i = 0x5f3759dfu - (u.i >> 1);
    u.f = u.f * (1.5f - xhalf * u.f * u.f);
    return u.f;
}
inline float softLimitVoice(float x) {
    return x * fastInvSqrtVoice(1.0f + x * x);
}
inline float sqrt3(const float x) {
    union {
        int i;
        float x;
    } u;

    u.x = x;
    u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
    return u.x;
}
inline float tanh3(float x) {
    // return x / (0.7f + fabsf(0.8f * x));
    return 1.5f * x / (1.7f + fabsf(0.34f * x * x));
}
inline float tanh4(float x) {
    return x / sqrt3(x * x + 1);
}
inline float sat25(float x) {
    if (unlikely(fabsf(x) > 4))
        return 0;
    return x * (1.f - fabsf(x * 0.25f));
}
inline float sat66(float x) {
    return x * (1 - (x * x * 0.055f));
}
inline float sat33(float x) {
    if (unlikely(fabsf(x) > 2.58f))
        return 0;
    return x * (1 - x * x * 0.15f);
}
inline float clamp(float d, float min, float max) {
    const float t = unlikely(d < min) ? min : d;
    return unlikely(t > max) ? max : t;
}
inline float satSin(float x, float drive, uint_fast16_t pos) {
    // https://www.desmos.com/calculator/pdsqpi5lp6
    return x + clamp(x, -0.5f * drive, drive) * sinTable[((uint_fast16_t)(fabsf(x) * 360) + pos) & 0x3FF];
}
// https://www.musicdsp.org/en/latest/Other/120-saturation.html
inline float sigmoid(float x) {
    return x * (1.5f - 0.5f * x * x);
}
inline float sigmoidPos(float x) {
    // x : 0 -> 1
    return (sigmoid((x * 2) - 1) + 1) * 0.5f;
}
float fold(float x4) {
    // https://www.desmos.com/calculator/ge2wvg2wgj
    // x4 = x / 4
    return (fabsf(x4 + 0.25f - roundf(x4 + 0.25f)) - 0.25f);
}
inline float wrap(float x) {
    const float u = 0.5f * x + 0.5f;
    return (u - floorf(u)) * 2.0f - 1.0f;
}
inline float tanh4ADAA1_F(float x) {
    const float u = x * x + 1.0f;
#if defined(__ARM_FP)
    float res;
    __asm__("vsqrt.f32 %0, %1" : "=t"(res) : "t"(u));
    return res;
#else
    return sqrtf(u);
#endif
}
inline float tanh4ADAA(float x, float& prevX, float& prevF) {
    const float f = tanh4ADAA1_F(x);
    const float out = (x + prevX) / (f + prevF);
    prevX = x;
    prevF = f;
    return out;
}
inline float sat25ADAA1_F(float x) {
    const float ax = fabsf(x);
    if (unlikely(ax > 4.0f)) {
        return 2.6666667f;
    }
    return x * x * (0.5f - 0.083333336f * ax);
}
inline float sat25ADAA(float x, float& prevX, float& prevF) {
    const float f = sat25ADAA1_F(x);
    const float diff = x - prevX;
    float out;
    if (likely(fabsf(diff) > 1e-5f)) {
        out = (f - prevF) / diff;
    } else {
        out = sat25(0.5f * (x + prevX));
    }
    prevX = x;
    prevF = f;
    return out;
}
inline float sigmoidADAA1_F(float x) {
    const float xc = clamp(x, -1.0f, 1.0f);
    const float x2 = xc * xc;
    return x2 * (0.75f - 0.125f * x2) + (fabsf(x) - fabsf(xc));
}
inline float sigmoidADAA(float x, float& prevX, float& prevF) {
    const float f = sigmoidADAA1_F(x);
    const float diff = x - prevX;
    float out;
    if (likely(fabsf(diff) > 1e-4f)) {
        out = (f - prevF) / diff;
    } else {
        out = sigmoid(0.5f * (x + prevX));
    }
    prevX = x;
    prevF = f;
    return out;
}
inline float foldADAA1_F(float x4) {
    const float u = x4 + 0.25f;
    const float d = u - roundf(u);
    return d * (0.5f * fabsf(d) - 0.25f);
}
inline float foldADAA(float x4, float& prevX4, float& prevF) {
    const float f = foldADAA1_F(x4);
    const float diff = x4 - prevX4;
    float out;
    if (likely(fabsf(diff) > 1e-5f)) {
        out = (f - prevF) / diff;
    } else {
        out = fold(0.5f * (x4 + prevX4));
    }
    prevX4 = x4;
    prevF = f;
    return out;
}
inline float wrapADAA1_F(float x) {
    const float u = 0.5f * x + 0.5f;
    const float d = u - floorf(u);
    return 2.0f * d * (d - 1.0f);
}
inline float wrapADAA(float x, float& prevX, float& prevF) {
    const float f = wrapADAA1_F(x);
    const float diff = x - prevX;
    float out;
    if (likely(fabsf(diff) > 1e-5f)) {
        out = (f - prevF) / diff;
    } else {
        out = wrap(0.5f * (x + prevX));
    }
    prevX = x;
    prevF = f;
    return out;
}
enum NewNoteType { NEW_NOTE_FREE = 0, NEW_NOTE_RELEASE, NEW_NOTE_OLD, NEW_NOTE_NONE };
// TB303 filter emulation ------
const float filterpoles[10][64] = {
    {-1.42475857e-02, -1.10558351e-02, -9.58097367e-03, -8.63568249e-03, -7.94942757e-03, -7.41570560e-03, -6.98187179e-03,
     -6.61819537e-03, -6.30631927e-03, -6.03415378e-03, -5.79333654e-03, -5.57785533e-03, -5.38325013e-03, -5.20612558e-03,
     -5.04383985e-03, -4.89429884e-03, -4.75581571e-03, -4.62701254e-03, -4.50674977e-03, -4.39407460e-03, -4.28818259e-03,
     -4.18838855e-03, -4.09410427e-03, -4.00482112e-03, -3.92009643e-03, -3.83954259e-03, -3.76281836e-03, -3.68962181e-03,
     -3.61968451e-03, -3.55276681e-03, -3.48865386e-03, -3.42715236e-03, -3.36808777e-03, -3.31130196e-03, -3.25665127e-03,
     -3.20400476e-03, -3.15324279e-03, -3.10425577e-03, -3.05694308e-03, -3.01121207e-03, -2.96697733e-03, -2.92415989e-03,
     -2.88268665e-03, -2.84248977e-03, -2.80350622e-03, -2.76567732e-03, -2.72894836e-03, -2.69326825e-03, -2.65858922e-03,
     -2.62486654e-03, -2.59205824e-03, -2.56012496e-03, -2.52902967e-03, -2.49873752e-03, -2.46921570e-03, -2.44043324e-03,
     -2.41236091e-03, -2.38497108e-03, -2.35823762e-03, -2.33213577e-03, -2.30664208e-03, -2.28173430e-03, -2.25739130e-03,
     -2.23359302e-03},
    {1.63323670e-16,  -1.61447133e-02, -1.99932070e-02, -2.09872000e-02, -2.09377795e-02, -2.04470150e-02, -1.97637613e-02,
     -1.90036975e-02, -1.82242987e-02, -1.74550383e-02, -1.67110053e-02, -1.59995606e-02, -1.53237941e-02, -1.46844019e-02,
     -1.40807436e-02, -1.35114504e-02, -1.29747831e-02, -1.24688429e-02, -1.19916965e-02, -1.15414484e-02, -1.11162818e-02,
     -1.07144801e-02, -1.03344362e-02, -9.97465446e-03, -9.63374867e-03, -9.31043725e-03, -9.00353710e-03, -8.71195702e-03,
     -8.43469084e-03, -8.17081077e-03, -7.91946102e-03, -7.67985179e-03, -7.45125367e-03, -7.23299254e-03, -7.02444481e-03,
     -6.82503313e-03, -6.63422244e-03, -6.45151640e-03, -6.27645413e-03, -6.10860728e-03, -5.94757730e-03, -5.79299303e-03,
     -5.64450848e-03, -5.50180082e-03, -5.36456851e-03, -5.23252970e-03, -5.10542063e-03, -4.98299431e-03, -4.86501921e-03,
     -4.75127814e-03, -4.64156716e-03, -4.53569463e-03, -4.43348032e-03, -4.33475462e-03, -4.23935774e-03, -4.14713908e-03,
     -4.05795659e-03, -3.97167614e-03, -3.88817107e-03, -3.80732162e-03, -3.72901453e-03, -3.65314257e-03, -3.57960420e-03,
     -3.50830319e-03},
    {-1.83545593e-06, -1.35008051e-03, -1.51527847e-03, -1.61437715e-03, -1.68536679e-03, -1.74064961e-03, -1.78587681e-03,
     -1.82410854e-03, -1.85719118e-03, -1.88632533e-03, -1.91233586e-03, -1.93581405e-03, -1.95719818e-03, -1.97682215e-03,
     -1.99494618e-03, -2.01177700e-03, -2.02748155e-03, -2.04219657e-03, -2.05603546e-03, -2.06909331e-03, -2.08145062e-03,
     -2.09317612e-03, -2.10432901e-03, -2.11496056e-03, -2.12511553e-03, -2.13483321e-03, -2.14414822e-03, -2.15309131e-03,
     -2.16168985e-03, -2.16996830e-03, -2.17794867e-03, -2.18565078e-03, -2.19309254e-03, -2.20029023e-03, -2.20725864e-03,
     -2.21401130e-03, -2.22056055e-03, -2.22691775e-03, -2.23309332e-03, -2.23909688e-03, -2.24493730e-03, -2.25062280e-03,
     -2.25616099e-03, -2.26155896e-03, -2.26682328e-03, -2.27196010e-03, -2.27697514e-03, -2.28187376e-03, -2.28666097e-03,
     -2.29134148e-03, -2.29591970e-03, -2.30039977e-03, -2.30478562e-03, -2.30908091e-03, -2.31328911e-03, -2.31741351e-03,
     -2.32145721e-03, -2.32542313e-03, -2.32931406e-03, -2.33313263e-03, -2.33688133e-03, -2.34056255e-03, -2.34417854e-03,
     -2.34773145e-03},
    {-2.96292613e-06, 6.75138822e-04, 6.96581050e-04, 7.04457808e-04, 7.07837502e-04, 7.09169651e-04, 7.09415480e-04, 7.09031433e-04,
     7.08261454e-04,  7.07246872e-04, 7.06074484e-04, 7.04799978e-04, 7.03460301e-04, 7.02080606e-04, 7.00678368e-04, 6.99265907e-04,
     6.97852005e-04,  6.96442963e-04, 6.95043317e-04, 6.93656323e-04, 6.92284301e-04, 6.90928882e-04, 6.89591181e-04, 6.88271928e-04,
     6.86971561e-04,  6.85690300e-04, 6.84428197e-04, 6.83185182e-04, 6.81961088e-04, 6.80755680e-04, 6.79568668e-04, 6.78399727e-04,
     6.77248505e-04,  6.76114631e-04, 6.74997722e-04, 6.73897392e-04, 6.72813249e-04, 6.71744904e-04, 6.70691972e-04, 6.69654071e-04,
     6.68630828e-04,  6.67621875e-04, 6.66626854e-04, 6.65645417e-04, 6.64677222e-04, 6.63721940e-04, 6.62779248e-04, 6.61848835e-04,
     6.60930398e-04,  6.60023644e-04, 6.59128290e-04, 6.58244058e-04, 6.57370684e-04, 6.56507909e-04, 6.55655483e-04, 6.54813164e-04,
     6.53980718e-04,  6.53157918e-04, 6.52344545e-04, 6.51540387e-04, 6.50745236e-04, 6.49958895e-04, 6.49181169e-04, 6.48411873e-04},
    {-1.00014774e+00, -1.35336624e+00, -1.42048887e+00, -1.46551548e+00, -1.50035433e+00, -1.52916086e+00, -1.55392254e+00,
     -1.57575858e+00, -1.59536715e+00, -1.61321568e+00, -1.62963377e+00, -1.64486333e+00, -1.65908760e+00, -1.67244897e+00,
     -1.68506052e+00, -1.69701363e+00, -1.70838333e+00, -1.71923202e+00, -1.72961221e+00, -1.73956855e+00, -1.74913935e+00,
     -1.75835773e+00, -1.76725258e+00, -1.77584919e+00, -1.78416990e+00, -1.79223453e+00, -1.80006075e+00, -1.80766437e+00,
     -1.81505964e+00, -1.82225940e+00, -1.82927530e+00, -1.83611794e+00, -1.84279698e+00, -1.84932127e+00, -1.85569892e+00,
     -1.86193740e+00, -1.86804360e+00, -1.87402388e+00, -1.87988413e+00, -1.88562983e+00, -1.89126607e+00, -1.89679760e+00,
     -1.90222885e+00, -1.90756395e+00, -1.91280679e+00, -1.91796101e+00, -1.92303002e+00, -1.92801704e+00, -1.93292509e+00,
     -1.93775705e+00, -1.94251559e+00, -1.94720328e+00, -1.95182252e+00, -1.95637561e+00, -1.96086471e+00, -1.96529188e+00,
     -1.96965908e+00, -1.97396817e+00, -1.97822093e+00, -1.98241904e+00, -1.98656411e+00, -1.99065768e+00, -1.99470122e+00,
     -1.99869613e+00},
    {1.30592376e-04, 3.54780202e-01, 4.22050344e-01, 4.67149412e-01, 5.02032084e-01, 5.30867858e-01, 5.55650170e-01, 5.77501296e-01,
     5.97121154e-01, 6.14978238e-01, 6.31402872e-01, 6.46637440e-01, 6.60865515e-01, 6.74229755e-01, 6.86843408e-01, 6.98798009e-01,
     7.10168688e-01, 7.21017938e-01, 7.31398341e-01, 7.41354603e-01, 7.50925074e-01, 7.60142923e-01, 7.69037045e-01, 7.77632782e-01,
     7.85952492e-01, 7.94016007e-01, 8.01841009e-01, 8.09443333e-01, 8.16837226e-01, 8.24035549e-01, 8.31049962e-01, 8.37891065e-01,
     8.44568531e-01, 8.51091211e-01, 8.57467223e-01, 8.63704040e-01, 8.69808551e-01, 8.75787123e-01, 8.81645657e-01, 8.87389629e-01,
     8.93024133e-01, 8.98553916e-01, 9.03983409e-01, 9.09316756e-01, 9.14557836e-01, 9.19710291e-01, 9.24777540e-01, 9.29762800e-01,
     9.34669099e-01, 9.39499296e-01, 9.44256090e-01, 9.48942030e-01, 9.53559531e-01, 9.58110882e-01, 9.62598250e-01, 9.67023698e-01,
     9.71389181e-01, 9.75696562e-01, 9.79947614e-01, 9.84144025e-01, 9.88287408e-01, 9.92379299e-01, 9.96421168e-01, 1.00041442e+00},
    {-2.96209812e-06, -2.45794824e-04, -8.18027564e-04, -1.19157447e-03, -1.46371229e-03, -1.67529045e-03, -1.84698016e-03,
     -1.99058664e-03, -2.11344205e-03, -2.22039065e-03, -2.31478873e-03, -2.39905115e-03, -2.47496962e-03, -2.54390793e-03,
     -2.60692676e-03, -2.66486645e-03, -2.71840346e-03, -2.76809003e-03, -2.81438252e-03, -2.85766225e-03, -2.89825096e-03,
     -2.93642247e-03, -2.97241172e-03, -3.00642174e-03, -3.03862912e-03, -3.06918837e-03, -3.09823546e-03, -3.12589065e-03,
     -3.15226077e-03, -3.17744116e-03, -3.20151726e-03, -3.22456591e-03, -3.24665644e-03, -3.26785166e-03, -3.28820859e-03,
     -3.30777919e-03, -3.32661092e-03, -3.34474723e-03, -3.36222800e-03, -3.37908995e-03, -3.39536690e-03, -3.41109012e-03,
     -3.42628855e-03, -3.44098902e-03, -3.45521647e-03, -3.46899410e-03, -3.48234354e-03, -3.49528498e-03, -3.50783728e-03,
     -3.52001812e-03, -3.53184405e-03, -3.54333061e-03, -3.55449241e-03, -3.56534320e-03, -3.57589590e-03, -3.58616273e-03,
     -3.59615520e-03, -3.60588419e-03, -3.61536000e-03, -3.62459235e-03, -3.63359049e-03, -3.64236316e-03, -3.65091867e-03,
     -3.65926491e-03},
    {-7.75894750e-06, 3.11294169e-03, 3.41779455e-03, 3.52160375e-03, 3.55957019e-03, 3.56903631e-03, 3.56431495e-03, 3.55194570e-03,
     3.53526954e-03,  3.51613008e-03, 3.49560287e-03, 3.47434152e-03, 3.45275527e-03, 3.43110577e-03, 3.40956242e-03, 3.38823540e-03,
     3.36719598e-03,  3.34648945e-03, 3.32614343e-03, 3.30617351e-03, 3.28658692e-03, 3.26738515e-03, 3.24856568e-03, 3.23012330e-03,
     3.21205091e-03,  3.19434023e-03, 3.17698219e-03, 3.15996727e-03, 3.14328577e-03, 3.12692791e-03, 3.11088400e-03, 3.09514449e-03,
     3.07970007e-03,  3.06454165e-03, 3.04966043e-03, 3.03504790e-03, 3.02069585e-03, 3.00659636e-03, 2.99274180e-03, 2.97912486e-03,
     2.96573849e-03,  2.95257590e-03, 2.93963061e-03, 2.92689635e-03, 2.91436713e-03, 2.90203718e-03, 2.88990095e-03, 2.87795312e-03,
     2.86618855e-03,  2.85460234e-03, 2.84318974e-03, 2.83194618e-03, 2.82086729e-03, 2.80994883e-03, 2.79918673e-03, 2.78857707e-03,
     2.77811607e-03,  2.76780009e-03, 2.75762559e-03, 2.74758919e-03, 2.73768761e-03, 2.72791768e-03, 2.71827634e-03, 2.70876064e-03},
    {-9.99869423e-01, -6.38561407e-01, -5.69514530e-01, -5.23990915e-01, -4.89176780e-01, -4.60615628e-01, -4.36195579e-01, -4.14739573e-01,
     -3.95520699e-01, -3.78056805e-01, -3.62010728e-01, -3.47136887e-01, -3.33250504e-01, -3.20208824e-01, -3.07899106e-01, -2.96230641e-01,
     -2.85129278e-01, -2.74533563e-01, -2.64391946e-01, -2.54660728e-01, -2.45302512e-01, -2.36285026e-01, -2.27580207e-01, -2.19163487e-01,
     -2.11013226e-01, -2.03110249e-01, -1.95437482e-01, -1.87979648e-01, -1.80723016e-01, -1.73655197e-01, -1.66764971e-01, -1.60042136e-01,
     -1.53477393e-01, -1.47062234e-01, -1.40788856e-01, -1.34650080e-01, -1.28639289e-01, -1.22750366e-01, -1.16977645e-01, -1.11315866e-01,
     -1.05760138e-01, -1.00305900e-01, -9.49488960e-02, -8.96851464e-02, -8.45109223e-02, -7.94227260e-02, -7.44172709e-02, -6.94914651e-02,
     -6.46423954e-02, -5.98673139e-02, -5.51636250e-02, -5.05288741e-02, -4.59607376e-02, -4.14570134e-02, -3.70156122e-02, -3.26345497e-02,
     -2.83119399e-02, -2.40459880e-02, -1.98349851e-02, -1.56773019e-02, -1.15713843e-02, -7.51574873e-03, -3.50897732e-03, 4.50285508e-04},
    {1.13389002e-04, 3.50509549e-01, 4.19971782e-01, 4.66835760e-01, 5.03053790e-01, 5.32907131e-01, 5.58475931e-01, 5.80942937e-01,
     6.01050219e-01, 6.19296203e-01, 6.36032925e-01, 6.51518847e-01, 6.65949666e-01, 6.79477330e-01, 6.92222311e-01, 7.04281836e-01,
     7.15735567e-01, 7.26649641e-01, 7.37079603e-01, 7.47072578e-01, 7.56668915e-01, 7.65903438e-01, 7.74806427e-01, 7.83404383e-01,
     7.91720644e-01, 7.99775871e-01, 8.07588450e-01, 8.15174821e-01, 8.22549745e-01, 8.29726527e-01, 8.36717208e-01, 8.43532720e-01,
     8.50183021e-01, 8.56677208e-01, 8.63023619e-01, 8.69229911e-01, 8.75303138e-01, 8.81249811e-01, 8.87075954e-01, 8.92787154e-01,
     8.98388600e-01, 9.03885123e-01, 9.09281227e-01, 9.14581119e-01, 9.19788738e-01, 9.24907772e-01, 9.29941684e-01, 9.34893728e-01,
     9.39766966e-01, 9.44564285e-01, 9.49288407e-01, 9.53941905e-01, 9.58527211e-01, 9.63046630e-01, 9.67502344e-01, 9.71896424e-01,
     9.76230838e-01, 9.80507456e-01, 9.84728057e-01, 9.88894335e-01, 9.93007906e-01, 9.97070310e-01, 1.00108302e+00, 1.00504744e+00}};

void Voice::fxAfterBlock(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterAmp) {
    TimbreSharedParams& shared = currentTimbre_->sharedParams();
    float ratioTimbres = 1.0f;

    // LP Algo
    int effectType = shared.effect1.type;
    float gainTmp = clamp(shared.effect1.param3 + matrixFilterAmp, 0, 16);
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;

    switch (effectType) {
    case FILTER_LP: {
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;

        float filterParam2 = 0.27f - clamp(shared.effect1.param2 + matrixFilterParam2, 0, 1) * 0.27f;

        // Low pass... on the Frequency
        fxParam1_ = (fxParamTmp + fxParam1_) * .5f;
        if (unlikely(fxParam1_ > 1.0f)) {
            fxParam1_ = 1.0f;
        }
        if (unlikely(fxParam1_ < 0.0f)) {
            fxParam1_ = 0.0f;
        }

        float pattern = (1 - filterParam2 * fxParam1_);

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv1L = v1L_;
        float localv0R = v0R_;
        float localv1R = v1R_;

        for (int k = 0; k < BLOCK_SIZE; k++) {

            // Left voice
            localv0L = pattern * localv0L - (fxParam1_)*localv1L + (fxParam1_) * (*sp);
            localv1L = pattern * localv1L + (fxParam1_)*localv0L;

            *sp = localv1L * mixerGain_;

            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }

            sp++;

            // Right voice
            localv0R = pattern * localv0R - (fxParam1_)*localv1R + (fxParam1_) * (*sp);
            localv1R = pattern * localv1R + (fxParam1_)*localv0R;

            *sp = localv1R * mixerGain_;

            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }

            sp++;
        }
        v0L_ = localv0L;
        v1L_ = localv1L;
        v0R_ = localv0R;
        v1R_ = localv1R;
    } break;
    case FILTER_HP: {
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;

        float filterParam2 = 0.27f - clamp(shared.effect1.param2 + matrixFilterParam2, 0, 1) * 0.27f;

        // Low pass... on the Frequency
        fxParam1_ = (fxParamTmp + 9.0f * fxParam1_) * .1f;
        if (unlikely(fxParam1_ > 1.0)) {
            fxParam1_ = 1.0;
        }
        if (unlikely(fxParam1_ < 0.0f)) {
            fxParam1_ = 0.0f;
        }
        float pattern = (1 - filterParam2 * fxParam1_);

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv1L = v1L_;
        float localv0R = v0R_;
        float localv1R = v1R_;

        for (int k = 0; k < BLOCK_SIZE; k++) {

            // Left voice
            localv0L = pattern * localv0L - (fxParam1_)*localv1L + (fxParam1_) * (*sp);
            localv1L = pattern * localv1L + (fxParam1_)*localv0L;

            *sp = (*sp - localv1L) * mixerGain_;

            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }

            sp++;

            // Right voice
            localv0R = pattern * localv0R - (fxParam1_)*localv1R + (fxParam1_) * (*sp);
            localv1R = pattern * localv1R + (fxParam1_)*localv0R;

            *sp = (*sp - localv1R) * mixerGain_;

            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }

            sp++;
        }
        v0L_ = localv0L;
        v1L_ = localv1L;
        v0R_ = localv0R;
        v1R_ = localv1R;

    } break;
    case FILTER_BASS: {
        // From musicdsp.com
        //        Bass Booster
        //
        //        Type : LP and SUM
        //        References : Posted by Johny Dupej
        //
        //        Notes :
        //        This function adds a low-passed signal to the original signal. The low-pass has a quite wide response.
        //
        //        selectivity - frequency response of the LP (higher value gives a steeper one) [70.0 to 140.0 sounds good]
        //        ratio - how much of the filtered signal is mixed to the original
        //        gain2 - adjusts the final volume to handle cut-offs (might be good to set dynamically)

        // static float selectivity, gain1, gain2, ratio, cap;
        // gain1 = 1.0/(selectivity + 1.0);
        //
        // cap= (sample + cap*selectivity )*gain1;
        // sample = saturate((sample + cap*ratio)*gain2);

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv0R = v0R_;

        fxParam2_ = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1) * 4;

        for (int k = 0; k < BLOCK_SIZE; k++) {

            localv0L = ((*sp) + localv0L * fxParam1_) * fxParam3_;
            (*sp) = ((*sp) + localv0L * fxParam2_) * mixerGain_;

            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }

            sp++;

            localv0R = ((*sp) + localv0R * fxParam1_) * fxParam3_;
            (*sp) = ((*sp) + localv0R * fxParam2_) * mixerGain_;

            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }

            sp++;
        }
        v0L_ = localv0L;
        v0R_ = localv0R;

    } break;
    case FILTER_MIXER: {
        float pan = shared.effect1.param1 * 2 - 1.0f;
        float* sp = this->sampleBlock_;
        float sampleR, sampleL;
        if (pan <= 0) {
            float onePlusPan = 1 + pan;
            float minusPan = -pan;
            for (int k = 0; k < BLOCK_SIZE; k++) {
                sampleL = *(sp);
                sampleR = *(sp + 1);

                *sp = (sampleL + sampleR * minusPan) * mixerGain_;
                sp++;
                *sp = sampleR * onePlusPan * mixerGain_;
                sp++;
            }
        } else if (pan > 0) {
            float oneMinusPan = 1 - pan;
            for (int k = 0; k < BLOCK_SIZE; k++) {
                sampleL = *(sp);
                sampleR = *(sp + 1);

                *sp = sampleL * oneMinusPan * mixerGain_;
                sp++;
                *sp = (sampleR + sampleL * pan) * mixerGain_;
                sp++;
            }
        }
    } break;
    case FILTER_CRUSHER: {
        // Algo from http://www.musicdsp.org/archive.php?classid=4#139
        // Lo-Fi Crusher
        // Type : Quantizer / Decimator with smooth control
        // References : Posted by David Lowenfels

        //        function output = crusher( input, normfreq, bits );
        //            step = 1/2^(bits);
        //            phasor = 0;
        //            last = 0;
        //
        //            for i = 1:length(input)
        //               phasor = phasor + normfreq;
        //               if (phasor >= 1.0)
        //                  phasor = phasor - 1.0;
        //                  last = step * floor( input(i)/step + 0.5 ); %quantize
        //               end
        //               output(i) = last; %sample and hold
        //            end
        //        end

        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency + .005f;
        if (unlikely(fxParamTmp > 1.0)) {
            fxParamTmp = 1.0;
        }
        if (unlikely(fxParamTmp < 0.005f)) {
            fxParamTmp = 0.005f;
        }
        fxParamA1_ = (fxParamTmp + 9.0f * fxParamA1_) * .1f;
        // Low pass... on the Sampling rate
        register float fxFreq = fxParamA1_;

        register float* sp = this->sampleBlock_;

        register float localPhase = fxPhase_;

        //        localPower = fxParam1 = pow(2, (int)(1.0f + 15.0f * params.effect.param2));
        //        localStep = fxParam2 = 1 / fxParam1;

        float param2Sum = matrixFilterParam2 + shared.effect1.param2;
        float effectTypeF = (float)effectType;
        if (param2Sum != fxParamB1_ || fxParamB2_ != effectTypeF) {
            fxParam1_ = 1 << (int)(1.0f + 15.0f * clamp(param2Sum, 0, 1));
            fxParam2_ = 1 / fxParam1_;
        }
        fxParamB1_ = param2Sum;
        fxParamB2_ = effectTypeF;

        register float localPower = fxParam1_;
        register float localStep = fxParam2_;

        register float localv0L = v0L_;
        register float localv0R = v0R_;

        for (int k = 0; k < BLOCK_SIZE; k++) {
            localPhase += fxFreq;
            if (unlikely(localPhase >= 1.0f)) {
                localPhase -= 1.0f;
                // Simulate floor by making the conversion always positive
                // simplify version
                register int iL = (*sp) * localPower + .75f;
                register int iR = (*(sp + 1)) * localPower + .75f;
                localv0L = localStep * iL;
                localv0R = localStep * iR;
            }

            *sp++ = localv0L * mixerGain_;
            *sp++ = localv0R * mixerGain_;
        }
        v0L_ = localv0L;
        v0R_ = localv0R;
        fxPhase_ = localPhase;

    } break;
    case FILTER_BP: {
        //        float input;                    // input sample
        //        float output;                   // output sample
        //        float v;                        // This is the intermediate value that
        //                                        //    gets stored in the delay registers
        //        float old1;                     // delay register 1, initialized to 0
        //        float old2;                     // delay register 2, initialized to 0
        //
        //        /* filter coefficients */
        //        omega1  = 2 * PI * f/srate; // f is your center frequency
        //        sn1 = (float)sin(omega1);
        //        cs1 = (float)cos(omega1);
        //        alpha1 = sn1/(2*Qvalue);        // Qvalue is none other than Q!
        //        a0 = 1.0f + alpha1;     // a0
        //        b0 = alpha1;            // b0
        //        b1 = 0.0f;          // b1/b0
        //        b2= -alpha1/b0          // b2/b0
        //        a1= -2.0f * cs1/a0;     // a1/a0
        //        a2= (1.0f - alpha1)/a0;          // a2/a0
        //        k = b0/a0;
        //
        //        /* The filter code */
        //
        //        v = k*input - a1*old1 - a2*old2;
        //        output = v + b1*old1 + b2*old2;
        //        old2 = old1;
        //        old1 = v;

        // fxParam1 v
        //

        float fxParam1PlusMatrixTmp = shared.effect1.param1 + matrixFilterFrequency;
        if (unlikely(fxParam1PlusMatrixTmp > 1.0f)) {
            fxParam1PlusMatrixTmp = 1.0f;
        }
        if (unlikely(fxParam1PlusMatrixTmp < 0.0f)) {
            fxParam1PlusMatrixTmp = 0.0f;
        }

        if (fxParam1PlusMatrix_ != fxParam1PlusMatrixTmp) {
            fxParam1PlusMatrix_ = fxParam1PlusMatrixTmp;
            recomputeBPValues(shared.effect1.param2, fxParam1PlusMatrix_ * fxParam1PlusMatrix_);
        }

        float localv0L = v0L_;
        float localv0R = v0R_;
        float localv1L = v1L_;
        float localv1R = v1R_;
        float* sp = this->sampleBlock_;

        for (int k = 0; k < BLOCK_SIZE; k++) {
            float localV = fxParam1_ /* k */ * (*sp) - fxParamA1_ * localv0L - fxParamA2_ * localv1L;
            *sp++ = (localV + /* fxParamB1 (==0) * localv0L  + */ fxParamB2_ * localv1L) * mixerGain_;
            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }
            localv1L = localv0L;
            localv0L = localV;

            localV = fxParam1_ /* k */ * (*sp) - fxParamA1_ * localv0R - fxParamA2_ * localv1R;
            *sp++ = (localV + /* fxParamB1 (==0) * localv0R + */ fxParamB2_ * localv1R) * mixerGain_;
            if (unlikely(*sp > ratioTimbres)) {
                *sp = ratioTimbres;
            }
            if (unlikely(*sp < -ratioTimbres)) {
                *sp = -ratioTimbres;
            }
            localv1R = localv0R;
            localv0R = localV;
        }

        v0L_ = localv0L;
        v0R_ = localv0R;
        v1L_ = localv1L;
        v1R_ = localv1R;

        break;
    }

    case FILTER_LP2: {
        float fxParamTmp = LP2OFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = 0.27f - clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1) * 0.267f;

        const float f = (fxParam1_);
        const float pattern = (1 - filterParam2 * f * 0.997f);

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv1L = v1L_;
        float localv0R = v0R_;
        float localv1R = v1R_;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.27f + f * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            localv0L = pattern * localv0L - f * sat25(localv1L + *sp);
            localv1L = pattern * localv1L + f * localv0L;

            localv0L = pattern * localv0L - f * (localv1L + *sp);
            localv1L = pattern * localv1L + f * localv0L;

            _ly1L = coef1 * (_ly1L + localv1L) - _lx1L; // allpass
            _lx1L = localv1L;

            *sp++ = clamp(_ly1L * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            localv0R = pattern * localv0R - f * sat25(localv1R + *sp);
            localv1R = pattern * localv1R + f * localv0R;

            localv0R = pattern * localv0R - f * (localv1R + *sp);
            localv1R = pattern * localv1R + f * localv0R;

            _ly1R = coef1 * (_ly1R + localv1R) - _lx1R; // allpass
            _lx1R = localv1R;

            *sp++ = clamp(_ly1R * mixerGain_, -ratioTimbres, ratioTimbres);
        }
        v0L_ = localv0L;
        v1L_ = localv1L;
        v0R_ = localv0R;
        v1R_ = localv1R;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_HP2: {
        float fxParamTmp = LP2OFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = 0.27f - clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1) * 0.267f;

        const float f = (fxParam1_);
        const float pattern = (1 - filterParam2 * f);

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv1L = v1L_;
        float localv0R = v0R_;
        float localv1R = v1R_;

        for (int k = 0; k < BLOCK_SIZE; k++) {

            // Left voice
            localv0L = pattern * localv0L + f * sat33(-localv1L + *sp);
            localv1L = pattern * localv1L + f * localv0L;

            localv0L = pattern * localv0L + f * (-localv1L + *sp);
            localv1L = pattern * localv1L + f * localv0L;

            *sp++ = clamp((*sp - localv1L) * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            localv0R = pattern * localv0R + f * sat33(-localv1R + *sp);
            localv1R = pattern * localv1R + f * localv0R;

            localv0R = pattern * localv0R + f * (-localv1R + *sp);
            localv1R = pattern * localv1R + f * localv0R;

            *sp++ = clamp((*sp - localv1R) * mixerGain_, -ratioTimbres, ratioTimbres);
        }
        v0L_ = localv0L;
        v1L_ = localv1L;
        v0R_ = localv0R;
        v1R_ = localv1R;

    } break;
    case FILTER_BP2: {
        float fxParam1PlusMatrixTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0, 1);
        if (fxParam1PlusMatrix_ != fxParam1PlusMatrixTmp) {
            fxParam1PlusMatrix_ = fxParam1PlusMatrixTmp;
            recomputeBPValues(shared.effect1.param2, fxParam1PlusMatrix_ * fxParam1PlusMatrix_);
        }

        float localv0L = v0L_;
        float localv0R = v0R_;
        float localv1L = v1L_;
        float localv1R = v1R_;
        float* sp = this->sampleBlock_;
        float in, temp, localV;

        for (int k = 0; k < BLOCK_SIZE; k++) {
            // Left
            in = fxParam1_ * sat33(*sp);
            temp = in - fxParamA1_ * localv0L - fxParamA2_ * localv1L;
            localv1L = localv0L;
            localv0L = temp;
            localV = (temp + (in - fxParamA1_ * localv0L - fxParamA2_ * localv1L)) * 0.5f;
            *sp++ = clamp((localV + fxParamB2_ * localv1L) * mixerGain_, -ratioTimbres, ratioTimbres);

            localv1L = localv0L;
            localv0L = localV;

            // Right
            in = fxParam1_ * sat33(*sp);
            temp = in - fxParamA1_ * localv0R - fxParamA2_ * localv1R;
            localv1R = localv0R;
            localv0R = temp;
            localV = (temp + (in - fxParamA1_ * localv0R - fxParamA2_ * localv1R)) * 0.5f;
            *sp++ = clamp((localV + fxParamB2_ * localv1R) * mixerGain_, -ratioTimbres, ratioTimbres);

            localv1R = localv0R;
            localv0R = localV;
        }

        v0L_ = localv0L;
        v0R_ = localv0R;
        v1L_ = localv1L;
        v1R_ = localv1R;

        break;
    }
    case FILTER_LP3: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = fxParam1_ * fxParam1_ * SVFRANGE;
        const float fb = sqrt3(1 - filterParam2 * 0.999f);
        const float scale = sqrt3(fb);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, bandL = v1L_;
        float lowR = v0R_, bandR = v1R_;

        const float svfGain = (1 + SVFGAINOFFSET + filterParam2 * filterParam2 * 0.75f) * mixerGain_;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.33f + f * 0.43f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice

            _ly1L = coef1 * (_ly1L + *sp) - _lx1L; // allpass
            _lx1L = *sp;

            lowL += f * bandL;
            bandL += f * (scale * _ly1L - lowL - fb * sat25(bandL));

            lowL += f * bandL;
            bandL += f * (scale * _ly1L - lowL - fb * (bandL));

            *sp++ = clamp(lowL * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice

            _ly1R = coef1 * (_ly1R + *sp) - _lx1R; // allpass
            _lx1R = *sp;

            lowR += f * bandR;
            bandR += f * (scale * _ly1R - lowR - fb * sat25(bandR));

            lowR += f * bandR;
            bandR += f * (scale * _ly1R - lowR - fb * (bandR));

            *sp++ = clamp(lowR * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_HP3: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = fxParam1_ * fxParam1_ * 0.93f;
        const float fb = sqrt3(1 - filterParam2 * 0.999f);
        const float scale = sqrt3(fb);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;

        const float svfGain = (1 + SVFGAINOFFSET + filterParam2 * filterParam2 * 0.75f) * mixerGain_;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.15f + f * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            _ly1L = coef1 * (_ly1L + *sp) - _lx1L; // allpass
            _lx1L = *sp;

            lowL = lowL + f * bandL;
            highL = scale * (_ly1L)-lowL - fb * sat33(bandL);
            bandL = f * highL + bandL;

            lowL = lowL + f * bandL;
            highL = scale * (_ly1L)-lowL - fb * bandL;
            bandL = f * highL + bandL;

            *sp++ = clamp(highL * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            _ly1R = coef1 * (_ly1R + *sp) - _lx1R; // allpass
            _lx1R = *sp;

            lowR = lowR + f * bandR;
            highR = scale * (_ly1R)-lowR - fb * sat33(bandR);
            bandR = f * highR + bandR;

            lowR = lowR + f * bandR;
            highR = scale * (_ly1R)-lowR - fb * bandR;
            bandR = f * highR + bandR;

            *sp++ = clamp(highR * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_BP3: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = fxParam1_ * fxParam1_ * SVFRANGE;
        const float fb = sqrt3(0.5f - filterParam2 * 0.495f);
        const float scale = sqrt3(fb);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;

        const float svfGain = (1 + SVFGAINOFFSET + filterParam2 * filterParam2 * 0.75f) * mixerGain_;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        float _ly2L = v4L_, _ly2R = v4R_;
        float _lx2L = v5L_, _lx2R = v5R_;
        const float f1 = clamp(f * 0.56f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);
        float coef2 = (1.0f - f1) / (1.0f + f1);

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            _ly1L = coef1 * (_ly1L + *sp) - _lx1L; // allpass
            _lx1L = *sp;

            lowL = lowL + f * bandL;
            highL = scale * _ly1L - lowL - fb * sat25(bandL);
            bandL = f * highL + bandL;

            _ly2L = coef2 * (_ly2L + bandL) - _lx2L; // allpass 2
            _lx2L = bandL;

            *sp++ = clamp(_ly2L * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            _ly1R = coef1 * (_ly1R + *sp) - _lx1R; // allpass
            _lx1R = *sp;

            lowR = lowR + f * bandR;
            highR = scale * _ly1R - lowR - fb * sat25(bandR);
            bandR = f * highR + bandR;

            _ly2R = coef2 * (_ly2R + bandR) - _lx2R; // allpass 2
            _lx2R = bandR;

            *sp++ = clamp(_ly2R * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
        v4L_ = _ly2L;
        v4R_ = _ly2R;
        v5L_ = _lx2L;
        v5R_ = _lx2R;

    } break;
    case FILTER_PEAK: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = fxParam1_ * fxParam1_ * SVFRANGE;
        const float fb = sqrt3(sqrt3(1 - filterParam2 * 0.999f));
        const float scale = sqrt3(fb);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;

        const float svfGain = (1 + SVFGAINOFFSET + filterParam2 * filterParam2 * 0.75f) * mixerGain_;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.27f + f * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        float out;

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            lowL = lowL + f * bandL;
            highL = scale * (*sp) - lowL - fb * bandL;
            bandL = f * highL + bandL;

            lowL = lowL + f * bandL;
            highL = scale * (*sp) - lowL - fb * sat33(bandL);
            bandL = f * highL + bandL;

            out = (bandL + highL + lowL);

            _ly1L = coef1 * (_ly1L + out) - _lx1L; // allpass
            _lx1L = out;

            *sp++ = clamp(_ly1L * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            lowR = lowR + f * bandR;
            highR = scale * (*sp) - lowR - fb * bandR;
            bandR = f * highR + bandR;

            lowR = lowR + f * bandR;
            highR = scale * (*sp) - lowR - fb * sat33(bandR);
            bandR = f * highR + bandR;

            out = (bandR + highR + lowR);

            _ly1R = coef1 * (_ly1R + out) - _lx1R; // allpass
            _lx1R = out;

            *sp++ = clamp(_ly1R * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_NOTCH: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = fxParam1_ * fxParam1_ * SVFRANGE;
        const float fb = sqrt3(1 - filterParam2 * 0.6f);
        const float scale = sqrt3(fb);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;
        float notch;

        const float svfGain = (1 + SVFGAINOFFSET) * mixerGain_;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(fxParam1_ * 0.66f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        for (int k = 0; k < BLOCK_SIZE; k++) {

            // Left voice
            lowL = lowL + f * bandL;
            highL = scale * (*sp) - lowL - fb * bandL;
            bandL = f * highL + bandL;
            notch = (highL + lowL);

            _ly1L = coef1 * (_ly1L + notch) - _lx1L; // allpass
            _lx1L = notch;

            *sp++ = clamp(_ly1L * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            lowR = lowR + f * bandR;
            highR = scale * (*sp) - lowR - fb * bandR;
            bandR = f * highR + bandR;
            notch = (highR + lowR);

            _ly1R = coef1 * (_ly1R + notch) - _lx1R; // allpass
            _lx1R = notch;

            *sp++ = clamp(_ly1R * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_BELL: {
        // filter algo from Andrew Simper
        // https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        // A = 10 ^ (db / 40)
        const float A = (tanh3(filterParam2 * 2) * 1.5f) + 0.5f;

        const float res = 0.6f;
        const float k = 1 / (0.0001f + res * A);
        const float g = 0.0001f + (fxParam1_);
        const float a1 = 1 / (1 + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float amp = k * (A * A - 1);

        float* sp = this->sampleBlock_;

        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float v1, v2, v3, out;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.25f + fxParam1_ * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            v3 = (*sp) - ic2eqL;
            v1 = a1 * ic1eqL + a2 * v3;
            v2 = ic2eqL + a2 * ic1eqL + a3 * v3;
            ic1eqL = 2 * v1 - ic1eqL;
            ic2eqL = 2 * v2 - ic2eqL;

            _ly1L = coef1 * (_ly1L + v1) - _lx1L; // allpass
            _lx1L = v1;

            out = (*sp + (amp * _ly1L));

            *sp++ = clamp(out * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            v3 = (*sp) - ic2eqR;
            v1 = a1 * ic1eqR + a2 * v3;
            v2 = ic2eqR + a2 * ic1eqR + a3 * v3;
            ic1eqR = 2 * v1 - ic1eqR;
            ic2eqR = 2 * v2 - ic2eqR;

            _ly1R = coef1 * (_ly1R + v1) - _lx1R; // allpass
            _lx1R = v1;

            out = (*sp + (amp * _ly1R));

            *sp++ = clamp(out * mixerGain_, -ratioTimbres, ratioTimbres);
        }

        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_LOWSHELF: {
        // filter algo from Andrew Simper
        // https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        // A = 10 ^ (db / 40)
        const float A = (tanh3(filterParam2 * 2) * 1) + 0.5f;

        const float res = 0.5f;
        const float k = 1 / (0.0001f + res);
        const float g = 0.0001f + (fxParam1_);
        const float a1 = 1 / (1 + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float m1 = k * (A - 1);
        const float m2 = (A * A - 1);

        float* sp = this->sampleBlock_;

        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float v1, v2, v3, out;

        const float svfGain = mixerGain_ * 0.8f;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.33f + fxParam1_ * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            v3 = (*sp) - ic2eqL;
            v1 = a1 * ic1eqL + a2 * v3;
            v2 = ic2eqL + a2 * ic1eqL + a3 * v3;
            ic1eqL = 2 * v1 - ic1eqL;
            ic2eqL = 2 * v2 - ic2eqL;
            out = (*sp + (m1 * v1 + m2 * v2));
            _ly1L = coef1 * (_ly1L + out) - _lx1L; // allpass
            _lx1L = out;

            *sp++ = clamp(_ly1L * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            v3 = (*sp) - ic2eqR;
            v1 = a1 * ic1eqR + a2 * v3;
            v2 = ic2eqR + a2 * ic1eqR + a3 * v3;
            ic1eqR = 2 * v1 - ic1eqR;
            ic2eqR = 2 * v2 - ic2eqR;
            out = (*sp + (m1 * v1 + m2 * v2));

            _ly1R = coef1 * (_ly1R + out) - _lx1R; // allpass
            _lx1R = out;

            *sp++ = clamp(_ly1R * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_HIGHSHELF: {
        // filter algo from Andrew Simper
        // https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        // A = 10 ^ (db / 40)
        const float A = (tanh3(filterParam2 * 2) * 1) + 0.5f;

        const float res = 0.5f;
        const float k = 1 / (0.0001f + res);
        const float g = 0.0001f + (fxParam1_);
        const float a1 = 1 / (1 + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float m0 = A * A;
        const float m1 = k * (A - 1) * A;
        const float m2 = (1 - A * A);

        float* sp = this->sampleBlock_;

        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float v1, v2, v3, out;

        const float svfGain = mixerGain_ * 0.8f;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.33f + fxParam1_ * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            v3 = (*sp) - ic2eqL;
            v1 = a1 * ic1eqL + a2 * v3;
            v2 = ic2eqL + a2 * ic1eqL + a3 * v3;
            ic1eqL = 2 * v1 - ic1eqL;
            ic2eqL = 2 * v2 - ic2eqL;

            out = (m0 * *sp + (m1 * v1 + m2 * v2));
            _ly1L = coef1 * (_ly1L + out) - _lx1L; // allpass
            _lx1L = out;

            *sp++ = clamp(_ly1L * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            v3 = (*sp) - ic2eqR;
            v1 = a1 * ic1eqR + a2 * v3;
            v2 = ic2eqR + a2 * ic1eqR + a3 * v3;
            ic1eqR = 2 * v1 - ic1eqR;
            ic2eqR = 2 * v2 - ic2eqR;

            out = (m0 * *sp + (m1 * v1 + m2 * v2));
            _ly1R = coef1 * (_ly1R + out) - _lx1R; // allpass
            _lx1R = out;

            *sp++ = clamp(_ly1R * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_LPHP: {
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = 0.27f - clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1) * 0.267f;

        const int mixWet = (fxParam1_ * 122);
        const float mixA = (1 + filterParam2 * 2) * panTable[122 - mixWet];
        const float mixB = 2.5f * panTable[5 + mixWet];

        const float f = fxParam1_ * fxParam1_ * 1.5f;
        const float pattern = (1 - filterParam2 * f);

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv1L = v1L_;
        float localv0R = v0R_;
        float localv1R = v1R_;
        float out;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;

        const float f1 = clamp(0.27f + fxParam1_ * 0.44f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        const float gain = mixerGain_ * 1.3f;

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice

            _ly1L = coef1 * (_ly1L + (*sp)) - _lx1L; // allpass
            _lx1L = (*sp);

            localv0L = pattern * localv0L + (f * (-localv1L + _ly1L));
            localv1L = pattern * localv1L + f * localv0L;

            localv0L = pattern * localv0L + (f * (-localv1L + _ly1L));
            localv1L = pattern * localv1L + f * localv0L;

            out = sat33((localv1L * mixA) + ((_ly1L - localv1L) * mixB));

            *sp++ = clamp(out * gain, -ratioTimbres, ratioTimbres);

            // Right voice

            _ly1R = coef1 * (_ly1R + (*sp)) - _lx1R; // allpass
            _lx1R = (*sp);

            localv0R = pattern * localv0R + (f * (-localv1R + _ly1R));
            localv1R = pattern * localv1R + f * localv0R;

            localv0R = pattern * localv0R + (f * (-localv1R + _ly1R));
            localv1R = pattern * localv1R + f * localv0R;

            out = sat33((localv1R * mixA) + ((_ly1R - localv1R) * mixB));

            *sp++ = clamp(out * gain, -ratioTimbres, ratioTimbres);
        }
        v0L_ = localv0L;
        v1L_ = localv1L;
        v0R_ = localv0R;
        v1R_ = localv1R;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_BPds: {
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = fxParam1_ * fxParam1_ * SVFRANGE;
        const float fb = sqrt3(0.5f - filterParam2 * 0.4995f);
        const float scale = sqrt3(fb);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;

        const float svfGain = (1 + SVFGAINOFFSET + filterParam2 * filterParam2 * 0.75f) * mixerGain_;

        const float sat = 1 + filterParam2;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(fxParam1_ * 0.35f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);
        float outL = 0, outR = 0;

        const float r = 0.9840f;

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            *sp = outL + ((-outL + *sp) * r);

            lowL = lowL + f * bandL;
            highL = scale * (*sp) - lowL - fb * bandL;
            bandL = (f * highL) + bandL;
            _ly1L = coef1 * (_ly1L + bandL) - _lx1L; // allpass
            _lx1L = bandL;

            outL = sat25(_ly1L * sat);
            *sp++ = clamp(outL * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = outR + ((-outR + *sp) * r);

            lowR = lowR + f * bandR;
            highR = scale * (*sp) - lowR - fb * bandR;
            bandR = (f * highR) + bandR;
            _ly1R = coef1 * (_ly1R + bandR) - _lx1R; // allpass
            _lx1R = bandR;

            outR = sat25(_ly1R * sat);
            *sp++ = clamp(outR * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_LPWS: {
        const float prevParam1 = clamp(fxParam1_, 0.0f, 1.0f);
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParamTmp *= fxParamTmp;
        fxParam1_ = clamp((fxParamTmp + 9.0f * prevParam1) * 0.1f, 0.0f, 1.0f);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);

        // ZDF 2-pole resonant Lowpass followed by ADAA warm tube waveshaper with Dry/Wet Mix
        const float g0 = 0.0025f + prevParam1 * (0.18f + 1.35f * prevParam1);
        const float g1 = 0.0025f + fxParam1_ * (0.18f + 1.35f * fxParam1_);
        const float dg = (g1 - g0) * 0.03125f;
        float g = g0;
        const float k = 1.15f - 0.35f * filterParam2;
        const float a1_0 = 1.0f / (1.0f + g0 * (g0 + k));
        const float a1_1 = 1.0f / (1.0f + g1 * (g1 + k));
        const float da1 = (a1_1 - a1_0) * 0.03125f;
        float a1_lin = a1_0;

        // Continuous unquantized equal-power wet/dry crossfade
        const float invW = 1.0f - filterParam2;
        const float mixWetGain = (1.0f - invW * invW) * mixerGain_;
        const float mixDryGain = (1.0f - filterParam2 * filterParam2) * mixerGain_;
        const float wsDrive = 1.8f + 1.4f * filterParam2;

        float* sp = this->sampleBlock_;
        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float prevXL = v2L_, prevXR = v2R_;
        float prevFL = sat25ADAA1_F(prevXL), prevFR = sat25ADAA1_F(prevXR);

        for (int kSample = BLOCK_SIZE; kSample--;) {
            g += dg;
            a1_lin += da1;
            const float a1 = a1_lin * (2.0f - (1.0f + g * (g + k)) * a1_lin);

            // Left voice
            float inL = *sp;
            float v3 = inL - ic2eqL;
            float v1 = a1 * (ic1eqL + g * v3);
            float v2 = ic2eqL + g * v1;
            ic1eqL = 2.0f * v1 - ic1eqL;
            ic2eqL = 2.0f * v2 - ic2eqL;
            float shapedL = softLimitVoice(sat25ADAA(v2 * wsDrive + 0.06f, prevXL, prevFL) * 1.35f - 0.04f);
            *sp++ = clamp(shapedL * mixWetGain + inL * mixDryGain, -ratioTimbres, ratioTimbres);

            // Right voice
            float inR = *sp;
            v3 = inR - ic2eqR;
            v1 = a1 * (ic1eqR + g * v3);
            v2 = ic2eqR + g * v1;
            ic1eqR = 2.0f * v1 - ic1eqR;
            ic2eqR = 2.0f * v2 - ic2eqR;
            float shapedR = softLimitVoice(sat25ADAA(v2 * wsDrive + 0.06f, prevXR, prevFR) * 1.35f - 0.04f);
            *sp++ = clamp(shapedR * mixWetGain + inR * mixDryGain, -ratioTimbres, ratioTimbres);
        }
        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;
        v2L_ = prevXL;
        v2R_ = prevXR;
    } break;
    case FILTER_TILT: {
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float filterParam2 = 0.27f - clamp(shared.effect1.param2 + matrixFilterParam2, 0, 1) * 0.27f;

        const float f1 = clamp(fxParam1_ * 0.24f + 0.09f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        const float res = 0.85f;

        const float amp = 19.93f;
        const float gain = (filterParam2 - 0.5f);
        const float gfactor = 10;
        float g1, g2;
        if (gain > 0) {
            g1 = -gfactor * gain;
            g2 = gain;
        } else {
            g1 = -gain;
            g2 = gfactor * gain;
        };

        // two separate gains
        const float lgain = expf_fast(g1 / amp) - 1.f;
        const float hgain = expf_fast(g2 / amp) - 1.f;

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv0R = v0R_;
        float localv1L = v1L_;
        float localv1R = v1R_;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _ly2L = v3L_, _ly2R = v3R_;
        float _lx1L = v4L_, _lx1R = v4R_;

        float out;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            localv0L = res * localv0L - fxParam1_ * localv1L + sigmoid(*sp);
            localv1L = res * localv1L + fxParam1_ * localv0L;

            out = (*sp + lgain * localv1L + hgain * (*sp - localv1L));

            _ly1L = coef1 * (_ly1L + out) - _lx1L; // allpass
            _lx1L = out;

            *sp++ = clamp(_ly1L * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            localv0R = res * localv0R - fxParam1_ * localv1R + sigmoid(*sp);
            localv1R = res * localv1R + fxParam1_ * localv0R;

            out = (*sp + lgain * localv1R + hgain * (*sp - localv1R));

            _ly1R = coef1 * (_ly1R + out) - _lx1R; // allpass
            _lx1R = out;

            *sp++ = clamp(_ly1R * mixerGain_, -ratioTimbres, ratioTimbres);
        }

        v0L_ = localv0L;
        v0R_ = localv0R;
        v1L_ = localv1L;
        v1R_ = localv1R;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _ly2L;
        v3R_ = _ly2R;
        v4L_ = _lx1L;
        v4R_ = _lx1R;

    } break;
    case FILTER_STEREO: {
        float fxParamTmp = fabsf(shared.effect1.param1 + matrixFilterFrequency);
        fxParam1_ = ((fxParamTmp + 19.0f * fxParam1_) * .05f);

        float OffsetTmp = clamp(fabsf(matrixFilterParam2 + shared.effect1.param2), 0, 1);
        fxParam2_ = ((OffsetTmp + 19.0f * fxParam2_) * .05f);

        const float offset = fxParam2_ * 0.66f - 0.33f;

        const float f1 = clamp((fxParam1_), 0.001f, 0.99f) * 1.8f - 0.9f;
        const float f2 = clamp((fxParam1_ + offset), 0.001f, 0.99f) * 1.8f - 0.9f;
        const float f3 = clamp((fxParam1_ + offset * 2), 0.001f, 0.99f) * 1.8f - 0.9f;
        ;

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;
        float lowL2 = v2L_, highL2 = 0, bandL2 = v3L_;
        float lowR2 = v2R_, highR2 = 0, bandR2 = v3R_;
        float lowL3 = v4L_, highL3 = 0, bandL3 = v5L_;
        float lowR3 = v4R_, highR3 = 0, bandR3 = v5R_;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice

            lowL = (*sp) + f1 * (bandL = lowL - f1 * (*sp));
            highL = (bandL + (*sp)) * 0.5f;

            lowL2 = highL + f2 * (bandL2 = (lowL2)-f2 * lowL);
            highL2 = ((bandL2) + highL) * 0.5f;

            lowL3 = lowL2 + f3 * (bandL3 = lowL3 - f3 * lowL2);
            highL3 = (bandL3 + highL2) * 0.5f;

            *sp++ = clamp(highL3 * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            lowR = (*sp) + f1 * (bandR = lowR - f1 * (*sp));
            highR = (bandR + (*sp)) * 0.5f;

            lowR2 = lowR + f2 * (bandR2 = (lowR2)-f2 * lowR);
            highR2 = ((bandR2) + highR) * 0.5f;

            lowR3 = lowR2 + f3 * (bandR3 = lowR3 - f3 * lowR2);
            highR3 = (bandR3 + highR2) * 0.5f;

            *sp++ = clamp(highR3 * mixerGain_, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = lowL2;
        v3L_ = bandL2;
        v2R_ = lowR2;
        v3R_ = bandR2;

        v4L_ = lowL3;
        v5L_ = bandL3;
        v4R_ = lowR3;
        v5R_ = bandR3;
    } break;
    case FILTER_SAT: {
        // Warm Asymmetric Tape / Triode Saturator ("Sat ": Thrs = param1, Tone = param2)
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParam1_ = (fxParamTmp + 9.0f * fxParam1_) * 0.1f;
        float toneParam = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);

        // As Thrs (fxParam1_) increases, saturation drive increases with automatic gain makeup
        const float drive = 1.0f + fxParam1_ * (2.2f + 2.8f * fxParam1_);
        const float asym = 0.08f * fxParam1_;
        const float makeup = (1.0f / (0.85f + 0.38f * fxParam1_)) * mixerGain_;

        // Post-saturation ZDF 2-pole Tone filter (sweeps from warm tape rolloff to open presence)
        const float g = 0.015f + toneParam * (0.22f + 1.15f * toneParam);
        const float k = 1.35f;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        float* sp = this->sampleBlock_;
        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float prevXL = v2L_, prevXR = v2R_;
        float prevFL = sigmoidADAA1_F(prevXL), prevFR = sigmoidADAA1_F(prevXR);

        for (int k = BLOCK_SIZE; k--;) {
            // Left
            float xL = clamp((*sp) * drive + asym, -1.45f, 1.45f);
            float satL = sigmoidADAA(xL, prevXL, prevFL) - 0.85f * asym;
            float v3 = satL - ic2eqL;
            float v1 = a1 * ic1eqL + a2 * v3;
            float v2 = ic2eqL + a2 * ic1eqL + a3 * v3;
            ic1eqL = 2.0f * v1 - ic1eqL;
            ic2eqL = 2.0f * v2 - ic2eqL;
            *sp++ = clamp(v2 * makeup, -ratioTimbres, ratioTimbres);

            // Right
            float xR = clamp((*sp) * drive + asym, -1.45f, 1.45f);
            float satR = sigmoidADAA(xR, prevXR, prevFR) - 0.85f * asym;
            v3 = satR - ic2eqR;
            v1 = a1 * ic1eqR + a2 * v3;
            v2 = ic2eqR + a2 * ic1eqR + a3 * v3;
            ic1eqR = 2.0f * v1 - ic1eqR;
            ic2eqR = 2.0f * v2 - ic2eqR;
            *sp++ = clamp(v2 * makeup, -ratioTimbres, ratioTimbres);
        }
        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;
        v2L_ = prevXL;
        v2R_ = prevXR;
    } break;
    case FILTER_SIGMOID: {
        // S-Curve Soft-Clipped Overdrive & Resonant Tone Shaper ("Sigm": Driv = param1, Tone = param2)
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParam1_ = (fxParamTmp + 9.0f * fxParam1_) * 0.1f;

        float toneParam = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);
        const float drive = 1.1f + fxParam1_ * (3.5f + 8.5f * fxParam1_);
        const float gainComp = (0.95f / (1.0f + 0.22f * sqrt3(fxParam1_))) * mixerGain_;

        // ZDF SVF Tone filter with subtle midrange focus
        const float g = 0.008f + toneParam * (0.20f + 1.15f * toneParam);
        const float k = 1.10f - 0.35f * fxParam1_;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        float* sp = this->sampleBlock_;
        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float prevXL = v2L_, prevXR = v2R_;
        float prevFL = tanh4ADAA1_F(prevXL), prevFR = tanh4ADAA1_F(prevXR);

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            float satL = tanh4ADAA((*sp) * drive, prevXL, prevFL);
            float v3 = satL - ic2eqL;
            float v1 = a1 * ic1eqL + a2 * v3;
            float v2 = ic2eqL + a2 * ic1eqL + a3 * v3;
            ic1eqL = 2.0f * v1 - ic1eqL;
            ic2eqL = 2.0f * v2 - ic2eqL;
            float outL = v2 + 0.22f * v1;
            *sp++ = clamp(outL * gainComp, -ratioTimbres, ratioTimbres);

            // Right voice
            float satR = tanh4ADAA((*sp) * drive, prevXR, prevFR);
            v3 = satR - ic2eqR;
            v1 = a1 * ic1eqR + a2 * v3;
            v2 = ic2eqR + a2 * ic1eqR + a3 * v3;
            ic1eqR = 2.0f * v1 - ic1eqR;
            ic2eqR = 2.0f * v2 - ic2eqR;
            float outR = v2 + 0.22f * v1;
            *sp++ = clamp(outR * gainComp, -ratioTimbres, ratioTimbres);
        }

        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;
        v2L_ = prevXL;
        v2R_ = prevXR;
    } break;
    case FILTER_FOLD: {
        // West-Coast Anti-Aliased Wavefolder + Input Phase Allpass + ZDF Resonant Lowpass Gate ("Fold": Driv = param1, Tone = param2)
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParam1_ = (fxParamTmp + 9.0f * fxParam1_) * 0.1f;

        float toneParam = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);
        const float drive = fxParam1_ * (0.45f + 0.55f * fxParam1_);
        const float gain = (1.0f + 14.0f * drive) * 0.25f;
        const float finalGain = (0.92f / (1.0f + 0.35f * sqrt3(drive))) * mixerGain_;

        // Input allpass disperser shifting harmonic symmetry before folding
        const float f1 = clamp(0.18f + 0.52f * toneParam - 0.15f * fxParam1_, filterWindowMin, filterWindowMax);
        const float coef1 = (1.0f - f1) / (1.0f + f1);

        // ZDF 2-pole Lowpass controlled by Tone
        const float g = 0.006f + toneParam * (0.38f + 0.78f * toneParam);
        const float k = 1.15f - 0.35f * fxParam1_;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        float* sp = this->sampleBlock_;
        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        float prevX4L = v4L_, prevX4R = v4R_;
        float prevFL = foldADAA1_F(prevX4L), prevFR = foldADAA1_F(prevX4R);

        for (int kSample = BLOCK_SIZE; kSample--;) {
            // Left
            float inL = *sp;
            _ly1L = coef1 * (_ly1L + inL) - _lx1L;
            _lx1L = inL;
            float foldedL = foldADAA(_ly1L * gain, prevX4L, prevFL) * 4.0f;
            float v3 = foldedL - ic2eqL;
            float v1 = a1 * ic1eqL + a2 * v3;
            float v2 = ic2eqL + a2 * ic1eqL + a3 * v3;
            ic1eqL = 2.0f * v1 - ic1eqL;
            ic2eqL = 2.0f * v2 - ic2eqL;
            *sp++ = clamp(v2 * finalGain, -ratioTimbres, ratioTimbres);

            // Right
            float inR = *sp;
            _ly1R = coef1 * (_ly1R + inR) - _lx1R;
            _lx1R = inR;
            float foldedR = foldADAA(_ly1R * gain, prevX4R, prevFR) * 4.0f;
            v3 = foldedR - ic2eqR;
            v1 = a1 * ic1eqR + a2 * v3;
            v2 = ic2eqR + a2 * ic1eqR + a3 * v3;
            ic1eqR = 2.0f * v1 - ic1eqR;
            ic2eqR = 2.0f * v2 - ic2eqR;
            *sp++ = clamp(v2 * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;
        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
        v4L_ = prevX4L;
        v4R_ = prevX4R;
    } break;
    case FILTER_WRAP: {
        // Anti-Aliased Phase-Wrap Distortion + ZDF Tone Filter ("Wrap": Driv = param1, Tone = param2)
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParam1_ = clamp((fxParamTmp + 9.0f * clamp(fxParam1_, 0.0f, 1.0f)) * 0.1f, 0.0f, 1.0f);

        float toneParam = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);
        // Progressive convex drive: fine control around the +/-1 wrap threshold, deep multi-wrap at high values
        const float drive = fxParam1_ * (0.35f + 0.65f * fxParam1_);
        const float gain = 0.90f + 5.70f * drive;
        const float wetMix = clamp(fxParam1_ * 2.0f, 0.0f, 1.0f);
        const float dryMix = 1.0f - wetMix;
        const float finalGain = (0.95f / (1.0f + 0.65f * sqrt3(drive))) * mixerGain_;

        // ZDF 2-pole Lowpass controlled by Tone
        const float g = 0.006f + toneParam * (0.20f + 1.20f * toneParam);
        const float k = 1.25f - 0.30f * drive;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        float* sp = this->sampleBlock_;
        float ic1eqL = v0L_, ic2eqL = v1L_;
        float ic1eqR = v0R_, ic2eqR = v1R_;
        float prevXL = v4L_, prevXR = v4R_;
        float prevFL = wrapADAA1_F(prevXL), prevFR = wrapADAA1_F(prevXR);

        for (int kSample = BLOCK_SIZE; kSample--;) {
            // Left
            float inL = *sp;
            float wrappedL = inL * dryMix + wrapADAA(inL * gain, prevXL, prevFL) * wetMix;
            float v3 = wrappedL - ic2eqL;
            float v1 = a1 * ic1eqL + a2 * v3;
            float v2 = ic2eqL + a2 * ic1eqL + a3 * v3;
            ic1eqL = 2.0f * v1 - ic1eqL;
            ic2eqL = 2.0f * v2 - ic2eqL;
            *sp++ = clamp(v2 * finalGain, -ratioTimbres, ratioTimbres);

            // Right
            float inR = *sp;
            float wrappedR = inR * dryMix + wrapADAA(inR * gain, prevXR, prevFR) * wetMix;
            v3 = wrappedR - ic2eqR;
            v1 = a1 * ic1eqR + a2 * v3;
            v2 = ic2eqR + a2 * ic1eqR + a3 * v3;
            ic1eqR = 2.0f * v1 - ic1eqR;
            ic2eqR = 2.0f * v2 - ic2eqR;
            *sp++ = clamp(v2 * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = ic1eqL;
        v1L_ = ic2eqL;
        v0R_ = ic1eqR;
        v1R_ = ic2eqR;
        v4L_ = prevXL;
        v4R_ = prevXR;
    } break;
    case FILTER_XOR: {
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        float pos;
        float p1sq = sqrt3(fxParam1_);
        if (p1sq > 0.5f) {
            pos = ((1 - panTable[127 - (int)(p1sq * 64)]) * 2) - 1;
        } else {
            pos = (panTable[(int)(p1sq * 64)] * 2) - 1;
        }

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv0R = v0R_;

        const float a = 0.95f - filterParam2 * 0.5f;
        const float b = 1 - a;

        int digitsA, digitsB;
        const float threshold = (0.66f - p1sq * 0.66f) * 1.0f;

        float _ly2L = v1L_, _ly2R = v1R_;
        float _lx2L = v5L_, _lx2R = v5R_;

        const float f2 = clamp(0.33f + fxParam1_ * 0.5f, filterWindowMin, filterWindowMax);
        float coef2 = (1.0f - f2) / (1.0f + f2);

        for (int k = BLOCK_SIZE; k--;) {
            // Left
            if (fabsf(*sp) > threshold) {
                localv0L = (*sp) - pos * (localv0L + pos * (*sp));
                digitsA = FLOAT2SHORT * (*sp);
                digitsB = FLOAT2SHORT * localv0L;
                localv0L = SHORT2FLOAT * roundf(digitsA ^ digitsB & 0xfff);
            } else {
                localv0L = *sp;
            }

            localv0L = (*sp * b) + (localv0L * a);

            _ly2L = coef2 * (_ly2L + localv0L) - _lx2L; // allpass
            _lx2L = localv0L;

            *sp++ = clamp(_ly2L * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right
            if (fabsf(*sp) > threshold) {
                localv0R = (*sp) - pos * (localv0R + pos * (*sp));
                digitsA = FLOAT2SHORT * (*sp);
                digitsB = FLOAT2SHORT * localv0R;
                localv0R = SHORT2FLOAT * roundf(digitsA ^ digitsB & 0xfff);
            } else {
                localv0R = *sp;
            }

            localv0R = (*sp * b) + (localv0R * a);

            _ly2R = coef2 * (_ly2R + localv0R) - _lx2R; // allpass
            _lx2R = localv0R;

            *sp++ = clamp(_ly2R * mixerGain_, -ratioTimbres, ratioTimbres);
        }

        v0L_ = localv0L;
        v0R_ = localv0R;

        v1L_ = _ly2L;
        v1R_ = _ly2R;
        v5L_ = _lx2L;
        v5R_ = _lx2R;
    } break;
    case FILTER_TEXTURE1: {
        float fxParamTmp = (shared.effect1.param1 + matrixFilterFrequency);
        fxParamTmp *= fxParamTmp;

        const uint_fast8_t random = (*(uint_fast8_t*)noise) & 0xff;
        if (random > 252) {
            fxParam1_ += ((random & 1) * 0.007874015748031f);
        }

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = sqrt3(1 - clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1) * 0.99f);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;
        float notch;

        const float f = (fxParam1_ * fxParam1_ * fxParam1_) * 0.5f;
        const float fb = filterParam2;
        const float scale = sqrt3(fb);

        const int highBits = 0xFFFFFD4F;
        const int lowBits = ~(highBits);

        const int ll = (int)(fxParam1_ * lowBits);
        int digitsL, digitsR;
        int lowDigitsL, lowDigitsR;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        const float f1 = clamp(0.33f + f * 0.47f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        // const float r = 0.9940f;

        for (int k = BLOCK_SIZE; k--;) {
            // LEFT
            //*sp = _ly1L + ((-_ly1L + *sp) * r);

            lowL = lowL + f * bandL;
            highL = scale * (*sp) - lowL - fb * bandL;
            bandL = f * highL + bandL;
            notch = lowL + highL;

            digitsL = FLOAT2SHORT * bandL;
            lowDigitsL = (digitsL & lowBits);
            bandL = SHORT2FLOAT * (float)((digitsL & highBits) ^ ((lowDigitsL ^ ll) & 0x1FFF));

            lowL = lowL + f * bandL;
            highL = scale * (notch)-lowL - fb * bandL;
            bandL = f * highL + bandL;

            _ly1L = coef1 * (_ly1L + notch) - _lx1L; // allpass
            _lx1L = notch;

            *sp++ = clamp(_ly1L * mixerGain_, -ratioTimbres, ratioTimbres);

            // RIGHT
            //*sp = _ly1R + ((-_ly1R + *sp) * r);

            lowR = lowR + f * bandR;
            highR = scale * (*sp) - lowR - fb * bandR;
            bandR = f * highR + bandR;
            notch = lowR + highR;

            digitsR = FLOAT2SHORT * bandR;
            lowDigitsR = (digitsR & lowBits);
            bandR = SHORT2FLOAT * (float)((digitsR & highBits) ^ ((lowDigitsR ^ ll) & 0x1FFF));

            lowR = lowR + f * bandR;
            highR = scale * (notch)-lowR - fb * bandR;
            bandR = f * highR + bandR;

            _ly1R = coef1 * (_ly1R + notch) - _lx1R; // allpass
            _lx1R = notch;

            *sp++ = clamp(_ly1R * mixerGain_, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
    } break;
    case FILTER_TEXTURE2: {
        float fxParamTmp = (shared.effect1.param1 + matrixFilterFrequency);
        fxParamTmp *= fxParamTmp;

        const uint_fast8_t random = (*(uint_fast8_t*)noise) & 0xff;
        if (random > 252) {
            fxParam1_ += ((random & 1) * 0.007874015748031f);
        }

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;
        float notch;

        const float f = (fxParam1_ * fxParam1_ * fxParam1_) * 0.5f;
        const float fb = filterParam2;
        const float scale = sqrt3(fb);

        const int highBits = 0xFFFFFFAF;
        const int lowBits = ~(highBits);

        const int ll = fxParam1_ * lowBits;
        int digitsL, digitsR;
        int lowDigitsL, lowDigitsR;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        float _ly2L = v4L_, _ly2R = v4R_;
        float _lx2L = v5L_, _lx2R = v5R_;
        const float f1 = clamp(0.57f - fxParam1_ * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);
        float coef2 = (1.0f - f1) / (1.0f + f1);

        const float r = 0.9940f;

        for (int k = BLOCK_SIZE; k--;) {
            // LEFT
            *sp = _ly2L + ((-_ly2L + *sp) * r);

            lowL = lowL + f * bandL;
            highL = scale * (*sp) - lowL - fb * bandL;
            bandL = f * highL + bandL;
            notch = lowL + highL;

            digitsL = FLOAT2SHORT * (bandL);
            lowDigitsL = (digitsL & lowBits);
            bandL = SHORT2FLOAT * (float)((digitsL & highBits) ^ ((lowDigitsL * ll) & 0x1FFF));

            _ly1L = coef1 * (_ly1L + notch) - _lx1L; // allpass
            _lx1L = notch;

            /*lowL = lowL + f * bandL;
             highL = scale * (_ly1L) - lowL - fb * bandL;
             bandL = f * highL + bandL;*/

            _ly2L = coef2 * (_ly2L + _ly1L) - _lx2L; // allpass 2
            _lx2L = _ly1L;

            *sp++ = clamp(_ly2L * mixerGain_, -ratioTimbres, ratioTimbres);

            // RIGHT
            *sp = _ly2R + ((-_ly2R + *sp) * r);

            lowR = lowR + f * bandR;
            highR = scale * (*sp) - lowR - fb * bandR;
            bandR = f * highR + bandR;
            notch = lowR + highR;

            digitsR = FLOAT2SHORT * (bandR);
            lowDigitsR = (digitsR & lowBits);
            bandR = SHORT2FLOAT * (float)((digitsR & highBits) ^ ((lowDigitsR * ll) & 0x1FFF));

            _ly1R = coef1 * (_ly1R + notch) - _lx1R; // allpass
            _lx1R = notch;

            /*lowR = lowR + f * bandR;
             highR = scale * (notch) - lowR - fb * bandR;
             bandR = f * highR + bandR;*/

            _ly2R = coef2 * (_ly2R + _ly1R) - _lx2R; // allpass 2
            _lx2R = _ly1R;

            *sp++ = clamp(_ly2R * mixerGain_, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
        v4L_ = _ly2L;
        v4R_ = _ly2R;
        v5L_ = _lx2L;
        v5R_ = _lx2R;
    } break;
    case FILTER_LPXOR: {
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParamTmp *= fxParamTmp;

        const uint_fast8_t random = (*(uint_fast8_t*)noise) & 0xff;
        if (random > 252) {
            fxParam1_ += ((random & 1) * 0.007874015748031f);
        }

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float a = 1.f - fxParam1_;
        const float a2f = a * SHORT2FLOAT;
        const float b = 1.f - a;

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv0R = v0R_;
        float digitized;

        float drive = (filterParam2 * filterParam2);
        float gain = (1 + 2 * drive);

        int digitsAL, digitsBL, digitsAR, digitsBR;
        digitsAL = digitsBL = digitsAR = digitsBR = 0;

        const short bitmask = 0xfac;

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        float prevX4L = v4L_, prevX4R = v4R_;
        float prevFL = foldADAA1_F(prevX4L), prevFR = foldADAA1_F(prevX4R);
        const float f1 = clamp(0.27f + fxParam1_ * 0.2f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        const float r = 0.9940f;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            *sp = _ly1L + ((-_ly1L + *sp) * r);

            digitsAL = FLOAT2SHORT * foldADAA(localv0L * gain, prevX4L, prevFL);
            digitsBL = FLOAT2SHORT * (*sp);
            digitized = (float)(digitsAL ^ (digitsBL & bitmask));
            localv0L = (*sp * b) + (digitized * a2f);

            _ly1L = coef1 * (_ly1L + localv0L) - _lx1L; // allpass
            _lx1L = localv0L;

            *sp++ = clamp(_ly1L * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = _ly1R + ((-_ly1R + *sp) * r);

            digitsAR = FLOAT2SHORT * foldADAA(localv0R * gain, prevX4R, prevFR);
            digitsBR = FLOAT2SHORT * (*sp);
            digitized = (float)(digitsAR ^ (digitsBR & bitmask));
            localv0R = (*sp * b) + (digitized * a2f);

            _ly1R = coef1 * (_ly1R + localv0R) - _lx1R; // allpass
            _lx1R = localv0R;

            *sp++ = clamp(_ly1R * mixerGain_, -ratioTimbres, ratioTimbres);
        }
        v0L_ = localv0L;
        v0R_ = localv0R;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
        v4L_ = prevX4L;
        v4R_ = prevX4R;
    } break;
    case FILTER_LPXOR2: {
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParamTmp *= fxParamTmp;

        const uint8_t random = (*(uint8_t*)noise) & 0xff;
        if (random > 252) {
            fxParam1_ += ((random & 1) * 0.007874015748031f);
        }

        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float a = 1.f - fxParam1_;
        const float a2f = a * SHORT2FLOAT;
        const float b = 1.f - a;

        float* sp = this->sampleBlock_;
        float localv0L = v0L_;
        float localv0R = v0R_;
        float digitized;

        float drive = (filterParam2 * filterParam2);
        float gain = (1 + 8 * (drive)) * 0.25f;

        int digitsAL, digitsBL, digitsAR, digitsBR;
        digitsAL = digitsBL = digitsAR = digitsBR = 0;

        const short bitmask = 0xfba - (int)(fxParam1_ * 7);

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;
        float prevX4L = v4L_, prevX4R = v4R_;
        float prevFL = foldADAA1_F(prevX4L), prevFR = foldADAA1_F(prevX4R);
        const float f1 = clamp(0.27f + fxParam1_ * 0.2f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        const float r = 0.9940f;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            *sp = _ly1L + ((-_ly1L + *sp) * r);

            digitsAL = FLOAT2SHORT * localv0L;
            digitsBL = FLOAT2SHORT * foldADAA(*sp * gain, prevX4L, prevFL);
            digitized = ((digitsAL | (digitsBL & bitmask)));
            localv0L = (*sp * b) + (digitized * a2f);

            _ly1L = coef1 * (_ly1L + localv0L) - _lx1L; // allpass
            _lx1L = localv0L;

            *sp++ = clamp(_ly1L * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = _ly1R + ((-_ly1R + *sp) * r);

            digitsAR = FLOAT2SHORT * localv0R;
            digitsBR = FLOAT2SHORT * foldADAA(*sp * gain, prevX4R, prevFR);
            digitized = ((digitsAR | (digitsBR & bitmask)));
            localv0R = (*sp * b) + (digitized * a2f);

            _ly1R = coef1 * (_ly1R + localv0R) - _lx1R; // allpass
            _lx1R = localv0R;

            *sp++ = clamp(_ly1R * mixerGain_, -ratioTimbres, ratioTimbres);
        }
        v0L_ = localv0L;
        v0R_ = localv0R;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;
        v4L_ = prevX4L;
        v4R_ = prevX4R;
    } break;
    case FILTER_LPSIN: {
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = filterParam2 * filterParam2 * filterParam2;
        const float fb = 0.45f;
        const float scale = sqrt3(fb);
        const int pos = (int)(fxParam1_ * 2060);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;

        const float svfGain = (1 + SVFGAINOFFSET + filterParam2 * filterParam2 * 0.75f) * mixerGain_;
        float drive = fxParam1_ * fxParam1_ * 0.25f;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            *sp = lowL + ((-lowL + *sp) * 0.9840f);

            lowL = lowL + f * bandL;
            highL = scale * satSin(*sp, drive, pos) - lowL - fb * bandL;
            bandL = f * highL + bandL;

            *sp++ = clamp(lowL * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = lowR + ((-lowR + *sp) * 0.9840f);

            lowR = lowR + f * bandR;
            highR = scale * satSin(*sp, drive, pos) - lowR - fb * bandR;
            bandR = f * highR + bandR;

            *sp++ = clamp(lowR * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;
    } break;
    case FILTER_HPSIN: {
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = filterParam2 * filterParam2 * filterParam2;
        const float fb = 0.94f;
        const float scale = sqrt3(fb);
        const int pos = (int)(fxParam1_ * 2060);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;

        const float svfGain = (1 + SVFGAINOFFSET + filterParam2 * filterParam2 * 0.75f) * mixerGain_;
        float drive = fxParam1_ * fxParam1_ * 0.25f;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            *sp = highL + ((-highL + *sp) * 0.9840f);

            lowL = lowL + f * bandL;
            highL = scale * satSin(*sp, drive, pos) - lowL - fb * bandL;
            bandL = f * highL + bandL;

            *sp++ = clamp(highL * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = highR + ((-highR + *sp) * 0.9840f);

            lowR = lowR + f * bandR;
            highR = scale * satSin(*sp, drive, pos) - lowR - fb * bandR;
            bandR = f * highR + bandR;

            *sp++ = clamp(highR * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;
    } break;
    case FILTER_QUADNOTCH: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = (shared.effect1.param1 + matrixFilterFrequency);
        fxParamTmp *= fxParamTmp;

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float bipolarf = (fxParam1_ - 0.5f);
        const float folded = fold(sigmoid(bipolarf * 13 * filterParam2)) * 4; // - < folded < 1

        fxParam1_ = ((fxParamTmp + 9.0f * fxParam1_) * .1f);

        float OffsetTmp = fabsf(shared.effect1.param2);
        filterParam2 = ((OffsetTmp + 9.0f * filterParam2) * .1f);

        const float offset = filterParam2 * filterParam2 * 0.17f;
        const float lrDelta = 0.005f * folded;
        const float range = 0.47f;

        const float windowMin = 0.005f, windowMax = 0.99f;

        const float f1L = clamp(fold((fxParam1_ - offset - lrDelta) * range) * 2, windowMin, windowMax);
        const float f2L = clamp(fold((fxParam1_ + offset + lrDelta) * range) * 2, windowMin, windowMax);
        const float f3L = clamp(fold((fxParam1_ - (offset * 2) - lrDelta) * range) * 2, windowMin, 1);
        const float f4L = clamp(fold((fxParam1_ + (offset * 2) + lrDelta) * range) * 2, windowMin, 1);
        const float f1R = clamp(fold((f1L - lrDelta) * range) * 2, windowMin, windowMax);
        const float f2R = clamp(fold((f2L + lrDelta) * range) * 2, windowMin, windowMax);
        const float f3R = clamp(fold((f3L - lrDelta) * range) * 2, windowMin, windowMax);
        const float f4R = clamp(fold((f3L + lrDelta) * range) * 2, windowMin, windowMax);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, highL = 0, bandL = v1L_;
        float lowR = v0R_, highR = 0, bandR = v1R_;
        float lowL2 = v2L_, highL2 = 0, bandL2 = v3L_;
        float lowR2 = v2R_, highR2 = 0, bandR2 = v3R_;
        float lowL3 = v4L_, highL3 = 0, bandL3 = v5L_;
        float lowR3 = v4R_, highR3 = 0, bandR3 = v5R_;
        float lowL4 = v6L_, highL4 = 0, bandL4 = v7L_;
        float lowR4 = v6R_, highR4 = 0, bandR4 = v7R_;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            *sp = (highL4 + lowL4) + ((-(highL4 + lowL4) + *sp) * 0.9840f);

            lowL = lowL + f1L * bandL;
            highL = (*sp) - lowL - bandL;
            bandL = f1L * highL + bandL;

            lowL2 = lowL2 + f2L * bandL2;
            highL2 = (highL + lowL) - lowL2 - bandL2;
            bandL2 = f2L * (highL2) + bandL2;

            lowL3 = lowL3 + f3L * bandL3;
            highL3 = (highL2 + lowL2) - lowL3 - bandL3;
            bandL3 = f3L * highL3 + bandL3;

            lowL4 = lowL4 + f4L * bandL4;
            highL4 = (highL3 + lowL3) - lowL4 - bandL4;
            bandL4 = f4L * highL4 + bandL4;

            *sp++ = clamp((highL4 + lowL4) * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = (highR4 + lowR4) + ((-(highR4 + lowR4) + *sp) * 0.9840f);

            lowR = lowR + f1R * bandR;
            highR = (*sp) - lowR - bandR;
            bandR = f1R * highR + bandR;

            lowR2 = lowR2 + f2R * bandR2;
            highR2 = (highR + lowR) - lowR2 - bandR2;
            bandR2 = f2R * (highR2) + bandR2;

            lowR3 = lowR3 + f3R * bandR3;
            highR3 = (highR2 + lowR2) - lowR3 - bandR3;
            bandR3 = f3R * highR3 + bandR3;

            lowR4 = lowR4 + f4R * bandR4;
            highR4 = (highR3 + lowR3) - lowR4 - bandR4;
            bandR4 = f4R * highR4 + bandR4;

            *sp++ = clamp((highR4 + lowR4) * mixerGain_, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = lowL2;
        v3L_ = bandL2;
        v2R_ = lowR2;
        v3R_ = bandR2;

        v4L_ = lowL3;
        v5L_ = bandL3;
        v4R_ = lowR3;
        v5R_ = bandR3;

        v6L_ = lowL4;
        v7L_ = bandL4;
        v6R_ = lowR4;
        v7R_ = bandR4;
    } break;
    case FILTER_AP4: {
        // http://denniscronin.net/dsp/vst.html
        float fxParamTmp = (shared.effect1.param1 + matrixFilterFrequency);
        fxParamTmp *= fxParamTmp;
        fxParam1_ = ((fxParamTmp + 9.0f * fxParam1_) * .1f);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        float OffsetTmp = fabsf(filterParam2);
        fxParam2_ = ((OffsetTmp + 9.0f * fxParam2_) * .1f);

        const float bipolarf = (fxParam1_ - 0.5f);

        const float folded = fold(sigmoid(bipolarf * 7 * fxParam2_)) * 4; // -1 < folded < 1

        const float offset = fxParam2_ * fxParam2_ * 0.17f;
        const float lrDelta = 0.005f * folded;

        const float range = 0.47f;

        const float f1L = clamp(fold((fxParam1_ - offset - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f2L = clamp(fold((fxParam1_ + offset + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f3L = clamp(fold((fxParam1_ - (offset * 2) - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f4L = clamp(fold((fxParam1_ + (offset * 2) + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        float coef1L = (1.0f - f1L) / (1.0f + f1L);
        float coef2L = (1.0f - f2L) / (1.0f + f2L);
        float coef3L = (1.0f - f3L) / (1.0f + f3L);
        float coef4L = (1.0f - f4L) / (1.0f + f4L);
        const float f1R = clamp(fold((f1L - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f2R = clamp(fold((f2L + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f3R = clamp(fold((f3L - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f4R = clamp(fold((f4L + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        float coef1R = (1.0f - f1R) / (1.0f + f1R);
        float coef2R = (1.0f - f2R) / (1.0f + f2R);
        float coef3R = (1.0f - f3R) / (1.0f + f3R);
        float coef4R = (1.0f - f4R) / (1.0f + f4R);

        float* sp = this->sampleBlock_;

        float inmix;
        float _ly1L = v0L_, _ly1R = v0R_;
        float _ly2L = v1L_, _ly2R = v1R_;
        float _ly3L = v2L_, _ly3R = v2R_;
        float _ly4L = v3L_, _ly4R = v3R_;
        float _lx1L = v4L_, _lx1R = v4R_;
        float _lx2L = v5L_, _lx2R = v5R_;
        float _lx3L = v6L_, _lx3R = v6R_;
        float _lx4L = v7L_, _lx4R = v7R_;
        float _feedback = 0.08f;
        float _crossFeedback = 0.16f;
        float finalGain = mixerGain_ * 0.5f;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            inmix = (*sp) + _ly3L * _feedback - _ly2R * _crossFeedback;

            _ly1L = coef1L * (_ly1L + inmix) - _lx1L; // do 1st filter
            _lx1L = inmix;
            _ly2L = coef2L * (_ly2L + _ly1L) - _lx2L; // do 2nd filter
            _lx2L = _ly1L;
            _ly3L = coef3L * (_ly3L + _ly2L) - _lx3L; // do 3rd filter
            _lx3L = _ly2L;
            _ly4L = coef4L * (_ly4L + _ly3L) - _lx4L; // do 4th filter
            _lx4L = _ly3L;

            *sp++ = clamp((*sp + _ly4L) * finalGain, -ratioTimbres, ratioTimbres);

            // Right voice
            inmix = (*sp) + _ly3R * _feedback - _ly2L * _crossFeedback;

            _ly1R = coef1R * (_ly1R + inmix) - _lx1R; // do 1st filter
            _lx1R = inmix;
            _ly2R = coef2R * (_ly2R + _ly1R) - _lx2R; // do 2nd filter
            _lx2R = _ly1R;
            _ly3R = coef3R * (_ly3R + _ly2R) - _lx3R; // do 3rd filter
            _lx3R = _ly2R;
            _ly4R = coef4R * (_ly4R + _ly3R) - _lx4R; // do 4th filter
            _lx4R = _ly3R;

            *sp++ = clamp((*sp + _ly4R) * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = _ly1L;
        v0R_ = _ly1R;
        v1L_ = _ly2L;
        v1R_ = _ly2R;
        v2L_ = _ly3L;
        v2R_ = _ly3R;
        v3L_ = _ly4L;
        v3R_ = _ly4R;
        v4L_ = _lx1L;
        v4R_ = _lx1R;
        v5L_ = _lx2L;
        v5R_ = _lx2R;
        v6L_ = _lx3L;
        v6R_ = _lx3R;
        v7L_ = _lx4L;
        v7R_ = _lx4R;
    } break;
    case FILTER_AP4B: {
        float fxParamTmp = (shared.effect1.param1 + matrixFilterFrequency);
        fxParamTmp *= fxParamTmp;
        fxParam1_ = ((fxParamTmp + 9.0f * fxParam1_) * .1f);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        float OffsetTmp = fabsf(filterParam2);
        fxParam2_ = ((OffsetTmp + 9.0f * fxParam2_) * .1f);

        const float bipolarf = (fxParam1_ - 0.5f);
        const float folded = fold(sigmoid(bipolarf * 19 * fxParam2_)) * 4; // -1 < folded < 1

        const float offset = fxParam2_ * fxParam2_ * 0.17f;
        const float lrDelta = 0.005f * folded;

        const float range = 0.47f;

        const float f1L = clamp(fold((fxParam1_ - offset - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f2L = clamp(fold((fxParam1_ + offset + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f3L = clamp(fold((fxParam1_ - (offset * 2) - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f4L = clamp(fold((fxParam1_ + (offset * 2) + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        float coef1L = (1.0f - f1L) / (1.0f + f1L);
        float coef2L = (1.0f - f2L) / (1.0f + f2L);
        float coef3L = (1.0f - f3L) / (1.0f + f3L);
        float coef4L = (1.0f - f4L) / (1.0f + f4L);
        const float f1R = clamp(fold((fxParam1_ - offset + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f2R = clamp(fold((fxParam1_ + offset - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f3R = clamp(fold((fxParam1_ - (offset * 2) + lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        const float f4R = clamp(fold((fxParam1_ + (offset * 2) - lrDelta) * range) * 2, filterWindowMin, filterWindowMax);
        float coef1R = (1.0f - f1R) / (1.0f + f1R);
        float coef2R = (1.0f - f2R) / (1.0f + f2R);
        float coef3R = (1.0f - f3R) / (1.0f + f3R);
        float coef4R = (1.0f - f4R) / (1.0f + f4R);

        float* sp = this->sampleBlock_;

        float _ly1L = v0L_, _ly1R = v0R_;
        float _ly2L = v1L_, _ly2R = v1R_;
        float _ly3L = v2L_, _ly3R = v2R_;
        float _ly4L = v3L_, _ly4R = v3R_;
        float _lx1L = v4L_, _lx1R = v4R_;
        float _lx2L = v5L_, _lx2R = v5R_;
        float _lx3L = v6L_, _lx3R = v6R_;
        float _lx4L = v7L_, _lx4R = v7R_;

        float _feedback = 0.68f;
        float _crossFeedback = 0.17f;
        float inmix;
        float finalGain = mixerGain_ * 0.5f;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            inmix = (*sp) - _feedback * _ly3L + _crossFeedback * _ly3R;

            _ly1L = coef1L * (_ly1L + inmix) - _lx1L; // do 1st filter
            _lx1L = inmix;
            _ly2L = coef2L * (_ly2L + _ly1L) - _lx2L; // do 2nd filter
            _lx2L = _ly1L;
            _ly3L = coef3L * (_ly3L + _ly2L) - _lx3L; // do 3rd filter
            _lx3L = _ly2L;
            _ly4L = coef4L * (_ly4L + _ly3L) - _lx4L; // do 4nth filter
            _lx4L = _ly3L;

            *sp++ = clamp((*sp + _ly4L) * finalGain, -ratioTimbres, ratioTimbres);

            // Right voice
            inmix = (*sp) - _feedback * _ly3R + _crossFeedback * _ly3L;

            _ly1R = coef1R * (_ly1R + inmix) - _lx1R; // do 1st filter
            _lx1R = inmix;
            _ly2R = coef2R * (_ly2R + _ly1R) - _lx2R; // do 2nd filter
            _lx2R = _ly1R;
            _ly3R = coef3R * (_ly3R + _ly2R) - _lx3R; // do 3rd filter
            _lx3R = _ly2R;
            _ly4R = coef4R * (_ly4R + _ly3R) - _lx4R; // do 4nth filter
            _lx4R = _ly3R;

            *sp++ = clamp((*sp + _ly4R) * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = _ly1L;
        v0R_ = _ly1R;
        v1L_ = _ly2L;
        v1R_ = _ly2R;
        v2L_ = _ly3L;
        v2R_ = _ly3R;
        v3L_ = _ly4L;
        v3R_ = _ly4R;
        v4L_ = _lx1L;
        v4R_ = _lx1R;
        v5L_ = _lx2L;
        v5R_ = _lx2R;
        v6L_ = _lx3L;
        v6R_ = _lx3R;
        v7L_ = _lx4L;
        v7R_ = _lx4R;
    } break;
    case FILTER_AP4D: {
        float fxParamTmp = (shared.effect1.param1 + matrixFilterFrequency);
        const uint_fast8_t random = (*(uint_fast8_t*)noise) & 0xff;
        float randomF = (float)random * 0.00390625f;
        if (random < 76) {
            fxParam1_ += (randomF * 0.002f) - 0.001f;
        }
        fxParamTmp *= fxParamTmp;
        fxParam1_ = ((fxParamTmp + 9.0f * fxParam1_) * .1f);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);
        float OffsetTmp = fabsf(filterParam2);
        fxParam2_ = ((OffsetTmp + 9.0f * fxParam2_) * .1f);

        const float offset = fxParam2_ * fxParam2_ * 0.17f;

        const float range1 = 0.33f;
        const float range2 = 0.14f;
        const float range3 = 0.05f;

        const float lowlimit = 0.027f;

        const float f1 = clamp(((lowlimit + fxParam1_ + offset) * range1), filterWindowMin, filterWindowMax);
        const float f2 = clamp(((lowlimit + fxParam1_ - offset) * range2), filterWindowMin, filterWindowMax);
        const float f3 = clamp(((lowlimit + fxParam1_ + offset) * range3), filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);
        float coef2 = (1.0f - f2) / (1.0f + f2);
        float coef3 = (1.0f - f3) / (1.0f + f3);

        float* sp = this->sampleBlock_;

        float _ly1L = v0L_, _ly1R = v0R_;
        float _ly2L = v1L_, _ly2R = v1R_;
        float _ly3L = v2L_, _ly3R = v2R_;
        float _ly4L = v3L_, _ly4R = v3R_;
        float _lx1L = v4L_, _lx1R = v4R_;
        float _lx2L = v5L_, _lx2R = v5R_;
        float _lx3L = v6L_, _lx3R = v6R_;
        float _lx4L = v7L_, _lx4R = v7R_;

        float _feedback = 0.15f + (randomF * 0.01f) - 0.005f;
        float inmix;
        float finalGain = mixerGain_ * 0.5f;

        const float a = 0.85f;
        const float b = 1.f - a;

        coef3 = clamp(coef3 + (*sp * 0.0075f), 0, 1); // nested f xmod

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            inmix = (*sp) + _feedback * _ly4L;
            _ly1L = coef1 * (_ly1L + inmix) - _lx1L; // do 1st filter
            _lx1L = inmix;
            _ly2L = coef2 * (_ly3L + _ly1L) - _lx2L; // do 2nd filter
            _lx2L = _ly1L;
            _ly3L = _lx4L * (_ly3L + _ly2L) - _lx3L; // nested filter
            _lx3L = _ly2L;

            _lx4L = (_lx4L * 31 + coef3) * 0.03125f; // xmod damp

            _ly4L = (_ly3L * b) + (_ly4L * a); // lowpass

            *sp++ = clamp((_ly1L + _ly2L - *sp) * finalGain, -ratioTimbres, ratioTimbres);

            // Right voice
            inmix = (*sp) + _feedback * _ly4R;
            _ly1R = coef1 * (_ly1R + inmix) - _lx1R; // do 1st filter
            _lx1R = inmix;
            _ly2R = coef2 * (_ly3R + _ly1R) - _lx2R; // do 2nd filter
            _lx2R = _ly1R;
            _ly3R = _lx4R * (_ly3R + _ly2R) - _lx3R; // nested filter
            _lx3R = _ly2R;

            _lx4R = (_lx4R * 31 + coef3) * 0.03125f; // xmod damp

            _ly4R = (_ly3R * b) + (_ly4R * a); // lowpass

            *sp++ = clamp((_ly1R + _ly2R - *sp) * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = _ly1L;
        v0R_ = _ly1R;
        v1L_ = _ly2L;
        v1R_ = _ly2R;
        v2L_ = _ly3L;
        v2R_ = _ly3R;
        v3L_ = _ly4L;
        v3R_ = _ly4R;
        v4L_ = _lx1L;
        v4R_ = _lx1R;
        v5L_ = _lx2L;
        v5R_ = _lx2R;
        v6L_ = _lx3L;
        v6R_ = _lx3R;
        v7L_ = _lx4L;
        v7R_ = _lx4R;
    } break;
    case FILTER_ORYX: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParamTmp = (fold((fxParamTmp - 0.5f) * 0.25f) + 0.25f) * 2;
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);
        fxParam2_ = ((filterParam2 + 19.0f * fxParam2_) * .05f);
        const float rootf = 0.011f + ((fxParam2_ * fxParam2_) - 0.005f) * 0.08f;
        const float frange = 0.14f;
        const float bipolarf = (fxParam1_ - 0.5f);

        float f1 = rootf + sigmoidPos((fxParam1_ > 0.5f) ? (1 - fxParam1_) : fxParam1_) * frange;
        /*//f1 = fold(sigmoid(((f1 - 0.5f) * (2/frange)) * 7 * fxParam2)) * 4; // -1 < folded < 1*/
        const float fb = 0.08f + fxParam1_ * 0.01f + noise[0] * 0.002f;
        const float scale = sqrt3(fb);

        const float spread = 1 + (fxParam2_ * 0.13f);

        const float fold2 = fold(0.125f + sigmoid(bipolarf * 21 * fxParam2_) * frange) + 0.25f;
        const float f2 = rootf + frange * 1.15f + sigmoidPos(1 - fxParam1_ + fold2 * 0.25f) * spread * frange * 1.25f;
        const float fb2 = 0.21f - fold2 * 0.08f - noise[1] * 0.0015f;
        const float scale2 = sqrt3(fb2);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, bandL = v1L_;
        float lowR = v0R_, bandR = v1R_;

        float lowL2 = v2L_, bandL2 = v3L_;
        float lowR2 = v2R_, bandR2 = v3R_;

        float lowL3 = v4L_, bandL3 = v5L_;
        float lowR3 = v4R_, bandR3 = v5R_;

        const float svfGain = (1 + SVFGAINOFFSET) * mixerGain_;

        float out;
        const float r = 0.985f;
        float nz;
        float drift = v6L_;
        float nexDrift = noise[7] * 0.02f;
        float deltaD = (nexDrift - drift) * 0.000625f;

        const float sat = 1.25f;

        for (int k = BLOCK_SIZE; k--;) {
            nz = noise[k] * 0.005f;

            // ----------- Left voice
            *sp = ((lowL + ((-lowL + *sp) * (r + nz))));
            // bandpass 1
            lowL += f1 * bandL;
            bandL += f1 * (scale * sat66(*sp * sat) - lowL - (fb + nz - drift) * bandL);

            // bandpass 2
            lowL2 += f2 * bandL2;
            bandL2 += f2 * (scale2 * *sp - lowL2 - fb2 * bandL2);

            out = bandL + bandL2;

            *sp++ = clamp(out * svfGain, -ratioTimbres, ratioTimbres);

            // ----------- Right voice
            *sp = ((lowR + ((-lowR + *sp) * (r - nz))));
            // bandpass 1
            lowR += f1 * bandR;
            bandR += f1 * (scale * sat66(*sp * sat) - lowR - (fb - nz + drift) * bandR);

            // bandpass 2
            lowR2 += f2 * bandR2;
            bandR2 += f2 * (scale2 * *sp - lowR2 - fb2 * bandR2);

            out = bandR + bandR2;

            *sp++ = clamp(out * svfGain, -ratioTimbres, ratioTimbres);

            drift += deltaD;
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = lowL2;
        v3L_ = bandL2;
        v2R_ = lowR2;
        v3R_ = bandR2;

        v4L_ = lowL3;
        v5L_ = bandL3;
        v4R_ = lowR3;
        v5R_ = bandR3;

        v6L_ = nexDrift;
    } break;
    case FILTER_ORYX2: {
        // https://www.musicdsp.org/en/latest/Filters/23-state-variable.html
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParamTmp = (fold((fxParamTmp - 0.5f) * 0.25f) + 0.25f) * 2;
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);
        fxParam2_ = ((filterParam2 + 19.0f * fxParam2_) * .05f);
        const float rootf = 0.031f + (fxParam2_) * 0.0465f;
        const float frange = 0.22f;
        const float bipolarf = sigmoid(fxParam1_ - 0.5f);

        const float f1 = rootf + sigmoidPos((fxParam1_ > 0.55f) ? (0.7f - (fxParam1_ * 0.7f)) : (fxParam1_ * fxParam1_)) * frange;
        const float fb = 0.074f + fxParam1_ * 0.01f + noise[0] * 0.002f;
        const float scale = sqrt3(fb);

        const float spread = 1 + (fxParam2_ * 0.13f);

        const float fold2 = fold(0.125f + sigmoid(bipolarf * 21 * fxParam2_) * frange) + 0.25f;
        const float f2 = rootf + frange * 1.1f - (sigmoidPos(fxParam1_ + fold2 * 0.25f) - 0.5f) * spread * frange * 0.5f;
        const float fb2 = 0.11f - f1 * fold2 * 0.4f - noise[1] * 0.0015f;
        const float scale2 = sqrt3(fb2);

        const float fold3 = fold(sigmoid(bipolarf * 17 * fxParam2_) * frange);
        const float f3 = rootf + frange * 1.35f - tanh3(((1 - fxParam1_) * 2 - fold3 * 0.2f)) * spread * frange * 0.25f;
        const float fb3 = 0.13f - f1 * fold3 * 0.5f - noise[2] * 0.001f;
        const float scale3 = sqrt3(fb3);

        float* sp = this->sampleBlock_;
        float lowL = v0L_, bandL = v1L_;
        float lowR = v0R_, bandR = v1R_;

        float lowL2 = v2L_, bandL2 = v3L_;
        float lowR2 = v2R_, bandR2 = v3R_;

        float lowL3 = v4L_, bandL3 = v5L_;
        float lowR3 = v4R_, bandR3 = v5R_;

        const float svfGain = (1 + SVFGAINOFFSET) * mixerGain_;

        float out;
        const float r = 0.985f;
        float nz;
        float drift = v6L_;
        float nexDrift = noise[7] * 0.02f;
        float deltaD = (nexDrift - drift) * 0.000625f;

        for (int k = BLOCK_SIZE; k--;) {
            nz = noise[k] * 0.016f;

            // ----------- Left voice
            *sp = lowL + ((-lowL + *sp) * (r + nz));
            // bandpass 1
            lowL += f1 * bandL;
            bandL += f1 * (scale * *sp - lowL - (fb + nz - drift) * bandL);

            // bandpass 2
            lowL2 += (f2 + drift) * bandL2;
            bandL2 += f2 * (scale2 * *sp - lowL2 - fb2 * bandL2);

            // bandpass 3
            lowL3 += f3 * bandL3;
            bandL3 += f3 * (scale3 * *sp - lowL3 - fb3 * bandL3);

            out = (bandL) + (bandL2 + bandL3);

            *sp++ = clamp(out * svfGain, -ratioTimbres, ratioTimbres);

            // ----------- Right voice
            *sp = lowR + ((-lowR + *sp) * (r - nz));
            // bandpass 1
            lowR += f1 * bandR;
            bandR += f1 * (scale * *sp - lowR - (fb - nz + drift) * bandR);

            // bandpass 2
            lowR2 += (f2 + drift) * bandR2;
            bandR2 += f2 * (scale2 * *sp - lowR2 - fb2 * bandR2);

            // bandpass 3
            lowR3 += f3 * bandR3;
            bandR3 += f3 * (scale3 * *sp - lowR3 - fb3 * bandR3);

            out = (bandR) + (bandR2 + bandR3);

            *sp++ = clamp(out * svfGain, -ratioTimbres, ratioTimbres);

            drift += deltaD;
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

        v2L_ = lowL2;
        v3L_ = bandL2;
        v2R_ = lowR2;
        v3R_ = bandR2;

        v4L_ = lowL3;
        v5L_ = bandL3;
        v4R_ = lowR3;
        v5R_ = bandR3;

        v6L_ = nexDrift;
    } break;
    case FILTER_ORYX3: {
        // Throat / Growl Binaural Talking Formant Filter:
        // 3 formants with smoothed laryngeal F1->F2/F3 cross-modulation, anti-formant phaser notches & binaural stereo spread
        const float prevParam1 = clamp(fxParam1_, 0.0f, 1.0f);
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParamTmp *= fxParamTmp;
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * 0.1f, 0.0f, 1.0f);

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);
        fxParam2_ = (filterParam2 + 19.0f * fxParam2_) * 0.05f;

        // Distinct throat-to-nasal formant trajectory (wider and more aggressive than ORYX1/2)
        const float rootf = 0.014f + (fxParam2_ * fxParam2_) * 0.065f;
        const float frange = 0.165f;
        const float bipolarf = (fxParam1_ - 0.5f);
        const float prevBipolarf = (prevParam1 - 0.5f);

        // F1: Pharynx / throat resonance
        const float f10 = clamp(rootf + sigmoidPos(0.5f - fabsf(prevBipolarf)) * frange * 1.15f, 0.012f, 0.32f);
        const float f1Target = clamp(rootf + sigmoidPos(0.5f - fabsf(bipolarf)) * frange * 1.15f, 0.012f, 0.32f);
        float f1 = f10;
        const float df1 = (f1Target - f10) * 0.03125f;
        const float fb = 0.105f + fxParam1_ * 0.015f;
        const float scale = 0.62f * fb + 0.38f * sqrt3(fb);

        const float spread = 1.0f + (fxParam2_ * 0.22f);

        // F2: Tongue / oral cavity contrapuntal sweep
        const float fold2 = 0.40f + sigmoid(bipolarf * 3.8f * (0.25f + 0.75f * fxParam2_)) * frange;
        const float fold2_0 = 0.40f + sigmoid(prevBipolarf * 3.8f * (0.25f + 0.75f * fxParam2_)) * frange;
        const float f20 =
            clamp(rootf + frange * 0.95f + sigmoidPos(clamp(1.0f - prevParam1 + fold2_0 * 0.35f, 0.0f, 1.0f)) * spread * frange * 1.45f,
                  0.035f, 0.44f);
        const float f2Target =
            clamp(rootf + frange * 0.95f + sigmoidPos(clamp(1.0f - fxParam1_ + fold2 * 0.35f, 0.0f, 1.0f)) * spread * frange * 1.45f,
                  0.035f, 0.44f);
        float f2 = f20;
        const float df2 = (f2Target - f20) * 0.03125f;
        const float fb2 = clamp(0.125f - fold2 * 0.035f, 0.095f, 0.35f);
        const float scale2 = 0.62f * fb2 + 0.38f * sqrt3(fb2);

        // F3: High nasal / vocal-tract edge resonance
        const float fold3 = sigmoid(bipolarf * 3.0f * (0.25f + 0.75f * fxParam2_)) * frange;
        const float fold3_0 = sigmoid(prevBipolarf * 3.0f * (0.25f + 0.75f * fxParam2_)) * frange;
        const float f30 =
            clamp(rootf + frange * 2.10f + tanh3(1.0f - (prevParam1 * 2.0f - fold3_0 * 0.3f)) * spread * frange * 0.65f, 0.08f, 0.48f);
        const float f3Target =
            clamp(rootf + frange * 2.10f + tanh3(1.0f - (fxParam1_ * 2.0f - fold3 * 0.3f)) * spread * frange * 0.65f, 0.08f, 0.48f);
        float f3 = f30;
        const float df3 = (f3Target - f30) * 0.03125f;
        const float fb3 = clamp(0.135f - fold3 * 0.04f, 0.10f, 0.35f);
        const float scale3 = 0.62f * fb3 + 0.38f * sqrt3(fb3);

        // Laryngeal F1 -> F2/F3 cross-modulation depth (band-limited via xmodEnv to prevent Nyquist FM foldover)
        const float xmodDepth = 0.024f + 0.052f * fxParam2_;

        float* sp = this->sampleBlock_;
        float low = v0L_, band = v1L_;
        float low2 = v2L_, band2 = v3L_;
        float low3 = v4L_, band3 = v5L_;
        float drift = v6L_;
        float xmodEnv = v8L_;

        float _ly1L = v0R_, _lx1L = v1R_;
        float _ly1R = v2R_, _lx1R = v3R_;

        float _ly2L = v4R_, _lx2L = v5R_;
        float _ly2R = v6R_, _lx2R = v7R_;

        const float svfGain = (1.08f + SVFGAINOFFSET) * mixerGain_;

        float inL, inR, in, outCore;
        const float r = 0.990f;
        float nexDrift = noise[7] * 0.010f;
        float deltaD = (nexDrift - drift) * 0.03125f;

        // Asymmetric binaural allpass poles creating distinct L/R anti-formant notches
        const float apf1 = clamp(0.18f + fxParam1_ * 0.32f + fxParam2_ * 0.18f, filterWindowMin, filterWindowMax);
        const float coef1 = (1.0f - apf1) / (1.0f + apf1);
        const float apf2 = clamp(0.52f - fxParam1_ * 0.28f - fxParam2_ * 0.14f, filterWindowMin, filterWindowMax);
        const float coef2 = (1.0f - apf2) / (1.0f + apf2);

        const float sat = 1.45f + 0.45f * fxParam2_;
        const float sat2 = 1.55f + 0.45f * fxParam2_;
        const float dampK = 0.075f;
        float prevSatX = v7L_, prevTanhX = v8R_;
        float prevSatF = sat25ADAA1_F(prevSatX), prevTanhF = tanh4ADAA1_F(prevTanhX);

        for (int k = BLOCK_SIZE; k--;) {
            f1 += df1;
            f2 += df2;
            f3 += df3;
            float fb1 = clamp(fb - drift, 0.095f, 0.35f);

            inL = *sp;
            inR = *(sp + 1);
            in = (inL + inR) * 0.5f;

            in = low + ((-low + in) * r);
            // Formant 1 (Throat / Chest)
            low += f1 * band;
            band += f1 * (scale * in - low - fb1 * band);
            band -= dampK * band * (band * band) * fastInvSqrtVoice(1.0f + band * band);

            // Band-limited laryngeal cross-modulation from F1 into F2 & F3 (prevents ultrasonic FM foldover)
            xmodEnv += 0.12f * (band - xmodEnv);
            float f2m = clamp(f2 + xmodEnv * xmodDepth, 0.025f, 0.45f);
            float f3m = clamp(f3 - xmodEnv * (xmodDepth * 1.25f), 0.05f, 0.47f);

            // Formant 2 (Oral cavity, fed with subtle L/R difference)
            low2 += f2m * band2;
            band2 += f2m * (scale2 * (in + 0.20f * (inL - inR)) - low2 - fb2 * band2);
            band2 -= dampK * band2 * (band2 * band2) * fastInvSqrtVoice(1.0f + band2 * band2);

            // Formant 3 (Nasal / Sibilant edge)
            low3 += f3m * band3;
            band3 += f3m * (scale3 * (in - 0.20f * (inL - inR) + 0.18f * xmodEnv) - low3 - fb3 * band3);
            band3 -= dampK * band3 * (band3 * band3) * fastInvSqrtVoice(1.0f + band3 * band3);

            float throat = sat25ADAA(band * sat + low * 0.22f, prevSatX, prevSatF);
            float mouth = tanh4ADAA(band2 * sat2 + band3 * 0.85f, prevTanhX, prevTanhF);
            outCore = throat * 0.72f + mouth * 0.58f;

            // ----------- Left voice (binaural anti-formant notch + F2 emphasis)
            float inApL = outCore + 0.25f * band2;
            _ly1L = coef1 * (_ly1L + inApL) - _lx1L;
            _lx1L = inApL;
            _ly2L = coef1 * (_ly2L + _ly1L) - _lx2L;
            _lx2L = _ly1L;
            float outL = softLimitVoice(0.65f * _ly2L + 0.35f * (throat + band3 * 0.6f));

            *sp++ = clamp(outL * svfGain, -ratioTimbres, ratioTimbres);

            // ----------- Right voice (complementary binaural anti-formant notch + F3 emphasis)
            float inApR = outCore + 0.25f * band3;
            _ly1R = coef2 * (_ly1R + inApR) - _lx1R;
            _lx1R = inApR;
            _ly2R = coef2 * (_ly2R + _ly1R) - _lx2R;
            _lx2R = _ly1R;
            float outR = softLimitVoice(0.65f * _ly2R + 0.35f * (throat + band2 * 0.6f));

            *sp++ = clamp(outR * svfGain, -ratioTimbres, ratioTimbres);

            drift += deltaD;
        }

        v0L_ = low;
        v1L_ = band;
        v2L_ = low2;
        v3L_ = band2;
        v4L_ = low3;
        v5L_ = band3;
        v6L_ = drift;
        v7L_ = prevSatX;
        v8L_ = xmodEnv;

        v0R_ = _ly1L;
        v1R_ = _lx1L;
        v2R_ = _ly1R;
        v3R_ = _lx1R;

        v4R_ = _ly2L;
        v5R_ = _lx2L;
        v6R_ = _ly2R;
        v7R_ = _lx2R;
        v8R_ = prevTanhX;

        break;
    }
    case FILTER_18DB: {
        // https://www.musicdsp.org/en/latest/Filters/26-moog-vcf-variation-2.html
        const float prevParam1 = clamp(fxParam1_, 0.0f, 1.0f);
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParamTmp *= fxParamTmp;

        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * 0.1f, 0.0f, 1.0f);
        const float cut0 = clamp(sqrt3(prevParam1), filterWindowMin, 0.96f);
        const float cut1 = clamp(sqrt3(fxParam1_), filterWindowMin, 0.96f);
        const float dCut = (cut1 - cut0) * 0.03125f;
        float cut = cut0;
        const float cut2 = cut1 * cut1;
        // True 3-pole DC normalization: 1 / (1 + inAtn)^3 = 1 / 1.3^3 = 0.455166 * cut^3
        const float attn0 = 0.455166f * cut0 * cut0 * cut0;
        const float attn1 = 0.455166f * cut2 * cut1;
        const float dAttn = (attn1 - attn0) * 0.03125f;
        float attn = attn0;
        const float inAtn = 0.3f;

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);
        float OffsetTmp = fabsf(filterParam2);
        fxParam2_ = (OffsetTmp + 9.0f * fxParam2_) * 0.1f;
        float res = fxParam2_ * (1.15f + 0.95f * fxParam2_) + cut2 * 0.12f;
        float fb = res * (1.0f - 0.22f * cut2);

        float* sp = this->sampleBlock_;

        float buf1L = v0L_, buf2L = v1L_, buf3L = v2L_;
        float buf1R = v0R_, buf2R = v1R_, buf3R = v2R_;

        float buf1inL = v4L_, buf2inL = v5L_, buf3inL = v6L_;
        float buf1inR = v4R_, buf2inR = v5R_, buf3inR = v6R_;

        float _ly1L = v3L_, _ly1R = v3R_;
        float _lx1L = v7L_, _lx1R = v7R_;
        float prevXL = v8L_, prevXR = v8R_;
        float prevFL = tanh4ADAA1_F(prevXL), prevFR = tanh4ADAA1_F(prevXR);

        const float f1 = clamp(0.37f + cut2 * 0.047f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        float finalGain = mixerGain_ * (1.0f + fxParam2_ * 0.55f) * 1.15f;

        const float r = 0.987f;

        for (int k = BLOCK_SIZE; k--;) {
            cut += dCut;
            attn += dAttn;
            const float invCut = 1.0f - cut;

            // Left voice
            *sp = buf3L + ((-buf3L + *sp) * r);

            *sp = tanh4ADAA(*sp - buf3L * fb, prevXL, prevFL);
            *sp *= attn;
            buf1L = (*sp) + inAtn * buf1inL + invCut * buf1L; // Pole 1
            buf1inL = (*sp);
            buf2L = (buf1L) + inAtn * buf2inL + invCut * buf2L; // Pole 2
            buf2inL = buf1L;
            buf3L = (buf2L) + inAtn * buf3inL + invCut * buf3L; // Pole 3
            buf3inL = buf2L;

            _ly1L = (coef1) * (_ly1L + buf3L) - _lx1L; // allpass
            _lx1L = buf3L;

            *sp++ = clamp(_ly1L * finalGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = buf3R + ((-buf3R + *sp) * r);

            *sp = tanh4ADAA(*sp - buf3R * fb, prevXR, prevFR);
            *sp *= attn;
            buf1R = (*sp) + inAtn * buf1inR + invCut * buf1R; // Pole 1
            buf1inR = (*sp);
            buf2R = (buf1R) + inAtn * buf2inR + invCut * buf2R; // Pole 2
            buf2inR = buf1R;
            buf3R = (buf2R) + inAtn * buf3inR + invCut * buf3R; // Pole 3
            buf3inR = buf2R;

            _ly1R = (coef1) * (_ly1R + buf3R) - _lx1R; // allpass
            _lx1R = buf3R;

            *sp++ = clamp(_ly1R * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = buf1L;
        v1L_ = buf2L;
        v2L_ = buf3L;

        v0R_ = buf1R;
        v1R_ = buf2R;
        v2R_ = buf3R;

        v4L_ = buf1inL;
        v5L_ = buf2inL;
        v6L_ = buf3inL;

        v4R_ = buf1inR;
        v5R_ = buf2inR;
        v6R_ = buf3inR;

        v3L_ = _ly1L;
        v3R_ = _ly1R;
        v7L_ = _lx1L;
        v7R_ = _lx1R;
        v8L_ = prevXL;
        v8R_ = prevXR;

        break;
    } break;
    case FILTER_LADDER: {
        // https://www.musicdsp.org/en/latest/Filters/26-moog-vcf-variation-2.html
        float fxParamTmp = (shared.effect1.param1 + matrixFilterFrequency);
        fxParamTmp *= fxParamTmp;

        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float cut = clamp(sqrt3(fxParam1_), filterWindowMin, 1);
        float cut2 = cut * cut;
        float attn = 0.35013f * cut2 * cut2;
        float inAtn = 0.3f;

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);
        float OffsetTmp = fabsf(filterParam2);
        fxParam2_ = ((OffsetTmp + 9.0f * fxParam2_) * .1f);
        float res = fxParam2_ * 4.5f * (0.8f + cut2 * 0.2f);
        float fb = res * (1 - 0.15f * cut2);
        float invCut = 1 - cut;

        const float r = 0.985f;

        float* sp = this->sampleBlock_;

        float buf1L = v0L_, buf2L = v1L_, buf3L = v2L_, buf4L = v3L_;
        float buf1R = v0R_, buf2R = v1R_, buf3R = v2R_, buf4R = v3R_;

        float buf1inL = v4L_, buf2inL = v5L_, buf3inL = v6L_, buf4inL = v7L_;
        float buf1inR = v4R_, buf2inR = v5R_, buf3inR = v6R_, buf4inR = v7R_;

        float finalGain = mixerGain_ * (1 + fxParam2_ * 2);

        for (int k = BLOCK_SIZE; k--;) {

            // Left voice
            *sp = buf4L + ((-buf4L + *sp) * r);

            *sp -= tanh3(0.005f + buf4L) * (fb + buf4R * 0.05f);
            *sp *= attn;
            buf1L = *sp + inAtn * buf1inL + invCut * buf1L; // Pole 1
            buf1inL = *sp;
            buf2L = buf1L + inAtn * buf2inL + invCut * buf2L; // Pole 2
            buf2inL = buf1L;
            buf3L = buf2L + inAtn * buf3inL + invCut * buf3L; // Pole 3
            buf3inL = buf2L;
            buf4L = buf3L + inAtn * buf4inL + invCut * buf4L; // Pole 4
            buf4inL = buf3L;

            *sp++ = clamp(buf4L * finalGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = buf4R + ((-buf4R + *sp) * r);

            *sp -= tanh3(0.005f + buf4R) * (fb - buf4L * 0.05f);
            *sp *= attn;
            buf1R = *sp + inAtn * buf1inR + invCut * buf1R; // Pole 1
            buf1inR = *sp;
            buf2R = buf1R + inAtn * buf2inR + invCut * buf2R; // Pole 2
            buf2inR = buf1R;
            buf3R = buf2R + inAtn * buf3inR + invCut * buf3R; // Pole 3
            buf3inR = buf2R;
            buf4R = buf3R + inAtn * buf4inR + invCut * buf4R; // Pole 4
            buf4inR = buf3R;

            *sp++ = clamp(buf4R * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = buf1L;
        v1L_ = buf2L;
        v2L_ = buf3L;
        v3L_ = buf4L;

        v0R_ = buf1R;
        v1R_ = buf2R;
        v2R_ = buf3R;
        v3R_ = buf4R;

        v4L_ = buf1inL;
        v5L_ = buf2inL;
        v6L_ = buf3inL;
        v7L_ = buf4inL;

        v4R_ = buf1inR;
        v5R_ = buf2inR;
        v6R_ = buf3inR;
        v7R_ = buf4inR;

        break;
    }
    case FILTER_LADDER2: {
        // https://www.musicdsp.org/en/latest/Filters/240-karlsen-fast-ladder.html
        const float prevParam1 = clamp(fxParam1_, 0.0f, 1.0f);
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParamTmp *= fxParamTmp;

        fxParam1_ = (fxParamTmp + 9.0f * fxParam1_) * 0.1f;
        const float cut0 = clamp(sqrt3(prevParam1), filterWindowMin, 0.92f);
        const float cut1 = clamp(sqrt3(fxParam1_), filterWindowMin, 0.92f);
        const float dCut = (cut1 - cut0) * 0.03125f;
        float cut = cut0;

        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);
        float OffsetTmp = fabsf(filterParam2);
        fxParam2_ = (OffsetTmp + 9.0f * fxParam2_) * 0.1f;
        float res = fxParam2_ * (0.72f + 0.42f * fxParam2_);

        float* sp = this->sampleBlock_;

        float buf1L = v0L_, buf2L = v1L_, buf3L = v2L_, buf4L = v3L_;
        float buf1R = v0R_, buf2R = v1R_, buf3R = v2R_, buf4R = v3R_;

        float _ly1L = v4L_, _lx1L = v5L_;
        float _ly1R = v4R_, _lx1R = v5R_;

        float _ly2L = v6L_, _ly2R = v6R_;
        float _lx2L = v7L_, _lx2R = v7R_;
        float prevXL = v8L_, prevXR = v8R_;
        float prevFL = sat25ADAA1_F(prevXL), prevFR = sat25ADAA1_F(prevXR);

        const float bipolarf = fxParam1_ - 0.5f;
        const float fold3 = (sigmoid(2.0f * bipolarf) * 0.25f + 0.125f) * 0.25f;
        const float f1 = clamp(0.33f + fold3 * (1.0f - cut1 * 0.5f), filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);
        const float f2 = clamp(f1 * f1, filterWindowMin, filterWindowMax);
        float coef2 = (1.0f - f2) / (1.0f + f2);

        float finalGain = mixerGain_ * (1.15f + 0.25f * fxParam2_);

        for (int k = BLOCK_SIZE; k--;) {
            cut += dCut;

            // Left voice
            *sp = *sp - sat25ADAA(buf4L * (res + buf4R * 0.05f), prevXL, prevFL);
            buf1L = ((*sp - buf1L) * cut) + buf1L;
            buf2L = ((buf1L - buf2L) * cut) + buf2L;
            _ly1L = coef1 * (_ly1L + buf2L) - _lx1L; // allpass
            _lx1L = buf2L;
            buf3L = ((_ly1L - buf3L) * cut) + buf3L;
            _ly2L = coef2 * (_ly2L + buf3L) - _lx2L; // allpass
            _lx2L = buf3L;
            buf4L = softLimitVoice(((_ly2L - buf4L) * cut) + buf4L);
            *sp++ = clamp(buf4L * finalGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = *sp - sat25ADAA(buf4R * (res - buf4L * 0.05f), prevXR, prevFR);
            buf1R = ((*sp - buf1R) * cut) + buf1R;
            buf2R = ((buf1R - buf2R) * cut) + buf2R;
            _ly1R = coef1 * (_ly1R + buf2R) - _lx1R; // allpass
            _lx1R = buf2R;
            buf3R = ((_ly1R - buf3R) * cut) + buf3R;
            _ly2R = coef2 * (_ly2R + buf3R) - _lx2R; // allpass
            _lx2R = buf3R;
            buf4R = softLimitVoice(((_ly2R - buf4R) * cut) + buf4R);
            *sp++ = clamp(buf4R * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = buf1L;
        v1L_ = buf2L;
        v2L_ = buf3L;
        v3L_ = buf4L;

        v0R_ = buf1R;
        v1R_ = buf2R;
        v2R_ = buf3R;
        v3R_ = buf4R;

        v4L_ = _ly1L;
        v5L_ = _lx1L;
        v4R_ = _ly1R;
        v5R_ = _lx1R;

        v6L_ = _ly2L;
        v6R_ = _ly2R;
        v7L_ = _lx2L;
        v7R_ = _lx2R;
        v8L_ = prevXL;
        v8R_ = prevXR;

        break;
    }
    case FILTER_DIOD: {
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float f = fxParam1_ * fxParam1_;
        const float fmeta = f * sigmoid(f);
        const float fb = sqrt3(0.7f - filterParam2 * 0.699f);
        const float scale = sqrt3(fb);

        float dc = f * 0.05f * fb;

        float* sp = this->sampleBlock_;
        float lowL = v0L_, bandL = v1L_;
        float lowR = v0R_, bandR = v1R_;

        const float svfGain = (1 + filterParam2 * filterParam2 * 0.75f) * mixerGain_;

        float attn = 0.05f + 0.2f * f * f * f;
        float r = 0.985f;
        float hibp;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice
            *sp = (lowL + ((-lowL + *sp) * r));

            hibp = attn * clamp(*sp - bandL, -ratioTimbres, ratioTimbres);

            lowL += fmeta * hibp;
            bandL += fmeta * (scale * *sp - lowL - fb * (hibp));

            lowL += f * bandL;
            bandL += f * (scale * (*sp + dc) - lowL - fb * (bandL));

            *sp++ = clamp(lowL * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = (lowR + ((-lowR + *sp) * r));

            hibp = attn * clamp(*sp - bandR, -ratioTimbres, ratioTimbres);

            lowR += fmeta * hibp;
            bandR += fmeta * (scale * *sp - lowR - fb * (hibp));

            lowR += f * bandR;
            bandR += f * (scale * (*sp + dc) - lowR - fb * (bandR));

            *sp++ = clamp(lowR * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = lowL;
        v1L_ = bandL;
        v0R_ = lowR;
        v1R_ = bandR;

    } break;
    case FILTER_KRMG: {
        // https://github.com/ddiakopoulos/MoogLadders/blob/master/src/KrajeskiModel.h
        float fxParamTmp = SVFOFFSET + shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        const float wc = fxParam1_ * fxParam1_;
        const float wc2 = wc * wc;
        const float wc3 = wc2 * wc;

        const float g = 0.9892f * wc - 0.4342f * wc2 + 0.1381f * wc3 - 0.0202f * wc2 * wc2;
        const float drive = 0.85f;
        const float gComp = 1;
        const float resonance = filterParam2 * 0.52f - fxParam1_ * 0.05f;
        const float gRes = 1.5f * resonance * (1.0029f + 0.0526f * wc - 0.926f * wc2 + 0.0218f * wc3);

        float* sp = this->sampleBlock_;
        float state0L = v0L_, state1L = v1L_, state2L = v2L_, state3L = v3L_, state4L = v4L_;
        float delay0L = v5L_, delay1L = v6L_, delay2L = v7L_, delay3L = v8L_;

        float state0R = v0R_, state1R = v1R_, state2R = v2R_, state3R = v3R_, state4R = v4R_;
        float delay0R = v5R_, delay1R = v6R_, delay2R = v7R_, delay3R = v8R_;

        const float va1 = 0.2307692308f; // 0.3f / 1.3f;
        const float va2 = 0.7692307692f; // 1 / 1.3f;
        float r = 0.989f;

        float drift = fxParamB2_;
        float nz;
        float nexDrift = noise[7] * 0.015f;
        float deltaD = (nexDrift - drift) * 0.000625f;

        float finalGain = mixerGain_ * 0.77f;

        for (int k = BLOCK_SIZE; k--;) {
            nz = noise[k] * 0.0012f;
            // Left voice
            *sp = (state4L + ((-state4L + *sp) * r));
            state0L = tanh4((drive - drift + nz) * (*sp - gRes * (state4L - gComp * *sp)));

            state1L = g * (va1 * state0L + va2 * delay0L - state1L) + state1L;
            delay0L = state0L;

            state2L = g * (va1 * state1L + va2 * delay1L - state2L) + state2L;
            delay1L = state1L;

            state3L = g * (va1 * state2L + va2 * delay2L - state3L) + state3L;
            delay2L = state2L;

            state4L = g * (va1 * state3L + va2 * delay3L - state4L) + state4L;
            delay3L = state3L;

            *sp++ = clamp(state4L * finalGain, -ratioTimbres, ratioTimbres);

            // Right voice
            *sp = (state4R + ((-state4R + *sp) * r));
            state0R = tanh4((drive + drift - nz) * (*sp - gRes * (state4R - gComp * *sp)));

            state1R = g * (va1 * state0R + va2 * delay0R - state1R) + state1R;
            delay0R = state0R;

            state2R = g * (va1 * state1R + va2 * delay1R - state2R) + state2R;
            delay1R = state1R;

            state3R = g * (va1 * state2R + va2 * delay2R - state3R) + state3R;
            delay2R = state2R;

            state4R = g * (va1 * state3R + va2 * delay3R - state4R) + state4R;
            delay3R = state3R;

            *sp++ = clamp(state4R * finalGain, -ratioTimbres, ratioTimbres);

            drift += deltaD;
        }

        v0L_ = state0L;
        v1L_ = state1L;
        v2L_ = state2L;
        v3L_ = state3L;
        v4L_ = state4L;

        v5L_ = delay0L;
        v6L_ = delay1L;
        v7L_ = delay2L;
        v8L_ = delay3L;

        v0R_ = state0R;
        v1R_ = state1R;
        v2R_ = state2R;
        v3R_ = state3R;
        v4R_ = state4R;

        v5R_ = delay0R;
        v6R_ = delay1R;
        v7R_ = delay2R;
        v8R_ = delay3R;

        fxParamB2_ = nexDrift;
    } break;
    case FILTER_TEEBEE: {
        // https://github.com/a1k0n/303
        //  Authentic TB-303 4-pole diode ladder + C13 "gummy capacitor" Accent circuit
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParam1_ = clamp((fxParamTmp + 4.0f * fxParam1_) * 0.2f, 0.0f, 1.0f);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);

        // Detect raw MIDI velocity >= 78 for authentic TB-303 Accent (independent of PreenFM3's engine1.velocity scaling)
        const int rawVelo = (uint8_t)this->midiVelocity_;
        const bool isAccented = (rawVelo >= 78);
        const float accentStrength = isAccented ? clamp((float)(rawVelo - 72) * (1.0f / 55.0f), 0.15f, 1.0f) : 0.0f;

        // State variables for the 2-stage TB-303 Accent circuit:
        // v6L_ = short 200ms Accent decay envelope (R_accent)
        // fxParamA2_ = C13 capacitor charge (accumulates across consecutive accented 16th notes)
        // fxParamA1_ = smoothed C13 "wow" sweep output
        float accentDecayEnv = v6L_;
        if (this->fxNoteTrigger_) {
            this->fxNoteTrigger_ = 0;
            if (isAccented) {
                // Trigger fast 200ms decay envelope and stack charge onto C13 capacitor
                accentDecayEnv = accentStrength;
                fxParamA2_ = clamp(fxParamA2_ * 0.55f + accentStrength * (0.45f + 0.35f * filterParam2), 0.0f, 1.65f);
            } else {
                // Unaccented note: dump accent envelope & C13 charge for sharp accented/unaccented contrast
                accentDecayEnv *= 0.15f;
                fxParamA2_ *= 0.25f;
            }
        }

        // 1) Short 190ms TB-303 Accent decay (decays immediately after note-on, even while key is held!)
        accentDecayEnv *= 0.9965f;
        v6L_ = accentDecayEnv;

        // 2) C13 "Gummy Capacitor" RC charge/discharge network (~14ms rounded "wow" attack + resonant bleed)
        const float c13Target = accentDecayEnv * (0.65f + 0.60f * filterParam2);
        if (c13Target > fxParamA2_) {
            fxParamA2_ += 0.26f * (c13Target - fxParamA2_); // ~14ms rounded rise ("wow" attack)
        } else {
            fxParamA2_ = fxParamA2_ * 0.9945f + 0.0055f * c13Target; // capacitor bleed
        }
        fxParamA1_ = (fxParamA1_ * 7.0f + fxParamA2_) * 0.125f;

        float* sp = this->sampleBlock_;
        float state0L = v0L_, state1L = v1L_, state2L = v2L_, state3L = v3L_, state4L = v4L_;
        float state0R = v0R_, state1R = v1R_, state2R = v2R_, state3R = v3R_, state4R = v4R_;

        // Continuous resonance index [0.0 .. 61.5] with linear interpolation across filterpoles table
        const float resoScaled = filterParam2 * (0.65f + 0.35f * filterParam2) * 61.5f;
        const int resoIdx = __USAT((int)resoScaled, 6);
        const int resoIdxNext = (resoIdx < 62) ? (resoIdx + 1) : 62;
        const float resoFrac = resoScaled - (float)resoIdx;
        const float resoInv = 1.0f - resoFrac;

        // Smooth C-infinity exponential cutoff via exp_pole (replaces piecewise-linear expf_fast octave corners)
        const float accentSweep = fxParamA1_ * (0.42f + 0.65f * filterParam2);
        const float vcf_cutoff = clamp(fxParam1_ + accentSweep, 0.0f, 1.55f);
        const float cutoffHz = 141.17496f * exp_pole(3.05f * vcf_cutoff) + 40.0f;
        const float wTarget = clamp(cutoffHz * (2.0f * 3.14159265358979f / PREENFM_FREQUENCY), 0.015f, 1.38f);
        const float wStart = (v7L_ > 0.0f) ? v7L_ : wTarget;

        // Interpolate closed-loop s-plane poles from a1k0n/303 table
        const float fp0 = filterpoles[0][resoIdx] * resoInv + filterpoles[0][resoIdxNext] * resoFrac;
        const float fp1 = filterpoles[1][resoIdx] * resoInv + filterpoles[1][resoIdxNext] * resoFrac;
        const float fp2 = filterpoles[2][resoIdx] * resoInv + filterpoles[2][resoIdxNext] * resoFrac;
        const float fp3 = filterpoles[3][resoIdx] * resoInv + filterpoles[3][resoIdxNext] * resoFrac;
        const float fp4 = filterpoles[4][resoIdx] * resoInv + filterpoles[4][resoIdxNext] * resoFrac;
        const float fp5 = filterpoles[5][resoIdx] * resoInv + filterpoles[5][resoIdxNext] * resoFrac;
        const float fp6 = filterpoles[6][resoIdx] * resoInv + filterpoles[6][resoIdxNext] * resoFrac;
        const float fp7 = filterpoles[7][resoIdx] * resoInv + filterpoles[7][resoIdxNext] * resoFrac;
        const float fp8 = filterpoles[8][resoIdx] * resoInv + filterpoles[8][resoIdxNext] * resoFrac;
        const float fp9 = filterpoles[9][resoIdx] * resoInv + filterpoles[9][resoIdxNext] * resoFrac;

        // Start-of-block and end-of-block biquad coefficients for 48 kHz sample-rate interpolation
        const float p0_0 = fp0 + wStart * fp1;
        const float p1r_0 = fp2 + wStart * fp4;
        const float p1i_0 = fp3 + wStart * fp5;
        const float p2r_0 = fp6 + wStart * fp8;
        const float p2i_0 = fp7 + wStart * fp9;

        const float p0_1 = fp0 + wTarget * fp1;
        const float p1r_1 = fp2 + wTarget * fp4;
        const float p1i_1 = fp3 + wTarget * fp5;
        const float p2r_1 = fp6 + wTarget * fp8;
        const float p2i_1 = fp7 + wTarget * fp9;

        const float reso_k = filterParam2 * 3.8f;
        const float targetgain = (1.85f / (1.0f + reso_k * 0.55f)) + 0.55f * filterParam2 + 0.50f * accentDecayEnv + 0.25f * fxParamA1_;

        const float p0f_0 = exp_pole(p0_0);
        const float p0f_1 = exp_pole(p0_1);
        // f0gain = halfGain * (1 - f0a1) is linear in f0a1: derive it per sample instead of interpolating it separately
        const float halfGain = targetgain * 0.5f;
        float f0a1 = -p0f_0;
        const float df0a1 = (-p0f_1 - f0a1) * 0.03125f;

        // Filter section 1: non-resonant 2-pole diode skirt (+ subtle mid-bark boost on Accent)
        const float accentBark = 1.0f + 0.25f * fxParamA1_;
        const float exp_p1r_0 = exp_pole(p1r_0);
        float f1a1 = -2.0f * exp_p1r_0 * cosf(p1i_0);
        float f1a2 = exp_p1r_0 * exp_p1r_0;

        const float exp_p1r_1 = exp_pole(p1r_1);
        const float f1a1_1 = -2.0f * exp_p1r_1 * cosf(p1i_1);
        const float f1a2_1 = exp_p1r_1 * exp_p1r_1;
        const float df1a1 = (f1a1_1 - f1a1) * 0.03125f;
        const float df1a2 = (f1a2_1 - f1a2) * 0.03125f;

        // Filter section 2: high-Q resonant 2-pole acid peak
        const float exp_p2r_0 = exp_pole(p2r_0);
        float f2a1 = -2.0f * exp_p2r_0 * cosf(p2i_0);
        float f2a2 = exp_p2r_0 * exp_p2r_0;

        const float exp_p2r_1 = exp_pole(p2r_1);
        const float f2a1_1 = -2.0f * exp_p2r_1 * cosf(p2i_1);
        const float f2a2_1 = exp_p2r_1 * exp_p2r_1;
        const float df2a1 = (f2a1_1 - f2a1) * 0.03125f;
        const float df2a2 = (f2a2_1 - f2a2) * 0.03125f;

        // Asymmetric diode-ladder inter-stage saturation & BA662 VCA output drive
        const float drive2 = 1.15f + 0.55f * filterParam2 + 0.65f * fxParamA1_;
        const float asymBias = 0.06f * (0.4f + 0.6f * filterParam2 + 0.5f * fxParamA1_);
        const float asymOut = asymBias * 1.45f;
        const float finalGain = mixerGain_ * (0.82f / (1.0f + 0.22f * filterParam2));
        float prevXL = v5L_, prevXR = v5R_;
        float prevFL = tanh4ADAA1_F(prevXL), prevFR = tanh4ADAA1_F(prevXR);

        for (int k = BLOCK_SIZE; k--;) {
            f0a1 += df0a1;
            f1a1 += df1a1;
            f1a2 += df1a2;
            f2a1 += df2a1;
            f2a2 += df2a2;
            const float f0gain = halfGain * (1.0f - f0a1);
            const float f1b = (1.0f + f1a1 + f1a2) * accentBark;
            const float f2b = 1.0f + f2a1 + f2a2;

            // -------- -------- -------- Left
            float inL = *sp;
            float y0L = f0gain * inL + state0L;
            state0L = -f0gain * inL - f0a1 * y0L;

            // First two-pole stage (diode skirt)
            float y1L = f1b * y0L + state1L;
            state1L = state2L - f1a1 * y1L;
            state2L = -f1a2 * y1L;

            // Subtle diode ladder inter-stage soft compression (tames extreme resonant whistling into liquid acid squelch)
            float midL = softLimitVoice(y1L * 0.65f + asymBias) * 1.5f - asymOut;

            // Second two-pole stage (acid resonant peak)
            float y2L = f2b * midL + state3L;
            state3L = state4L - f2a1 * y2L;
            state4L = -f2a2 * y2L;

            *sp++ = clamp(tanh4ADAA(y2L * drive2, prevXL, prevFL) * finalGain, -ratioTimbres, ratioTimbres);

            // -------- -------- -------- Right
            float inR = *sp;
            float y0R = f0gain * inR + state0R;
            state0R = -f0gain * inR - f0a1 * y0R;

            // First two-pole stage (diode skirt)
            float y1R = f1b * y0R + state1R;
            state1R = state2R - f1a1 * y1R;
            state2R = -f1a2 * y1R;

            float midR = softLimitVoice(y1R * 0.65f + asymBias) * 1.5f - asymOut;

            // Second two-pole stage (acid resonant peak)
            float y2R = f2b * midR + state3R;
            state3R = state4R - f2a1 * y2R;
            state4R = -f2a2 * y2R;

            *sp++ = clamp(tanh4ADAA(y2R * drive2, prevXR, prevFR) * finalGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = state0L;
        v1L_ = state1L;
        v2L_ = state2L;
        v3L_ = state3L;
        v4L_ = state4L;
        v5L_ = prevXL;
        v7L_ = wTarget;

        v0R_ = state0R;
        v1R_ = state1R;
        v2R_ = state2R;
        v3R_ = state3R;
        v4R_ = state4R;
        v5R_ = prevXR;
    } break;
    case FILTER_SVFLH: {
        const float prevParam1 = clamp(fxParam1_, 0.0f, 1.0f);
        float fxParamTmp = clamp(shared.effect1.param1 + matrixFilterFrequency, 0.0f, 1.0f);
        fxParamTmp *= fxParamTmp;
        fxParam1_ = clamp((fxParamTmp + 9.0f * prevParam1) * 0.1f, 0.0f, 1.0f);
        float res = clamp(matrixFilterParam2 + shared.effect1.param2, 0.0f, 1.0f);

        // MS-20 style dual ZDF SVF + Allpass phase disperser: Resonant Highpass -> Allpass -> Driven Resonant Lowpass
        const float gLP0 = 0.0025f + prevParam1 * (0.42f + 0.72f * prevParam1);
        const float gLP1 = 0.0025f + fxParam1_ * (0.42f + 0.72f * fxParam1_);
        const float dgLP = (gLP1 - gLP0) * 0.03125f;
        float gLP = gLP0;

        const float gHP0 = 0.0015f + prevParam1 * (0.015f + 0.045f * res);
        const float gHP1 = 0.0015f + fxParam1_ * (0.015f + 0.045f * res);
        const float dgHP = (gHP1 - gHP0) * 0.03125f;
        float gHP = gHP0;

        const float kLP = 1.92f - 1.88f * res;
        const float kHP = 1.45f - 1.15f * res;

        const float f1 = clamp(0.28f + 0.42f * fxParam1_, filterWindowMin, filterWindowMax);
        const float coef1 = (1.0f - f1) / (1.0f + f1);

        const float a1_lp_0 = 1.0f / (1.0f + gLP0 * (gLP0 + kLP));
        const float a1_lp_1 = 1.0f / (1.0f + gLP1 * (gLP1 + kLP));
        const float da1_lp = (a1_lp_1 - a1_lp_0) * 0.03125f;
        float a1_lp_lin = a1_lp_0;

        const float a1_hp_0 = 1.0f / (1.0f + gHP0 * (gHP0 + kHP));
        const float a1_hp_1 = 1.0f / (1.0f + gHP1 * (gHP1 + kHP));
        const float da1_hp = (a1_hp_1 - a1_hp_0) * 0.03125f;
        float a1_hp_lin = a1_hp_0;

        const float sat = 1.10f + res * 0.65f;
        const float satK = 0.10f + 0.28f * res;
        const float svfGain = (0.95f + 0.25f * res) * mixerGain_;

        float* sp = this->sampleBlock_;
        float hp_ic1L = v0L_, hp_ic2L = v1L_, lp_ic1L = v2L_, lp_ic2L = v3L_;
        float hp_ic1R = v0R_, hp_ic2R = v1R_, lp_ic1R = v2R_, lp_ic2R = v3R_;
        float _ly1L = v4L_, _ly1R = v4R_;
        float _lx1L = v5L_, _lx1R = v5R_;
        float prevXL = v8L_, prevXR = v8R_;
        float prevFL = sat25ADAA1_F(prevXL), prevFR = sat25ADAA1_F(prevXR);

        for (int kSample = BLOCK_SIZE; kSample--;) {
            gLP += dgLP;
            gHP += dgHP;
            a1_lp_lin += da1_lp;
            a1_hp_lin += da1_hp;
            const float a1_lp = a1_lp_lin * (2.0f - (1.0f + gLP * (gLP + kLP)) * a1_lp_lin);
            const float a1_hp = a1_hp_lin * (2.0f - (1.0f + gHP * (gHP + kHP)) * a1_hp_lin);

            // Left voice: ADAA input drive -> Resonant HP -> Allpass -> Resonant LP
            float inL = sat25ADAA((*sp) * sat, prevXL, prevFL);
            float v3 = inL - hp_ic2L;
            float v1 = a1_hp * (hp_ic1L + gHP * v3);
            float v2 = hp_ic2L + gHP * v1;
            hp_ic1L = 2.0f * v1 - hp_ic1L;
            hp_ic2L = 2.0f * v2 - hp_ic2L;
            float hpOutL = inL - kHP * v1 - v2;

            _ly1L = coef1 * (_ly1L + hpOutL) - _lx1L;
            _lx1L = hpOutL;

            v3 = _ly1L - lp_ic2L;
            v1 = a1_lp * (lp_ic1L + gLP * v3);
            v2 = lp_ic2L + gLP * v1;
            lp_ic1L = 2.0f * v1 - lp_ic1L;
            lp_ic1L -= satK * lp_ic1L * (lp_ic1L * lp_ic1L) * fastInvSqrtVoice(1.0f + lp_ic1L * lp_ic1L);
            lp_ic2L = 2.0f * v2 - lp_ic2L;
            *sp++ = clamp(v2 * svfGain, -ratioTimbres, ratioTimbres);

            // Right voice
            float inR = sat25ADAA((*sp) * sat, prevXR, prevFR);
            v3 = inR - hp_ic2R;
            v1 = a1_hp * (hp_ic1R + gHP * v3);
            v2 = hp_ic2R + gHP * v1;
            hp_ic1R = 2.0f * v1 - hp_ic1R;
            hp_ic2R = 2.0f * v2 - hp_ic2R;
            float hpOutR = inR - kHP * v1 - v2;

            _ly1R = coef1 * (_ly1R + hpOutR) - _lx1R;
            _lx1R = hpOutR;

            v3 = _ly1R - lp_ic2R;
            v1 = a1_lp * (lp_ic1R + gLP * v3);
            v2 = lp_ic2R + gLP * v1;
            lp_ic1R = 2.0f * v1 - lp_ic1R;
            lp_ic1R -= satK * lp_ic1R * (lp_ic1R * lp_ic1R) * fastInvSqrtVoice(1.0f + lp_ic1R * lp_ic1R);
            lp_ic2R = 2.0f * v2 - lp_ic2R;
            *sp++ = clamp(v2 * svfGain, -ratioTimbres, ratioTimbres);
        }

        v0L_ = hp_ic1L;
        v1L_ = hp_ic2L;
        v2L_ = lp_ic1L;
        v3L_ = lp_ic2L;
        v0R_ = hp_ic1R;
        v1R_ = hp_ic2R;
        v2R_ = lp_ic1R;
        v3R_ = lp_ic2R;
        v4L_ = _ly1L;
        v4R_ = _ly1R;
        v5L_ = _lx1L;
        v5R_ = _lx1R;
        v8L_ = prevXL;
        v8R_ = prevXR;
    } break;
    case FILTER_CRUSH2: {
        float fxParamTmp = shared.effect1.param1 + matrixFilterFrequency;
        fxParamTmp *= fxParamTmp;
        // Low pass... on the Frequency
        fxParam1_ = clamp((fxParamTmp + 9.0f * fxParam1_) * .1f, 0, 1);
        float filterParam2 = clamp(matrixFilterParam2 + shared.effect1.param2, 0, 1);

        // const float f = 0.05f + clamp(sigmoid(fxParam1 + fxParam2) * 0.67f, 0, 0.85f);
        const float f = 0.55f + clamp(sigmoid(fxParam1_) * 0.45f, 0, 0.45f);
        const float reso = 0.28f + (1 - fxParam1_) * 0.55f;
        const float pattern = (1 - reso * f);

        float* sp = this->sampleBlock_;

        float bits1 = v8L_;
        float bits2 = v8R_;

        float b1inc = fxParam1_ * fxParam1_ * fxParam1_;
        float b2inc = clamp(filterParam2 * filterParam2 * filterParam2 * 0.95f + b1inc * 0.05f, 0, 1);

        float _ly1L = v2L_, _ly1R = v2R_;
        float _lx1L = v3L_, _lx1R = v3R_;

        float _ly2L = v4L_, _ly2R = v4R_;
        float _lx2L = v5L_, _lx2R = v5R_;

        float localv0L = v0L_;
        float localv1L = v1L_;
        float localv0R = v0R_;
        float localv1R = v1R_;

        float drift = fxParamB2_;
        float nexDrift = *sp * 0.015f;
        float deltaD = (nexDrift - drift) * 0.000625f;

        const float f1 = clamp(0.23f + filterParam2 * filterParam2 * 0.33f, filterWindowMin, filterWindowMax);
        float coef1 = (1.0f - f1) / (1.0f + f1);

        const float f2 = clamp(0.33f + fxParam1_ * fxParam1_ * 0.43f, filterWindowMin, filterWindowMax);
        float coef2 = (1.0f - f2) / (1.0f + f2);

        float inL = v6L_, inR = v6R_, destL = v7L_, destR = v7R_;

        float sat = 1.25f + fxParam1_ * 0.25f;

        for (int k = BLOCK_SIZE; k--;) {
            // Left voice

            if (bits1 >= 1) {
                drift += deltaD;
                destL = (*sp);
            }
            if (bits2 >= 1) {
                drift -= deltaD;
                destL = sat25(*sp * sat);
            }
            inL = (inL * 7 + destL) * 0.125f; // smoothing

            localv0L = pattern * localv0L - f * sat25(localv1L + inL);
            localv1L = pattern * localv1L + f * localv0L;

            _ly1L = (coef1 + drift) * (_ly1L + localv1L) - _lx1L; // allpass
            _lx1L = localv1L;

            _ly2L = (coef2 - drift) * (_ly2L + _ly1L) - _lx2L; // allpass
            _lx2L = _ly1L;

            *sp++ = clamp(_ly2L * mixerGain_, -ratioTimbres, ratioTimbres);

            // Right voice

            if (bits1 >= 1) {
                bits1 -= 1;
                destR = (*sp);
            }
            if (bits2 >= 1) {
                bits2 -= 1;
                destR = sat25(*sp * sat);
            }
            inR = (inR * 7 + destR) * 0.125f;

            localv0R = pattern * localv0R - f * sat25(localv1R + inR);
            localv1R = pattern * localv1R + f * localv0R;

            _ly1R = (coef1 - drift) * (_ly1R + localv1R) - _lx1R; // allpass
            _lx1R = localv1R;

            _ly2R = (coef2 + drift) * (_ly2R + _ly1R) - _lx2R; // allpass
            _lx2R = _ly1R;

            *sp++ = clamp(_ly2R * mixerGain_, -ratioTimbres, ratioTimbres);

            bits1 += b1inc;
            bits2 += b2inc;
        }

        v8L_ = bits1;
        v8R_ = bits2;

        v0L_ = localv0L;
        v1L_ = localv1L;
        v0R_ = localv0R;
        v1R_ = localv1R;

        v2L_ = _ly1L;
        v2R_ = _ly1R;
        v3L_ = _lx1L;
        v3R_ = _lx1R;

        v4L_ = _ly2L;
        v4R_ = _ly2R;
        v5L_ = _lx2L;
        v5R_ = _lx2R;

        v6L_ = inL;
        v6R_ = inR;
        v7L_ = destL;
        v7R_ = destR;

        fxParamB2_ = nexDrift;
    } break;

    case FILTER_OFF: {
        // Filter off has gain...
        float* sp = this->sampleBlock_;
        for (int k = 0; k < BLOCK_SIZE; k++) {
            *sp++ = (*sp) * mixerGain_;
            *sp++ = (*sp) * mixerGain_;
        }
    } break;

    default:
        // NO EFFECT
        break;
    }
}

void Voice::recomputeBPValues(float q, float fSquare) {
    //        /* filter coefficients */
    //        omega1  = 2 * PI * f/srate; // f is your center frequency
    //        sn1 = (float)sin(omega1);
    //        cs1 = (float)cos(omega1);
    //        alpha1 = sn1/(2*Qvalue);        // Qvalue is none other than Q!
    //        a0 = 1.0f + alpha1;     // a0
    //        b0 = alpha1;            // b0
    //        b1 = 0.0f;          // b1/b0
    //        b2= -alpha1/b0          // b2/b0
    //        a1= -2.0f * cs1/a0;     // a1/a0
    //        a2= (1.0f - alpha1)/a0;          // a2/a0
    //        k = b0/a0;

    // frequency must be up to SR / 2.... So 1024 * param1 :
    // 1000 instead of 1024 to get rid of strange border effect....

    // limit low values to avoid cracklings :
    if (fSquare < 0.1f && q < 0.15f) {
        q = 0.15f;
    }

    float sn1 = sinTable[(int)(12 + 1000 * fSquare)];
    // sin(x) = cos( PI/2 - x)
    int cosPhase = 500 - 1000 * fSquare;
    if (cosPhase < 0) {
        cosPhase += 2048;
    }
    float cs1 = sinTable[cosPhase];

    float alpha1 = sn1 * 12.5f;
    if (q > 0) {
        alpha1 = sn1 / (8 * q);
    }

    float A0 = 1.0f + alpha1;
    float A0Inv = 1 / A0;

    float B0 = alpha1;
    // fxParamB1 = 0.0f;
    fxParamB2_ = -alpha1 * A0Inv;
    fxParamA1_ = -2.0f * cs1 * A0Inv;
    fxParamA2_ = (1.0f - alpha1) * A0Inv;

    fxParam1_ = B0 * A0Inv;
}

void Voice::setNewEffectParam(int encoder) {
    if (encoder == 0) {
        v0L_ = v1L_ = v2L_ = v3L_ = v4L_ = v5L_ = v6L_ = v7L_ = v8L_ = v0R_ = v1R_ = v2R_ = v3R_ = v4R_ = v5R_ = v6R_ = v7R_ = v8R_ = 0.0f;
        fxParamA1_ = fxParamA2_ = fxParamB2_ = 0;

        for (int k = 1; k < PFM3_PARAMS_PER_ROW; k++) {
            setNewEffectParam(k);
        }
    }
    switch ((int)currentTimbre_->sharedParams().effect1.type) {
    case FILTER_BASS:
        // Selectivity = fxParam1
        // ratio = fxParam2
        // gain1 = fxParam3
        fxParam1_ = 50 + 200 * currentTimbre_->sharedParams().effect1.param1;
        fxParam3_ = 1.0f / (fxParam1_ + 1.0f);
        break;
    case FILTER_BP:
    case FILTER_BP2: {
        fxParam1PlusMatrix_ = -1.0f;
        break;
    }
    default:
        break;
    }
}

#if defined(__GNUC__) && !defined(DEBUG)
#pragma GCC pop_options
#endif
