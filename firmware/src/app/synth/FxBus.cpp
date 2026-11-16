/*
 * Copyright 2021  - Patrice Vigouroux
 *
 * Author: Patrice Vigouroux (Toltekradiation)
 * Forum discussion : http://ixox.fr/forum/index.php?topic=69835.0
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
#include "FxBus.h"

inline float fold(float x4) {
    // https://www.desmos.com/calculator/ge2wvg2wgj
    // x4 : x / 4
    return (fabsf(x4 + 0.25f - roundf(x4 + 0.25f)) - 0.25f);
}
inline
int modulo(int d, int max) {
    return unlikely(d >= max) ? d - max : d;
}
inline
float modulo2(float readPos, int bufferLen) {
    if( unlikely(readPos < 0) )
        readPos += bufferLen;
    return readPos;
}
inline
float clamp(float d, float min, float max) {
    const float t = unlikely(d < min) ? min : d;
    return unlikely(t > max) ? max : t;
}
inline
float sqrt3(const float x)
{
    union
    {
        int i;
        float x;
    } u;

    u.x = x;
    u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
    return u.x;
}
inline
float fastroot(float f,int n)
{
    long *lp,l;
    lp=(long*)(&f);
    l=*lp;l-=0x3F800000l;l>>=(n-1);l+=0x3F800000l;
    *lp=l;
    return f;
}
inline
float sigmoid(float x)
{
    return x * (1.5f - 0.5f * x * x);
}
//***------------***------------***------------***------------***----- FxBus -------***------------***------------***------------

float FxBus::delay1Buffer[delay1BufferSize] __attribute__((section(".ram_d1")));
float FxBus::delay2Buffer[delay2BufferSize] __attribute__((section(".ram_d1")));
float FxBus::delay3Buffer[delay3BufferSize] __attribute__((section(".ram_d2")));
float FxBus::delay4Buffer[delay4BufferSize] __attribute__((section(".ram_d2")));

float FxBus::predelayBuffer[predelayBufferSize] __attribute__((section(".ram_d2")));

float FxBus::inputBuffer1[inputBufferLen1] __attribute__((section(".ram_d2")));
float FxBus::inputBuffer2[inputBufferLen2] __attribute__((section(".ram_d2")));
float FxBus::inputBuffer3[inputBufferLen3] __attribute__((section(".ram_d2")));
float FxBus::inputBuffer4[inputBufferLen4] __attribute__((section(".ram_d2")));

float FxBus::diffuserBuffer1[diffuserBufferLen1] __attribute__((section(".ram_d2")));
float FxBus::diffuserBuffer2[diffuserBufferLen2] __attribute__((section(".ram_d2")));
float FxBus::diffuserBuffer3[diffuserBufferLen3] __attribute__((section(".ram_d2b")));
float FxBus::diffuserBuffer4[diffuserBufferLen4] __attribute__((section(".ram_d2b")));

FxBus::FxBus() {}

void FxBus::init() {

    for (int s = 0; s < predelayBufferSize; s++) {
        predelayBuffer[s] = 0;
    }
    for (int s = 0; s < delay1BufferSize; s++) {
        delay1Buffer[ s ] = 0;
    }
    for (int s = 0; s < delay2BufferSize; s++) {
        delay2Buffer[ s ] = 0;
    }
    for (int s = 0; s < delay3BufferSize; s++) {
        delay3Buffer[ s ] = 0;
    }
    for (int s = 0; s < delay4BufferSize; s++) {
        delay4Buffer[ s ] = 0;
    }

    for (int s = 0; s < inputBufferLen1; s++) {
        inputBuffer1[ s ] = 0;
    }
    for (int s = 0; s < inputBufferLen2; s++) {
        inputBuffer2[ s ] = 0;
    }
    for (int s = 0; s < inputBufferLen3; s++) {
        inputBuffer3[ s ] = 0;
    }
    for (int s = 0; s < inputBufferLen4; s++) {
        inputBuffer4[ s ] = 0;
    }

    for (int s = 0; s < diffuserBufferLen1; s++) {
        diffuserBuffer1[ s ] = 0;
    }
    for (int s = 0; s < diffuserBufferLen2; s++) {
        diffuserBuffer2[ s ] = 0;
    }
    for (int s = 0; s < diffuserBufferLen3; s++) {
        diffuserBuffer3[ s ] = 0;
    }
    for (int s = 0; s < diffuserBufferLen4; s++) {
        diffuserBuffer4[ s ] = 0;
    }
    setDefaultValue();
}


void FxBus::setDefaultValue() {
    // Master FX default value
    masterfxConfig_[GLOBALFX_PREDELAYTIME] = GLOBALFX_PREDELAYTIME_DEFAULT;
    masterfxConfig_[GLOBALFX_PREDELAYMIX] = GLOBALFX_PREDELAYMIX_DEFAULT;
    masterfxConfig_[GLOBALFX_SIZE] = GLOBALFX_SIZE_DEFAULT;
    masterfxConfig_[GLOBALFX_DIFFUSION] = GLOBALFX_DIFFUSION_DEFAULT;
    masterfxConfig_[GLOBALFX_DAMPING] = GLOBALFX_DAMPING_DEFAULT;
    masterfxConfig_[GLOBALFX_DECAY] = GLOBALFX_DECAY_DEFAULT;
    masterfxConfig_[GLOBALFX_LFODEPTH] = GLOBALFX_LFODEPTH_DEFAULT;
    masterfxConfig_[GLOBALFX_LFOSPEED] = GLOBALFX_LFOSPEED_DEFAULT;
    masterfxConfig_[GLOBALFX_INPUTBASE] = GLOBALFX_INPUTBASE_DEFAULT;
    masterfxConfig_[GLOBALFX_INPUTWIDTH] = GLOBALFX_INPUTWIDTH_DEFAULT;
    masterfxConfig_[GLOBALFX_NOTCHBASE] = GLOBALFX_NOTCHBASE_DEFAULT;
    masterfxConfig_[GLOBALFX_NOTCHSPREAD] = GLOBALFX_NOTCHSPREAD_DEFAULT;
    masterfxConfig_[GLOBALFX_LOOPHP] = GLOBALFX_LOOPHP_DEFAULT;
}

/**
 * init before timbres summing
 */
void FxBus::mixSumInit() {

    if(totalSent_ == 0.0f) {
        return;
    }

    sample_ = getSampleBlock();

    for (int s = 0; s < 8; s++) {
        *(sample_++) = 0;
        *(sample_++) = 0;
        *(sample_++) = 0;
        *(sample_++) = 0;
        *(sample_++) = 0;
        *(sample_++) = 0;
        *(sample_++) = 0;
        *(sample_++) = 0;
    }

    totalSent_ = 0.0f;

    if (somethingChanged_) {
        if (waitCountBeforeChange_-- == 0 ) {
            somethingChanged_ = false;
            if (currentPresetNum_ != nextPresetNum_) {
                presetChanged(nextPresetNum_);
            } else {
                paramChanged();
            }
        }
    }
}

void FxBus::slowParamChange() {
    somethingChanged_ = true;
    waitCountBeforeChange_ = 100;
}

void FxBus::presetChanged(int presetNum) {
    setDefaultValue();
    if (presetNum < 15)
    {
        int size = presetNum * 0.333333333f;
        int brightness = presetNum % 3;

        switch (size)
        {
        case 0:
            masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.57f;
            masterfxConfig_[GLOBALFX_LFOSPEED] = 0.1f;
            masterfxConfig_[GLOBALFX_LFODEPTH] = 0.9;
            masterfxConfig_[GLOBALFX_LOOPHP] = 0.55f;

            switch (presetNum)
            {
            case 0:
                masterfxConfig_[GLOBALFX_SIZE] = 0.1f;
                masterfxConfig_[GLOBALFX_DECAY] = 0.04f;
                masterfxConfig_[GLOBALFX_DIFFUSION] = 0.68f;
                break;
            case 1:
                masterfxConfig_[GLOBALFX_SIZE] = 0.13f;
                masterfxConfig_[GLOBALFX_DECAY] = 0.02f;
                masterfxConfig_[GLOBALFX_DIFFUSION] = 0.86f;
                break;
            case 2:
                masterfxConfig_[GLOBALFX_SIZE] = 0.23f;
                masterfxConfig_[GLOBALFX_DECAY] = 0.08f;
                masterfxConfig_[GLOBALFX_DIFFUSION] = 1;
                break;
            default:
                break;
            }
            break;
            case 1:
                //small
                masterfxConfig_[GLOBALFX_SIZE] = 0.26f;
                masterfxConfig_[GLOBALFX_DECAY] = 0.1f;
                masterfxConfig_[GLOBALFX_DIFFUSION] = 0.68f;
                masterfxConfig_[GLOBALFX_LFOSPEED] = 0.55f;
                masterfxConfig_[GLOBALFX_LFODEPTH] = 0.35f;
                masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.05f;
                masterfxConfig_[GLOBALFX_LOOPHP] = 0.3f;
                break;
            case 2:
                //medium
                masterfxConfig_[GLOBALFX_SIZE] = 0.465f;
                masterfxConfig_[GLOBALFX_DECAY] = 0.18f;
                masterfxConfig_[GLOBALFX_DIFFUSION] = 0.57f;
                masterfxConfig_[GLOBALFX_LFOSPEED] = 0.57f;
                masterfxConfig_[GLOBALFX_LFODEPTH] = 0.28f;
                masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.22f;
                masterfxConfig_[GLOBALFX_LOOPHP] = 0.3f;
                break;
            case 3:
                //large
                masterfxConfig_[GLOBALFX_SIZE] = 0.775f;
                masterfxConfig_[GLOBALFX_DECAY] = 0.38f;
                masterfxConfig_[GLOBALFX_DIFFUSION] = 0.56f;
                masterfxConfig_[GLOBALFX_LFOSPEED] = 0.57f;
                masterfxConfig_[GLOBALFX_LFODEPTH] = 0.21f;
                masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.05f;
                masterfxConfig_[GLOBALFX_LOOPHP] = 0.3f;
                break;
            case 4:
                //xtra large
                masterfxConfig_[GLOBALFX_SIZE] = 0.87f;
                masterfxConfig_[GLOBALFX_DECAY] = 0.72f;
                masterfxConfig_[GLOBALFX_DIFFUSION] = 0.68f;
                masterfxConfig_[GLOBALFX_LFOSPEED] = 0.57f;
                masterfxConfig_[GLOBALFX_LFODEPTH] = 0.21f;
                masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.02f;
                masterfxConfig_[GLOBALFX_LOOPHP] = 0.35f;
                break;
            default:
                break;
        }

        switch (brightness)
        {
        case 0:
            masterfxConfig_[GLOBALFX_DAMPING] = 0.19f;
            break;
        case 1:
            masterfxConfig_[GLOBALFX_DAMPING] = 0.42f * (1 - masterfxConfig_[GLOBALFX_SIZE] * 0.06f);
            break;
        case 2:
            masterfxConfig_[GLOBALFX_DAMPING] = 0.8f * (1 - masterfxConfig_[GLOBALFX_SIZE] * 0.15f);
            break;
        default:
            break;
        }

    }
    else
    {
        switch (presetNum)
        {
        case 15:
            //freeze
            masterfxConfig_[GLOBALFX_DAMPING] = 0.96f;
            masterfxConfig_[GLOBALFX_LFOSPEED] = 0.91f;
            masterfxConfig_[GLOBALFX_LFODEPTH] = 0.05f;
            masterfxConfig_[GLOBALFX_SIZE] = 1;
            masterfxConfig_[GLOBALFX_DECAY] = 0.943f;
            masterfxConfig_[GLOBALFX_DIFFUSION] = 0.96f;
            masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.245f;
            masterfxConfig_[GLOBALFX_LOOPHP] = 0.1f;
            break;
        case 16:
            //hall
            masterfxConfig_[GLOBALFX_DAMPING] = 0.57f;
            masterfxConfig_[GLOBALFX_LFOSPEED] = 0.28f;
            masterfxConfig_[GLOBALFX_LFODEPTH] = 0.27f;
            masterfxConfig_[GLOBALFX_SIZE] = 0.72f;
            masterfxConfig_[GLOBALFX_DECAY] = 0.49f;
            masterfxConfig_[GLOBALFX_DIFFUSION] = 0.66f;
            masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.06f;
            masterfxConfig_[GLOBALFX_LOOPHP] = 0.15f;
            break;
        case 17:
            //cave
            masterfxConfig_[GLOBALFX_DAMPING] = 0.38f;
            masterfxConfig_[GLOBALFX_LFOSPEED] = 0.37f;
            masterfxConfig_[GLOBALFX_LFODEPTH] = 0.33f;
            masterfxConfig_[GLOBALFX_SIZE] = 0.94f;
            masterfxConfig_[GLOBALFX_DECAY] = 0.43f;
            masterfxConfig_[GLOBALFX_DIFFUSION] = 0.8f;
            masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.11f;
            masterfxConfig_[GLOBALFX_LOOPHP] = 0.29f;
            break;
        case 18:
            //apartment
            masterfxConfig_[GLOBALFX_DAMPING] = 0.44f;
            masterfxConfig_[GLOBALFX_LFOSPEED] = 0.33f;
            masterfxConfig_[GLOBALFX_LFODEPTH] = 0.33f;
            masterfxConfig_[GLOBALFX_SIZE] = 0.6f;
            masterfxConfig_[GLOBALFX_DECAY] = 0.22f;
            masterfxConfig_[GLOBALFX_DIFFUSION] = 0.72f;
            masterfxConfig_[GLOBALFX_NOTCHBASE] = 0.15f;
            masterfxConfig_[GLOBALFX_LOOPHP] = 0.34f;
            break;
        default:
            break;
        }
    }
    currentPresetNum_ = presetNum;
    nextPresetNum_ = presetNum;

    paramChanged();
}


void FxBus::paramChanged() {
    float temp;

    inputWidth_         =     masterfxConfig_[GLOBALFX_INPUTWIDTH];
    float filterBase = masterfxConfig_[GLOBALFX_INPUTBASE];

    if (prevFilterBase_ != filterBase || prevInputWidth_ != inputWidth_)
    {
        float filterB2     = filterBase * filterBase;
        float filterB     = (filterB2 * filterB2 * 0.5f);

        // input hp coefs calc :
        _in_b1_ = (1 - filterB);// expf(-_2M_PI * _cutoffFreq * _1_sampleRate)
        _in_a0_ = (1 + _in_b1_ * _in_b1_ * _in_b1_) * 0.5f;
        _in_a1_ = -_in_a0_;

        // input lp coefs calc :
        filterBase = clamp(filterBase + inputWidth_, 0, 1);
        float filterA2     = filterBase * filterBase;
        float filterA     = (filterA2 * filterA2 * 0.5f);

        _in_lp_b_ = 1 - filterA;
        _in_lp_a_ = 1 - _in_lp_b_;
    }
    prevFilterBase_ = filterBase;
    prevInputWidth_ = inputWidth_;

    float loopHp = masterfxConfig_[GLOBALFX_LOOPHP];
    if (prevLoopHp_ != loopHp)
    {
        float filterB2     = loopHp * loopHp;
        float filterB     = (filterB2 * filterB2 * 0.5f);

        // tank hp coefs calc :
        _b1_ = (1 - filterB);
        _a0_ = (1 + _b1_ * _b1_ * _b1_) * 0.5f;
        _a1_ = -_a0_;
    }
    prevLoopHp_ = loopHp;


    fxTimeLinear_ = masterfxConfig_[GLOBALFX_PREDELAYTIME];
    if (prevFxTimeLinear_ != fxTimeLinear_)
    {
        fxTime_     = clamp(fxTimeLinear_, 0.0003f, 0.9996f);
        fxTime_     *= fxTime_ * fxTime_;
        predelaySize_ = fxTimeLinear_ * predelayBufferSizeM1;
   }
    prevFxTimeLinear_ = fxTimeLinear_;

    predelayMixLevel_ = masterfxConfig_[GLOBALFX_PREDELAYMIX];
    predelayMixAttn_ = predelayMixLevel_ * (1 - (predelayMixLevel_ * predelayMixLevel_ * 0.1f));

    lfoSpeedLinear_ = masterfxConfig_[GLOBALFX_LFOSPEED];
    if (prevLfoSpeedLinear_ != lfoSpeedLinear_)
    {
        temp = lfoSpeedLinear_;
        temp *= temp * temp;
        lfoSpeed_ = temp;
    }
    prevLfoSpeedLinear_ = lfoSpeedLinear_;

    temp = masterfxConfig_[GLOBALFX_LFODEPTH];
    temp = temp * (1 - lfoSpeedLinear_ * 0.5f) * (1 - sizeParam_ * 0.5f);
    lfoDepth_ = temp;

    // ------ page 2

    sizeParam_     = clamp(masterfxConfig_[GLOBALFX_SIZE], 0.03f, 1);

    diffusion_         =     masterfxConfig_[GLOBALFX_DIFFUSION];
    damping_         =     masterfxConfig_[GLOBALFX_DAMPING];

    decayVal_         =     masterfxConfig_[GLOBALFX_DECAY ];
    notchBase_         =     masterfxConfig_[GLOBALFX_NOTCHBASE ];
    notchSpread_     =     masterfxConfig_[GLOBALFX_NOTCHSPREAD];

    //------- some process

    if(damping_ != prevDamping_) {
        damp_b_ = 1 - damping_  * damping_;
        damp_a_ = 1 - damp_b_;
    }
    prevDamping_ = damping_;

    if(prevDecayVal_ != decayVal_) {
        decayFdbck_ =  fastroot(decayVal_, 3) * decayMaxVal_;

        float decayValSquare = decayVal_ * decayVal_;

        headRoomMultiplier_ = (1 - decayValSquare * 0.5f) * 5.5f;
        sampleMultipler_ = headRoomMultiplier_ * (float) 0x7fffff;
    }
    prevDecayVal_ = decayVal_;

    if(notchBase_ != prevNotchBase_ || notchSpread_ != prevNotchSpread_) {
        float notchB =  notchBase_ * 0.25f;
        float offset = (notchSpread_) * 0.125f;
        float range = 0.67f;

        f1L_ = clamp( fold(notchB + offset)             * range, windowMin_, windowMax_);
        f2L_ = clamp( fold(notchB - offset)             * range, windowMin_, windowMax_);
        f3L_ = clamp( fold(notchB + offset * 1.5f)     * range, windowMin_, windowMax_);
        f4L_ = clamp( fold(notchB + offset * 1.8f)     * range, windowMin_, windowMax_);

        coef1L_ = (1.0f - f1L_) / (1.0f + f1L_);
        coef2L_ = (1.0f - f2L_) / (1.0f + f2L_);
        coef3L_ = (1.0f - f3L_) / (1.0f + f3L_);
        coef4L_ = (1.0f - f4L_) / (1.0f + f4L_);
    }
    prevNotchBase_ = notchBase_;
    prevNotchSpread_ = notchSpread_;

    if(sizeParam_ != prevSizeParam_) {
        float sizeSqrt = sqrt3(sizeParam_);

        float sizeParamInpt = 0.1f + sizeSqrt * 0.9f;
        inputBuffer1ReadLen_ = inputBufferLen1 * sizeParamInpt;
        inputBuffer2ReadLen_ = inputBufferLen2 * sizeParamInpt;
        inputBuffer3ReadLen_ = inputBufferLen3 * sizeParamInpt;
        inputBuffer4ReadLen_ = inputBufferLen4 * sizeParamInpt;

        delay1ReadLen_ = 1 + delay1BufferSizeM1 * (1 - sizeParam_);
        delay2ReadLen_ = 1 + delay2BufferSizeM1 * (1 - sizeParam_);
        delay3ReadLen_ = 1 + delay3BufferSizeM1 * (1 - sizeParam_);
        delay4ReadLen_ = 1 + delay4BufferSizeM1 * (1 - sizeParam_);

        diffuserBuffer1ReadLen_ = -1 + diffuserBufferLen1M1 * sizeParam_;
        diffuserBuffer2ReadLen_ = -1 + diffuserBufferLen2M1 * sizeParam_;
        diffuserBuffer3ReadLen_ = -1 + diffuserBufferLen3M1 * sizeParam_;
        diffuserBuffer4ReadLen_ = -1 + diffuserBufferLen4M1 * sizeParam_;

        diffuserBuffer1ReadLen_b_ = diffuserBuffer1ReadLen_ * (0.1333f     * 1.5f);
        diffuserBuffer2ReadLen_b_ = diffuserBuffer2ReadLen_ * (0.2237f     * 1.5f);
        diffuserBuffer3ReadLen_b_ = diffuserBuffer3ReadLen_ * (0.18f         * 1.5f);
        diffuserBuffer4ReadLen_b_ = diffuserBuffer4ReadLen_ * (0.4237f     * 1.5f);
    }
    prevSizeParam_ = sizeParam_;

}

/**
 * add timbre block to bus mix
 */
void FxBus::mixAdd(float *inStereo, float send, float reverbLevel) {
    if (send > 0) {
        const float level = - panTable[(int)(send * 255)] * 0.0625f * reverbLevel;
        
        totalSent_ += level;

        sample_ = getSampleBlock();
        for (int s = 0; s < 8; s++) {
            *(sample_++) += *inStereo++ * level;
            *(sample_++) += *inStereo++ * level;
            *(sample_++) += *inStereo++ * level;
            *(sample_++) += *inStereo++ * level;
            *(sample_++) += *inStereo++ * level;
            *(sample_++) += *inStereo++ * level;
            *(sample_++) += *inStereo++ * level;
            *(sample_++) += *inStereo++ * level;
        }
    }
}

/**
 * process fx on bus mix
 */
void FxBus::processBlock(int32_t *outBuff) {

    if(totalSent_ == 0.0f) {
        return;
    }

    sample_ = getSampleBlock();

    for (int s = 0; s < BLOCK_SIZE; s++) {

        // --- audio in

        inR_ = *(sample_);
        inL_ = *(sample_ + 1);

        /*dcBlock1a = inR - dcBlock1b + dcBlockerCoef1 * dcBlock1a;            // dc blocker
        dcBlock1b = inR;

        dcBlock2a = inL - dcBlock2b + dcBlockerCoef1 * dcBlock2a;            // dc blocker
        dcBlock2b = inL;*/

        monoIn_ = (inR_ + inL_);


        // allpass / notch

        lowL_ = coef1L_     * (lowL_ + monoIn_) - bandL_;
        bandL_ = monoIn_;
        lowL2_ = coef2L_     * (lowL2_ + lowL_) - bandL2_;
        bandL2_ = lowL_;
        lowL3_ = coef3L_     * (lowL3_ + lowL2_) - bandL3_;
        bandL3_ = lowL2_;
        lowL4_ = coef4L_     * (lowL4_ + lowL3_) - bandL4_;
        bandL4_ = lowL3_;

        monoIn_ += lowL4_;

        // --- hi pass

        hp_in_x0_     = monoIn_;
        hp_in_y0_     = _in_a0_ * hp_in_x0_ + _in_a1_ * hp_in_x1_ + _in_b1_ * hp_in_y1_;
        hp_in_y1_     = hp_in_y0_;
        hp_in_x1_     = hp_in_x0_;
        monoIn_         = hp_in_y0_;

        // --- low pass

        inLpF_ =  _in_lp_a_ * monoIn_ + inLpF_ * _in_lp_b_;
        monoIn_ = inLpF_;

        //--- pre delay

        predelayBuffer[predelayWritePos_] = monoIn_;

        predelayReadPos_ = modulo2(predelayWritePos_ - predelaySize_, predelayBufferSize);
        preDelayOut_ = delayInterpolation(predelayReadPos_, predelayBuffer, predelayBufferSizeM1);
        monoIn_ = predelayMixAttn_ * preDelayOut_ + (1 - predelayMixAttn_) * monoIn_;

        // --- input diffuser

        // ---- diffuser 1

        inputReadPos1_     = modulo2(inputWritePos1_ - inputBuffer1ReadLen_, inputBufferLen1);
        float in_apSum1 = monoIn_ + inputBuffer1[inputReadPos1_] * inputCoef1_;
        diff1Out_         = inputBuffer1[inputReadPos1_] - in_apSum1 * inputCoef1_;
        inputBuffer1[inputWritePos1_] = in_apSum1;

        // ---- diffuser 2

        inputReadPos2_     = modulo2(inputWritePos2_ - inputBuffer2ReadLen_, inputBufferLen2);
        float in_apSum2 = diff1Out_ + inputBuffer2[inputReadPos2_] * inputCoef1_;
        diff2Out_         = inputBuffer2[inputReadPos2_] - in_apSum2 * inputCoef1_;
        inputBuffer2[inputWritePos2_]         = in_apSum2;

        // ---- diffuser 3

        inputReadPos3_     = modulo2(inputWritePos3_ - inputBuffer3ReadLen_, inputBufferLen3);
        float in_apSum3 = diff2Out_ + inputBuffer3[inputReadPos3_] * inputCoef2_;
        diff3Out_         = inputBuffer3[inputReadPos3_] - in_apSum3 * inputCoef2_;
        inputBuffer3[inputWritePos3_]         = in_apSum3;

        // ---- diffuser 4

        inputReadPos4_     = modulo2(inputWritePos4_ - inputBuffer4ReadLen_, inputBufferLen4);
        float in_apSum4 = diff3Out_ + inputBuffer4[inputReadPos4_] * inputCoef2_;
        diff4Out_         = inputBuffer4[inputReadPos4_] - in_apSum4 * inputCoef2_;
        inputBuffer4[inputWritePos4_]         = in_apSum4;

        monoIn_ = monoIn_ * (1 - diffusion_) + diff4Out_ * diffusion_;

        // ---- ap 1

        ap1In_ = monoIn_ + feedbackInL_ * decayFdbck_;

        diffuserReadPos1_ = modulo2(diffuserWritePos1_ - timeCvControl1_, diffuserBufferLen1);

        float inSum1     =     ap1In_ + int1_ * diffuserCoef1_;
        ap1Out_             =     int1_ - inSum1 * diffuserCoef1_;
        diffuserBuffer1[diffuserWritePos1_]         = inSum1;
        int1_ = delayAllpassInterpolation(diffuserReadPos1_, diffuserBuffer1, diffuserBufferLen1M1, int1_);

        // ----------------------------------------------< inject in delay1
        delay1Buffer[ delay1WritePos_ ]         = ap1Out_;
        // ----------------------------------------------> read delay1
        ap2In_ = delay1Buffer[ delay1ReadPos_ ];

        // ---------------------------------------------------- filter

        hp1_x0_     = ap2In_;                                        // hipass
        hp1_y0_     = _a0_ * hp1_x0_ + _a1_ * hp1_x1_ + _b1_ * hp1_y1_;
        hp1_y1_     = hp1_y0_;
        hp1_x1_     = hp1_x0_;
        ap2In_     = hp1_y0_;

        tankLp1a_ =  damp_a_ * ap2In_ + tankLp1a_ * damp_b_;            // lowpass
        tankLp1a_ =  damp_a_ * ap2In_ + tankLp1a_ * damp_b_;            // lowpass
        //tankLp1b =  damp_a * tankLp1a + tankLp1b * damp_b;        // lowpass

        ap2In_     = tankLp1a_ * decayFdbck_;

        // ---- ap 2

        diffuserReadPos2_ = modulo2(diffuserWritePos2_ - timeCvControl2_, diffuserBufferLen2);

        float inSum2     =     ap2In_ + int2_ * diffuserCoef2_;
        ap2Out_             =     int2_ - inSum2 * diffuserCoef2_;
        diffuserBuffer2[diffuserWritePos2_]         = inSum2;
        int2_ = delayAllpassInterpolation(diffuserReadPos2_, diffuserBuffer2, diffuserBufferLen2M1, int2_);

        // ----------------------------------------------< inject in delay2
        delay2Buffer[ delay2WritePos_ ]         = ap2Out_;
        // ----------------------------------------------> read delay2

        ap2Out_ = delay2Buffer[ delay2ReadPos_ ];

        // ---- ap 3

        ap3In_ = monoIn_ + feedbackInR_ * decayFdbck_;

        diffuserReadPos3_ = modulo2(diffuserWritePos3_ - timeCvControl3_, diffuserBufferLen3);

        float inSum3     =     ap3In_ + int3_ * diffuserCoef1_;
        ap3Out_             =     int3_ - inSum3 * diffuserCoef1_;
        diffuserBuffer3[diffuserWritePos3_]         = inSum3;
        int3_ = delayAllpassInterpolation(diffuserReadPos3_, diffuserBuffer3, diffuserBufferLen3M1, int3_);

        // ----------------------------------------------< inject in delay3
        delay3Buffer[ delay3WritePos_ ]         = ap3Out_;
        // ----------------------------------------------> read delay3
        ap4In_ = delay3Buffer[ delay3ReadPos_ ];

        // ---------------------------------------------------- filter

        hp2_x0_     = ap4In_;                                        // hipass
        hp2_y0_     = _a0_ * hp2_x0_ + _a1_ * hp2_x1_ + _b1_ * hp2_y1_;
        hp2_y1_     = hp2_y0_;
        hp2_x1_     = hp2_x0_;
        ap4In_     = hp2_y0_;

        tankLp2a_ =  damp_a_ * ap4In_ + tankLp2a_ * damp_b_;            // lowpass
        tankLp2a_ =  damp_a_ * ap4In_ + tankLp2a_ * damp_b_;            // lowpass
        //tankLp2b =  damp_a * tankLp2b + tankLp2b * damp_b;        // lowpass

        ap4In_ = tankLp2a_ * decayFdbck_;                // decay

        // ---- ap 4

        diffuserReadPos4_ = modulo2(diffuserWritePos4_ - timeCvControl4_, diffuserBufferLen4);

        float inSum4     =     ap4In_ + int4_ * diffuserCoef2_;
        ap4Out_             =     int4_ - inSum4 * diffuserCoef2_;
        diffuserBuffer4[diffuserWritePos4_]         = inSum4;
        int4_ = delayAllpassInterpolation(diffuserReadPos4_, diffuserBuffer4, diffuserBufferLen4M1, int4_);

        // ----------------------------------------------< inject in delay4
        delay4Buffer[ delay4WritePos_ ]         = ap4Out_;
        // ----------------------------------------------> read delay4
        ap4Out_ = delay4Buffer[ delay4ReadPos_ ];

        // ================================================  mix out

        feedbackInL_ = ap4Out_;
        feedbackInR_ = ap2Out_;

        outL_ = ap1Out_;
        outL_ += delay1Buffer[         modulo(_kLeftTaps_[0] + delay1WritePos_,         delay1BufferSize)        ];
        outL_ += delay1Buffer[         modulo(_kLeftTaps_[1] + delay1WritePos_,         delay1BufferSize)        ];
        outL_ -= diffuserBuffer2[     modulo(_kLeftTaps_[2] + diffuserWritePos2_,     diffuserBufferLen2)        ];
        outL_ += delay2Buffer[         modulo(_kLeftTaps_[3] + delay2WritePos_,         delay2BufferSize)        ];
        outL_ -= delay3Buffer[         modulo(_kLeftTaps_[4] + delay3WritePos_,         delay3BufferSize)        ];
        outL_ -= diffuserBuffer4[     modulo(_kLeftTaps_[5] + diffuserWritePos4_,     diffuserBufferLen4)        ];
        outL_ -= delay4Buffer[         modulo(_kLeftTaps_[6] + delay4WritePos_,         delay4BufferSize)        ];

        outR_ = ap3Out_;
        outR_ += delay3Buffer[         modulo(_kRightTaps_[0] + delay3WritePos_,     delay3BufferSize)         ];
        outR_ += delay3Buffer[         modulo(_kRightTaps_[1] + delay3WritePos_,     delay3BufferSize)         ];
        outR_ -= diffuserBuffer4[     modulo(_kRightTaps_[2] + diffuserWritePos4_,     diffuserBufferLen4)     ];
        outR_ += delay4Buffer[         modulo(_kRightTaps_[3] + delay4WritePos_,     delay4BufferSize)         ];
        outR_ -= delay1Buffer[         modulo(_kRightTaps_[4] + delay1WritePos_,     delay1BufferSize)         ];
        outR_ -= diffuserBuffer2[     modulo(_kRightTaps_[5] + diffuserWritePos2_,     diffuserBufferLen2)     ];
        outR_ -= delay2Buffer[         modulo(_kRightTaps_[6] + delay2WritePos_,     delay2BufferSize)         ];

        *(outBuff++) += (int32_t) ( outL_ * sampleMultipler_);
        *(outBuff++) += (int32_t) ( outR_ * sampleMultipler_);

        // ================================================ index increment

        sample_ += 2;

        inputWritePos1_        = modulo(inputWritePos1_ + 1 , inputBufferLen1);
        inputWritePos2_        = modulo(inputWritePos2_ + 1 , inputBufferLen2);
        inputWritePos3_        = modulo(inputWritePos3_ + 1 , inputBufferLen3);
        inputWritePos4_        = modulo(inputWritePos4_ + 1 , inputBufferLen4);

        if(++inputIncCount_ >= inputRateDivider_) {
            inputIncCount_ = 0;
            predelayWritePos_    = modulo(predelayWritePos_ + 1 , predelayBufferSize);
        }

        diffuserWritePos1_    = modulo(diffuserWritePos1_ + 1 , diffuserBufferLen1);
        diffuserWritePos2_    = modulo(diffuserWritePos2_ + 1 , diffuserBufferLen2);
        diffuserWritePos3_    = modulo(diffuserWritePos3_ + 1 , diffuserBufferLen3);
        diffuserWritePos4_    = modulo(diffuserWritePos4_ + 1 , diffuserBufferLen4);

        delay1WritePos_        = modulo(delay1WritePos_ + 1 , delay1BufferSize);
        delay2WritePos_        = modulo(delay2WritePos_ + 1 , delay2BufferSize);
        delay3WritePos_        = modulo(delay3WritePos_ + 1 , delay3BufferSize);
        delay4WritePos_        = modulo(delay4WritePos_ + 1 , delay4BufferSize);

        delay1ReadPos_         = modulo(delay1WritePos_ + delay1ReadLen_, delay1BufferSize);
        delay2ReadPos_         = modulo(delay2WritePos_ + delay2ReadLen_, delay2BufferSize);
        delay3ReadPos_         = modulo(delay3WritePos_ + delay3ReadLen_, delay3BufferSize);
        delay4ReadPos_         = modulo(delay4WritePos_ + delay4ReadLen_, delay4BufferSize);

        // --- lfo increment

        lfoProcess(&lfo1_, &lfo1tri_, &lfo1Inc_);
        lfoProcess(&lfo2_, &lfo2tri_, &lfo2Inc_);
        lfoProcess(&lfo3_, &lfo3tri_, &lfo3Inc_);
        lfoProcess(&lfo4_, &lfo4tri_, &lfo4Inc_);

        // -------- mods :

        timeCvControl1_ = clamp(diffuserBufferLen1 - diffuserBuffer1ReadLen_      +      lfo1_      * lfoDepth_ * diffuserBuffer1ReadLen_b_, -diffuserBufferLen1M1, diffuserBufferLen1);
        timeCvControl2_ = clamp(diffuserBufferLen2 - diffuserBuffer2ReadLen_      +     lfo2_     * lfoDepth_ * diffuserBuffer2ReadLen_b_, -diffuserBufferLen2M1, diffuserBufferLen2);
        timeCvControl3_ = clamp(diffuserBufferLen3 - diffuserBuffer3ReadLen_      +      lfo3_    * lfoDepth_ * diffuserBuffer3ReadLen_b_, -diffuserBufferLen3M1, diffuserBufferLen3);
        timeCvControl4_ = clamp(diffuserBufferLen4 - diffuserBuffer4ReadLen_      +     lfo4_    * lfoDepth_ * diffuserBuffer4ReadLen_b_, -diffuserBufferLen4M1, diffuserBufferLen4);
    }

}

float FxBus::delayAllpassInterpolation(float readPos, float buffer[], int bufferLenM1, float prevVal) {
    //v[n] = VoiceL[i + 1] + (1 - frac)  * VoiceL[i] - (1 - frac)  * v[n - 1]
    int readPosInt = readPos;
    float y1 = buffer[readPosInt];
    float y0 = buffer[(unlikely(readPosInt <= 0) ? readPosInt + bufferLenM1 : readPosInt - 1)];
    float x = 1 - (readPos - floorf(readPos));
    return y1 + x * (y0 - prevVal);
}

float FxBus::delayInterpolation(float readPos, float buffer[], int bufferLenM1) {
    int readPosInt = readPos;
    float y1 = buffer[readPosInt];
    float y0 = buffer[(unlikely(readPosInt <= 0) ? readPosInt + bufferLenM1 : readPosInt - 1)];
    float x = 1 - (readPos - floorf(readPos));
    return y0 + x * (y1 - y0);
}

void FxBus::lfoProcess(float *lfo, float *lfotri, float *lfoInc) {
    *lfotri += *lfoInc * lfoSpeed_;
    if(unlikely(*lfotri >= 1)) {
        *lfotri = 1;
        *lfoInc = -*lfoInc;
    }
    if(unlikely(*lfotri <= 0)) {
        *lfotri = 0;
        *lfoInc = -*lfoInc;
    }
    *lfo = (*lfo * lfoLpCoef1_ + *lfotri ) * lfoLpCoef2_;
}
