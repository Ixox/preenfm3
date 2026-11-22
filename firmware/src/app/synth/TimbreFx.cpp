#include "SynthState.h"
#include "Timbre.h"
#include "Voice.h"
#include <math.h>

#if defined(__GNUC__) && !defined(DEBUG)
#pragma GCC push_options
#pragma GCC optimize("Ofast", "fast-math")
#endif

#define INV_BLOCK_SIZE (1.0f / BLOCK_SIZE)

extern float noise[32];

static inline float modulo2(float readPos, int bufferLen) {
    return unlikely(readPos < 0) ? readPos + bufferLen : readPos;
}
static inline float modulo(float d, float max) {
    return unlikely(d >= max) ? d - max : d;
}
static inline float clamp(float d, float min, float max) {
    const float t = unlikely(d < min) ? min : d;
    return unlikely(t > max) ? max : t;
}
static inline float sqrt3(const float x) {
    union {
        int i;
        float x;
    } u;

    u.x = x;
    u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
    return u.x;
}
static inline float foldAbs(float x) {
    x *= 0.5f;
    float f = (fabsf(x - roundf(x)));
    return f + f;
}
static inline float fold(float x4) {
    return fabsf(x4 + 0.25f - roundf(x4 + 0.25f)) - 0.25f;
}
static inline float sigmoid(float x) {
    return x * (1.5f - 0.5f * x * x);
}
static inline float sigmoidPos(float x) {
    // x : 0 -> 1
    return (sigmoid((x * 2) - 1) + 1) * 0.5f;
}
static inline float tanh4(float x) {
    return x / sqrt3(x * x + 1);
}
static inline float fastSin(float x) {
    return 3.9961f * x * (1 - fabsf(x));
}
static inline float hann(float x) {
    float s = sqrt3(x * (1 - x));
    return s + s;
}
static inline float max(float x1, float x2) {
    return (x1 > x2) ? x1 : x2;
}
static inline float fast_log2f(float x) {
    union {
        float f;
        uint32_t i;
    } vx = {x};
    int exp = (int)((vx.i >> 23) & 0xFF) - 127;        // exposant IEEE754
    float mant = (vx.i & 0x7FFFFF) / (float)(1 << 23); // mantisse normalisée
    return exp + mant;                                 // approx log2
}
static inline float fast_expf(float x) {
    union {
        float f;
        int32_t i;
    } u;
    u.i = (int32_t)(12102203.0f * x + 127 * (1 << 23));
    return u.f;
}
static inline float fast_pow2(float p) {
    // approx 2^p
    union {
        uint32_t i;
        float f;
    } v;
    v.i = (uint32_t)((1 << 23) * (p + 126.94269504f));
    return v.f;
}
static inline float fastExpNeg(float x) {
    // x > 0
    return fast_expf(-x);
}
static inline float fast_cos_2pi(float x) {
    // x entre 0 et 0.5 environ (fréquence normalisée)
    float x2 = x * x;
    return 1.0f - 19.7392088f * x2 + 64.3776844f * x2 * x2;
}
static inline float soft_clip(float x) {
    return x / (1.0f + fabsf(x));
}
static inline float blend_screen_smooth(float x1, float x2) {
    float a = 0.5f * (x1 + 1.0f);
    float b = 0.5f * (x2 + 1.0f);
    float y = a + b - a * b; // smooth polynomial, no branches
    return 2.0f * y - 1.0f;
}
static inline float blend_softlight(float x1, float x2) {
    float a = 0.5f * (x1 + 1.0f);
    float b = 0.5f * (x2 + 1.0f);
    float y = a - (1.0f - 2.0f * b) * a * (1.0f - a); // continuous polynomial
    return 2.0f * y - 1.0f;
}
static inline float mix_soft(float x1, float x2, float drive) {
    float m = 0.5f * (x1 + x2);
    float d = 0.5f * (x1 - x2);
    float y = m + d * (1.0f - drive * fabsf(m));
    return soft_clip(y);
};
// all pass params
const float f1 = 0.0156f;
const float apcoef1 = (1.0f - f1) / (1.0f + f1);
const float f2 = (0.17f + f1);
const float apcoef2 = (1.0f - f2) / (1.0f + f2);
const float f3 = (0.17f + f2);
const float apcoef3 = (1.0f - f3) / (1.0f + f3);
const float f4 = (0.17f + f3);
const float apcoef4 = (1.0f - f4) / (1.0f + f4);

// delay sizes
const float delayBufferSizeF = delayBufferSize;
const float delayBufferSize90 = delayBufferSize * 0.25f;
const float delayBufferSize180 = delayBufferSize * 0.5f;
const int delayBufferSizeM1 = delayBufferSize - 1;
const int delayBufferSizeM4 = delayBufferSize - 4;
const float delayBufferSizeInv = 1.0f / delayBufferSize;
const int delayBufStereoSize = delayBufferSize * 0.5f;
const int delayBufStereoSizeM1 = delayBufStereoSize - 1;
const float delayBufStereoDiv4 = delayBufStereoSize * 0.25f;
const float delayBufStereoSizeInv = 1.0f / delayBufStereoSize;

void Timbre::initFx() {
    hb_x1_[0] = &hb1_x1_;
    hb_x1_[1] = &hb2_x1_;
    hb_x1_[2] = &hb3_x1_;
    hb_x1_[3] = &hb4_x1_;
    hb_x1_[4] = &hb5_x1_;
    hb_x1_[5] = &hb6_x1_;
    hb_x1_[6] = &hb7_x1_;
    hb_x1_[7] = &hb8_x1_;

    hb_x2_[0] = &hb1_x2_;
    hb_x2_[1] = &hb2_x2_;
    hb_x2_[2] = &hb3_x2_;
    hb_x2_[3] = &hb4_x2_;
    hb_x2_[4] = &hb5_x2_;
    hb_x2_[5] = &hb6_x2_;
    hb_x2_[6] = &hb7_x2_;
    hb_x2_[7] = &hb8_x2_;

    hb_y1_[0] = &hb1_y1_;
    hb_y1_[1] = &hb2_y1_;
    hb_y1_[2] = &hb3_y1_;
    hb_y1_[3] = &hb4_y1_;
    hb_y1_[4] = &hb5_y1_;
    hb_y1_[5] = &hb6_y1_;
    hb_y1_[6] = &hb7_y1_;
    hb_y1_[7] = &hb8_y1_;

    hb_y2_[0] = &hb1_y2_;
    hb_y2_[1] = &hb2_y2_;
    hb_y2_[2] = &hb3_y2_;
    hb_y2_[3] = &hb4_y2_;
    hb_y2_[4] = &hb5_y2_;
    hb_y2_[5] = &hb6_y2_;
    hb_y2_[6] = &hb7_y2_;
    hb_y2_[7] = &hb8_y2_;
}

void Timbre::fxAfterBlock() {
    int fx2Type = sharedParams().effect2.type;
    if (fx2Type == FILTER2_OFF) {
        prevFx2Type_ = fx2Type;
        newNotePlayed_ = false;
        return;
    }

    float matrixFilterFrequency = 0.0f;
    float matrixFilterParam2 = 0.0f;
    float matrixFilterAmp = 0.0f;
    float matrixFilterPan = 0.0f;

    if ((int)numberOfVoices_ > 0) {
        if (!voices_[lastPlayedNote_]->isPlaying()) {
            // this voice is not playing but still need to calculate lfo
            engineImpl_->prepareFxMatrix(voices_[lastPlayedNote_]);
        }

        matrixFilterFrequency = engineImpl_->getMatrixDestination(FILTER2_PARAM1);
        matrixFilterParam2 = engineImpl_->getMatrixDestination(FILTER2_PARAM2);
        matrixFilterAmp = engineImpl_->getMatrixDestination(FILTER2_AMP);
        matrixFilterPan = clamp(engineImpl_->getMatrixDestination(ALL_PAN), -1, 1);
    }
    float gainTmp = clamp(sharedParams().effect2.param3 + matrixFilterAmp, 0, 16);

    if (prevFx2Type_ != fx2Type) {
        // anti click on fx change
        mixerGain_ = 0;
        feedbackInput_ = 0;
        feedback_ = 0;
        for (int s = 0; s < delayBufferSize; s++) {
            delayBuffer_[s] = 0;
        }
        low1_ = low2_ = low3_ = low4_ = low5_ = low6_ = 0;
        band1_ = band2_ = band3_ = band4_ = band5_ = band6_ = 0;
        hb1_x1_ = hb1_x2_ = hb1_y1_ = hb1_y2_ = 0;
        hb2_x1_ = hb2_x2_ = hb2_y1_ = hb2_y2_ = 0;
        hb3_x1_ = hb3_x2_ = hb3_y1_ = hb3_y2_ = 0;
        hb4_x1_ = hb4_x2_ = hb4_y1_ = hb4_y2_ = 0;
        hb5_x1_ = hb5_x2_ = hb5_y1_ = hb5_y2_ = 0;
        hb6_x1_ = hb6_x2_ = hb6_y1_ = hb6_y2_ = 0;
        hb7_x1_ = hb7_x2_ = hb7_y1_ = hb7_y2_ = 0;
        hb8_x1_ = hb8_x2_ = hb8_y1_ = hb8_y2_ = 0;
        inputWritePos1_ = inputWritePos2_ = inputWritePos3_ = inputWritePos4_ = inputWritePos5_ = 0;
    }
    prevFx2Type_ = fx2Type;

    switch (fx2Type) {
    case FILTER2_FLANGE:
        fx2Flange(matrixFilterFrequency, matrixFilterParam2, gainTmp);
        break;
    case FILTER2_CHORUS:
        fx2Chorus(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_DIMENSION:
        fx2Dimension(matrixFilterFrequency, matrixFilterParam2, gainTmp);
        break;
    case FILTER2_DOUBLER:
        fx2Doubler(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_TRIPLER:
        fx2Tripler(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_BODE:
        fx2Bode(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_WIDE:
        fx2Wide(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_DELAYCRUNCH:
        fx2DelayCrunch(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_PINGPONG:
        fx2PingPong(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_DIFFUSER:
        fx2Diffuser(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_GRAIN1:
        fx2Grain1(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_GRAIN2:
        fx2Grain2(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    case FILTER2_STEREO_BP:
        fx2StereoBp(matrixFilterFrequency, matrixFilterParam2, matrixFilterPan, gainTmp);
        break;
    default:
        // NO EFFECT
        break;
    }

    this->newNotePlayed_ = false;
}

__attribute__((noinline)) void Timbre::fx2Flange(float matrixFilterFrequency, float matrixFilterParam2, float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 0.5f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    param1S_ = 0.02f * this->sharedParams().effect2.param1 + .98f * param1S_;
    float speed = clamp(param1S_ * param1S_ * param1S_, 0, 1);

    float lfo1Inc = clamp(hb8_y1_ * speed * 0.015f, -1, 1);
    hb8_x1_ += lfo1Inc;
    if (hb8_x1_ >= 1) {
        hb8_x1_ = 1;
        hb8_y1_ = -1;
    }
    if (hb8_x1_ <= 0) {
        hb8_x1_ = 0;
        hb8_y1_ = 1;
    }

    float lfoRaw = foldAbs(hb8_x1_ + matrixFilterFrequency) * 0.5f;
    float fxParamTmp = sigmoidPos(lfoRaw);
    delayReadFrac_ = (fxParamTmp + 99 * delayReadFrac_) * 0.01f; // smooth change

    float currentDelaySize1 = clamp(delaySize1_, 2.0f, (float)(delayBufStereoSize - 3));
    delaySize1_ = clamp(delayBufStereoDiv4 * delayReadFrac_, 2.0f, (float)(delayBufStereoSize - 3));
    float delaySizeInc1 = (delaySize1_ - currentDelaySize1) * INV_BLOCK_SIZE;

    float currentFeedback = feedback_;
    feedback_ = clamp(sqrt3(fabsf(this->sharedParams().effect2.param2 + matrixFilterParam2)), -1, 1) * 0.95f;
    float feedbackInc = (feedback_ - currentFeedback) * INV_BLOCK_SIZE;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ delayBuf = delayBuffer_;

    float delayReadPos90;

    float filterB2 = 0.1f;
    float filterB = (filterB2 * filterB2 * 0.5f);

    float _in3_b1 = (1 - filterB);
    float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;

    const float f = 0.8f;
    const float f2 = 0.85f;
    const float fnotch = 1.03f;
    const float driftOffset = 37.f + fabsf(noise[6]) * 0.8f;

    int dWritePos = delayWritePos_;
    float dReadPos = delayReadPos_;
    float l1 = low1_, b1 = band1_, l2 = low2_, b2 = band2_;
    float l3 = low3_, b3 = band3_, l4 = low4_, b4 = band4_;
    float l5 = low5_, l6 = low6_;
    float ly1 = _ly1_, lx1 = _lx1_, ly2 = _ly2_, lx2 = _lx2_;
    float h4_x1 = hb4_x1_, h4_y1 = hb4_y1_, h4_x2 = hb4_x2_, h4_y2 = hb4_y2_;
    float h1_x1 = hb1_x1_, h1_y1 = hb1_y1_, h1_x2 = hb1_x2_, h1_y2 = hb1_y2_;
    float hp1_x1 = hp_in_x1_, hp1_y1 = hp_in_y1_, hp1_y0 = hp_in_y0_;
    float hp2_x1 = hp_in2_x1_, hp2_y1 = hp_in2_y1_, hp2_y0 = hp_in2_y0_;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        l3 += f * b3;
        b3 += f * (*sp - l3 - b3);

        l4 += f * b4;
        b4 += f * (*(sp + 1) - l4 - b4);

        // feedback_
        float feedL = l5 * currentFeedback;
        float feedR = l6 * currentFeedback;

        // L
        ly1 = apcoef2 * (ly1 + feedL) - lx1; // allpass
        lx1 = feedL;

        h4_y1 = apcoef4 * (h4_y1 + ly1) - h4_x1; // allpass
        h4_x1 = ly1;

        // R
        ly2 = apcoef2 * (ly2 + feedR) - lx2; // allpass
        lx2 = feedR;

        h4_y2 = apcoef4 * (h4_y2 + ly2) - h4_x2; // allpass
        h4_x2 = ly2;

        // audio in hp
        float hp_in_x0 = tanh4(l3 + l3 - h4_y1);
        hp1_y0 = _in3_a0 * (hp_in_x0 - hp1_x1) + _in3_b1 * hp1_y1;
        hp1_y1 = hp1_y0;
        hp1_x1 = hp_in_x0;

        float hp_in2_x0 = tanh4(l4 + l4 - h4_y2);
        hp2_y0 = _in3_a0 * (hp_in2_x0 - hp2_x1) + _in3_b1 * hp2_y1;
        hp2_y1 = hp2_y0;
        hp2_x1 = hp_in2_x0;

        dWritePos = (dWritePos + 1) & delayBufStereoSizeM1;
        delayBuf[dWritePos] = hp1_y0;
        delayBuf[dWritePos + delayBufStereoSize] = hp2_y0;

        dReadPos = modulo2((float)dWritePos - currentDelaySize1, delayBufStereoSize);
        delayReadPos90 = modulo2(dReadPos - driftOffset, delayBufStereoSize);

        l5 = delayInterpolationHermite(dReadPos, delayBuf, delayBufStereoSizeM1, 0);
        l6 = delayInterpolationHermite(delayReadPos90, delayBuf, delayBufStereoSizeM1, delayBufStereoSize);

        l1 += f2 * b1;
        b1 += f2 * (l5 - l1 - b1);

        l2 += f2 * b2;
        b2 += f2 * (l6 - l2 - b2);

        // notch L
        h1_x1 += fnotch * h1_y1;
        float high7 = l1 - h1_x1 - h1_y1;
        h1_y1 += fnotch * high7;
        float notchL = (high7 + h1_x1);

        // notch R
        h1_x2 += fnotch * h1_y2;
        float high8 = l2 - h1_x2 - h1_y2;
        h1_y2 += fnotch * high8;
        float notchR = (high8 + h1_x2);

        *sp = *sp * dry + notchL * wet;
        sp++;
        *sp = *sp * dry + notchR * wet;
        sp++;

        currentDelaySize1 += delaySizeInc1;
        currentFeedback += feedbackInc;
    }

    delayWritePos_ = dWritePos;
    delayReadPos_ = dReadPos;
    low1_ = l1;
    band1_ = b1;
    low2_ = l2;
    band2_ = b2;
    low3_ = l3;
    band3_ = b3;
    low4_ = l4;
    band4_ = b4;
    low5_ = l5;
    low6_ = l6;
    _ly1_ = ly1;
    _lx1_ = lx1;
    _ly2_ = ly2;
    _lx2_ = lx2;
    hb4_x1_ = h4_x1;
    hb4_y1_ = h4_y1;
    hb4_x2_ = h4_x2;
    hb4_y2_ = h4_y2;
    hb1_x1_ = h1_x1;
    hb1_y1_ = h1_y1;
    hb1_x2_ = h1_x2;
    hb1_y2_ = h1_y2;
    hp_in_x1_ = hp1_x1;
    hp_in_y1_ = hp1_y1;
    hp_in_y0_ = hp1_y0;
    hp_in2_x1_ = hp2_x1;
    hp_in2_y1_ = hp2_y1;
    hp_in2_y0_ = hp2_y0;
}

__attribute__((noinline)) void Timbre::fx2Chorus(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                 float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 0.5f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    param1S_ = 0.02f * this->sharedParams().effect2.param1 + .98f * param1S_;
    float speed = clamp(param1S_ * param1S_ + matrixFilterFrequency, 0, 1);
    float speedMod = speed * 4;

    param2S_ = 0.01f * (this->sharedParams().effect2.param2) + .99f * param2S_;
    float width = param2S_ * 0.6f;

    float lfo1Inc = clamp(hb8_y1_ * 0.0007f * speedMod, -1.f, 1.f);
    hb8_x1_ += lfo1Inc;
    if (hb8_x1_ >= 1) {
        hb8_x1_ = 1;
        hb8_y1_ = -1;
    }
    if (hb8_x1_ <= 0) {
        hb8_x1_ = 0;
        hb8_y1_ = 1;
    }

    float lfo2Inc = clamp(hb8_y2_ * 0.008f * speedMod, -1.f, 1.f);
    hb8_x2_ += lfo2Inc;
    if (hb8_x2_ >= 1) {
        hb8_x2_ = 1;
        hb8_y2_ = -1;
    }
    if (hb8_x2_ <= 0) {
        hb8_x2_ = 0;
        hb8_y2_ = 1;
    }

    float lfo = (hb8_x1_ * 0.3f + sigmoidPos(hb8_x2_) * width) * 0.5f;

    float fxParamTmp = (lfo + matrixFilterParam2);
    delayReadFrac_ = (fxParamTmp + 99 * delayReadFrac_) * 0.01f; // smooth change

    float readPos1 = foldAbs(delayReadFrac_);
    float readPos2 = foldAbs(readPos1 + 0.3333f);
    float readPos3 = foldAbs(readPos1 + 0.6666f);

    float currentDelaySize1 = clamp(delaySize1_, 2.0f, (float)(delayBufferSize - 3));
    float currentDelaySize2 = clamp(delaySize2_, 2.0f, (float)(delayBufferSize - 3));
    float currentDelaySize3 = clamp(delaySize3_, 2.0f, (float)(delayBufferSize - 3));
    float chorusSize = delayBufStereoSize; // 21ms
    delaySize1_ = clamp(1 + chorusSize * readPos1, 2.0f, (float)(delayBufferSize - 3));
    delaySize2_ = clamp(1 + chorusSize * readPos2, 2.0f, (float)(delayBufferSize - 3));
    delaySize3_ = clamp(1 + chorusSize * readPos3, 2.0f, (float)(delayBufferSize - 3));
    float delaySizeInc1 = (delaySize1_ - currentDelaySize1) * INV_BLOCK_SIZE;
    float delaySizeInc2 = (delaySize2_ - currentDelaySize2) * INV_BLOCK_SIZE;
    float delaySizeInc3 = (delaySize3_ - currentDelaySize3) * INV_BLOCK_SIZE;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ delayBuf = delayBuffer_;

    float delReadPos1, delReadPos2, delReadPos3, monoIn;
    float delayOut1 = 0, delayOut2 = 0, delayOut3 = 0;

    const float f = 0.8f;
    const float f2 = 0.87f;

    // hi pass params
    float filterB2 = 0.25f;
    float filterB = (filterB2 * filterB2 * 0.5f);

    float _in_b1 = (1 - filterB);
    float _in_a0 = (1 + _in_b1 * _in_b1 * _in_b1) * 0.5f;

    int dWritePos = delayWritePos_;
    float l1 = low1_, b1 = band1_, l2 = low2_, b2 = band2_;
    float l3 = low3_, b3 = band3_, l5 = low5_, b5 = band5_, l6 = low6_, b6 = band6_;
    float hp1_x1 = hp_in_x1_, hp1_y1 = hp_in_y1_, hp1_y0 = hp_in_y0_;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        monoIn = (*sp + *(sp + 1)) * 0.5f;

        // input lp
        l3 += f * b3;
        b3 += f * (monoIn - l3 - b3);

        // audio in hp
        float hp_in_x0 = l3;
        hp1_y0 = _in_a0 * (hp_in_x0 - hp1_x1) + _in_b1 * hp1_y1;
        hp1_y1 = hp1_y0;
        hp1_x1 = hp_in_x0;

        float bass = l3 - hp1_y0;

        dWritePos = (dWritePos + 1) & delayBufferSizeM1;
        delayBuf[dWritePos] = hp1_y0;

        float dWritePosF = (float)dWritePos;

        delReadPos1 = modulo2(dWritePosF - currentDelaySize1, delayBufferSize);
        delayOut1 = delayInterpolationHermite(delReadPos1, delayBuf, delayBufferSizeM1, 0);

        delReadPos2 = modulo2(dWritePosF - currentDelaySize2, delayBufferSize);
        delayOut2 = delayInterpolationHermite(delReadPos2, delayBuf, delayBufferSizeM1, 0);

        delReadPos3 = modulo2(dWritePosF - currentDelaySize3, delayBufferSize);
        delayOut3 = delayInterpolationHermite(delReadPos3, delayBuf, delayBufferSizeM1, 0);

        float delaySumOut = bass + (delayOut1 - delayOut3 + delayOut2);
        float delaySumOut2 = bass + (delayOut3 - delayOut1 + delayOut2);

        l1 += f2 * b1;
        b1 += f2 * (delaySumOut - l1 - b1);

        l2 += f2 * b2;
        b2 += f2 * (l1 - l2 - b2);

        l5 += f2 * b5;
        b5 += f2 * (delaySumOut2 - l5 - b5);

        l6 += f2 * b6;
        b6 += f2 * (l5 - l6 - b6);

        *sp = *sp * dry + l2 * wetL;
        sp++;
        *sp = *sp * dry + l6 * wetR;
        sp++;

        currentDelaySize1 += delaySizeInc1;
        currentDelaySize2 += delaySizeInc2;
        currentDelaySize3 += delaySizeInc3;
    }

    delayWritePos_ = dWritePos;
    delayWritePosF_ = (float)dWritePos;
    low1_ = l1;
    band1_ = b1;
    low2_ = l2;
    band2_ = b2;
    low3_ = l3;
    band3_ = b3;
    low5_ = l5;
    band5_ = b5;
    low6_ = l6;
    band6_ = b6;
    hp_in_x1_ = hp1_x1;
    hp_in_y1_ = hp1_y1;
    hp_in_y0_ = hp1_y0;
}

__attribute__((noinline)) void Timbre::fx2Dimension(float matrixFilterFrequency, float matrixFilterParam2, float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255];
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float param1 = this->sharedParams().effect2.param1;
    float speed = clamp(param1 * param1 * 0.5f, 0, 1);

    float lfo1Inc = clamp(hb8_y1_ * speed * 0.01f, -1, 1);
    hb8_x1_ += lfo1Inc;
    if (hb8_x1_ >= 1) {
        hb8_x1_ = 1;
        hb8_y1_ = -1;
    }
    if (hb8_x1_ <= 0) {
        hb8_x1_ = 0;
        hb8_y1_ = 1;
    }

    float lfo2Inc = clamp(hb8_y2_ * speed * 0.0097f, -1, 1); // ~3% plus lent
    hb8_x2_ += lfo2Inc;
    if (hb8_x2_ >= 1) {
        hb8_x2_ = 1;
        hb8_y2_ = -1;
    }
    if (hb8_x2_ <= 0) {
        hb8_x2_ = 0;
        hb8_y2_ = 1;
    }

    float lfo = hb8_x1_ * 0.5f;
    float lfo2 = hb8_x2_ * 0.5f;

    param1S_ = 0.02f * matrixFilterFrequency + .98f * param1S_;

    matrixFilterFrequency *= 0.5f;

    float fxParamTmp = sigmoidPos(foldAbs(0.25f + (0.125f * ((lfo + param1S_)))));
    delayReadFrac_ = (fxParamTmp + 99 * delayReadFrac_) * 0.01f; // smooth change
    float fxParamTmp2 = sigmoidPos(foldAbs(0.25f + (0.125f * (lfo2 + param1S_))));
    delayReadFrac2_ = (fxParamTmp2 + 99 * delayReadFrac2_) * 0.01f; // smooth change

    float currentDelaySize1 = clamp(delaySize1_, 2.0f, (float)(delayBufStereoSize - 3));
    float currentDelaySize2 = clamp(delaySize2_, 2.0f, (float)(delayBufStereoSize - 3));
    delaySize1_ = clamp(1 + delayBufStereoSize * delayReadFrac_, 2.0f, (float)(delayBufStereoSize - 3));
    delaySize2_ = clamp(1 + delayBufStereoSize * delayReadFrac2_, 2.0f, (float)(delayBufStereoSize - 3));
    float delaySizeInc1 = (delaySize1_ - currentDelaySize1) * INV_BLOCK_SIZE;
    float delaySizeInc2 = (delaySize2_ - currentDelaySize2) * INV_BLOCK_SIZE;

    param2S_ = 0.1f * (this->sharedParams().effect2.param2 + matrixFilterParam2) + .9f * param2S_;

    const float f = 0.82f;
    const float f2 = 0.2f;

    // hi pass params
    float filterB = (f2 * f2 * 0.5f);

    float _in2_b1 = (1 - filterB);
    float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ delayBuf = delayBuffer_;

    float delReadPos, delReadPos2;
    float delayOut1, delayOut3 = 0;

    // mid / side
    float outL, outR, mid, side;
    float width = clamp(param2S_ * 2, 0, 2);
    float tmp = 1.f / (1.f + width);
    float coef_M = tmp * wet;
    float coef_S = width * tmp * wet;

    int dWritePos = delayWritePos_;
    float l1 = low1_, b1 = band1_, l2 = low2_, b2 = band2_;
    float hp1_x1 = hp_in_x1_, hp1_y1 = hp_in_y1_, hp1_y0 = hp_in_y0_;
    float hp2_x1 = hp_in2_x1_, hp2_y1 = hp_in2_y1_, hp2_y0 = hp_in2_y0_;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        l1 += f * b1;
        b1 += f * (*sp - l1 - b1);

        l2 += f * b2;
        b2 += f * (*(sp + 1) - l2 - b2);

        // audio in hp
        float hp_in_x0 = l1;
        hp1_y0 = _in2_a0 * (hp_in_x0 - hp1_x1) + _in2_b1 * hp1_y1;
        hp1_y1 = hp1_y0;
        hp1_x1 = hp_in_x0;

        float hp_in2_x0 = l2;
        hp2_y0 = _in2_a0 * (hp_in2_x0 - hp2_x1) + _in2_b1 * hp2_y1;
        hp2_y1 = hp2_y0;
        hp2_x1 = hp_in2_x0;

        float lpc1 = (l1 - hp1_y0);
        float lpc2 = (l2 - hp2_y0);

        dWritePos = (dWritePos + 1) & delayBufStereoSizeM1;

        delayBuf[dWritePos] = hp1_y0 + hp2_y0 * 0.05f; // cross feed for more dimension
        delayBuf[dWritePos + delayBufStereoSize] = hp2_y0 + hp1_y0 * 0.05f;

        float dWritePosF = (float)dWritePos;

        delReadPos = modulo2(dWritePosF - currentDelaySize1, delayBufStereoSize);
        delReadPos2 = modulo2(dWritePosF - currentDelaySize2, delayBufStereoSize);

        delayOut1 = delayInterpolationHermite(delReadPos, delayBuf, delayBufStereoSizeM1, 0);
        delayOut3 = delayInterpolationHermite(delReadPos2, delayBuf, delayBufStereoSizeM1, delayBufStereoSize);

        outL = lpc1 + delayOut3;
        outR = lpc2 + delayOut1;

        mid = coef_M * (outL + outR);
        side = coef_S * (outL - outR);

        *sp = (*sp) * dry + (mid + side);
        sp++;
        *sp = (*sp) * dry + (mid - side);
        sp++;

        currentDelaySize1 += delaySizeInc1;
        currentDelaySize2 += delaySizeInc2;
    }

    delayWritePos_ = dWritePos;
    delayWritePosF_ = (float)dWritePos;
    low1_ = l1;
    band1_ = b1;
    low2_ = l2;
    band2_ = b2;
    hp_in_x1_ = hp1_x1;
    hp_in_y1_ = hp1_y1;
    hp_in_y0_ = hp1_y0;
    hp_in2_x1_ = hp2_x1;
    hp_in2_y1_ = hp2_y1;
    hp_in2_y0_ = hp2_y0;
}

__attribute__((noinline)) void Timbre::fx2Doubler(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                  float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 0.75f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    param1S_ = 0.1f * this->sharedParams().effect2.param1 + .9f * param1S_;
    param2S_ = 0.1f * (this->sharedParams().effect2.param2 + matrixFilterParam2) + .9f * param2S_;

    float currentShift = shift_;
    shift_ = clamp(fabsf(param1S_ * 2 + matrixFilterFrequency * 0.5f), 0, 16);
    float shiftInc = (shift_ - currentShift) * INV_BLOCK_SIZE;

    float quadrant = fabsf((clamp(shift_, 0, 2) - 1)); // 2 quadrant for up & down shift_
    float feedbackZeroZone = clamp(0.82f + quadrant * 40, 0, 0.999f);

    float feed = clamp(param2S_ * feedbackZeroZone, 0, 1);
    float currentFeedback = feedback_;
    feedback_ = clamp(feed, -1, 1) * 0.44f;
    float feedbackInc = (feedback_ - currentFeedback) * INV_BLOCK_SIZE;

    float lpZeroZone = clamp(quadrant * 50, 0, 1);

    float filterA2 = 0.5f + lpZeroZone * 0.2f;
    float filterA = (filterA2 * filterA2 * 0.5f);
    float _in_lp_b = 1 - filterA;
    float _in_lp_a = 1 - _in_lp_b;

    const float f = 0.7f;
    const float f2 = 0.38f;
    const float f3 = 0.72f;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ delayBuf = delayBuffer_;

    float delayReadPos180, level1, level2;
    float delayOut1, delayOut2;

    // hi pass params
    float hpZeroZone = 1 - clamp(sqrt3(quadrant) * 4.5f, 0, 1);

    float filterB2 = 0.1f + (0.32f - (param1S_ * 0.2f) + hpZeroZone * 0.3f) * clamp(param2S_ * 1.2f, 0, 1);
    float filterB = (filterB2 * filterB2 * 0.5f);

    float _in2_b1 = (1 - filterB);
    float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;

    int dWritePos = delayWritePos_;
    float dReadPos = delayReadPos_;
    float fbInput = feedbackInput_;
    float h1_x1 = hb1_x1_, h1_x2 = hb1_x2_;
    float hp1_x1 = hp_in_x1_, hp1_y1 = hp_in_y1_, hp1_y0 = hp_in_y0_;
    float l1 = low1_, b1 = band1_, l2 = low2_, b2 = band2_;
    float l3 = low3_, b3 = band3_, l4 = low4_, b4 = band4_;
    float l5 = low5_, b5 = band5_, l6 = low6_, b6 = band6_;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        float monoIn = (*sp + *(sp + 1)) * 0.5f;

        // feedback_ lp
        h1_x1 = _in_lp_a * fbInput + h1_x1 * _in_lp_b;
        h1_x2 = _in_lp_b * h1_x1 + h1_x2 * _in_lp_b;

        // delay in hp
        float hp_in_x0 = clamp(tanh4((monoIn + h1_x2 * currentFeedback)), -1, 1);
        hp1_y0 = _in2_a0 * (hp_in_x0 - hp1_x1) + _in2_b1 * hp1_y1;
        hp1_y1 = hp1_y0;
        hp1_x1 = hp_in_x0;

        dWritePos = (dWritePos + 1) & delayBufferSizeM1;

        l1 += f * b1;
        b1 += f * (hp1_y0 - l1 - b1);
        l2 += f * b2;
        b2 += f * (l1 - l2 - b2);

        delayBuf[dWritePos] = l2;

        dReadPos = modulo(dReadPos + currentShift, delayBufferSize);
        delayReadPos180 = modulo2(dReadPos - delayBufferSize180, delayBufferSize);

        delayOut1 = delayInterpolation(dReadPos, delayBuf, delayBufferSizeM1);
        delayOut2 = delayInterpolation(delayReadPos180, delayBuf, delayBufferSizeM1);

        float dWritePosF = (float)dWritePos;
        float rwp1 = modulo2(dWritePosF - dReadPos, delayBufferSize);
        float rwp3 = modulo2(dWritePosF - delayReadPos180, delayBufferSize);

        level1 = hann(rwp1 * delayBufferSizeInv);
        level2 = hann(rwp3 * delayBufferSizeInv);

        float out1 = delayOut1 * (level1);
        float out2 = delayOut2 * (level2);

        float delaySumOut = out1 + out2;
        float delaySumOut2 = out1 - out2;

        fbInput = delaySumOut;

        l5 += f3 * b5;
        b5 += f3 * (delaySumOut - l5 - b5);

        l6 += f3 * b6;
        b6 += f3 * (delaySumOut2 - l6 - b6);

        // bass boost
        l3 += f2 * b3;
        b3 += f2 * (l5 - l3 - b3);
        l4 += f2 * b4;
        b4 += f2 * (l6 - l4 - b4);

        *sp = (*sp * dry + ((l3 + l5) * wetL));
        sp++;
        *sp = (*sp * dry + ((l4 + l6) * wetR));
        sp++;

        currentFeedback += feedbackInc;
        currentShift += shiftInc;
    }

    delayWritePos_ = dWritePos;
    delayWritePosF_ = (float)dWritePos;
    delayReadPos_ = dReadPos;
    feedbackInput_ = fbInput;
    hb1_x1_ = h1_x1;
    hb1_x2_ = h1_x2;
    hp_in_x1_ = hp1_x1;
    hp_in_y1_ = hp1_y1;
    hp_in_y0_ = hp1_y0;
    low1_ = l1;
    band1_ = b1;
    low2_ = l2;
    band2_ = b2;
    low3_ = l3;
    band3_ = b3;
    low4_ = l4;
    band4_ = b4;
    low5_ = l5;
    band5_ = b5;
    low6_ = l6;
    band6_ = b6;
}

__attribute__((noinline)) void Timbre::fx2Tripler(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                  float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 0.75f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    param1S_ = 0.005f * this->sharedParams().effect2.param1 + .995f * param1S_;
    param2S_ = 0.005f * this->sharedParams().effect2.param2 + .995f * param2S_;

    float currentShift = shift_;
    shift_ = clamp(fabsf(param1S_ * 2 + matrixFilterFrequency * 0.5f), 0, 16);
    float shiftInc = (shift_ - currentShift) * INV_BLOCK_SIZE;

    float currentShift2 = shift2_;
    shift2_ = clamp(fabsf(param2S_ * 2 + matrixFilterParam2 * 0.5f), 0, 16);
    float shiftInc2 = (shift2_ - currentShift2) * INV_BLOCK_SIZE;

    const float f = 0.75f;
    const float f2 = 0.72f;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ delayBuf = delayBuffer_;

    float level1, level2, level3, level4;
    float delayOut1 = 0, delayOut2 = 0, delayOut3 = 0, delayOut4 = 0;
    float delayReadPos180;

    int dWritePos = delayWritePos_;
    float dReadPos = delayReadPos_;
    float dReadPos2 = delayReadPos2_;
    float l1 = low1_, b1 = band1_, l3 = low3_, b3 = band3_;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        float monoIn = (*sp + *(sp + 1)) * 0.5f;

        dWritePos = (dWritePos + 1) & delayBufferSizeM1;

        l1 += f * b1;
        b1 += f * (monoIn - l1 - b1);

        delayBuf[dWritePos] = l1;

        //--------------- shifter 1

        dReadPos = modulo(dReadPos + currentShift, delayBufferSize);
        delayReadPos180 = modulo(dReadPos + delayBufferSize180, delayBufferSize);

        delayOut1 = delayInterpolation(dReadPos, delayBuf, delayBufferSizeM1);
        delayOut2 = delayInterpolation(delayReadPos180, delayBuf, delayBufferSizeM1);

        float dWritePosF = (float)dWritePos;
        float rwp1 = modulo2(dWritePosF - dReadPos, delayBufferSize);
        float rwp2 = modulo2(dWritePosF - delayReadPos180, delayBufferSize);

        level1 = hann(rwp1 * delayBufferSizeInv);
        level2 = hann(rwp2 * delayBufferSizeInv);

        float out1 = delayOut1 * (level1);
        float out2 = delayOut2 * (level2);

        float delaySumOut = out1 + out2;

        //--------------- shifter 2

        dReadPos2 = modulo(dReadPos2 + currentShift2, delayBufferSize);
        delayReadPos180 = modulo(dReadPos2 + delayBufferSize90, delayBufferSize);

        delayOut3 = delayInterpolation(dReadPos2, delayBuf, delayBufferSizeM1);
        delayOut4 = delayInterpolation(delayReadPos180, delayBuf, delayBufferSizeM1);

        float rwp3 = modulo2(dWritePosF - dReadPos2, delayBufferSize);
        float rwp4 = modulo2(dWritePosF - delayReadPos180, delayBufferSize);

        level3 = hann(rwp3 * delayBufferSizeInv);
        level4 = hann(rwp4 * delayBufferSizeInv);

        float out3 = delayOut3 * (level3);
        float out4 = delayOut4 * (level4);

        delaySumOut -= out3 + out4;

        // lp output
        l3 += f2 * b3;
        b3 += f2 * ((delaySumOut)-l3 - b3);

        *sp = *sp * dry + l3 * wetL;
        sp++;
        *sp = *sp * dry + l3 * wetR;
        sp++;

        currentShift += shiftInc;
        currentShift2 += shiftInc2;
    }

    delayWritePos_ = dWritePos;
    delayReadPos_ = dReadPos;
    delayReadPos2_ = dReadPos2;
    low1_ = l1;
    band1_ = b1;
    low3_ = l3;
    band3_ = b3;
}

__attribute__((noinline)) void Timbre::fx2Bode(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                               float gainTmp) {
    // dry wet
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 0.5f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    float param1 = this->sharedParams().effect2.param1;

    // mix between freq shift_ + and - :
    int param255 = 0;
    if (param1 <= 0.4f) {
        param255 = 0;
    } else if (param1 >= 0.6f) {
        param255 = 255;
    } else {
        param255 = 255 * (5.f * (param1 - 0.4f));
    }
    float shiftMinus = panTable[255 - param255];
    float shiftPlus = panTable[param255];

    // shift_ val
    param1S_ = 0.1f * param1 + 0.9f * param1S_;
    float quadrant = fabsf(param1S_ - 0.5f); // 2 quadrant for up & down shift_
    float quadrantSq = sqrt3(quadrant);

    // shift_ increment :
    shift_ = clamp(shift_, 0, 0.1f);
    float currentShift = shift_;
    float shiftval = clamp(fabsf(quadrant * 1.34f + matrixFilterFrequency * 0.5f), 0, 0.9999f);
    shiftval *= shiftval * 0.05f;
    shift_ = shift_ * 0.96f + 0.04f * shiftval;
    float shiftInc = (shift_ - currentShift) * INV_BLOCK_SIZE;

    // feedback_
    float feedbackZeroZone = clamp(0.6f + (quadrant * 6), 0, 1);

    float feedbackParam = clamp(this->sharedParams().effect2.param2 + matrixFilterParam2, 0, 1) * 0.8f;

    feedback_ = clamp(feedback_, 0, 1);

    float currentFeedback = feedback_;
    feedback_ = feedbackParam * feedbackZeroZone;
    float feedbackInc = (feedback_ - currentFeedback) * INV_BLOCK_SIZE;

    float currentDelaySize1 = clamp(delaySize1_, 0, delayBufferSize);
    delaySize1_ = clamp(430 + 70 * quadrant, 0, delayBufferSize);
    float delaySizeInc1 = (delaySize1_ - currentDelaySize1) * INV_BLOCK_SIZE;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;

    float iirFilter1, iirFilter2, iirFilter3, iirFilter4, iirFilter5, iirFilter6, iirFilter7, iirFilter8;
    float cos, sin;
    float phase2;
    float shifterIn;
    float shifterOutR = 0, shifterOutI = 0, shifterOut = 0, shifterOut2 = 0;

    const float f = 0.73f;
    const float f2 = 0.75f;
    const float f3 = 0.25f;

    // hi pass params
    float filterB2 = 0.2f;
    float filterB = (filterB2 * filterB2 * 0.5f);

    float _in2_b1 = (1 - filterB);
    float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;

    float hpZeroZone = clamp((1.3f - param1S_) * sqrt3(param1 * quadrantSq * 16), 0, 1);
    filterB2 = 0.32f - 0.3f * hpZeroZone * (1 - (feedbackZeroZone * fabsf(feedbackParam) * 0.125f));
    filterB = (filterB2 * filterB2 * 0.5f);

    float _in3_b1 = (1 - filterB);
    float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;

    float delayOut1;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        float monoIn = (*sp + *(sp + 1)) * 0.5f;

        // monoIn lp
        low1_ += f * band1_;
        band1_ += f * (monoIn - low1_ - band1_);

        // feedback_ hp
        float feedbackIn = tanh4(feedbackInput_ * 1.35f) * currentFeedback;

        _ly1_ = apcoef3 * (_ly1_ + feedbackIn) - _lx1_; // allpass
        _lx1_ = feedbackIn;

        _ly2_ = apcoef4 * (_ly2_ + _ly1_) - _lx2_; // allpass
        _lx2_ = _ly1_;

        float hp_in_x0 = (feedbackIn + _ly2_) * 0.5f;
        hp_in_y0_ = _in3_a0 * (hp_in_x0 - hp_in_x1_) + _in3_b1 * hp_in_y1_;
        hp_in_y1_ = hp_in_y0_;
        hp_in_x1_ = hp_in_x0;

        // feedback_ lp
        low2_ += f2 * band2_;
        band2_ += f2 * (hp_in_y0_ - low2_ - band2_);
        low3_ += f2 * band3_;
        band3_ += f2 * (low2_ - low3_ - band3_);

        delayWritePos_ = (delayWritePos_ + 1) & delayBufferSizeM1;
        delayBuf[delayWritePos_] = low3_;

        delayReadPos_ = modulo2(delayWritePos_ - currentDelaySize1, delayBufferSize);
        delayOut1 = delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);

        shifterIn = clamp(low1_ - delayOut1, -1.f, 1.f);

        float hp_in2_x0 = shifterIn;
        hp_in2_y0_ = _in2_a0 * (hp_in2_x0 - hp_in2_x1_) + _in2_b1 * hp_in2_y1_;
        hp_in2_y1_ = hp_in2_y0_;
        hp_in2_x1_ = hp_in2_x0;

        low4_ += f * band4_;
        band4_ += f * (hp_in2_y0_ - low4_ - band4_);

        // Frequency shifter

        //     Phase reference path
        iirFilter1 = iirFilter(low4_, 0.48645677879491144857f, &hb1_x1_, &hb1_x2_, &hb1_y1_, &hb1_y2_);
        iirFilter2 = iirFilter(iirFilter1, 0.88068726735639790704f, &hb2_x1_, &hb2_x2_, &hb2_y1_, &hb2_y2_);
        iirFilter3 = iirFilter(iirFilter2, 0.97790456293916316888f, &hb3_x1_, &hb3_x2_, &hb3_y1_, &hb3_y2_);
        iirFilter4 = iirFilter(iirFilter3, 0.99767037906310385154f, &hb4_x1_, &hb4_x2_, &hb4_y1_, &hb4_y2_);

        //     +90 deg path
        iirFilter5 = iirFilter(low4_, 0.16507919004304125177f, &hb5_x1_, &hb5_x2_, &hb5_y1_, &hb5_y2_);
        iirFilter6 = iirFilter(iirFilter5, 0.73969068299070206418f, &hb6_x1_, &hb6_x2_, &hb6_y1_, &hb6_y2_);
        iirFilter7 = iirFilter(iirFilter6, 0.94788883423814862539f, &hb7_x1_, &hb7_x2_, &hb7_y1_, &hb7_y2_);
        iirFilter8 = iirFilter(iirFilter7, 0.99119752093109647628f, &hb8_x1_, &hb8_x2_, &hb8_y1_, &hb8_y2_);

        //     sin
        phase1_ = phase1_ + currentShift;
        if (phase1_ >= 1) {
            phase1_ -= 2.f;
        }
        //     cos = sin( x + 90°)
        phase2 = phase1_ + 0.5f;
        if (phase2 >= 1) {
            phase2 -= 2.f;
        }

        sin = fastSin(phase1_);
        cos = fastSin(phase2);

        shifterOutR = sin * iirFilter4;
        shifterOutI = cos * iirFilter8;
        shifterOut = shifterOutI + shifterOutR;
        shifterOut2 = shifterOutI - shifterOutR;
        float shifterOutMixA = shifterOut2 * shiftPlus;
        float shifterOutMixB = shifterOut * shiftMinus;
        float shifterOutMix = shifterOutMixA + shifterOutMixB;
        float shifterOutMix2 = shifterOutMixA - shifterOutMixB;

        feedbackInput_ = shifterOutMix;

        // bass boost
        low5_ += f3 * band5_;
        band5_ += f3 * (shifterOutMix - low5_ - band5_);

        *sp = (*sp * dry) - tanh4((shifterOutMix + low5_) * 1.5f) * wetL;
        sp++;
        *sp = (*sp * dry) - tanh4((shifterOutMix2 + low5_) * 1.5f) * wetR;
        sp++;

        currentFeedback += feedbackInc;
        currentShift += shiftInc;
        currentDelaySize1 += delaySizeInc1;
    }
}

__attribute__((noinline)) void Timbre::fx2Wide(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                               float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 0.3333f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    param1S_ = 0.02f * fabsf(this->sharedParams().effect2.param1 + matrixFilterFrequency) + .98f * param1S_;
    param2S_ = 0.02f * clamp(this->sharedParams().effect2.param2 + matrixFilterParam2, 0.f, 1.f) + .98f * param2S_;

    float detune = param1S_ * param1S_ * 0.03125f;

    float currentShift = shift_;
    shift_ = clamp(1 + detune, 0, 16);
    float shiftInc = (shift_ - currentShift) * INV_BLOCK_SIZE;

    float currentShift2 = shift2_;
    shift2_ = clamp(1 - detune, 0, 16);
    float shiftInc2 = (shift2_ - currentShift2) * INV_BLOCK_SIZE;

    const int delaySizeInt = 500;

    const float f = 0.72f;
    const float f2 = 0.7f + f * 0.1f;
    const float f3 = 0.32f;

    // hi pass params
    float filterB2 = clamp(param2S_ * param2S_ * 0.93f, 0, 1);
    float filterB = (filterB2 * filterB2 * 0.5f);

    const float _hp_b1 = (1 - filterB);
    const float _hp_a0 = (1 + _hp_b1 * _hp_b1 * _hp_b1) * 0.5f;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;

    float level1, level2, level3, level4;
    float delayReadPos180;
    float delayOut1 = 0, delayOut2 = 0, delayOut3 = 0, delayOut4 = 0;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        float monoIn = (*sp + *(sp + 1)) * 0.5f;

        low1_ += f * band1_;
        band1_ += f * (monoIn - low1_ - band1_);

        delayWritePos_ = (delayWritePos_ + 1) & delayBufStereoSizeM1;
        delayWritePosF_ = (float)delayWritePos_;

        // predelay
        delayBuf[delayBufStereoSize + delayWritePos_] = low1_;
        int wp = (delayWritePos_ - delaySizeInt) & delayBufStereoSizeM1;

        // hp
        float hp_in_x0 = delayBuf[delayBufStereoSize + wp];
        hp_in_y0_ = _hp_a0 * (hp_in_x0 - hp_in_x1_) + _hp_b1 * hp_in_y1_;
        hp_in_y1_ = hp_in_y0_;
        hp_in_x1_ = hp_in_x0;

        delayBuf[delayWritePos_] = hp_in_y0_;

        float hpComplement = hp_in_x0 - hp_in_y0_;

        //--------------- shifter 1

        delayReadPos_ = modulo(delayReadPos_ + currentShift, delayBufStereoSize);
        delayReadPos180 = modulo(delayReadPos_ + delayBufferSize90, delayBufStereoSize);

        delayOut1 = delayInterpolation(delayReadPos_, delayBuf, delayBufStereoSizeM1);
        delayOut2 = delayInterpolation(delayReadPos180, delayBuf, delayBufStereoSizeM1);

        float rwp1 = modulo2(delayWritePosF_ - delayReadPos_, delayBufStereoSize);
        float rwp2 = modulo2(delayWritePosF_ - delayReadPos180, delayBufStereoSize);

        level1 = hann(rwp1 * delayBufStereoSizeInv);
        level2 = hann(rwp2 * delayBufStereoSizeInv);

        float out1 = delayOut1 * (level1);
        float out2 = delayOut2 * (level2);

        //--------------- shifter 2

        delayReadPos2_ = modulo(delayReadPos2_ + currentShift2, delayBufStereoSize);
        delayReadPos180 = modulo(delayReadPos2_ + delayBufferSize90, delayBufStereoSize);

        delayOut3 = delayInterpolation(delayReadPos2_, delayBuf, delayBufStereoSizeM1);
        delayOut4 = delayInterpolation(delayReadPos180, delayBuf, delayBufStereoSizeM1);

        float rwp3 = modulo2(delayWritePosF_ - delayReadPos2_, delayBufStereoSize);
        float rwp4 = modulo2(delayWritePosF_ - delayReadPos180, delayBufStereoSize);

        level3 = hann(rwp3 * delayBufStereoSizeInv);
        level4 = hann(rwp4 * delayBufStereoSizeInv);

        float out3 = delayOut3 * (level3);
        float out4 = delayOut4 * (level4);

        // lp output
        low3_ += f2 * band3_;
        band3_ += f2 * ((hpComplement + out1 + out2) - low3_ - band3_);

        low4_ += f2 * band4_;
        band4_ += f2 * ((hpComplement + out3 + out4) - low4_ - band4_);

        // bass boost
        low5_ += f3 * band5_;
        band5_ += f3 * (low3_ - low5_ - band5_);
        low6_ += f3 * band6_;
        band6_ += f3 * (low4_ - low6_ - band6_);

        *sp = *sp * dry + (low3_ + low3_ + low5_) * wetL;
        sp++;
        *sp = *sp * dry + (low4_ + low4_ + low6_) * wetR;
        sp++;

        currentShift += shiftInc;
        currentShift2 += shiftInc2;
    }
}

__attribute__((noinline)) void Timbre::fx2DelayCrunch(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                      float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 1.2f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    param1S_ = 0.005f * (this->sharedParams().effect2.param1) + .995f * param1S_;
    matrixFilterFrequencyS_ = 0.02f * (matrixFilterFrequency * matrixFilterFrequency) + .98f * matrixFilterFrequencyS_;
    param2S_ = 0.05f * (this->sharedParams().effect2.param2 + matrixFilterParam2) + .95f * param2S_;

    param2S_ = clamp(param2S_, 0, 1.f);
    float param2Square = param2S_ * param2S_;

    feedback_ = param2S_ * 1.2f;

    const float sampleRateDivide = 4;
    const float sampleRateDivideInv = 1 / sampleRateDivide;
    float inputIncCount = 0;

    float currentDelaySize1 = clamp(delaySize1_, 0, delayBufferSize);
    delaySize1_ = 1.f + (delayBufferSize - 16) * clamp(param1S_ + (matrixFilterFrequencyS_ * 0.0625f), 0.f, 1.f);
    float delaySizeInc1 = (delaySize1_ - currentDelaySize1) * sampleRateDivideInv * INV_BLOCK_SIZE;

    // hp input
    const float filterB2 = 0.1f + param2Square * 0.4f;
    const float filterB = (filterB2 * filterB2 * 0.5f);
    const float _in2_b1 = (1 - filterB);
    const float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;

    // hp feedback_
    float filterC2 = 0.1f + param2Square * 0.3f;
    float filterC = (filterC2 * filterC2 * 0.5f);

    float _in3_b1 = (1 - filterC);
    float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;

    const float f = 0.8f + param2Square * 0.198f;
    const float f2 = 0.9f - param2Square * 0.15f;

    const float fnotch = 1.03f;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;

    float delayOut1 = 0;

    for (int k = 0; k < BLOCK_SIZE; k++) {

        if (++inputIncCount >= sampleRateDivide) {
            float monoIn = (*sp + *(sp + 1)) * 0.5f;

            inputIncCount = 0;
            delayWritePos_ = (delayWritePos_ + 1) & delayBufferSizeM1;
            delayWritePosF_ = (float)delayWritePos_;

            // hp
            hb4_x1_ = monoIn;
            hb4_y1_ = _in2_a0 * (hb4_x1_ - hb4_x2_) + _in2_b1 * hb4_y2_;
            hb4_y2_ = hb4_y1_;
            hb4_x2_ = hb4_x1_;

            hb6_x1_ = hb4_y1_;
            hb6_y1_ = _in2_a0 * (hb6_x1_ - hb6_x2_) + _in2_b1 * hb6_y2_;
            hb6_y2_ = hb6_y1_;
            hb6_x2_ = hb6_x1_;

            float hp_in_x0 = hb6_y1_;
            hp_in_y0_ = _in2_a0 * (hp_in_x0 - hp_in_x1_) + _in2_b1 * hp_in_y1_;
            hp_in_y1_ = hp_in_y0_;
            hp_in_x1_ = hp_in_x0;

            // lp & hp feedback_
            low1_ += f * band1_;
            band1_ += f * ((delayOut1 * feedback_) - low1_ - band1_);

            float hp_in2_x0 = low1_;
            hp_in2_y0_ = _in3_a0 * (hp_in2_x0 - hp_in2_x1_) + _in3_b1 * hp_in2_y1_;
            hp_in2_y1_ = hp_in2_y0_;
            hp_in2_x1_ = hp_in2_x0;

            hb5_x1_ = hp_in2_y0_ + hp_in_y0_;
            hb5_y1_ = _in3_a0 * (hb5_x1_ - hb5_x2_) + _in3_b1 * hb5_y2_;
            hb5_y2_ = hb5_y1_;
            hb5_x2_ = hb5_x1_;

            delayBuf[delayWritePos_] = hb5_y1_;
        }

        delayReadPos_ = modulo2(delayWritePosF_ - currentDelaySize1, delayBufferSize);
        delayOut1 = delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
        delayWritePosF_ += sampleRateDivideInv;

        // lp
        low3_ += f2 * band3_;
        band3_ += f2 * (delayOut1 - low3_ - band3_);
        low4_ += f2 * band4_;
        band4_ += f2 * (low3_ - low4_ - band4_);
        low5_ += f2 * band5_;
        band5_ += f2 * (low4_ - low5_ - band5_);
        low6_ += f2 * band6_;
        band6_ += f2 * (low5_ - low6_ - band6_);

        // notch
        hb1_x1_ += fnotch * hb1_y1_;
        float high7 = low6_ - hb1_x1_ - hb1_y1_;
        hb1_y1_ += fnotch * high7;
        float notch = (high7 + hb1_x1_);

        *sp = *sp * dry + notch * wetL;
        sp++;
        *sp = *sp * dry + notch * wetR;
        sp++;

        currentDelaySize1 += delaySizeInc1;
    }
}

__attribute__((noinline)) void Timbre::fx2PingPong(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                   float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 1.25f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    param1S_ = 0.005f * (this->sharedParams().effect2.param1) + .995f * param1S_;
    matrixFilterFrequencyS_ = 0.02f * (matrixFilterFrequency * matrixFilterFrequency) + .98f * matrixFilterFrequencyS_;

    // Interpolation linéaire de param2S_ sur le bloc pour éviter l'aliasing
    // lors d'une modulation rapide (enveloppe). La valeur cible est calculée
    // une fois, puis param2S_ est incrémenté sample par sample dans la boucle.
    float param2Target = clamp(0.05f * (this->sharedParams().effect2.param2 + matrixFilterParam2) + .95f * param2S_, 0.f, 1.f);
    float param2SInc = (param2Target - param2S_) * INV_BLOCK_SIZE;

    // Calcul des paramètres du filtre à partir de la valeur courante de param2S_
    // (sera mis à jour chaque sample dans la boucle)
    float param2Square = param2S_ * param2S_;

    const float sampleRateDivide = 4;
    const float sampleRateDivideInv = 1 / sampleRateDivide;
    float inputIncCount = 0;

    float currentDelaySize1 = clamp(delaySize1_, 0, delayBufferSizeF);
    delaySize1_ = 1.f + (delayBufferSize - 16) * clamp(param1S_ + (matrixFilterFrequencyS_ * 0.0625f), 0.f, 1.f);
    float delaySizeInc1 = (delaySize1_ - currentDelaySize1) * sampleRateDivideInv * INV_BLOCK_SIZE;

    float currentDelaySize2 = clamp(delaySize2_, 0, delayBufferSizeF);
    delaySize2_ = delaySize1_ * 0.5f;
    float delaySizeInc2 = (delaySize2_ - currentDelaySize2) * sampleRateDivideInv * INV_BLOCK_SIZE;

    // hp 1 — coefficients calculés avant la boucle, stables sur le bloc
    float filterB2 = 0.1f + param2Square * 0.43f;
    float filterB = (filterB2 * filterB2 * 0.5f);
    float _in2_b1 = (1 - filterB);
    float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;

    // hp 2
    float filterC2 = 0.1f + param2Square * 0.33f;
    float filterC = (filterC2 * filterC2 * 0.5f);

    float _in3_b1 = (1 - filterC);
    float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;

    float f = 0.8f + param2Square * 0.198f;
    float airAttn = (param2Square + param2Square + param1S_) * 0.03f;
    float f2 = 0.85f - airAttn;
    float f3 = f2 - 0.03f;
    const float fnotch = 1.03f;

    float delayOut1, delayOut2;
    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;

    for (int k = 0; k < BLOCK_SIZE; k++) {
        // Lissage sample par sample de param2S_ pour éviter l'aliasing
        param2S_ += param2SInc;
        param2Square = param2S_ * param2S_;

        // f2/f3 servent à 48 kHz : mis à jour à chaque sample (coût négligeable)
        airAttn = (param2Square + param2Square + param1S_) * 0.03f;
        f2 = 0.85f - airAttn;
        f3 = f2 - 0.03f;

        if (++inputIncCount >= sampleRateDivide) {
            // Coefficients utilisés uniquement à SR/4 : calculés ici (4x moins souvent)
            feedback_ = param2S_ * 1.22f;
            filterB2 = 0.1f + param2Square * 0.43f;
            filterB = (filterB2 * filterB2 * 0.5f);
            _in2_b1 = (1 - filterB);
            _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;
            filterC2 = 0.1f + param2Square * 0.33f;
            filterC = (filterC2 * filterC2 * 0.5f);
            _in3_b1 = (1 - filterC);
            _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            f = 0.8f + param2Square * 0.198f;

            float monoIn = (*sp + *(sp + 1)) * 0.5f;

            inputIncCount = 0;
            delayWritePos_ = (delayWritePos_ + 1) & delayBufferSizeM1;
            delayWritePosF_ = (float)delayWritePos_;

            low1_ += f * band1_;
            band1_ += f * ((feedbackInput_ * feedback_) - low1_ - band1_);

            // hp
            hb4_x1_ = monoIn;
            hb4_y1_ = _in2_a0 * (hb4_x1_ - hb4_x2_) + _in2_b1 * hb4_y2_;
            hb4_y2_ = hb4_y1_;
            hb4_x2_ = hb4_x1_;

            hb6_x1_ = hb4_y1_;
            hb6_y1_ = _in2_a0 * (hb6_x1_ - hb6_x2_) + _in2_b1 * hb6_y2_;
            hb6_y2_ = hb6_y1_;
            hb6_x2_ = hb6_x1_;

            float hp_in_x0 = hb6_y1_;
            hp_in_y0_ = _in2_a0 * (hp_in_x0 - hp_in_x1_) + _in2_b1 * hp_in_y1_;
            hp_in_y1_ = hp_in_y0_;
            hp_in_x1_ = hp_in_x0;

            float hp_in2_x0 = low1_;
            hp_in2_y0_ = _in3_a0 * (hp_in2_x0 - hp_in2_x1_) + _in3_b1 * hp_in2_y1_;
            hp_in2_y1_ = hp_in2_y0_;
            hp_in2_x1_ = hp_in2_x0;

            hb5_x1_ = hp_in2_y0_ + hp_in_y0_;
            hb5_y1_ = _in3_a0 * (hb5_x1_ - hb5_x2_) + _in3_b1 * hb5_y2_;
            hb5_y2_ = hb5_y1_;
            hb5_x2_ = hb5_x1_;

            delayBuf[delayWritePos_] = hb5_y1_;
        }

        delayReadPos_ = modulo2(delayWritePosF_ - currentDelaySize1, delayBufferSize);
        delayOut1 = delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
        delayReadPos_ = modulo2(delayWritePosF_ - currentDelaySize2, delayBufferSize);
        delayOut2 = delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);

        feedbackInput_ = delayOut1;

        delayWritePosF_ += sampleRateDivideInv;

        // lp L
        low3_ += f2 * band3_;
        band3_ += f2 * (delayOut1 - low3_ - band3_);
        low4_ += f2 * band4_;
        band4_ += f2 * (low3_ - low4_ - band4_);
        low5_ += f2 * band5_;
        band5_ += f2 * (low4_ - low5_ - band5_);
        low6_ += f2 * band6_;
        band6_ += f2 * (low5_ - low6_ - band6_);

        // lp R
        hb2_x1_ += f3 * hb2_y1_;
        hb2_y1_ += f3 * (delayOut2 - hb2_x1_ - hb2_y1_);
        hb2_x2_ += f3 * hb2_y2_;
        hb2_y2_ += f3 * (hb2_x1_ - hb2_x2_ - hb2_y2_);
        hb3_x1_ += f3 * hb3_y1_;
        hb3_y1_ += f3 * (hb2_x2_ - hb3_x1_ - hb3_y1_);
        hb3_x2_ += f3 * hb3_y2_;
        hb3_y2_ += f3 * (hb3_x1_ - hb3_x2_ - hb3_y2_);

        // notch L
        hb1_x1_ += fnotch * hb1_y1_;
        float high7 = low6_ - hb1_x1_ - hb1_y1_;
        hb1_y1_ += fnotch * high7;
        float notchL = (high7 + hb1_x1_);

        // notch R
        hb1_x2_ += fnotch * hb1_y2_;
        float high8 = hb3_x2_ - hb1_x2_ - hb1_y2_;
        hb1_y2_ += fnotch * high8;
        float notchR = (high8 + hb1_x2_);

        *sp = *sp * dry + notchL * wetL;
        sp++;
        *sp = *sp * dry + notchR * wetR;
        sp++;

        currentDelaySize1 += delaySizeInc1;
        currentDelaySize2 += delaySizeInc2;
    }
}

__attribute__((noinline)) void Timbre::fx2Diffuser(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                   float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255];
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    param1S_ = 0.005f * (this->sharedParams().effect2.param1) + .995f * param1S_;
    matrixFilterFrequencyS_ = 0.01f * (matrixFilterFrequency) + .99f * matrixFilterFrequencyS_;
    param2S_ = 0.05f * (this->sharedParams().effect2.param2 + matrixFilterParam2) + .95f * param2S_;

    param1S_ = clamp(param1S_, 0, 1);
    param2S_ = clamp(param2S_, 0, 1);

    feedback_ = param2S_ * 0.4999f;

    const float sampleRateDivide = 4;
    const float sampleRateDivideInv = 1 / sampleRateDivide;
    float inputIncCount = 0;

    float currentDelaySize1 = clamp(delaySize1_, 0, delayBufStereoSize);
    delaySize1_ = 1.f + (delayBufStereoSize - 120) * clamp(param1S_ + (matrixFilterFrequencyS_ * 0.125f), 0.f, 1.f) * 0.5f;
    float delaySizeInc1 = (delaySize1_ - currentDelaySize1) * sampleRateDivideInv * INV_BLOCK_SIZE;

    float filterB2 = 0.15f + param2S_ * 0.1f;
    float filterB = (filterB2 * filterB2 * 0.5f);

    float _in3_b1 = (1 - filterB);
    float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;

    float f = 0.7f - clamp(feedback_ - (param1S_ * 0.3f), 0, 1) * 0.6f;
    float f2 = 0.75f - param1S_ * 0.1f;
    const float fnotch = 1.03f;

    const float inputCoef1 = 0.7f;
    const float inputCoef2 = 0.625f;

    float diff1Out = 0, diff2Out = 0, diff3Out = 0, diff4Out = 0, diff5Out = 0;

    float sizeParamInpt = 0.2f + param1S_ * 0.2f;

    // Modulation lente et indépendante de chaque allpass — brise les résonances métalliques.
    // shift_/shift2_ : marcheurs aléatoires bornés (± ~2 samples à SR/4), signes alternés
    // pour éviter un pitch-shift_ global uniforme.
    shift_ = shift_ * 0.9985f + noise[4] * 0.04f;
    shift2_ = shift2_ * 0.9990f + noise[5] * 0.03f;
    const float modAmt = 4.5f;

    float inputBuffer1ReadLen = inputBufferLen1_ * sizeParamInpt + shift_ * modAmt;
    float inputBuffer2ReadLen = inputBufferLen2_ * sizeParamInpt - shift2_ * modAmt;
    float inputBuffer3ReadLen = inputBufferLen3_ * sizeParamInpt + shift2_ * modAmt * 0.7f;
    float inputBuffer4ReadLen = inputBufferLen4_ * sizeParamInpt - shift_ * modAmt * 0.5f;
    float inputBuffer5ReadLen = inputBufferLen5_ * sizeParamInpt + (shift_ - shift2_) * modAmt * 0.4f;

    // Offsets de lecture entiers, constants sur le bloc : (int)modulo2(wp - len) == wrap(wp - ceil(len))
    const int readOff1 = (int)ceilf(inputBuffer1ReadLen);
    const int readOff2 = (int)ceilf(inputBuffer2ReadLen);
    const int readOff3 = (int)ceilf(inputBuffer3ReadLen);
    const int readOff4 = (int)ceilf(inputBuffer4ReadLen);
    const int readOff5 = (int)ceilf(inputBuffer5ReadLen);

    const int bufferStart1 = delayBufStereoSize;
    const int bufferStart2 = bufferStart1 + inputBufferLen1_;
    const int bufferStart3 = bufferStart2 + inputBufferLen2_;
    const int bufferStart4 = bufferStart3 + inputBufferLen3_;
    const int bufferStart5 = bufferStart4 + inputBufferLen4_;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;
    float delayOut1, delayOut2;

    for (int k = 0; k < BLOCK_SIZE; k++) {

        if (++inputIncCount >= sampleRateDivide) {
            float monoIn = (*sp + *(sp + 1)) * 0.5f;

            inputIncCount = 0;
            delayWritePos_ = (delayWritePos_ + 1) & delayBufStereoSizeM1;
            delayWritePosF_ = (float)delayWritePos_;
            if (++inputWritePos1_ >= inputBufferLen1_)
                inputWritePos1_ = 0;
            if (++inputWritePos2_ >= inputBufferLen2_)
                inputWritePos2_ = 0;
            if (++inputWritePos3_ >= inputBufferLen3_)
                inputWritePos3_ = 0;
            if (++inputWritePos4_ >= inputBufferLen4_)
                inputWritePos4_ = 0;
            if (++inputWritePos5_ >= inputBufferLen5_)
                inputWritePos5_ = 0;

            // ---- feedback_ lp
            low1_ += f * band1_;
            band1_ += f * ((feedbackInput_ * feedback_) - low1_ - band1_);

            // ---- diffuser 1
            int rp1 = inputWritePos1_ - readOff1;
            if (rp1 < 0)
                rp1 += inputBufferLen1_;
            int inputReadPos1 = bufferStart1 + rp1;
            float in_apSum1 = (monoIn - low1_) + delayBuf[inputReadPos1] * inputCoef1;
            diff1Out = delayBuf[inputReadPos1] - in_apSum1 * inputCoef1;
            delayBuf[bufferStart1 + inputWritePos1_] = in_apSum1;

            // ---- diffuser 2
            int rp2 = inputWritePos2_ - readOff2;
            if (rp2 < 0)
                rp2 += inputBufferLen2_;
            int inputReadPos2 = bufferStart2 + rp2;
            float in_apSum2 = diff1Out + delayBuf[inputReadPos2] * inputCoef2;
            diff2Out = delayBuf[inputReadPos2] - in_apSum2 * inputCoef2;
            delayBuf[bufferStart2 + inputWritePos2_] = in_apSum2;

            // ---- diffuser 3
            int rp3 = inputWritePos3_ - readOff3;
            if (rp3 < 0)
                rp3 += inputBufferLen3_;
            int inputReadPos3 = bufferStart3 + rp3;
            float in_apSum3 = -diff2Out + delayBuf[inputReadPos3] * inputCoef2;
            diff3Out = delayBuf[inputReadPos3] - in_apSum3 * inputCoef2;
            delayBuf[bufferStart3 + inputWritePos3_] = in_apSum3;

            // ---- diffuser 4
            int rp4 = inputWritePos4_ - readOff4;
            if (rp4 < 0)
                rp4 += inputBufferLen4_;
            int inputReadPos4 = bufferStart4 + rp4;
            float in_apSum4 = diff3Out + delayBuf[inputReadPos4] * inputCoef2;
            diff4Out = delayBuf[inputReadPos4] - in_apSum4 * inputCoef2;
            delayBuf[bufferStart4 + inputWritePos4_] = in_apSum4;

            // ---- diffuser 5
            int rp5 = inputWritePos5_ - readOff5;
            if (rp5 < 0)
                rp5 += inputBufferLen5_;
            int inputReadPos5 = bufferStart5 + rp5;
            float in_apSum5 = -diff4Out + delayBuf[inputReadPos5] * inputCoef2;
            diff5Out = delayBuf[inputReadPos5] - in_apSum5 * inputCoef2;
            delayBuf[bufferStart5 + inputWritePos5_] = in_apSum5;

            low1_ += f2 * band1_;
            band1_ += f2 * (diff5Out - low1_ - band1_);

            // hp
            float hp_in_x0 = diff5Out;
            hp_in_y0_ = _in3_a0 * (hp_in_x0 - hp_in_x1_) + _in3_b1 * hp_in_y1_;
            hp_in_y1_ = hp_in_y0_;
            hp_in_x1_ = hp_in_x0;

            float hp_in2_x0 = hp_in_y0_;
            hp_in2_y0_ = _in3_a0 * (hp_in2_x0 - hp_in2_x1_) + _in3_b1 * hp_in2_y1_;
            hp_in2_y1_ = hp_in2_y0_;
            hp_in2_x1_ = hp_in2_x0;

            hb5_x1_ = hp_in2_y0_;
            hb5_y1_ = _in3_a0 * (hb5_x1_ - hb5_x2_) + _in3_b1 * hb5_y2_;
            hb5_y2_ = hb5_y1_;
            hb5_x2_ = hb5_x1_;

            low2_ += f * band2_;
            band2_ += f * (hb5_y1_ - low2_ - band2_);

            delayBuf[delayWritePos_] = low1_ + low2_;
        }

        delayReadPos_ = modulo2(delayWritePosF_ - currentDelaySize1, delayBufStereoSize);
        delayOut1 = delayInterpolation(delayReadPos_, delayBuf, delayBufStereoSizeM1);

        delayReadPos_ = modulo2(delayReadPos_ - 512, delayBufStereoSize);
        delayOut2 = delayInterpolation(delayReadPos_, delayBuf, delayBufStereoSizeM1);

        feedbackInput_ = delayOut1;

        delayWritePosF_ += sampleRateDivideInv;

        // lp L
        low3_ += f2 * band3_;
        band3_ += f2 * (delayOut1 - low3_ - band3_);
        low4_ += f2 * band4_;
        band4_ += f2 * (low3_ - low4_ - band4_);
        low5_ += f2 * band5_;
        band5_ += f2 * (low4_ - low5_ - band5_);
        low6_ += f2 * band6_;
        band6_ += f2 * (low5_ - low6_ - band6_);

        // lp R
        hb2_x1_ += f2 * hb2_y1_;
        hb2_y1_ += f2 * (delayOut2 - hb2_x1_ - hb2_y1_);
        hb2_x2_ += f2 * hb2_y2_;
        hb2_y2_ += f2 * (hb2_x1_ - hb2_x2_ - hb2_y2_);
        hb3_x1_ += f2 * hb3_y1_;
        hb3_y1_ += f2 * (hb2_x2_ - hb3_x1_ - hb3_y1_);
        hb3_x2_ += f2 * hb3_y2_;
        hb3_y2_ += f2 * (hb3_x1_ - hb3_x2_ - hb3_y2_);

        // notch L
        hb1_x1_ += fnotch * hb1_y1_;
        float high7 = low6_ - hb1_x1_ - hb1_y1_;
        hb1_y1_ += fnotch * high7;
        float notchL = (high7 + hb1_x1_);

        // notch R
        hb1_x2_ += fnotch * hb1_y2_;
        float high8 = hb3_x2_ - hb1_x2_ - hb1_y2_;
        hb1_y2_ += fnotch * high8;
        float notchR = (high8 + hb1_x2_);

        *sp = *sp * dry + notchL * wetL;
        sp++;
        *sp = *sp * dry + notchR * wetR;
        sp++;

        currentDelaySize1 += delaySizeInc1;
    }
}

__attribute__((noinline)) void Timbre::fx2Grain1(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                 float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 1.5f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    float param2 = clamp(fabsf(this->sharedParams().effect2.param2 + matrixFilterParam2), 0, 1);
    param2 *= param2;

    float lock = ((param2 > 0.99f) ? 0.f : 1.f);
    lockA_ = lockA_ * 0.98f + lock * 0.02f;
    lockB_ = (1 - lockA_);

    matrixFilterFrequencyS_ = 0.02f * (matrixFilterFrequency * matrixFilterFrequency * 0.125f) + 0.98f * matrixFilterFrequencyS_;
    param1S_ = 0.02f * fabs(this->sharedParams().effect2.param1 + matrixFilterFrequencyS_) + .98f * param1S_;

    if (lockA_ >= 0.9999f) {
        param2S_ = 0.05f * param2 + 0.95f * param2S_;
    }

    const float sampleRateDivide = 4;
    const float sampleRateDivideInv = 1 / sampleRateDivide;
    float inputIncCount = 0;

    bool grainProb = 0.985f >= fabs(noise[0]);

    const float filterB2 = 0.25f;
    const float filterB = (filterB2 * filterB2 * 0.5f);

    const float _in3_b1 = (1 - filterB);
    const float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;

    if (grainProb) {
        if (grainTable_[grainNext_][GRAIN_RAMP] >= 1 && grainTable_[grainPrev_][GRAIN_RAMP] > (0.33f - param2S_ * 0.27f)) {
            // grain done, compute another one
            float jitter = foldAbs(param2S_);
            float param2sq = sqrt3(param2S_);
            float grainRate = sampleRateDivideInv * (1 + jitter * noise[4] * 0.0025f);
            grainTable_[grainNext_][GRAIN_RAMP] = 0;
            grainTable_[grainNext_][GRAIN_SIZE] =
                clamp((1800 + (noise[2]) * 40 * jitter * jitter) * param1S_ * param1S_, 432, delayBufferSize - 100);
            grainTable_[grainNext_][GRAIN_POS] = modulo2(delayWritePosF_ - (400 + param2sq * noise[5] * 1290), delayBufferSize);
            float invGrainSize = 1 / grainTable_[grainNext_][GRAIN_SIZE];
            grainTable_[grainNext_][GRAIN_CURRENT_SHIFT] = grainTable_[grainNext_][GRAIN_NEXT_SHIFT];
            grainTable_[grainNext_][GRAIN_NEXT_SHIFT] = grainRate * invGrainSize;
            grainTable_[grainNext_][GRAIN_INC] =
                clamp((grainTable_[grainNext_][GRAIN_NEXT_SHIFT] - grainTable_[grainNext_][GRAIN_CURRENT_SHIFT]) * invGrainSize, 0, 0.5f);
            grainTable_[grainNext_][GRAIN_VOL] = clamp(0.25f + sqrt3(fabsf(noise[3])), 0, 1);
            grainTable_[grainNext_][GRAIN_PAN] = clamp(1 + (noise[6]) * param2sq, 0, 2) * 0.5f;
            grainPrev_ = grainNext_;

            if (lockB_ < 0.02f) {
                loopSize_ = clamp((1 - param1S_) * 1400, 48, 1800);
            }
        }
        if (++grainNext_ > 2) {
            grainNext_ = 0;
        }
    }

    float env;

    const float f2 = 0.85f;
    const float fnotch = 1.03f;

    float grain1, grain2, grain3;
    float grain1L, grain1R;
    float grain2L, grain2R;
    float grain3L, grain3R;
    float grainSumR, grainSumL;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;
    float monoIn;

    for (int k = 0; k < BLOCK_SIZE; k++) {

        if (++inputIncCount >= sampleRateDivide) {
            inputIncCount = 0;
            delayWritePos_ = (delayWritePos_ + 1) & delayBufferSizeM1;
            delayWritePosF_ = (float)delayWritePos_;

            delayReadPos_ = modulo(delayWritePos_ + loopSize_, delayBufferSize);
            float feedbackIn = delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);

            monoIn = (*sp + *(sp + 1)) * 0.5f;

            // hp
            float hp_in_x0 = monoIn;
            hp_in_y0_ = _in3_a0 * (hp_in_x0 - hp_in_x1_) + _in3_b1 * hp_in_y1_;
            hp_in_y1_ = hp_in_y0_;
            hp_in_x1_ = hp_in_x0;

            delayBuf[delayWritePos_] = hp_in_y0_ * lockA_ + feedbackIn * lockB_;
        }

        ///-------- grain 1
        grain1L = grain1R = 0;
        if (grainTable_[0][GRAIN_RAMP] < 1) {
            delayReadPos_ = modulo(grainTable_[0][GRAIN_POS] + grainTable_[0][GRAIN_RAMP] * grainTable_[0][GRAIN_SIZE], delayBufferSize);
            env = hann(grainTable_[0][GRAIN_RAMP]) * grainTable_[0][GRAIN_VOL];
            grain1 = env * delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
            grain1L = grain1 * grainTable_[0][GRAIN_PAN];
            grain1R = grain1 - grain1L;
        }
        grainTable_[0][GRAIN_RAMP] += grainTable_[0][GRAIN_CURRENT_SHIFT];
        grainTable_[0][GRAIN_CURRENT_SHIFT] += grainTable_[0][GRAIN_INC];

        ///-------- grain 2
        grain2L = grain2R = 0;
        if (grainTable_[1][GRAIN_RAMP] < 1) {
            delayReadPos_ = modulo(grainTable_[1][GRAIN_POS] + grainTable_[1][GRAIN_RAMP] * grainTable_[1][GRAIN_SIZE], delayBufferSize);
            env = hann(grainTable_[1][GRAIN_RAMP]) * grainTable_[1][GRAIN_VOL];
            grain2 = env * delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
            grain2L = grain2 * grainTable_[1][GRAIN_PAN];
            grain2R = grain2 - grain2L;
        }
        grainTable_[1][GRAIN_RAMP] += grainTable_[1][GRAIN_CURRENT_SHIFT];
        grainTable_[1][GRAIN_CURRENT_SHIFT] += grainTable_[1][GRAIN_INC];

        ///-------- grain 3
        grain3L = grain3R = 0;
        if (grainTable_[2][GRAIN_RAMP] < 1) {
            delayReadPos_ = modulo(grainTable_[2][GRAIN_POS] + grainTable_[2][GRAIN_RAMP] * grainTable_[2][GRAIN_SIZE], delayBufferSize);
            env = hann(grainTable_[2][GRAIN_RAMP]) * grainTable_[2][GRAIN_VOL];
            grain3 = env * delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
            grain3L = grain3 * grainTable_[2][GRAIN_PAN];
            grain3R = grain3 - grain3L;
        }
        grainTable_[2][GRAIN_RAMP] += grainTable_[2][GRAIN_CURRENT_SHIFT];
        grainTable_[2][GRAIN_CURRENT_SHIFT] += grainTable_[2][GRAIN_INC];

        grainSumL = grain1L + grain2L + grain3L;
        grainSumR = grain1R + grain2R + grain3R;

        // lp L
        low1_ += f2 * band1_;
        band1_ += f2 * ((grainSumL)-low1_ - band1_);
        low3_ += f2 * band3_;
        band3_ += f2 * ((low1_)-low3_ - band3_);
        low5_ += f2 * band5_;
        band5_ += f2 * ((low3_)-low5_ - band5_);

        // lp R
        low2_ += f2 * band2_;
        band2_ += f2 * ((grainSumR)-low2_ - band2_);
        low4_ += f2 * band4_;
        band4_ += f2 * ((low2_)-low4_ - band4_);
        low6_ += f2 * band6_;
        band6_ += f2 * ((low4_)-low6_ - band6_);

        // notch L
        hb1_x1_ += fnotch * hb1_y1_;
        float high7 = low5_ - hb1_x1_ - hb1_y1_;
        hb1_y1_ += fnotch * high7;
        float notchL = (high7 + hb1_x1_);

        // notch R
        hb1_x2_ += fnotch * hb1_y2_;
        float high8 = low6_ - hb1_x2_ - hb1_y2_;
        hb1_y2_ += fnotch * high8;
        float notchR = (high8 + hb1_x2_);

        *sp = *sp * dry + notchL * wetL;
        sp++;
        *sp = *sp * dry + notchR * wetR;
        sp++;
    }
}

__attribute__((noinline)) void Timbre::fx2Grain2(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                 float gainTmp) {
    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255] * 1.5f;
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    float param2 = clamp(fabsf(this->sharedParams().effect2.param2 + matrixFilterParam2), 0, 1);
    param2 *= param2;
    matrixFilterFrequencyS_ = 0.02f * (matrixFilterFrequency * matrixFilterFrequency * 0.125f) + 0.98f * matrixFilterFrequencyS_;
    param1S_ = 0.02f * fabs(this->sharedParams().effect2.param1 + matrixFilterFrequencyS_) + .98f * param1S_;

    float lock = ((param2 > 0.99f) ? 0.f : 1.f);
    lockA_ = lockA_ * 0.98f + lock * 0.02f;
    lockB_ = (1 - lockA_);

    if (lockA_ >= 0.9999f) {
        param1S_ = 0.0005f * fabs(this->sharedParams().effect2.param1 + matrixFilterFrequency) + .9995f * param1S_;
        param2S_ = 0.05f * param2 + 0.95f * param2S_;
    } else {
        param1S_ = 0.005f * fabs(this->sharedParams().effect2.param1 + matrixFilterFrequency) + .995f * param1S_;
    }

    const float sampleRateDivide = 4;
    const float sampleRateDivideInv = 1 / sampleRateDivide;
    float inputIncCount = 0;

    bool grainProb = clamp(param1S_ * 3, 0.05f, 0.95f) >= fabs(noise[0]);

    const float filterB2 = 0.25f;
    const float filterB = (filterB2 * filterB2 * 0.5f);

    const float _in3_b1 = (1 - filterB);
    const float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;

    if (grainProb) {
        if (grainTable_[grainNext_][GRAIN_RAMP] >= 1 && grainTable_[grainPrev_][GRAIN_RAMP] > (0.33f - param2S_ * 0.27f)) {
            // grain done, compute another one
            float param2sq = sqrt3(param2S_);
            float grainRate = sampleRateDivideInv * clamp(0.5f + param1S_ + param2S_ * noise[4] * 0.025f, 0, 2);
            grainTable_[grainNext_][GRAIN_RAMP] = 0;
            grainTable_[grainNext_][GRAIN_SIZE] =
                clamp((1800 + (noise[2]) * 40 * param2S_ * param2S_) * param1S_ * param1S_, 432, delayBufferSize - 100);
            grainTable_[grainNext_][GRAIN_POS] =
                modulo2(delayWritePosF_ - (800 + param2sq * noise[5] * 390 * (1.25f - param1S_)), delayBufferSize);
            float invGrainSize = 1 / grainTable_[grainNext_][GRAIN_SIZE];
            grainTable_[grainNext_][GRAIN_CURRENT_SHIFT] = grainTable_[grainNext_][GRAIN_NEXT_SHIFT];
            grainTable_[grainNext_][GRAIN_NEXT_SHIFT] = grainRate * invGrainSize;
            grainTable_[grainNext_][GRAIN_INC] =
                clamp((grainTable_[grainNext_][GRAIN_NEXT_SHIFT] - grainTable_[grainNext_][GRAIN_CURRENT_SHIFT]) * invGrainSize, 0, 0.5f);
            grainTable_[grainNext_][GRAIN_VOL] = clamp(0.25f + sqrt3(fabsf(noise[3])), 0, 1);
            grainTable_[grainNext_][GRAIN_PAN] = clamp(1 + (noise[6]) * param2sq, 0, 2) * 0.5f;
            grainPrev_ = grainNext_;

            if (lockB_ < 0.02f) {
                loopSize_ = clamp((1 - param1S_) * 1400, 48, 1800);
            }
        }
        if (++grainNext_ > 2) {
            grainNext_ = 0;
        }
    }

    float env;

    const float f2 = 0.85f;
    const float fnotch = 1.03f;

    float grain1, grain2, grain3;
    float grain1L, grain1R;
    float grain2L, grain2R;
    float grain3L, grain3R;
    float grainSumR, grainSumL;

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;

    for (int k = 0; k < BLOCK_SIZE; k++) {

        if (++inputIncCount >= sampleRateDivide) {
            inputIncCount = 0;
            delayWritePos_ = (delayWritePos_ + 1) & delayBufferSizeM1;
            delayWritePosF_ = (float)delayWritePos_;

            delayReadPos_ = modulo(delayWritePos_ + loopSize_, delayBufferSize);
            float feedbackIn = delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);

            // hp
            float hp_in_x0 = (*sp + *(sp + 1)) * 0.5f;
            hp_in_y0_ = _in3_a0 * (hp_in_x0 - hp_in_x1_) + _in3_b1 * hp_in_y1_;
            hp_in_y1_ = hp_in_y0_;
            hp_in_x1_ = hp_in_x0;

            delayBuf[delayWritePos_] = hp_in_y0_ * lockA_ + feedbackIn * lockB_;
        }

        ///-------- grain 1
        grain1L = grain1R = 0;
        if (grainTable_[0][GRAIN_RAMP] < 1) {
            delayReadPos_ = modulo(grainTable_[0][GRAIN_POS] + grainTable_[0][GRAIN_RAMP] * grainTable_[0][GRAIN_SIZE], delayBufferSize);
            env = hann(grainTable_[0][GRAIN_RAMP]) * grainTable_[0][GRAIN_VOL];
            grain1 = env * delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
            grain1L = grain1 * grainTable_[0][GRAIN_PAN];
            grain1R = grain1 - grain1L;
        }
        grainTable_[0][GRAIN_RAMP] += grainTable_[0][GRAIN_CURRENT_SHIFT];
        grainTable_[0][GRAIN_CURRENT_SHIFT] += grainTable_[0][GRAIN_INC];

        ///-------- grain 2
        grain2L = grain2R = 0;
        if (grainTable_[1][GRAIN_RAMP] < 1) {
            delayReadPos_ = modulo(grainTable_[1][GRAIN_POS] + grainTable_[1][GRAIN_RAMP] * grainTable_[1][GRAIN_SIZE], delayBufferSize);
            env = hann(grainTable_[1][GRAIN_RAMP]) * grainTable_[1][GRAIN_VOL];
            grain2 = env * delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
            grain2L = grain2 * grainTable_[1][GRAIN_PAN];
            grain2R = grain2 - grain2L;
        }
        grainTable_[1][GRAIN_RAMP] += grainTable_[1][GRAIN_CURRENT_SHIFT];
        grainTable_[1][GRAIN_CURRENT_SHIFT] += grainTable_[1][GRAIN_INC];

        ///-------- grain 3
        grain3L = grain3R = 0;
        if (grainTable_[2][GRAIN_RAMP] < 1) {
            delayReadPos_ = modulo(grainTable_[2][GRAIN_POS] + grainTable_[2][GRAIN_RAMP] * grainTable_[2][GRAIN_SIZE], delayBufferSize);
            env = hann(grainTable_[2][GRAIN_RAMP]) * grainTable_[2][GRAIN_VOL];
            grain3 = env * delayInterpolation(delayReadPos_, delayBuf, delayBufferSizeM1);
            grain3L = grain3 * grainTable_[2][GRAIN_PAN];
            grain3R = grain3 - grain3L;
        }
        grainTable_[2][GRAIN_RAMP] += grainTable_[2][GRAIN_CURRENT_SHIFT];
        grainTable_[2][GRAIN_CURRENT_SHIFT] += grainTable_[2][GRAIN_INC];

        grainSumL = grain1L + grain2L + grain3L;
        grainSumR = grain1R + grain2R + grain3R;

        // lp L
        low1_ += f2 * band1_;
        band1_ += f2 * ((grainSumL)-low1_ - band1_);
        low3_ += f2 * band3_;
        band3_ += f2 * ((low1_)-low3_ - band3_);
        low5_ += f2 * band5_;
        band5_ += f2 * ((low3_)-low5_ - band5_);

        // lp R
        low2_ += f2 * band2_;
        band2_ += f2 * ((grainSumR)-low2_ - band2_);
        low4_ += f2 * band4_;
        band4_ += f2 * ((low2_)-low4_ - band4_);
        low6_ += f2 * band6_;
        band6_ += f2 * ((low4_)-low6_ - band6_);

        // notch L
        hb1_x1_ += fnotch * hb1_y1_;
        float high7 = low5_ - hb1_x1_ - hb1_y1_;
        hb1_y1_ += fnotch * high7;
        float notchL = (high7 + hb1_x1_);

        // notch R
        hb1_x2_ += fnotch * hb1_y2_;
        float high8 = low6_ - hb1_x2_ - hb1_y2_;
        hb1_y2_ += fnotch * high8;
        float notchR = (high8 + hb1_x2_);

        *sp = *sp * dry + notchL * wetL;
        sp++;
        *sp = *sp * dry + notchR * wetR;
        sp++;
    }
}

__attribute__((noinline)) void Timbre::fx2StereoBp(float matrixFilterFrequency, float matrixFilterParam2, float matrixFilterPan,
                                                   float gainTmp) {

    mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
    float mixerGain_01 = clamp(mixerGain_, 0, 1);
    int mixerGain255 = mixerGain_01 * 255;
    float dry = panTable[255 - mixerGain255];
    float wet = panTable[mixerGain255];
    float extraAmp = clamp(mixerGain_ - 1, 0, 1);
    wet += extraAmp;

    param1S_ = 0.02f * (this->sharedParams().effect2.param1) + .98f * param1S_;

    const float f = param1S_ * param1S_ * param1S_ * 0.9f;
    const float matrixFreqAtnn = matrixFilterFrequency * 0.125f;

    float bpf1 = clamp(0.015f + fold((f + matrixFreqAtnn) * 0.25f) * 3.8f, 0.01f, 0.9f);
    float bpf2 = clamp(0.015f + fold((f - matrixFreqAtnn) * 0.25f) * 3.8f, 0.01f, 0.9f);

    float* __restrict__ sp = sampleBlock_;
    float* __restrict__ const delayBuf = delayBuffer_;

    float filterParam2 = clamp(matrixFilterParam2 + this->sharedParams().effect2.param2, 0, 1) * (1 - param1S_ * param1S_ * 0.06f);

    const float fb = sqrt3(0.5f - filterParam2 * 0.497f);
    const float scale = sqrt3(fb);
    const float fb2 = fb * 0.982f;
    const float scale2 = sqrt3(fb2);

    const float inputGain = 1.0f;
    const float finalGain = 3 * (1 - filterParam2 * filterParam2 * 0.5f);

    wet *= finalGain;

    float wetL = wet * (1 + matrixFilterPan);
    float wetR = wet * (1 - matrixFilterPan);

    float high1 = 0;
    float high2 = 0;
    float high3 = 0;
    float high4 = 0;
    float high5 = 0;
    float high6 = 0;
    float high7 = 0;
    float high8 = 0;

    const float f1 = clamp(0.15f + f * 0.5f, 0.01f, 0.99f);
    float coef1 = (1.0f - f1) / (1.0f + f1);

    float drift = _ly1_;
    float nexDrift = noise[7] * 0.005f;
    float deltaD = (nexDrift - drift) * 0.000625f;
    _ly1_ = nexDrift;

    shift_ = shift_ * 0.999f + noise[4] * 0.0001f;

    //   h1 = fondamentale, feedback_ tanh (résonance non-linéaire, singing quality)
    //   h2 = fondamentale légèrement décalée (~1.5%, drift analogique)
    //   h3 = octave  (×2), excité par la sortie de h1
    //   h4 = tierce  (×3), excité par la sortie de h1
    float jitter = shift_ * 0.004f;
    float bpf_h1 = bpf1 * (1.0f + jitter);
    float bpf_h2 = bpf1 * (1.015f - jitter * 0.5f);
    float bpf_h3 = clamp(bpf1 * 2.0f * (1.0f + jitter * 0.3f), 0.01f, 0.92f);
    float bpf_h1_r = bpf2 * (1.0f + jitter);
    float bpf_h2_r = bpf2 * (1.015f - jitter * 0.5f);
    float bpf_h3_r = clamp(bpf2 * 2.0f * (1.0f + jitter * 0.3f), 0.01f, 0.92f);
    float bpf_h4 = clamp(bpf1 * 3.0f * (1.0f - jitter * 0.2f), 0.01f, 0.92f);
    float bpf_h4_r = clamp(bpf2 * 3.0f * (1.0f - jitter * 0.2f), 0.01f, 0.92f);

    // input hp coefs calc :
    const float cutoff = 0.05f;
    const float _in_b1 = (1 - cutoff);
    const float _in_a0 = (1 + _in_b1 * _in_b1 * _in_b1) * 0.5f;
    const float _in_a1 = -_in_a0;

    // limiter
    const float threshold = 0.7f;
    const float kneeWidth = 0.2f;
    const float kneeWidthInv = 1 / (2 * kneeWidth);
    const float threshKneeP = threshold + kneeWidth * 0.5f;
    const float threshKneeM = threshold - kneeWidth * 0.5f;
    const float makeup = 1 / threshold;

    const int delaySize = 256;
    const int delaySizeM1 = delaySize - 1;

    const float attackCoeff = 0.5f;
    const float releaseCoeff = 0.996f;
    const float holdTime = 0.03f;
    const int holdSampleCount = static_cast<int>(holdTime * PREENFM_FREQUENCY);
    // hold counter persistant entre blocs (hb4_y1_ inutilisé dans cet effet, pas de RAM en plus)
    int holdSamples = (int)hb4_y1_;

    hb4_x1_ = clamp(hb4_x1_, 0, 1);
    hb4_x2_ = clamp(hb4_x2_, 0, 1);

    const float lpCoef = 0.7f;

    float target_gain = 1.0f;

    const float _in2_a0 = _in_b1 * _in_a0;
    const float _in2_b1 = _in_b1 * _in_b1;

    const float wetLMakeup = wetL * makeup;
    const float wetRMakeup = wetR * makeup;

    for (int k = BLOCK_SIZE; k--;) {
        float fbM = fb + drift;
        drift += deltaD;

        // hp input L
        hb5_x1_ = (*sp) * inputGain;
        hb5_y1_ = _in_a0 * hb5_x1_ + _in_a1 * hb5_x2_ + _in_b1 * hb5_y2_;
        hb5_y2_ = hb5_y1_;
        hb5_x2_ = hb5_x1_;

        hb6_x1_ = hb5_y1_;
        hb6_y1_ = _in2_a0 * (hb6_x1_ - hb6_x2_) + _in2_b1 * hb6_y2_;
        hb6_y2_ = hb6_y1_;
        hb6_x2_ = hb6_x1_;

        // hp input R
        hb7_x1_ = *(sp + 1) * inputGain;
        hb7_y1_ = _in_a0 * hb7_x1_ + _in_a1 * hb7_x2_ + _in_b1 * hb7_y2_;
        hb7_y2_ = hb7_y1_;
        hb7_x2_ = hb7_x1_;

        hb8_x1_ = hb7_y1_;
        hb8_y1_ = _in2_a0 * (hb8_x1_ - hb8_x2_) + _in2_b1 * hb8_y2_;
        hb8_y2_ = hb8_y1_;
        hb8_x2_ = hb8_x1_;

        // Left voice

        hb1_y1_ = coef1 * (hb1_y1_ + hb6_y1_) - hb1_x1_; // allpass (shared phase smear)
        hb1_x1_ = hb6_y1_;

        // h1 — fondamentale, feedback_ non-linéaire (transistor ladder)
        low1_ = low1_ + bpf_h1 * band1_;
        high1 = scale2 * hb1_y1_ - low1_ - fb2 * tanh4(band1_);
        band1_ = bpf_h1 * high1 + band1_;

        // h2 — fondamentale légèrement décalée (drift analogique)
        low2_ = low2_ + bpf_h2 * band2_;
        high2 = scale * hb1_y1_ - low2_ - fbM * band2_;
        band2_ = bpf_h2 * high2 + band2_;

        // h3 — 2e harmonique (octave), en cascade de h1
        low3_ = low3_ + bpf_h3 * band3_;
        high3 = scale2 * band1_ * 0.7f - low3_ - fb2 * band3_;
        band3_ = bpf_h3 * high3 + band3_;

        // h4 — 3e harmonique (3f), en cascade de h1
        hb2_x1_ = hb2_x1_ + bpf_h4 * hb2_x2_;
        high7 = scale2 * band1_ * 0.5f - hb2_x1_ - fb2 * hb2_x2_;
        hb2_x2_ = bpf_h4 * high7 + hb2_x2_;

        float outL = band1_ + band2_ * 0.5f + band3_ * 0.7f + hb2_x2_ * 0.5f;
        // LP post-SVF tracking BP center
        hb3_y1_ += bpf_h1 * (outL - hb3_y1_);

        // Right voice

        hb1_y2_ = coef1 * (hb1_y2_ + hb8_y1_) - hb1_x2_; // allpass (shared phase smear)
        hb1_x2_ = hb8_y1_;

        // h1 — fondamentale, feedback_ non-linéaire (transistor ladder)
        low4_ = low4_ + bpf_h1_r * band4_;
        high4 = scale2 * hb1_y2_ - low4_ - fb2 * tanh4(band4_);
        band4_ = bpf_h1_r * high4 + band4_;

        // h2 — fondamentale légèrement décalée (drift analogique)
        low5_ = low5_ + bpf_h2_r * band5_;
        high5 = scale * hb1_y2_ - low5_ - fbM * band5_;
        band5_ = bpf_h2_r * high5 + band5_;

        // h3 — 2e harmonique (octave), en cascade de h1
        low6_ = low6_ + bpf_h3_r * band6_;
        high6 = scale2 * band4_ * 0.7f - low6_ - fb2 * band6_;
        band6_ = bpf_h3_r * high6 + band6_;

        // h4 — 3e harmonique (3f), en cascade de h1
        hb2_y1_ = hb2_y1_ + bpf_h4_r * hb2_y2_;
        high8 = scale2 * band4_ * 0.5f - hb2_y1_ - fb2 * hb2_y2_;
        hb2_y2_ = bpf_h4_r * high8 + hb2_y2_;

        float outR = band4_ + band5_ * 0.5f + band6_ * 0.7f + hb2_y2_ * 0.5f;
        // LP post-SVF tracking BP center
        hb3_y2_ += bpf_h1 * (outR - hb3_y2_);

        // limiter delay

        delayWritePos_ = (delayWritePos_ + 1) & delaySizeM1;

        _lx1_ = _lx1_ * lpCoef + tanh4(hb3_y1_) * (1.0f - lpCoef);
        _lx2_ = _lx2_ * lpCoef + tanh4(hb3_y2_) * (1.0f - lpCoef);

        delayBuf[delayWritePos_] = _lx1_;
        delayBuf[delayWritePos_ + delaySize] = _lx2_;

        // limiter — detect on _lx1_/_lx2_ so envelope matches what is in the delay buffer

        float gain = hb4_x1_;
        float envelope = hb4_x2_;

        int readpos = (delayWritePos_ - delaySize) & delaySizeM1;

        float fltOut1 = delayBuf[readpos];
        float fltOut2 = delayBuf[delaySize + readpos];

        float absLeft = fabsf(_lx1_);
        float absRight = fabsf(_lx2_);
        float absSample = (absLeft > absRight) ? absLeft : absRight;

        envelope = max(absSample, envelope * releaseCoeff);

        target_gain = 1.0f;

        if (envelope > threshKneeP) {
            target_gain = threshKneeP / envelope;
            holdSamples = holdSampleCount;
        } else if (envelope > threshKneeM) {
            // soft knee
            float x = envelope - threshKneeM;
            float y = x * x * kneeWidthInv;
            target_gain = (threshKneeM + y) / envelope;
        }

        // Attaque toujours autorisée ; le hold (30 ms, persistant entre blocs) ne bloque que le relâchement.
        if (target_gain < gain || holdSamples <= 0) {
            gain = gain * attackCoeff + target_gain * (1.0f - attackCoeff);
        }
        if (holdSamples > 0) {
            holdSamples--;
        }

        hb4_x1_ = gain;
        hb4_x2_ = envelope;

        //  ------------

        *sp = *sp * dry + fltOut1 * wetLMakeup * gain;
        sp++;

        *sp = *sp * dry + fltOut2 * wetRMakeup * gain;
        sp++;
    }
    hb4_y1_ = (float)holdSamples;
}

inline float Timbre::iirFilter(float x, float a1, float* xn1, float* xn2, float* yn1, float* yn2) {
    // https://dsp.stackexchange.com/a/59157
    // 𝑦[𝑘] = 𝑐( 𝑥[𝑘] + 𝑦[𝑘−2] ) − 𝑥[𝑘−2]
    float y = a1 * (x + *yn2) - *xn2;
    *yn2 = *yn1;
    *yn1 = y;
    *xn2 = *xn1;
    *xn1 = x;

    return y;
}

inline float Timbre::delayInterpolation(float readPos, float buffer[], int bufferLenM1) {
    int readPosInt = readPos;
    float y1 = buffer[readPosInt];
    float y0 = buffer[(readPosInt - 1) & bufferLenM1];
    float x = 1 - (readPos - floorf(readPos));
    return (y0 - y1) * x + y1;
}

inline float Timbre::delayInterpolation2(float readPos, float buffer[], int bufferLenM1, int offset) {
    int readPosInt = readPos;
    float y1 = buffer[offset + readPosInt];
    float y0 = buffer[offset + ((readPosInt - 1) & bufferLenM1)];
    float x = 1 - (readPos - floorf(readPos));
    return (y0 - y1) * x + y1;
}

__attribute__((noinline)) float Timbre::delayInterpolationHermite(float readPos, const float buffer[], int bufferLenM1, int offset) {
    int readPosInt = (int)readPos;
    float frac = readPos - (float)readPosInt;
    float xm1 = buffer[offset + ((readPosInt - 1) & bufferLenM1)];
    float x0 = buffer[offset + (readPosInt & bufferLenM1)];
    float x1 = buffer[offset + ((readPosInt + 1) & bufferLenM1)];
    float x2 = buffer[offset + ((readPosInt + 2) & bufferLenM1)];
    return hermiteInterpolation(frac, xm1, x0, x1, x2);
}

inline float Timbre::hermiteInterpolation(float frac, float xm1, float x0, float x1, float x2) {
    float c0 = x0;
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

#if defined(__GNUC__) && !defined(DEBUG)
#pragma GCC pop_options
#endif
