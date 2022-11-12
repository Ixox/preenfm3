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

float FxBus::predelayBuffer[predelayBufferSize] __attribute__((section(".ram_d1")));

float FxBus::inputBuffer1[inputBufferLen1] __attribute__((section(".ram_d2")));
float FxBus::inputBuffer2[inputBufferLen2] __attribute__((section(".ram_d2")));
float FxBus::inputBuffer3[inputBufferLen3] __attribute__((section(".ram_d2")));
float FxBus::inputBuffer4[inputBufferLen4] __attribute__((section(".ram_d2")));

float FxBus::diffuserBuffer1[diffuserBufferLen1] __attribute__((section(".ram_d2")));
float FxBus::diffuserBuffer2[diffuserBufferLen2] __attribute__((section(".ram_d2")));
float FxBus::diffuserBuffer3[diffuserBufferLen3] __attribute__((section(".ram_d2")));
float FxBus::diffuserBuffer4[diffuserBufferLen4] __attribute__((section(".ram_d2")));

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
    masterfxConfig[GLOBALFX_PREDELAYTIME] = GLOBALFX_PREDELAYTIME_DEFAULT;
    masterfxConfig[GLOBALFX_PREDELAYMIX] = GLOBALFX_PREDELAYMIX_DEFAULT;
    masterfxConfig[GLOBALFX_SIZE] = GLOBALFX_SIZE_DEFAULT;
    masterfxConfig[GLOBALFX_DIFFUSION] = GLOBALFX_DIFFUSION_DEFAULT;
    masterfxConfig[GLOBALFX_DAMPING] = GLOBALFX_DAMPING_DEFAULT;
    masterfxConfig[GLOBALFX_DECAY] = GLOBALFX_DECAY_DEFAULT;
    masterfxConfig[GLOBALFX_LFODEPTH] = GLOBALFX_LFODEPTH_DEFAULT;
    masterfxConfig[GLOBALFX_LFOSPEED] = GLOBALFX_LFOSPEED_DEFAULT;
    masterfxConfig[GLOBALFX_INPUTBASE] = GLOBALFX_INPUTBASE_DEFAULT;
    masterfxConfig[GLOBALFX_INPUTWIDTH] = GLOBALFX_INPUTWIDTH_DEFAULT;
    masterfxConfig[GLOBALFX_NOTCHBASE] = GLOBALFX_NOTCHBASE_DEFAULT;
    masterfxConfig[GLOBALFX_NOTCHSPREAD] = GLOBALFX_NOTCHSPREAD_DEFAULT;
    masterfxConfig[GLOBALFX_LOOPHP] = GLOBALFX_LOOPHP_DEFAULT;
}

/**
 * init before timbres summing
 */
void FxBus::mixSumInit() {

    if(totalSent == 0.0f) {
        return;
    }

    sample = getSampleBlock();

    for (int s = 0; s < 8; s++) {
        *(sample++) = 0;
        *(sample++) = 0;
        *(sample++) = 0;
        *(sample++) = 0;
        *(sample++) = 0;
        *(sample++) = 0;
        *(sample++) = 0;
        *(sample++) = 0;
    }

    totalSent = 0.0f;

    if (somethingChanged) {
        if (waitCountBeforeChange-- == 0 ) {
            somethingChanged = false;
            if (currentPresetNum != nextPresetNum) {
                presetChanged(nextPresetNum);
            } else {
                paramChanged();
            }
        }
    }
}

void FxBus::slowParamChange() {
    somethingChanged = true;
    waitCountBeforeChange = 100;
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
            masterfxConfig[GLOBALFX_NOTCHBASE] = 0.57f;
            masterfxConfig[GLOBALFX_LFOSPEED] = 0.1f;
            masterfxConfig[GLOBALFX_LFODEPTH] = 0.9;
            masterfxConfig[GLOBALFX_LOOPHP] = 0.55f;

            switch (presetNum)
            {
            case 0:
                masterfxConfig[GLOBALFX_SIZE] = 0.1f;
                masterfxConfig[GLOBALFX_DECAY] = 0.04f;
                masterfxConfig[GLOBALFX_DIFFUSION] = 0.68f;
                break;
            case 1:
                masterfxConfig[GLOBALFX_SIZE] = 0.13f;
                masterfxConfig[GLOBALFX_DECAY] = 0.02f;
                masterfxConfig[GLOBALFX_DIFFUSION] = 0.86f;
                break;
            case 2:
                masterfxConfig[GLOBALFX_SIZE] = 0.23f;
                masterfxConfig[GLOBALFX_DECAY] = 0.08f;
                masterfxConfig[GLOBALFX_DIFFUSION] = 1;
                break;
            default:
                break;
            }
            break;
            case 1:
                //small
                masterfxConfig[GLOBALFX_SIZE] = 0.26f;
                masterfxConfig[GLOBALFX_DECAY] = 0.1f;
                masterfxConfig[GLOBALFX_DIFFUSION] = 0.68f;
                masterfxConfig[GLOBALFX_LFOSPEED] = 0.55f;
                masterfxConfig[GLOBALFX_LFODEPTH] = 0.35f;
                masterfxConfig[GLOBALFX_NOTCHBASE] = 0.05f;
                masterfxConfig[GLOBALFX_LOOPHP] = 0.3f;
                break;
            case 2:
                //medium
                masterfxConfig[GLOBALFX_SIZE] = 0.465f;
                masterfxConfig[GLOBALFX_DECAY] = 0.18f;
                masterfxConfig[GLOBALFX_DIFFUSION] = 0.57f;
                masterfxConfig[GLOBALFX_LFOSPEED] = 0.57f;
                masterfxConfig[GLOBALFX_LFODEPTH] = 0.28f;
                masterfxConfig[GLOBALFX_NOTCHBASE] = 0.22f;
                masterfxConfig[GLOBALFX_LOOPHP] = 0.3f;
                break;
            case 3:
                //large
                masterfxConfig[GLOBALFX_SIZE] = 0.775f;
                masterfxConfig[GLOBALFX_DECAY] = 0.38f;
                masterfxConfig[GLOBALFX_DIFFUSION] = 0.56f;
                masterfxConfig[GLOBALFX_LFOSPEED] = 0.57f;
                masterfxConfig[GLOBALFX_LFODEPTH] = 0.21f;
                masterfxConfig[GLOBALFX_NOTCHBASE] = 0.05f;
                masterfxConfig[GLOBALFX_LOOPHP] = 0.3f;
                break;
            case 4:
                //xtra large
                masterfxConfig[GLOBALFX_SIZE] = 0.87f;
                masterfxConfig[GLOBALFX_DECAY] = 0.72f;
                masterfxConfig[GLOBALFX_DIFFUSION] = 0.68f;
                masterfxConfig[GLOBALFX_LFOSPEED] = 0.57f;
                masterfxConfig[GLOBALFX_LFODEPTH] = 0.21f;
                masterfxConfig[GLOBALFX_NOTCHBASE] = 0.02f;
                masterfxConfig[GLOBALFX_LOOPHP] = 0.35f;
                break;
            default:
                break;
        }

        switch (brightness)
        {
        case 0:
            masterfxConfig[GLOBALFX_DAMPING] = 0.19f;
            break;
        case 1:
            masterfxConfig[GLOBALFX_DAMPING] = 0.42f * (1 - masterfxConfig[GLOBALFX_SIZE] * 0.06f);
            break;
        case 2:
            masterfxConfig[GLOBALFX_DAMPING] = 0.8f * (1 - masterfxConfig[GLOBALFX_SIZE] * 0.15f);
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
            masterfxConfig[GLOBALFX_DAMPING] = 0.96f;
            masterfxConfig[GLOBALFX_LFOSPEED] = 0.91f;
            masterfxConfig[GLOBALFX_LFODEPTH] = 0.05f;
            masterfxConfig[GLOBALFX_SIZE] = 1;
            masterfxConfig[GLOBALFX_DECAY] = 0.943f;
            masterfxConfig[GLOBALFX_DIFFUSION] = 0.96f;
            masterfxConfig[GLOBALFX_NOTCHBASE] = 0.245f;
            masterfxConfig[GLOBALFX_LOOPHP] = 0.1f;
            break;
        case 16:
            //hall
            masterfxConfig[GLOBALFX_DAMPING] = 0.57f;
            masterfxConfig[GLOBALFX_LFOSPEED] = 0.28f;
            masterfxConfig[GLOBALFX_LFODEPTH] = 0.27f;
            masterfxConfig[GLOBALFX_SIZE] = 0.72f;
            masterfxConfig[GLOBALFX_DECAY] = 0.49f;
            masterfxConfig[GLOBALFX_DIFFUSION] = 0.66f;
            masterfxConfig[GLOBALFX_NOTCHBASE] = 0.06f;
            masterfxConfig[GLOBALFX_LOOPHP] = 0.15f;
            break;
        case 17:
            //cave
            masterfxConfig[GLOBALFX_DAMPING] = 0.38f;
            masterfxConfig[GLOBALFX_LFOSPEED] = 0.37f;
            masterfxConfig[GLOBALFX_LFODEPTH] = 0.33f;
            masterfxConfig[GLOBALFX_SIZE] = 0.94f;
            masterfxConfig[GLOBALFX_DECAY] = 0.43f;
            masterfxConfig[GLOBALFX_DIFFUSION] = 0.8f;
            masterfxConfig[GLOBALFX_NOTCHBASE] = 0.11f;
            masterfxConfig[GLOBALFX_LOOPHP] = 0.29f;
            break;
        case 18:
            //apartment
            masterfxConfig[GLOBALFX_DAMPING] = 0.44f;
            masterfxConfig[GLOBALFX_LFOSPEED] = 0.33f;
            masterfxConfig[GLOBALFX_LFODEPTH] = 0.33f;
            masterfxConfig[GLOBALFX_SIZE] = 0.6f;
            masterfxConfig[GLOBALFX_DECAY] = 0.22f;
            masterfxConfig[GLOBALFX_DIFFUSION] = 0.72f;
            masterfxConfig[GLOBALFX_NOTCHBASE] = 0.15f;
            masterfxConfig[GLOBALFX_LOOPHP] = 0.34f;
            break;
        default:
            break;
        }
    }
    currentPresetNum = presetNum;
    nextPresetNum = presetNum;

    paramChanged();
}


void FxBus::paramChanged() {
    float temp;

    inputWidth         =     masterfxConfig[GLOBALFX_INPUTWIDTH];
    float filterBase = masterfxConfig[GLOBALFX_INPUTBASE];

    if (prevFilterBase != filterBase || prevInputWidth != inputWidth)
    {
        float filterB2     = filterBase * filterBase;
        float filterB     = (filterB2 * filterB2 * 0.5f);

        // input hp coefs calc :
        _in_b1 = (1 - filterB);// expf(-_2M_PI * _cutoffFreq * _1_sampleRate)
        _in_a0 = (1 + _in_b1 * _in_b1 * _in_b1) * 0.5f;
        _in_a1 = -_in_a0;

        // input lp coefs calc :
        filterBase = clamp(filterBase + inputWidth, 0, 1);
        float filterA2     = filterBase * filterBase;
        float filterA     = (filterA2 * filterA2 * 0.5f);

        _in_lp_b = 1 - filterA;
        _in_lp_a = 1 - _in_lp_b;
    }
    prevFilterBase = filterBase;
    prevInputWidth = inputWidth;

    float loopHp = masterfxConfig[GLOBALFX_LOOPHP];
    if (prevLoopHp != loopHp)
    {
        float filterB2     = loopHp * loopHp;
        float filterB     = (filterB2 * filterB2 * 0.5f);

        // tank hp coefs calc :
        _b1 = (1 - filterB);
        _a0 = (1 + _b1 * _b1 * _b1) * 0.5f;
        _a1 = -_a0;
    }
    prevLoopHp = loopHp;


    fxTimeLinear = masterfxConfig[GLOBALFX_PREDELAYTIME];
    if (prevFxTimeLinear != fxTimeLinear)
    {
        fxTime     = clamp(fxTimeLinear, 0.0003f, 0.9996f);
        fxTime     *= fxTime * fxTime;
        predelaySize = fxTimeLinear * predelayBufferSizeM1;
   }
    prevFxTimeLinear = fxTimeLinear;

    predelayMixLevel = masterfxConfig[GLOBALFX_PREDELAYMIX];
    predelayMixAttn = predelayMixLevel * (1 - (predelayMixLevel * predelayMixLevel * 0.1f));

    lfoSpeedLinear = masterfxConfig[GLOBALFX_LFOSPEED];
    if (prevLfoSpeedLinear != lfoSpeedLinear)
    {
        temp = lfoSpeedLinear;
        temp *= temp * temp;
        lfoSpeed = temp;
    }
    prevLfoSpeedLinear = lfoSpeedLinear;

    temp = masterfxConfig[GLOBALFX_LFODEPTH];
    temp = temp * (1 - lfoSpeedLinear * 0.5f) * (1 - sizeParam * 0.5f);
    lfoDepth = temp;

    // ------ page 2

    sizeParam     = clamp(masterfxConfig[GLOBALFX_SIZE], 0.03f, 1);

    diffusion         =     masterfxConfig[GLOBALFX_DIFFUSION];
    damping         =     masterfxConfig[GLOBALFX_DAMPING];

    decayVal         =     masterfxConfig[GLOBALFX_DECAY ];
    notchBase         =     masterfxConfig[GLOBALFX_NOTCHBASE ];
    notchSpread     =     masterfxConfig[GLOBALFX_NOTCHSPREAD];

    //------- some process

    if(damping != prevDamping) {
        damp_b = 1 - damping  * damping;
        damp_a = 1 - damp_b;
    }
    prevDamping = damping;

    if(prevDecayVal != decayVal) {
        decayFdbck =  fastroot(decayVal, 3) * decayMaxVal;

        float decayValSquare = decayVal * decayVal;

        headRoomMultiplier = (1 - decayValSquare * 0.5f) * 5.5f;
        sampleMultipler = headRoomMultiplier * (float) 0x7fffff;
    }
    prevDecayVal = decayVal;

    if(notchBase != prevNotchBase || notchSpread != prevNotchSpread) {
        float notchB =  notchBase * 0.25f;
        float offset = (notchSpread) * 0.125f;
        float range = 0.67f;

        f1L = clamp( fold(notchB + offset)             * range, windowMin, windowMax);
        f2L = clamp( fold(notchB - offset)             * range, windowMin, windowMax);
        f3L = clamp( fold(notchB + offset * 1.5f)     * range, windowMin, windowMax);
        f4L = clamp( fold(notchB + offset * 1.8f)     * range, windowMin, windowMax);

        coef1L = (1.0f - f1L) / (1.0f + f1L);
        coef2L = (1.0f - f2L) / (1.0f + f2L);
        coef3L = (1.0f - f3L) / (1.0f + f3L);
        coef4L = (1.0f - f4L) / (1.0f + f4L);
    }
    prevNotchBase = notchBase;
    prevNotchSpread = notchSpread;

    if(sizeParam != prevSizeParam) {
        float sizeSqrt = sqrt3(sizeParam);

        float sizeParamInpt = 0.1f + sizeSqrt * 0.9f;
        inputBuffer1ReadLen = inputBufferLen1 * sizeParamInpt;
        inputBuffer2ReadLen = inputBufferLen2 * sizeParamInpt;
        inputBuffer3ReadLen = inputBufferLen3 * sizeParamInpt;
        inputBuffer4ReadLen = inputBufferLen4 * sizeParamInpt;

        delay1ReadLen = 1 + delay1BufferSizeM1 * (1 - sizeParam);
        delay2ReadLen = 1 + delay2BufferSizeM1 * (1 - sizeParam);
        delay3ReadLen = 1 + delay3BufferSizeM1 * (1 - sizeParam);
        delay4ReadLen = 1 + delay4BufferSizeM1 * (1 - sizeParam);

        diffuserBuffer1ReadLen = -1 + diffuserBufferLen1M1 * sizeParam;
        diffuserBuffer2ReadLen = -1 + diffuserBufferLen2M1 * sizeParam;
        diffuserBuffer3ReadLen = -1 + diffuserBufferLen3M1 * sizeParam;
        diffuserBuffer4ReadLen = -1 + diffuserBufferLen4M1 * sizeParam;

        diffuserBuffer1ReadLen_b = diffuserBuffer1ReadLen * (0.1333f     * 1.5f);
        diffuserBuffer2ReadLen_b = diffuserBuffer2ReadLen * (0.2237f     * 1.5f);
        diffuserBuffer3ReadLen_b = diffuserBuffer3ReadLen * (0.18f         * 1.5f);
        diffuserBuffer4ReadLen_b = diffuserBuffer4ReadLen * (0.4237f     * 1.5f);
    }
    prevSizeParam = sizeParam;

}

/**
 * add timbre block to bus mix
 */
void FxBus::mixAdd(float *inStereo, float send, float reverbLevel) {
    if (send > 0) {
        const float level = - panTable[(int)(send * 128)] * 0.0625f * reverbLevel;
        
        totalSent += level;

        sample = getSampleBlock();
        for (int s = 0; s < 8; s++) {
            *(sample++) += *inStereo++ * level;
            *(sample++) += *inStereo++ * level;
            *(sample++) += *inStereo++ * level;
            *(sample++) += *inStereo++ * level;
            *(sample++) += *inStereo++ * level;
            *(sample++) += *inStereo++ * level;
            *(sample++) += *inStereo++ * level;
            *(sample++) += *inStereo++ * level;
        }
    }
}

/**
 * process fx on bus mix
 */
void FxBus::processBlock(int32_t *outBuff) {

    if(totalSent == 0.0f) {
        return;
    }

    sample = getSampleBlock();

    for (int s = 0; s < BLOCK_SIZE; s++) {

        // --- audio in

        inR = *(sample);
        inL = *(sample + 1);

        /*dcBlock1a = inR - dcBlock1b + dcBlockerCoef1 * dcBlock1a;            // dc blocker
        dcBlock1b = inR;

        dcBlock2a = inL - dcBlock2b + dcBlockerCoef1 * dcBlock2a;            // dc blocker
        dcBlock2b = inL;*/

        monoIn = (inR + inL);


        // allpass / notch

        lowL = coef1L     * (lowL + monoIn) - bandL;
        bandL = monoIn;
        lowL2 = coef2L     * (lowL2 + lowL) - bandL2;
        bandL2 = lowL;
        lowL3 = coef3L     * (lowL3 + lowL2) - bandL3;
        bandL3 = lowL2;
        lowL4 = coef4L     * (lowL4 + lowL3) - bandL4;
        bandL4 = lowL3;

        monoIn += lowL4;

        // --- hi pass

        hp_in_x0     = monoIn;
        hp_in_y0     = _in_a0 * hp_in_x0 + _in_a1 * hp_in_x1 + _in_b1 * hp_in_y1;
        hp_in_y1     = hp_in_y0;
        hp_in_x1     = hp_in_x0;
        monoIn         = hp_in_y0;

        // --- low pass

        inLpF =  _in_lp_a * monoIn + inLpF * _in_lp_b;
        monoIn = inLpF;

        //--- pre delay

        predelayBuffer[predelayWritePos] = monoIn;

        predelayReadPos = modulo2(predelayWritePos - predelaySize, predelayBufferSize);
        preDelayOut = delayInterpolation(predelayReadPos, predelayBuffer, predelayBufferSizeM1);
        monoIn = predelayMixAttn * preDelayOut + (1 - predelayMixAttn) * monoIn;

        // --- input diffuser

        // ---- diffuser 1

        inputReadPos1     = modulo2(inputWritePos1 - inputBuffer1ReadLen, inputBufferLen1);
        float in_apSum1 = monoIn + inputBuffer1[inputReadPos1] * inputCoef1;
        diff1Out         = inputBuffer1[inputReadPos1] - in_apSum1 * inputCoef1;
        inputBuffer1[inputWritePos1] = in_apSum1;

        // ---- diffuser 2

        inputReadPos2     = modulo2(inputWritePos2 - inputBuffer2ReadLen, inputBufferLen2);
        float in_apSum2 = diff1Out + inputBuffer2[inputReadPos2] * inputCoef1;
        diff2Out         = inputBuffer2[inputReadPos2] - in_apSum2 * inputCoef1;
        inputBuffer2[inputWritePos2]         = in_apSum2;

        // ---- diffuser 3

        inputReadPos3     = modulo2(inputWritePos3 - inputBuffer3ReadLen, inputBufferLen3);
        float in_apSum3 = diff2Out + inputBuffer3[inputReadPos3] * inputCoef2;
        diff3Out         = inputBuffer3[inputReadPos3] - in_apSum3 * inputCoef2;
        inputBuffer3[inputWritePos3]         = in_apSum3;

        // ---- diffuser 4

        inputReadPos4     = modulo2(inputWritePos4 - inputBuffer4ReadLen, inputBufferLen4);
        float in_apSum4 = diff3Out + inputBuffer4[inputReadPos4] * inputCoef2;
        diff4Out         = inputBuffer4[inputReadPos4] - in_apSum4 * inputCoef2;
        inputBuffer4[inputWritePos4]         = in_apSum4;

        monoIn = monoIn * (1 - diffusion) + diff4Out * diffusion;

        // ---- ap 1

        ap1In = monoIn + feedbackInL * decayFdbck;

        diffuserReadPos1 = modulo2(diffuserWritePos1 - timeCvControl1, diffuserBufferLen1);

        float inSum1     =     ap1In + int1 * diffuserCoef1;
        ap1Out             =     int1 - inSum1 * diffuserCoef1;
        diffuserBuffer1[diffuserWritePos1]         = inSum1;
        int1 = delayAllpassInterpolation(diffuserReadPos1, diffuserBuffer1, diffuserBufferLen1M1, int1);

        // ----------------------------------------------< inject in delay1
        delay1Buffer[ delay1WritePos ]         = ap1Out;
        // ----------------------------------------------> read delay1
        ap2In = delay1Buffer[ delay1ReadPos ];

        // ---------------------------------------------------- filter

        hp1_x0     = ap2In;                                        // hipass
        hp1_y0     = _a0 * hp1_x0 + _a1 * hp1_x1 + _b1 * hp1_y1;
        hp1_y1     = hp1_y0;
        hp1_x1     = hp1_x0;
        ap2In     = hp1_y0;

        tankLp1a =  damp_a * ap2In + tankLp1a * damp_b;            // lowpass
        tankLp1a =  damp_a * ap2In + tankLp1a * damp_b;            // lowpass
        //tankLp1b =  damp_a * tankLp1a + tankLp1b * damp_b;        // lowpass

        ap2In     = tankLp1a * decayFdbck;

        // ---- ap 2

        diffuserReadPos2 = modulo2(diffuserWritePos2 - timeCvControl2, diffuserBufferLen2);

        float inSum2     =     ap2In + int2 * diffuserCoef2;
        ap2Out             =     int2 - inSum2 * diffuserCoef2;
        diffuserBuffer2[diffuserWritePos2]         = inSum2;
        int2 = delayAllpassInterpolation(diffuserReadPos2, diffuserBuffer2, diffuserBufferLen2M1, int2);

        // ----------------------------------------------< inject in delay2
        delay2Buffer[ delay2WritePos ]         = ap2Out;
        // ----------------------------------------------> read delay2

        ap2Out = delay2Buffer[ delay2ReadPos ];

        // ---- ap 3

        ap3In = monoIn + feedbackInR * decayFdbck;

        diffuserReadPos3 = modulo2(diffuserWritePos3 - timeCvControl3, diffuserBufferLen3);

        float inSum3     =     ap3In + int3 * diffuserCoef1;
        ap3Out             =     int3 - inSum3 * diffuserCoef1;
        diffuserBuffer3[diffuserWritePos3]         = inSum3;
        int3 = delayAllpassInterpolation(diffuserReadPos3, diffuserBuffer3, diffuserBufferLen3M1, int3);

        // ----------------------------------------------< inject in delay3
        delay3Buffer[ delay3WritePos ]         = ap3Out;
        // ----------------------------------------------> read delay3
        ap4In = delay3Buffer[ delay3ReadPos ];

        // ---------------------------------------------------- filter

        hp2_x0     = ap4In;                                        // hipass
        hp2_y0     = _a0 * hp2_x0 + _a1 * hp2_x1 + _b1 * hp2_y1;
        hp2_y1     = hp2_y0;
        hp2_x1     = hp2_x0;
        ap4In     = hp2_y0;

        tankLp2a =  damp_a * ap4In + tankLp2a * damp_b;            // lowpass
        tankLp2a =  damp_a * ap4In + tankLp2a * damp_b;            // lowpass
        //tankLp2b =  damp_a * tankLp2b + tankLp2b * damp_b;        // lowpass

        ap4In = tankLp2a * decayFdbck;                // decay

        // ---- ap 4

        diffuserReadPos4 = modulo2(diffuserWritePos4 - timeCvControl4, diffuserBufferLen4);

        float inSum4     =     ap4In + int4 * diffuserCoef2;
        ap4Out             =     int4 - inSum4 * diffuserCoef2;
        diffuserBuffer4[diffuserWritePos4]         = inSum4;
        int4 = delayAllpassInterpolation(diffuserReadPos4, diffuserBuffer4, diffuserBufferLen4M1, int4);

        // ----------------------------------------------< inject in delay4
        delay4Buffer[ delay4WritePos ]         = ap4Out;
        // ----------------------------------------------> read delay4
        ap4Out = delay4Buffer[ delay4ReadPos ];

        // ================================================  mix out

        feedbackInL = ap4Out;
        feedbackInR = ap2Out;

        outL = ap1Out;
        outL += delay1Buffer[         modulo(_kLeftTaps[0] + delay1WritePos,         delay1BufferSize)        ];
        outL += delay1Buffer[         modulo(_kLeftTaps[1] + delay1WritePos,         delay1BufferSize)        ];
        outL -= diffuserBuffer2[     modulo(_kLeftTaps[2] + diffuserWritePos2,     diffuserBufferLen2)        ];
        outL += delay2Buffer[         modulo(_kLeftTaps[3] + delay2WritePos,         delay2BufferSize)        ];
        outL -= delay3Buffer[         modulo(_kLeftTaps[4] + delay3WritePos,         delay3BufferSize)        ];
        outL -= diffuserBuffer4[     modulo(_kLeftTaps[5] + diffuserWritePos4,     diffuserBufferLen4)        ];
        outL -= delay4Buffer[         modulo(_kLeftTaps[6] + delay4WritePos,         delay4BufferSize)        ];

        outR = ap3Out;
        outR += delay3Buffer[         modulo(_kRightTaps[0] + delay3WritePos,     delay3BufferSize)         ];
        outR += delay3Buffer[         modulo(_kRightTaps[1] + delay3WritePos,     delay3BufferSize)         ];
        outR -= diffuserBuffer4[     modulo(_kRightTaps[2] + diffuserWritePos4,     diffuserBufferLen4)     ];
        outR += delay4Buffer[         modulo(_kRightTaps[3] + delay4WritePos,     delay4BufferSize)         ];
        outR -= delay1Buffer[         modulo(_kRightTaps[4] + delay1WritePos,     delay1BufferSize)         ];
        outR -= diffuserBuffer2[     modulo(_kRightTaps[5] + diffuserWritePos2,     diffuserBufferLen2)     ];
        outR -= delay2Buffer[         modulo(_kRightTaps[6] + delay2WritePos,     delay2BufferSize)         ];

        *(outBuff++) += (int32_t) ( outL * sampleMultipler);
        *(outBuff++) += (int32_t) ( outR * sampleMultipler);

        // ================================================ index increment

        sample += 2;

        inputWritePos1        = modulo(inputWritePos1 + 1 , inputBufferLen1);
        inputWritePos2        = modulo(inputWritePos2 + 1 , inputBufferLen2);
        inputWritePos3        = modulo(inputWritePos3 + 1 , inputBufferLen3);
        inputWritePos4        = modulo(inputWritePos4 + 1 , inputBufferLen4);

        predelayWritePos    = modulo(predelayWritePos + 1 , predelayBufferSize);

        diffuserWritePos1    = modulo(diffuserWritePos1 + 1 , diffuserBufferLen1);
        diffuserWritePos2    = modulo(diffuserWritePos2 + 1 , diffuserBufferLen2);
        diffuserWritePos3    = modulo(diffuserWritePos3 + 1 , diffuserBufferLen3);
        diffuserWritePos4    = modulo(diffuserWritePos4 + 1 , diffuserBufferLen4);

        delay1WritePos        = modulo(delay1WritePos + 1 , delay1BufferSize);
        delay2WritePos        = modulo(delay2WritePos + 1 , delay2BufferSize);
        delay3WritePos        = modulo(delay3WritePos + 1 , delay3BufferSize);
        delay4WritePos        = modulo(delay4WritePos + 1 , delay4BufferSize);

        delay1ReadPos         = modulo(delay1WritePos + delay1ReadLen, delay1BufferSize);
        delay2ReadPos         = modulo(delay2WritePos + delay2ReadLen, delay2BufferSize);
        delay3ReadPos         = modulo(delay3WritePos + delay3ReadLen, delay3BufferSize);
        delay4ReadPos         = modulo(delay4WritePos + delay4ReadLen, delay4BufferSize);

        // --- lfo increment

        lfoProcess(&lfo1, &lfo1tri, &lfo1Inc);
        lfoProcess(&lfo2, &lfo2tri, &lfo2Inc);
        lfoProcess(&lfo3, &lfo3tri, &lfo3Inc);
        lfoProcess(&lfo4, &lfo4tri, &lfo4Inc);

        // -------- mods :

        timeCvControl1 = clamp(diffuserBufferLen1 - diffuserBuffer1ReadLen      +      lfo1      * lfoDepth * diffuserBuffer1ReadLen_b, -diffuserBufferLen1M1, diffuserBufferLen1);
        timeCvControl2 = clamp(diffuserBufferLen2 - diffuserBuffer2ReadLen      +     lfo2     * lfoDepth * diffuserBuffer2ReadLen_b, -diffuserBufferLen2M1, diffuserBufferLen2);
        timeCvControl3 = clamp(diffuserBufferLen3 - diffuserBuffer3ReadLen      +      lfo3    * lfoDepth * diffuserBuffer3ReadLen_b, -diffuserBufferLen3M1, diffuserBufferLen3);
        timeCvControl4 = clamp(diffuserBufferLen4 - diffuserBuffer4ReadLen      +     lfo4    * lfoDepth * diffuserBuffer4ReadLen_b, -diffuserBufferLen4M1, diffuserBufferLen4);
    }

}

float FxBus::delayAllpassInterpolation(float readPos, float buffer[], int bufferLenM1, float prevVal) {
    //v[n] = VoiceL[i + 1] + (1 - frac)  * VoiceL[i] - (1 - frac)  * v[n - 1]
    int readPosInt = readPos;
    float y0 = buffer[readPosInt];
    float y1 = buffer[(unlikely(readPosInt >= bufferLenM1) ? readPosInt - bufferLenM1 + 1 : readPosInt + 1)];
    //float y1 = buffer[((readPosInt == 0 ) ? bufferLenM1: readPosInt - 1)];
    float x = readPos - floorf(readPos);
    return y1 + (1 - x) * (y0 - prevVal);
}

float FxBus::delayInterpolation(float readPos, float buffer[], int bufferLenM1) {
    int readPosInt = readPos;
    float y0 = buffer[readPosInt];
    float y1 = buffer[(unlikely(readPosInt == 0) ? bufferLenM1 : readPosInt - 1)];
    float x = readPos - floorf(readPos);
    return y0 + x * (y1 - y0);
}

void FxBus::lfoProcess(float *lfo, float *lfotri, float *lfoInc) {
    *lfotri += *lfoInc * lfoSpeed;
    if(unlikely(*lfotri >= 1)) {
        *lfotri = 1;
        *lfoInc = -*lfoInc;
    }
    if(unlikely(*lfotri <= 0)) {
        *lfotri = 0;
        *lfoInc = -*lfoInc;
    }
    *lfo = (*lfo * lfoLpCoef1 + *lfotri ) * lfoLpCoef2;
}
