/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <.> hosxe < a t > gmail.com)
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

#include "Timbre.h"
#include "MidiControlChange.h"
#include "Pfm3Param.h"
#include "SynthEngineFactory.h"
#include "SynthState.h"
#include "Voice.h"
#include "VoicePool.h"
#include <math.h>
#include <string.h>

#define INV127         .00787401574803149606f
#define INV16          .0625
#define INV_BLOCK_SIZE (1.0f / BLOCK_SIZE)

// Engine state in RAM_D1.
float midiNoteScale[2][NUMBER_OF_TIMBRES][128] __attribute__((section(".ram_d1")));
TimbreSharedParams Timbre::sharedParams_[NUMBER_OF_TIMBRES] __attribute__((section(".ram_d1")));
float Timbre::delayBuffer[NUMBER_OF_TIMBRES][delayBufferSize] __attribute__((section(".ram_d2b")));

#define CALLED_PER_SECOND (PREENFM_FREQUENCY / 32.0f)

// Static to all 6 instrument
uint32_t Timbre::voiceIndex_;

enum ArpeggiatorDirection {
    ARPEGGIO_DIRECTION_UP = 0,
    ARPEGGIO_DIRECTION_DOWN,
    ARPEGGIO_DIRECTION_UP_DOWN,
    ARPEGGIO_DIRECTION_PLAYED,
    ARPEGGIO_DIRECTION_RANDOM,
    ARPEGGIO_DIRECTION_CHORD,
    /*
     * ROTATE modes rotate the first note played, e.g. UP: C-E-G -> E-G-C -> G-C-E -> repeat
     */
    ARPEGGIO_DIRECTION_ROTATE_UP,
    ARPEGGIO_DIRECTION_ROTATE_DOWN,
    ARPEGGIO_DIRECTION_ROTATE_UP_DOWN,
    /*
     * SHIFT modes rotate and extend with transpose, e.g. UP: C-E-G -> E-G-C1 -> G-C1-E1 -> repeat
     */
    ARPEGGIO_DIRECTION_SHIFT_UP,
    ARPEGGIO_DIRECTION_SHIFT_DOWN,
    ARPEGGIO_DIRECTION_SHIFT_UP_DOWN,
    ARPEGGIO_DIRECTION_DOWN_UP,
    ARPEGGIO_DIRECTION_UP_DOWN_INC,
    ARPEGGIO_DIRECTION_CONVERGE,
    ARPEGGIO_DIRECTION_DIVERGE,
    ARPEGGIO_DIRECTION_THUMB,
    ARPEGGIO_DIRECTION_PINKY,
    ARPEGGIO_DIRECTION_RANDOM_WALK,
    ARPEGGIO_DIRECTION_STRIDE,
    ARPEGGIO_DIRECTION_LEAP,
    ARPEGGIO_DIRECTION_OCTAVE_NOTE,

    ARPEGGIO_DIRECTION_COUNT
};

// TODO Maybe add something like struct ArpDirectionParams { dir, can_change, use_start_step }

inline static int __getDirection(int _direction) {
    switch (_direction) {
    case ARPEGGIO_DIRECTION_DOWN:
    case ARPEGGIO_DIRECTION_ROTATE_DOWN:
    case ARPEGGIO_DIRECTION_SHIFT_DOWN:
    case ARPEGGIO_DIRECTION_DOWN_UP:
        return -1;
    default:
        return 1;
    }
}

inline static int __canChangeDir(int _direction) {
    switch (_direction) {
    case ARPEGGIO_DIRECTION_UP_DOWN:
    case ARPEGGIO_DIRECTION_ROTATE_UP_DOWN:
    case ARPEGGIO_DIRECTION_SHIFT_UP_DOWN:
    case ARPEGGIO_DIRECTION_DOWN_UP:
    case ARPEGGIO_DIRECTION_UP_DOWN_INC:
        return 1;
    default:
        return 0;
    }
}

inline static int __canTranspose(int _direction) {
    switch (_direction) {
    case ARPEGGIO_DIRECTION_SHIFT_UP:
    case ARPEGGIO_DIRECTION_SHIFT_DOWN:
    case ARPEGGIO_DIRECTION_SHIFT_UP_DOWN:
        return 1;
    default:
        return 0;
    }
}

enum NewNoteType { NEW_NOTE_FREE = 0, NEW_NOTE_RELEASE, NEW_NOTE_OLD, NEW_NOTE_NONE };

arp_pattern_t lut_res_arpeggiator_patterns[ARPEGGIATOR_PRESET_PATTERN_COUNT] = {
    ARP_PATTERN_ACCENT(21845, 0x1041), ARP_PATTERN_ACCENT(62965, 0x50a1), ARP_PATTERN_ACCENT(46517, 0x1081),
    ARP_PATTERN_ACCENT(54741, 0x4141), ARP_PATTERN_ACCENT(43861, 0x0841), ARP_PATTERN_ACCENT(22869, 0x1041),
    ARP_PATTERN_ACCENT(38293, 0x1081), ARP_PATTERN_ACCENT(2313, 0x0101),  ARP_PATTERN_ACCENT(37449, 0x0201),
    ARP_PATTERN_ACCENT(21065, 0x0201), ARP_PATTERN_ACCENT(18761, 0x0101), ARP_PATTERN_ACCENT(54553, 0x4101),
    ARP_PATTERN_ACCENT(27499, 0x0841), ARP_PATTERN_ACCENT(23387, 0x1111), ARP_PATTERN_ACCENT(30583, 0x1111),
    ARP_PATTERN_ACCENT(28087, 0x2111), ARP_PATTERN_ACCENT(22359, 0x0441), ARP_PATTERN_ACCENT(28527, 0x2449),
    ARP_PATTERN_ACCENT(30431, 0x2449), ARP_PATTERN_ACCENT(43281, 0x0801), ARP_PATTERN_ACCENT(28609, 0x0901),
    ARP_PATTERN_ACCENT(53505, 0x4001)};

const uint16_t lut_res_arpeggiator_ties[ARPEGGIATOR_PRESET_PATTERN_COUNT] = {0x0041, 0x0001, 0x0001, 0x0101, 0x0041, 0x0041, 0x1001, 0x0001,
                                                                             0x0001, 0x0001, 0x0001, 0x0101, 0x0840, 0x1010, 0x0444, 0x0100,
                                                                             0x0440, 0x0048, 0x0400, 0x0001, 0x0801, 0x0001};

uint16_t Timbre::getArpeggiatorPattern() const {
    const int pattern = (int)sharedParams().engineArp2.pattern;
    if (pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT)
        return ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[pattern]);
    else
        return ARP_PATTERN_GETMASK(sharedParams().engineArpUserPatterns.patterns[pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT]);
}

void Timbre::updateArpPresetCache(int pattern) const {
    int seed = arpAdvancedGetEuclideanSeed(sharedParams().arpAdvanced);
    if (arpPresetCachePattern_ != (uint8_t)pattern || arpPresetCacheSeed_ != (uint8_t)seed) {
        arpComputePresetMasks(ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[pattern]),
                              ARP_PATTERN_GETACCENT(lut_res_arpeggiator_patterns[pattern]), lut_res_arpeggiator_ties[pattern], seed,
                              &arpPresetCacheAcc_, &arpPresetCacheTie_, &arpPresetCacheOct_, &arpPresetCacheRt_, &arpPresetCacheOctDown_,
                              &arpPresetCacheDyad_, &arpPresetCacheChord_, &arpPresetCacheSlap_);
        arpPresetCachePattern_ = (uint8_t)pattern;
        arpPresetCacheSeed_ = (uint8_t)seed;
    }
}

uint16_t Timbre::getArpeggiatorAccentMask() const {
    const int pattern = (int)sharedParams().engineArp2.pattern;
    if (pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
        updateArpPresetCache(pattern);
        return arpPresetCacheAcc_;
    }
    return ARP_PATTERN_GETACCENT(sharedParams().engineArpUserPatterns.patterns[pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT]);
}

int Timbre::getArpeggiatorPatternLength() const {
    const int pattern = (int)sharedParams().engineArp2.pattern;
    if (pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT)
        return arpPresetLength(arpAdvancedGetEuclideanSeed(sharedParams().arpAdvanced));
    return arpAdvancedGetUserLength(sharedParams().arpAdvanced, pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT);
}

int Timbre::getArpeggiatorStepMode() const {
    const int pattern = (int)sharedParams().engineArp2.pattern;
    if (bitmask_ == 0) {
        return ARP_STEP_MODE_NORM;
    }
    if (pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
        updateArpPresetCache(pattern);
        if ((bitmask_ & arpPresetCacheTie_) != 0) {
            return ARP_STEP_MODE_TIE;
        }
        if ((bitmask_ & arpPresetCacheOct_) != 0) {
            return ARP_STEP_MODE_OCT_UP;
        }
        if ((bitmask_ & arpPresetCacheRt_) != 0) {
            return ARP_STEP_MODE_RATCHET2;
        }
        if ((bitmask_ & arpPresetCacheOctDown_) != 0) {
            return ARP_STEP_MODE_OCT_DOWN;
        }
        if ((bitmask_ & arpPresetCacheDyad_) != 0) {
            return ARP_STEP_MODE_DYAD;
        }
        if ((bitmask_ & arpPresetCacheChord_) != 0) {
            return ARP_STEP_MODE_CHORD;
        }
        if ((bitmask_ & arpPresetCacheSlap_) != 0) {
            return ARP_STEP_MODE_SLAP;
        }
        return ARP_STEP_MODE_NORM;
    }
    int step = 0;
    uint16_t m = bitmask_;
    while ((m & 1u) == 0 && step < 15) {
        m >>= 1;
        step++;
    }
    return arpAdvancedGetStepMode(sharedParams().arpAdvanced, pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT, step);
}

const uint8_t midi_clock_tick_per_step[17] = {192, 144, 96, 72, 64, 48, 36, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1};

extern float noise[32];

float panTable[] = {
    0.0000, 0.0007, 0.0020, 0.0036, 0.0055, 0.0077, 0.0101, 0.0128, 0.0156, 0.0186, 0.0218, 0.0252, 0.0287, 0.0324, 0.0362, 0.0401, 0.0442,
    0.0484, 0.0527, 0.0572, 0.0618, 0.0665, 0.0713, 0.0762, 0.0812, 0.0863, 0.0915, 0.0969, 0.1023, 0.1078, 0.1135, 0.1192, 0.1250, 0.1309,
    0.1369, 0.1430, 0.1492, 0.1554, 0.1618, 0.1682, 0.1747, 0.1813, 0.1880, 0.1947, 0.2015, 0.2085, 0.2154, 0.2225, 0.2296, 0.2369, 0.2441,
    0.2515, 0.2589, 0.2664, 0.2740, 0.2817, 0.2894, 0.2972, 0.3050, 0.3129, 0.3209, 0.3290, 0.3371, 0.3453, 0.3536, 0.3619, 0.3703, 0.3787,
    0.3872, 0.3958, 0.4044, 0.4131, 0.4219, 0.4307, 0.4396, 0.4485, 0.4575, 0.4666, 0.4757, 0.4849, 0.4941, 0.5034, 0.5128, 0.5222, 0.5316,
    0.5411, 0.5507, 0.5604, 0.5700, 0.5798, 0.5896, 0.5994, 0.6093, 0.6193, 0.6293, 0.6394, 0.6495, 0.6597, 0.6699, 0.6802, 0.6905, 0.7009,
    0.7114, 0.7218, 0.7324, 0.7430, 0.7536, 0.7643, 0.7750, 0.7858, 0.7967, 0.8076, 0.8185, 0.8295, 0.8405, 0.8516, 0.8627, 0.8739, 0.8851,
    0.8964, 0.9077, 0.9191, 0.9305, 0.9420, 0.9535, 0.9651, 0.9767, 0.9883, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.0000, 1.0000};

inline float modulo2(float readPos, int bufferLen) {
    return unlikely(readPos < 0) ? readPos + bufferLen : readPos;
}
inline float modulo(float d, float max) {
    return unlikely(d >= max) ? d - max : d;
}
inline float clamp(float d, float min, float max) {
    const float t = unlikely(d < min) ? min : d;
    return unlikely(t > max) ? max : t;
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
inline float foldAbs(float x) {
    x *= 0.5f;
    float f = (fabsf(x - roundf(x)));
    return f + f;
}
inline float fold(float x4) {
    return fabsf(x4 + 0.25f - roundf(x4 + 0.25f)) - 0.25f;
}
inline float sigmoid(float x) {
    return x * (1.5f - 0.5f * x * x);
}
inline float sigmoidPos(float x) {
    // x : 0 -> 1
    return (sigmoid((x * 2) - 1) + 1) * 0.5f;
}
inline float tanh4(float x) {
    return x / sqrt3(x * x + 1);
}
inline float fastSin(float x) {
    return 3.9961f * x * (1 - fabsf(x));
}
inline float hann(float x) {
    float s = sqrt3(x * (1 - x));
    return s + s;
}
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

Timbre::Timbre() {

    synthState_ = nullptr;
    engine_ = SYNTH_ENGINE_PFM3;
    engineImpl_ = nullptr;
    maxVoices_ = 0;
    numberOfVoices_ = 0.0f;
    numberOfVoiceInverse_ = 1.0f;
    voicePool_ = nullptr;
    for (int c = 0; c < 16; c++) {
        mpeChannelVoice_[c] = -1;
    }
    sbMax_ = &sampleBlock_[64];
    holdPedal_ = false;
    lastPlayedNote_ = 0;
    // arpegiator
    setNewBPMValue(90);
    arpegiatorStep_ = 0.0;
    idle_ticks_ = 96;
    running_ = 0;
    ignore_note_off_messages_ = 0;
    recording_ = 0;
    chord_cycle_ = 0;
    extClockCounter_ = 0;
    extBlocksPerTick_ = 31.25f;
    arpRatchetSubBlocks_ = 0.0f;
    arpRatchetGateBlocks_ = 0.0f;
    arpRatchetTotalBlocks_ = 0.0f;
    arpRatchetElapsedBlocks_ = 0.0f;
    arpRatchetSubElapsed_ = 0.0f;
    arpRatchetCount_ = 0;
    arpRatchetVelocity_ = 0;
    arpRatchetHitIndex_ = 0;
    arpRatchetSubDiv_ = 1;
    arpRatchetSign_ = 1;
    arpRatchetGateOpen_ = false;
    arpRatchetActive_ = false;
    arpStrumCount_ = 0;
    arpPendingStepMode_ = ARP_STEP_MODE_NORM;
    arpSwingStepScale_ = 1.0f;
    arpPresetCachePattern_ = 0xFF;
    arpPresetCacheSeed_ = 0xFF;
    arpPresetCacheAcc_ = 0;
    arpPresetCacheTie_ = 0;
    arpPresetCacheOct_ = 0;
    arpPresetCacheRt_ = 0;
    arpPresetCacheOctDown_ = 0;
    arpPresetCacheDyad_ = 0;
    arpPresetCacheChord_ = 0;
    arpPresetCacheSlap_ = 0;
    note_stack_.Init();
    event_scheduler_.Init();
    // Arpeggiator start
    Start();
}

Timbre::~Timbre() {
}

void Timbre::init(SynthState* synthState, int timbreNumber) {

    mixerState_ = &synthState->mixerState_;
    synthState_ = synthState;
    timbreNumber_ = timbreNumber;

    SynthEngineFactory::init(this, synthState);
    engineImpl_ = SynthEngineFactory::get(engine_, timbreNumber_);
    clampVoicesToEngineMax();
    sharedParams().engineArp1 = preenMainPreset.engineArp1;
    sharedParams().engineArp2 = preenMainPreset.engineArp2;
    sharedParams().engineArpUserPatterns = preenMainPreset.engineArpUserPatterns;
    sharedParams().effect1 = preenMainPreset.effect1;
    sharedParams().effect2 = preenMainPreset.effect2;
    memcpy(sharedParams().presetName, preenMainPreset.presetName, sizeof(sharedParams().presetName));

    for (int s = 0; s < 2; s++) {
        for (int n = 0; n < 128; n++) {
            midiNoteScale[s][timbreNumber][n] = INV127 * (float)n;
        }
    }

    lowerNote_ = 64;
    lowerNoteReleased_ = true;

    /** --------------FX init--------------  */

    delayBuffer_ = delayBuffer[timbreNumber_];

    for (int s = 0; s < delayBufferSize; s++) {
        delayBuffer_[s] = 0;
    }
    initFx();
}

void Timbre::setVoiceNumber(int v, int n) {
    voiceNumber_[v] = n;
    if (n >= 0) {
        engineImpl_->bindVoice(voices_[n]);
    }
}

void Timbre::initVoicePointer(int n, Voice* voice) {
    voices_[n] = voice;
}

void Timbre::setEngine(SynthEngineType engine) {
    if (engine_ == engine && engineImpl_ != nullptr) {
        clampVoicesToEngineMax();
        return;
    }
    stopPlayingNow();
    engine_ = engine;
    engineImpl_ = SynthEngineFactory::get(engine_, timbreNumber_);
    engineImpl_->afterNewParamsLoad();
    clampVoicesToEngineMax();
}

void Timbre::clampVoicesToEngineMax() {
    if (engineImpl_ == nullptr) {
        return;
    }
    const int max = (int)engineImpl_->getMaxVoices();
    if (mixerState_ != nullptr && mixerState_->instrumentState_[timbreNumber_].numberOfVoices > max) {
        mixerState_->instrumentState_[timbreNumber_].numberOfVoices = (uint8_t)max;
    }
    if (maxVoices_ > max) {
        numberOfVoicesChanged((uint8_t)max);
    }
}

void Timbre::loadDx7Patch(const uint8_t* packedPatch) {
    // A DX7 sysex carries no arpeggiator/FX settings. Reset the shared
    // params so the new instrument does not inherit the previous
    // instrument's FX/arp.
    sharedParams().engineArp1 = preenMainPreset.engineArp1;
    sharedParams().engineArp2 = preenMainPreset.engineArp2;
    sharedParams().engineArpUserPatterns = preenMainPreset.engineArpUserPatterns;
    sharedParams().effect1 = preenMainPreset.effect1;
    sharedParams().effect2 = preenMainPreset.effect2;
    memcpy(sharedParams().presetName, preenMainPreset.presetName, sizeof(sharedParams().presetName));
    SynthEngineFactory::getPfm3(timbreNumber_)->syncSharedParams(sharedParams());

    engineImpl_->loadDx7Patch(packedPatch);
    SynthEngineFactory::get(SYNTH_ENGINE_PFM3, timbreNumber_)->loadHexterPatch(packedPatch);
}

void Timbre::noteOn(char note, char velocity) {
    if (sharedParams().engineArp1.clock) {
        arpeggiatorNoteOn(note, velocity);
    } else {
        engineNoteOn(note, velocity);
    }
}

void Timbre::noteOff(char note) {
    if (sharedParams().engineArp1.clock) {
        arpeggiatorNoteOff(note);
    } else {
        engineNoteOff(note);
    }
}

void Timbre::noteOnMPE(uint8_t channel, uint8_t note, uint8_t velocity) {
    engineImpl_->noteOnMpe(channel, note, velocity);
}

void Timbre::noteOffMPE(uint8_t channel, uint8_t note, uint8_t velocityOff) {
    engineImpl_->noteOffMpe(channel, note, velocityOff);
}

void Timbre::clearMpeMapping(int voiceIndex) {
    for (int c = 0; c < 16; c++) {
        if (mpeChannelVoice_[c] == voiceIndex) {
            mpeChannelVoice_[c] = -1;
        }
    }
}

void Timbre::numberOfVoicesChanged(uint8_t newNumberOfVoices) {
    maxVoices_ = newNumberOfVoices;
    numberOfVoiceInverse_ = newNumberOfVoices > 0 ? 1.0f / (float)newNumberOfVoices : 1.0f;
    trimActiveVoices((int)maxVoices_);
}

void Timbre::trimActiveVoices(int count) {
    while ((int)numberOfVoices_ > count) {
        int n = voiceNumber_[(int)numberOfVoices_ - 1];
        engineImpl_->killVoice(voices_[n]);
        clearMpeMapping(n);
        if (voicePool_ != nullptr) {
            voicePool_->release(n);
        }
        numberOfVoices_ -= 1.0f;
    }
}

void Timbre::removeActiveVoice(int voiceIndex) {
    int count = (int)numberOfVoices_;
    for (int k = 0; k < count; k++) {
        if (voiceNumber_[k] == voiceIndex) {
            voiceNumber_[k] = voiceNumber_[count - 1];
            numberOfVoices_ = (float)(count - 1);
            clearMpeMapping(voiceIndex);
            return;
        }
    }
}

void Timbre::clearActiveVoices() {
    for (int k = 0; k < (int)numberOfVoices_; k++) {
        int n = voiceNumber_[k];
        engineImpl_->killVoice(voices_[n]);
        if (voicePool_ != nullptr) {
            voicePool_->release(n);
        }
    }
    numberOfVoices_ = 0.0f;
    for (int c = 0; c < 16; c++) {
        mpeChannelVoice_[c] = -1;
    }
}

bool Timbre::ensureActiveVoices(int count) {
    while ((int)numberOfVoices_ < count) {
        int n = voicePool_->findFree(voices_);
        if (n == -1 || voicePool_->owner(n) != -1) {
            return false;
        }
        voicePool_->claim(n, timbreNumber_);
        engineImpl_->bindVoice(voices_[n]);
        voiceNumber_[(int)numberOfVoices_] = n;
        numberOfVoices_ += 1.0f;
    }
    return true;
}

int Timbre::allocateVoice(int note) {
    int count = (int)numberOfVoices_;

    int freeVoice = -1;
    int oldestReleasedVoice = -1;
    int oldestPlayingVoice = -1;
    uint32_t oldestReleasedIndex = UINT32_MAX;
    uint32_t oldestPlayingIndex = UINT32_MAX;

    for (int k = 0; k < count; k++) {
        int n = voiceNumber_[k];
        if (likely(voices_[n]->isPlaying())) {
            if (unlikely(voices_[n]->getNote() == note && engineImpl_->reusesSameNoteVoice())) {
                return n;
            }
            uint32_t index = voices_[n]->getIndex();
            if (voices_[n]->isReleased()) {
                if (index < oldestReleasedIndex) {
                    oldestReleasedIndex = index;
                    oldestReleasedVoice = n;
                }
            } else if (index < oldestPlayingIndex) {
                oldestPlayingIndex = index;
                oldestPlayingVoice = n;
            }
        } else if (freeVoice == -1) {
            freeVoice = n;
        }
    }

    if (freeVoice != -1) {
        return freeVoice;
    }

    if (count + countPendingVoices() < maxVoices_) {
        int n = voicePool_->findFree(voices_);
        if (n != -1) {
            if (voicePool_->owner(n) == -1) {
                voicePool_->claim(n, timbreNumber_);
                engineImpl_->bindVoice(voices_[n]);
                voiceNumber_[count] = n;
                numberOfVoices_ = (float)(count + 1);
                return n;
            }
            return reuseVoice(n);
        }

        n = voicePool_->findOldestReleased(voices_);
        if (n == -1) {
            n = voicePool_->findOldestPlaying(voices_, timbreNumber_);
        }
        if (n != -1) {
            return reuseVoice(n);
        }
    }

    if (oldestReleasedVoice != -1) {
        return oldestReleasedVoice;
    }

    if (oldestPlayingVoice != -1) {
        return oldestPlayingVoice;
    }

    int n = voicePool_->findOldestPlaying(voices_, -1);
    if (n != -1) {
        return reuseVoice(n);
    }

    return -1;
}

int Timbre::reuseVoice(int voiceIndex) {
    Voice* voice = voices_[voiceIndex];
    if (voice->getCurrentTimbre() == this) {
        return voiceIndex;
    }

    Timbre* oldTimbre = voice->getCurrentTimbre();
    bool bothPfm3 = engineImpl_->supportsDeferredSteal() && oldTimbre != nullptr && oldTimbre->supportsDeferredSteal();

    if (!voice->isPlaying() || !bothPfm3) {
        if (oldTimbre != nullptr) {
            oldTimbre->engineImpl_->killVoice(voice);
            oldTimbre->removeActiveVoice(voiceIndex);
        }
        voicePool_->claim(voiceIndex, timbreNumber_);
        engineImpl_->bindVoice(voice);
        voiceNumber_[(int)numberOfVoices_] = voiceIndex;
        numberOfVoices_ += 1.0f;
        return voiceIndex;
    }

    voice->setPendingTimbre(this);
    return voiceIndex;
}

bool Timbre::adoptVoice(Voice* voice, int voiceIndex) {
    if ((int)numberOfVoices_ >= maxVoices_) {
        return false;
    }
    voicePool_->claim(voiceIndex, timbreNumber_);
    engineImpl_->bindVoice(voice);
    voiceNumber_[(int)numberOfVoices_] = voiceIndex;
    numberOfVoices_ += 1.0f;
    lastPlayedNote_ = (int8_t)voiceIndex;
    return true;
}

void Timbre::dropVoice(int voiceIndex) {
    int count = (int)numberOfVoices_;
    for (int k = 0; k < count; k++) {
        if (voiceNumber_[k] == voiceIndex) {
            voiceNumber_[k] = voiceNumber_[count - 1];
            numberOfVoices_ = (float)(count - 1);
            clearMpeMapping(voiceIndex);
            if (lastPlayedNote_ == voiceIndex) {
                lastPlayedNote_ = (count > 1) ? voiceNumber_[0] : 0;
            }
            return;
        }
    }
}

int Timbre::countPendingVoices() const {
    int count = 0;
    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        if (voices_[v]->getPendingTimbre() == this) {
            count++;
        }
    }
    return count;
}

void Timbre::allNoteOff() {
    for (int k = 0; k < (int)numberOfVoices_; k++) {
        Voice* voice = voices_[voiceNumber_[k]];
        if (voice->isPlaying() && !voice->isReleased()) {
            engineImpl_->noteOffVoice(voice);
        }
    }
}

void Timbre::allNoteOffQuick() {
    for (int k = 0; k < (int)numberOfVoices_; k++) {
        Voice* voice = voices_[voiceNumber_[k]];
        if (voice->isPlaying()) {
            engineImpl_->noteOffVoiceQuick(voice);
        }
    }
}

void Timbre::allSoundOff() {
    for (int k = 0; k < (int)numberOfVoices_; k++) {
        engineImpl_->killVoice(voices_[voiceNumber_[k]]);
    }
}

void Timbre::noteOffVoice(Voice* voice) {
    engineImpl_->noteOffVoice(voice);
}

void Timbre::noteOffVoiceQuick(Voice* voice) {
    engineImpl_->noteOffVoiceQuick(voice);
}

void Timbre::killVoice(Voice* voice) {
    engineImpl_->killVoice(voice);
}

void Timbre::startVoiceAfterTransfer(Voice* voice, int note, float noteFrequency, uint8_t velocity, float phase, bool alreadyOff) {
    engineImpl_->startVoiceAfterTransfer(voice, note, noteFrequency, velocity, phase, alreadyOff);
}

void Timbre::completeVoiceTransfer(Voice* voice, int poolIndex, VoicePool* pool) {
    engineImpl_->completeVoiceTransfer(voice, poolIndex, pool);
}

void Timbre::stopPlayingNow() {
    for (int k = 0; k < (int)numberOfVoices_; k++) {
        int n = voiceNumber_[k];
        if (n != -1) {
            engineImpl_->killVoice(voices_[n]);
            if (voicePool_ != nullptr) {
                voicePool_->release(n);
            }
        }
    }
    numberOfVoices_ = 0.0f;
    for (int c = 0; c < 16; c++) {
        mpeChannelVoice_[c] = -1;
    }
}

void Timbre::setHoldPedal(int value) {
    if (value < 64) {
        holdPedal_ = false;
        int nVoices = numberOfVoices_;
        for (int k = 0; k < nVoices; k++) {
            // voice number k of timbre
            int n = voiceNumber_[k];
            if (voices_[n]->isHoldedByPedal()) {
                engineImpl_->noteOffVoice(voices_[n]);
            }
        }
        arpeggiatorSetHoldPedal(0);
    } else {
        holdPedal_ = true;
        arpeggiatorSetHoldPedal(127);
    }
}

void Timbre::setNewBPMValue(float bpm) {
    ticksPerSecond_ = bpm * 24.0f / 60.0f;
    ticksEveryNCalls_ = CALLED_PER_SECOND / ticksPerSecond_;
    ticksEveyNCallsInteger_ = (int)ticksEveryNCalls_;
}

void Timbre::setArpeggiatorClock(float clockValue) {
    if (clockValue == CLOCK_OFF) {
        FlushQueue();
        note_stack_.Clear();
    }
    if (clockValue == CLOCK_INTERNAL) {
        setNewBPMValue(sharedParams().engineArp1.BPM);
    }
    if (clockValue == CLOCK_EXTERNAL) {
        // Let's consider we're running
        running_ = 1;
    }
}

void Timbre::updateArpegiatorInternalClock() {

    // Apeggiator clock : internal
    if (sharedParams().engineArp1.clock == CLOCK_INTERNAL) {
        arpegiatorStep_ += 1.0f;
        if (unlikely((arpegiatorStep_) > ticksEveryNCalls_)) {
            arpegiatorStep_ -= ticksEveryNCalls_;
            Tick();
        }
    } else if (sharedParams().engineArp1.clock == CLOCK_EXTERNAL) {
        if (extClockCounter_ < 2000) {
            extClockCounter_++;
        }
    }
    if (unlikely(arpRatchetActive_)) {
        updateArpRatchet();
    }
    if (unlikely(arpStrumCount_ > 0)) {
        updateArpStrum();
    }
}

void Timbre::cleanNextBlock() {

    float* sp = sampleBlock_;
    while (sp < sbMax_) {
        *sp++ = 0;
        *sp++ = 0;
        *sp++ = 0;
        *sp++ = 0;
        *sp++ = 0;
        *sp++ = 0;
        *sp++ = 0;
        *sp++ = 0;
    }
}

void Timbre::prepareMatrixForNewBlock() {
    engineImpl_->prepareBlock();
}

void Timbre::glide() {
    engineImpl_->glide();
}

uint8_t Timbre::voicesNextBlock() {
    if ((int)numberOfVoices_ == 0) {
        cleanNextBlock();
        return 0;
    }
    return engineImpl_->voicesNextBlock();
}

void Timbre::voicesToTimbre(float volumeGain) {
    if ((int)numberOfVoices_ == 0) {
        // sampleBlock_ was already cleared by voicesNextBlock().
        return;
    }

    int numberOfVoicesToCopy = numberOfVoices_;

    // If unison : all voices have already been added to the first one
    if (unlikely(isUnisonMode())) {
        numberOfVoicesToCopy = 1;
    }

    for (int k = 0; k < numberOfVoicesToCopy; k++) {
        float* timbreBlock = sampleBlock_;
        const float* voiceBlock = voices_[voiceNumber_[k]]->getSampleBlock();

        if (unlikely(k == 0)) {
            if (unlikely(numberOfVoicesToCopy == 1)) {
                for (int s = 0; s < BLOCK_SIZE; s++) {
                    *timbreBlock++ = *voiceBlock++ * volumeGain;
                    *timbreBlock++ = *voiceBlock++ * volumeGain;
                }
            } else {
                for (int s = 0; s < BLOCK_SIZE; s++) {
                    *timbreBlock++ = *voiceBlock++;
                    *timbreBlock++ = *voiceBlock++;
                }
            }
        } else if (k == numberOfVoicesToCopy - 1) {
            for (int s = 0; s < BLOCK_SIZE; s++) {
                *timbreBlock = (*timbreBlock + *voiceBlock++) * volumeGain;
                timbreBlock++;
                *timbreBlock = (*timbreBlock + *voiceBlock++) * volumeGain;
                timbreBlock++;
            }
        } else {
            for (int s = 0; s < BLOCK_SIZE; s++) {
                *timbreBlock++ += *voiceBlock++;
                *timbreBlock++ += *voiceBlock++;
            }
        }
    }
}

void Timbre::gateFx() {
    engineImpl_->gateFx();
}

void Timbre::afterNewParamsLoad() {

    engineImpl_->afterNewParamsLoad();

    resetArpeggiator();
    for (int k = 0; k < PFM3_PARAMS_PER_ROW; k++) {
        setNewEffectParam(k);
    }
    // Update midi note scale
    updateMidiNoteScale(0);
    updateMidiNoteScale(1);
}

void Timbre::resetArpeggiator() {
    // Reset Arpeggiator
    FlushQueue();
    note_stack_.Clear();
    setArpeggiatorClock(sharedParams().engineArp1.clock);
    setLatchMode(sharedParams().engineArp2.latche);
}

bool Timbre::decodeSharedControlChange(uint8_t cc, uint8_t value, SharedParamId& param, float& scaledValue) const {
    switch (cc) {
    case CC_FILTER_TYPE:
        param = SHARED_PARAM_EFFECT1_TYPE;
        scaledValue = (float)value;
        return true;
    case CC_FILTER_PARAM1:
        param = SHARED_PARAM_EFFECT1_PARAM1;
        scaledValue = (float)value * INV127;
        return true;
    case CC_FILTER_PARAM2:
        param = SHARED_PARAM_EFFECT1_PARAM2;
        scaledValue = (float)value * INV127;
        return true;
    case CC_FILTER_GAIN:
        param = SHARED_PARAM_EFFECT1_PARAM3;
        scaledValue = (float)value * .01f;
        return true;
    case CC_FILTER2_TYPE:
        param = SHARED_PARAM_EFFECT2_TYPE;
        scaledValue = (float)value;
        return true;
    case CC_FILTER2_PARAM1:
        param = SHARED_PARAM_EFFECT2_PARAM1;
        scaledValue = (float)value * INV127;
        return true;
    case CC_FILTER2_PARAM2:
        param = SHARED_PARAM_EFFECT2_PARAM2;
        scaledValue = (float)value * INV127;
        return true;
    case CC_FILTER2_MIX:
        param = SHARED_PARAM_EFFECT2_PARAM3;
        scaledValue = (float)value * .01f;
        return true;
    case CC_ARP_CLOCK:
        param = SHARED_PARAM_ARP_CLOCK;
        scaledValue = (float)value;
        return true;
    case CC_ARP_DIRECTION:
        param = SHARED_PARAM_ARP_DIRECTION;
        scaledValue = (float)value;
        return true;
    case CC_ARP_OCTAVE:
        param = SHARED_PARAM_ARP_OCTAVE;
        scaledValue = (float)value;
        return true;
    case CC_ARP_PATTERN:
        param = SHARED_PARAM_ARP_PATTERN;
        scaledValue = (float)value;
        return true;
    case CC_ARP_DIVISION:
        param = SHARED_PARAM_ARP_DIVISION;
        scaledValue = (float)value;
        return true;
    case CC_ARP_DURATION:
        param = SHARED_PARAM_ARP_DURATION;
        scaledValue = (float)value;
        return true;
    default:
        return false;
    }
}

bool Timbre::midiData(const MidiData& data) {
    if (data.type == MIDI_DATA_CC) {
        SharedParamId param;
        float scaledValue;
        if (decodeSharedControlChange(data.cc, data.ccValue, param, scaledValue)) {
            setSharedParamFromMidi(getSharedParamId(param), scaledValue);
            return true;
        }
    }
    return engineImpl_->midiData(timbreNumber_, data);
}

void Timbre::setSharedParamFromMidi(CommonParamId cid, float newValue) {
    uint16_t index = commonFlatIndex(cid);
    if (index == 0xFFFF) {
        return;
    }
    const ParamDesc& d = commonDesc(cid);
    const ParamUiDesc& ui = commonFullDesc(cid).ui;

    int sharedIndex;
    if (index >= Pfm3PresetLayout::sharedBlockStart && index < Pfm3PresetLayout::sharedBlockEnd) {
        sharedIndex = index - Pfm3PresetLayout::sharedBlockStart;
    } else if (index >= Pfm3PresetLayout::effect2Start && index < Pfm3PresetLayout::presetNameStart) {
        sharedIndex = index - Pfm3PresetLayout::effect2Start + Pfm3PresetLayout::sharedGap;
    } else {
        return;
    }

    float oldValue = ((float*)&sharedParams())[sharedIndex];

    if (newValue > d.max) {
        // in v2, matrix target were removed so some values are > to max value but we need to accept it
        bool mustConstraint = true;
        if (ui.valueOrder != NULL) {
            for (int v = 0; v < (int)ui.values; v++) {
                if ((int)ui.valueOrder[v] == (int)(newValue + .01)) {
                    mustConstraint = false;
                }
            }
        }
        if (mustConstraint) {
            newValue = d.max;
        }
    } else if (newValue < d.min) {
        newValue = d.min;
    }
    ((float*)&sharedParams())[sharedIndex] = newValue;

    if (oldValue != newValue) {
        SynthEngineFactory::getPfm3(timbreNumber_)->syncSharedParams(sharedParams());
        synthState_->propagateNewEngineParamValueFromExternal(timbreNumber_, (int)cid, 1, oldValue, newValue);
    }
}

CommonParamId Timbre::getSharedParamId(SharedParamId param) const {
    switch (param) {
    case SHARED_PARAM_ARP_CLOCK:
        return commonParam(CommonBlock::Arp1, 0, 0);
    case SHARED_PARAM_ARP_DIRECTION:
        return commonParam(CommonBlock::Arp1, 0, 2);
    case SHARED_PARAM_ARP_OCTAVE:
        return commonParam(CommonBlock::Arp1, 0, 3);
    case SHARED_PARAM_ARP_PATTERN:
        return commonParam(CommonBlock::Arp2, 0, 0);
    case SHARED_PARAM_ARP_DIVISION:
        return commonParam(CommonBlock::Arp2, 0, 1);
    case SHARED_PARAM_ARP_DURATION:
        return commonParam(CommonBlock::Arp2, 0, 2);
    case SHARED_PARAM_EFFECT1_TYPE:
        return commonParam(CommonBlock::Effect1, 0, 0);
    case SHARED_PARAM_EFFECT1_PARAM1:
        return commonParam(CommonBlock::Effect1, 0, 1);
    case SHARED_PARAM_EFFECT1_PARAM2:
        return commonParam(CommonBlock::Effect1, 0, 2);
    case SHARED_PARAM_EFFECT1_PARAM3:
        return commonParam(CommonBlock::Effect1, 0, 3);
    case SHARED_PARAM_EFFECT2_TYPE:
        return commonParam(CommonBlock::Effect2, 0, 0);
    case SHARED_PARAM_EFFECT2_PARAM1:
        return commonParam(CommonBlock::Effect2, 0, 1);
    case SHARED_PARAM_EFFECT2_PARAM2:
        return commonParam(CommonBlock::Effect2, 0, 2);
    case SHARED_PARAM_EFFECT2_PARAM3:
        return commonParam(CommonBlock::Effect2, 0, 3);
    default:
        return COMMON_PARAM_NONE;
    }
}

void Timbre::setParamValue(int index, float oldValue, float newValue) {
    if (index >= Pfm3PresetLayout::presetNameStart) {
        return;
    }
    if (index >= Pfm3PresetLayout::sharedBlockStart && index < Pfm3PresetLayout::sharedBlockEnd) {
        ((float*)&sharedParams())[index - Pfm3PresetLayout::sharedBlockStart] = newValue;
    } else if (index >= Pfm3PresetLayout::effect2Start && index < Pfm3PresetLayout::presetNameStart) {
        ((float*)&sharedParams())[index - Pfm3PresetLayout::effect2Start + Pfm3PresetLayout::sharedGap] = newValue;
    } else {
        engineImpl_->setParamValue(index, newValue);
    }

    engineImpl_->paramChanged(index, oldValue, newValue);

    CommonParamId commonId = commonFlatToParam(index);
    if (commonId != COMMON_PARAM_NONE) {
        switch ((CommonBlock)commonBlockOf(commonId)) {
        case CommonBlock::Arp1:
            switch (commonFieldOf(commonId)) {
            case 0: // clock
                allNoteOff();
                setArpeggiatorClock((uint8_t)newValue);
                break;
            case 1: // BPM
                setNewBPMValue((uint8_t)newValue);
                break;
            case 2: // direction
                setDirection((uint8_t)newValue);
                break;
            default:
                break;
            }
            break;
        case CommonBlock::Arp2:
            if (commonFieldOf(commonId) == 3) { // latch
                setLatchMode((uint8_t)newValue);
            }
            break;
        case CommonBlock::Effect1:
            setNewEffectParam(commonFieldOf(commonId));
            break;
        default:
            break;
        }
    }
}

void Timbre::engineParamChanged(int paramId, int context, float oldValue, float newValue) {
    engineImpl_->engineParamChanged(paramId, context, oldValue, newValue);
}

void Timbre::setPresetNameChar(int index, char c) {
    sharedParams().presetName[index] = c;
    SynthEngineFactory::getPfm3(timbreNumber_)->syncSharedParams(sharedParams());
}

void Timbre::setPresetName(const char* name) {
    int length = 0;
    while (length < 12 && name[length] != 0) {
        length++;
    }
    for (int k = 0; k < 13; k++) {
        char c = k < length ? name[k] : 0;
        sharedParams().presetName[k] = c;
    }
    SynthEngineFactory::getPfm3(timbreNumber_)->syncSharedParams(sharedParams());
}

uint32_t Timbre::getStorageMagic() const {
    return engineImpl_->getStorageMagic();
}

uint32_t Timbre::getStorageBlob(uint8_t* out, uint32_t maxSize) {
    return engineImpl_->getStorageBlob(out, maxSize, sharedParams());
}

bool Timbre::loadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic) {
    SynthEngineType type = SynthEngineFactory::engineTypeForStorageMagic(magic);
    if (type == SYNTH_ENGINE_COUNT) {
        return false;
    }
    return loadStorageBlob(in, size, type, magic);
}

bool Timbre::loadStorageBlob(const uint8_t* in, uint32_t size, SynthEngineType engine, uint32_t magic) {
    if (engine >= SYNTH_ENGINE_COUNT) {
        return false;
    }
    if (engine_ != engine) {
        setEngine(engine);
    }
    return engineImpl_->loadStorageBlob(in, size, magic);
}

int Timbre::getStorageNameOffset(uint32_t magic) const {
    SynthEngine* engine = SynthEngineFactory::getForStorageMagic(magic, timbreNumber_);
    return engine != nullptr ? engine->getStorageNameOffset(magic) : -1;
}

const char* Timbre::getStorageDefaultName(uint32_t magic) const {
    SynthEngine* engine = SynthEngineFactory::getForStorageMagic(magic, timbreNumber_);
    return engine != nullptr ? engine->getStorageDefaultName() : nullptr;
}

void Timbre::setSharedParams(const TimbreSharedParams& shared) {
    sharedParams() = shared;
    SynthEngineFactory::getPfm3(timbreNumber_)->syncSharedParams(shared);
}

void Timbre::setArpAdvanced(const ArpAdvancedParams& arp) {
    sharedParams().arpAdvanced = arp;
    SynthEngineFactory::getPfm3(timbreNumber_)->syncSharedParams(sharedParams());
}

void Timbre::setArpUserPattern(int index, arp_pattern_t pattern) {
    if (index < 0 || index >= ARPEGGIATOR_USER_PATTERN_COUNT) {
        return;
    }
    sharedParams().engineArpUserPatterns.patterns[index] = pattern;
    SynthEngineFactory::getPfm3(timbreNumber_)->syncSharedParams(sharedParams());
}

void Timbre::setNewEffectParam(int encoder) {

    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->setNewEffectParam(encoder);
    }
}

// Code bellowed have been adapted by Xavier Hosxe for PreenFM2
// It come from Muteable Instrument midiPAL

/////////////////////////////////////////////////////////////////////////////////
// Copyright 2011 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// Arpeggiator app.

void Timbre::arpeggiatorNoteOn(char note, char velocity) {
    bool isNewLatchedChord = (latch_ && !recording_);
    bool wasEmpty = (note_stack_.size() == 0) || isNewLatchedChord;
    // CLOCK_MODE_INTERNAL
    if (sharedParams().engineArp1.clock == CLOCK_INTERNAL) {
        if (wasEmpty || idle_ticks_ >= 96 || !running_) {
            Start();
        }
        idle_ticks_ = 0;
    } else if (wasEmpty) {
        resetArpPatternState();
    }

    if (isNewLatchedChord) {
        note_stack_.Clear();
        recording_ = 1;
    }
    note_stack_.NoteOn(note, velocity);
}

void Timbre::arpeggiatorNoteOff(char note) {
    if (ignore_note_off_messages_) {
        return;
    }
    if (!latch_) {
        note_stack_.NoteOff(note);
        if (note_stack_.size() == 0) {
            resetArpPatternState();
        }
    } else {
        if (note == note_stack_.most_recent_note().note) {
            recording_ = 0;
        }
    }
}

void Timbre::OnMidiContinue() {
    if (sharedParams().engineArp1.clock == CLOCK_EXTERNAL) {
        running_ = 1;
    }
}

void Timbre::OnMidiStart() {
    if (sharedParams().engineArp1.clock == CLOCK_EXTERNAL) {
        Start();
    }
}

void Timbre::OnMidiStop() {
    if (sharedParams().engineArp1.clock == CLOCK_EXTERNAL) {
        running_ = 0;
        FlushQueue();
    }
}

void Timbre::OnMidiClock() {
    if (sharedParams().engineArp1.clock == CLOCK_EXTERNAL) {
        if (extClockCounter_ > 2 && extClockCounter_ < 2000) {
            extBlocksPerTick_ = (float)extClockCounter_;
        }
        extClockCounter_ = 0;
        if (running_) {
            Tick();
        }
    }
}

void Timbre::updateArpStrum() {
    uint8_t remaining = 0;
    bool stoppedPrevRatchet = false;
    for (uint8_t i = 0; i < arpStrumCount_; i++) {
        if (arpStrumDelay_[i] > 0) {
            arpStrumDelay_[i]--;
        }
        if (arpStrumDelay_[i] == 0) {
            if (!stoppedPrevRatchet) {
                stopArpRatchet();
                stoppedPrevRatchet = true;
            }
            SendNote(arpStrumNotes_[i], arpStrumVelocities_[i], (int)arpPendingStepMode_);
        } else {
            if (remaining != i) {
                arpStrumNotes_[remaining] = arpStrumNotes_[i];
                arpStrumVelocities_[remaining] = arpStrumVelocities_[i];
                arpStrumDelay_[remaining] = arpStrumDelay_[i];
            }
            remaining++;
        }
    }
    arpStrumCount_ = remaining;
}

void Timbre::stopArpRatchet() {
    if (arpRatchetActive_) {
        if (arpRatchetGateOpen_) {
            for (uint8_t i = 0; i < arpRatchetCount_; i++) {
                engineNoteOff(arpRatchetNotes_[i]);
            }
            arpRatchetGateOpen_ = false;
        }
        arpRatchetActive_ = false;
        arpRatchetCount_ = 0;
    }
}

static inline float getRatchetDensityScale(float subBlocks) {
    if (subBlocks >= 90.0f) {
        return 1.0f;
    }
    float scale = 0.70f + 0.30f * ((subBlocks - 42.0f) / 48.0f);
    return (scale < 0.70f) ? 0.70f : scale;
}

void Timbre::updateArpRatchet() {
    arpRatchetElapsedBlocks_ += 1.0f;
    arpRatchetSubElapsed_ += 1.0f;

    if (arpRatchetGateOpen_ && arpRatchetSubElapsed_ >= arpRatchetGateBlocks_) {
        for (uint8_t i = 0; i < arpRatchetCount_; i++) {
            engineNoteOff(arpRatchetNotes_[i]);
        }
        arpRatchetGateOpen_ = false;
    }

    if (arpRatchetElapsedBlocks_ >= (arpRatchetTotalBlocks_ - 0.5f)) {
        stopArpRatchet();
        return;
    }

    if (arpRatchetSubElapsed_ >= arpRatchetSubBlocks_) {
        arpRatchetSubElapsed_ -= arpRatchetSubBlocks_;
        if ((arpRatchetTotalBlocks_ - arpRatchetElapsedBlocks_) >= (arpRatchetSubBlocks_ * 0.4f)) {
            arpRatchetHitIndex_++;
            float progress = (arpRatchetSubDiv_ > 1) ? ((float)arpRatchetHitIndex_ / (float)(arpRatchetSubDiv_ - 1)) : 1.0f;
            if (progress > 1.0f) {
                progress = 1.0f;
            }
            float rampScale = (arpRatchetSign_ >= 0) ? (0.25f + 0.75f * progress) : (1.0f - 0.75f * progress);
            float densityScale = getRatchetDensityScale(arpRatchetSubBlocks_);

            int effVel = (int)arpRatchetVelocity_;
            if (numberOfVoices_ > 0.0f) {
                effVel += (int)roundf(engineImpl_->getMatrixDestination(MTX_DEST_ARP_VEL) * 64.0f);
            }
            effVel = (int)roundf((float)effVel * rampScale * densityScale);
            if (effVel < 1) {
                effVel = 1;
            } else if (effVel > 127) {
                effVel = 127;
            }
            uint8_t hitVelocity = (uint8_t)effVel;
            for (uint8_t i = 0; i < arpRatchetCount_; i++) {
                engineNoteOn(arpRatchetNotes_[i], hitVelocity);
            }
            arpRatchetGateOpen_ = true;
        } else {
            stopArpRatchet();
        }
    }
}

int Timbre::getArpStepTicks() const {
    int divIdx = (int)sharedParams().engineArp2.division;
    if (numberOfVoices_ > 0.0f) {
        divIdx += (int)roundf(engineImpl_->getMatrixDestination(MTX_DEST_ARP_DIV) * 12.0f);
    }
    if (divIdx < 0) {
        divIdx = 0;
    } else if (divIdx > 16) {
        divIdx = 16;
    }
    return midi_clock_tick_per_step[divIdx];
}

void Timbre::SendNote(uint8_t note, uint8_t velocity, int stepMode) {
    uint8_t baseVelocity = velocity;
    int ratchetMode = 0;
    int8_t ratchetSign = 1;
    const bool hasMatrix = numberOfVoices_ > 0.0f;
    if (hasMatrix) {
        int effNote = (int)note + (int)roundf(engineImpl_->getMatrixDestination(MTX_DEST_ARP_NOTE) * 12.0f);
        if (effNote < 0) {
            effNote = 0;
        } else if (effNote > 127) {
            effNote = 127;
        }
        note = (uint8_t)effNote;

        int effVel = (int)velocity + (int)roundf(engineImpl_->getMatrixDestination(MTX_DEST_ARP_VEL) * 64.0f);
        if (effVel < 1) {
            effVel = 1;
        } else if (effVel > 127) {
            effVel = 127;
        }
        velocity = (uint8_t)effVel;

        float ratchetVal = engineImpl_->getMatrixDestination(MTX_DEST_ARP_RATCHET);
        ratchetSign = (ratchetVal < 0.0f) ? -1 : 1;
        float absRatchet = fabsf(ratchetVal);
        if (absRatchet >= 0.04f) {
            float curved = 0.25f * absRatchet + 0.75f * absRatchet * absRatchet * absRatchet;
            ratchetMode = (int)roundf(curved * 7.0f);
            if (ratchetMode < 1) {
                ratchetMode = 1;
            } else if (ratchetMode > 11) {
                ratchetMode = 11;
            }
        }
    }

    // If there are some Note Off messages for the note about to be triggeered
    // remove them from the queue and process them now.
    if (event_scheduler_.Remove(note, 0)) {
        engineNoteOff(note);
    }

    // Send a note on and schedule a note off later.
    if (stepMode < 0) {
        stepMode = getArpeggiatorStepMode();
    }
    if (stepMode == ARP_STEP_MODE_RATCHET2 && ratchetMode == 0) {
        ratchetMode = 1;
    } else if (stepMode == ARP_STEP_MODE_SLAP) {
        int slapVel = (int)velocity + ((int)velocity >> 2);
        velocity = (uint8_t)((slapVel > 127) ? 127 : slapVel);
    }

    int rawDurTicks = (int)midi_clock_tick_per_step[(int)sharedParams().engineArp2.duration];
    int stepTicks = getArpStepTicks();
    if (hasMatrix) {
        rawDurTicks += (int)roundf(engineImpl_->getMatrixDestination(MTX_DEST_ARP_GATE) * (float)stepTicks);
    }
    if (stepMode == ARP_STEP_MODE_TIE && rawDurTicks < (stepTicks + 1)) {
        rawDurTicks = stepTicks + 1;
    } else if (stepMode == ARP_STEP_MODE_SLAP) {
        rawDurTicks = (int)roundf((float)rawDurTicks * 0.25f);
        if (rawDurTicks < 2) {
            rawDurTicks = 2;
        } else if (rawDurTicks > 7) {
            rawDurTicks = 7;
        }
    }

    int subDiv = 1;
    float totalBlocks = 0.0f;
    float subBlocks = 0.0f;
    float densityScale = 1.0f;
    if (ratchetMode > 0) {
        // Keep ratchet spacing >= 42 audio blocks (~28 ms at 1500 Hz) so transients
        // do not stack into audio-rate buzz.
        const float minSubBlocks = 42.0f;
        float blocksPerTick = (sharedParams().engineArp1.clock == CLOCK_EXTERNAL)
                                  ? extBlocksPerTick_
                                  : ((ticksEveryNCalls_ > 2.0f) ? ticksEveryNCalls_ : 31.25f);
        totalBlocks = (float)stepTicks * blocksPerTick * arpSwingStepScale_;
        int maxSubDiv = (int)floorf(totalBlocks / minSubBlocks);
        subDiv = ratchetMode + 1;
        if (subDiv > maxSubDiv) {
            subDiv = maxSubDiv;
        }
        if (subDiv < 2) {
            ratchetMode = 0;
        } else {
            subBlocks = totalBlocks / (float)subDiv;
            densityScale = getRatchetDensityScale(subBlocks);
        }
    }

    uint8_t firstHitVel = velocity;
    if (ratchetMode > 0) {
        float firstScale = (ratchetSign > 0) ? (0.25f * densityScale) : densityScale;
        int scaled = (int)roundf((float)velocity * firstScale);
        firstHitVel = (uint8_t)((scaled < 1) ? 1 : scaled);
    }
    engineNoteOn(note, firstHitVel);

    if (ratchetMode > 0) {
        arpRatchetTotalBlocks_ = totalBlocks;
        arpRatchetSubBlocks_ = subBlocks;
        float gateRatio = (stepTicks > 0) ? ((float)rawDurTicks / (float)stepTicks) : 0.75f;
        if (gateRatio < 0.35f) {
            gateRatio = 0.35f;
        } else if (gateRatio > 0.85f) {
            gateRatio = 0.85f;
        }
        arpRatchetGateBlocks_ = arpRatchetSubBlocks_ * gateRatio;
        if (arpRatchetGateBlocks_ < 10.0f) {
            arpRatchetGateBlocks_ = 10.0f;
        }
        if (arpRatchetGateBlocks_ > (arpRatchetSubBlocks_ - 4.0f)) {
            arpRatchetGateBlocks_ = arpRatchetSubBlocks_ - 4.0f;
        }
        arpRatchetElapsedBlocks_ = 0.0f;
        arpRatchetSubElapsed_ = 0.0f;
        if (arpRatchetCount_ < 8) {
            arpRatchetNotes_[arpRatchetCount_++] = note;
        }
        arpRatchetVelocity_ = baseVelocity;
        arpRatchetHitIndex_ = 0;
        arpRatchetSubDiv_ = (uint8_t)subDiv;
        arpRatchetSign_ = ratchetSign;
        arpRatchetGateOpen_ = true;
        arpRatchetActive_ = true;
        return;
    }

    int durTicks = rawDurTicks - 1;
    if (arpSwingStepScale_ < 1.0f && durTicks > 1) {
        durTicks = (int)roundf((float)durTicks * arpSwingStepScale_);
    }
    if (durTicks < 1) {
        durTicks = 1;
    } else if (durTicks > 192) {
        durTicks = 192;
    }
    event_scheduler_.Schedule(note, 0, (uint8_t)durTicks, 0);
}

void Timbre::SendLater(uint8_t note, uint8_t velocity, uint8_t when, uint8_t tag) {
    event_scheduler_.Schedule(note, velocity, when, tag);
}

void Timbre::SendScheduledNotes() {
    uint8_t current = event_scheduler_.root();
    while (current) {
        const SchedulerEntry& entry = event_scheduler_.entry(current);
        if (entry.when) {
            break;
        }
        if (entry.note != kZombieSlot) {
            if (entry.velocity == 0) {
                engineNoteOff(entry.note);
            } else {
                engineNoteOn(entry.note, entry.velocity);
            }
        }
        current = entry.next;
    }
    event_scheduler_.Tick();
}

void Timbre::FlushQueue() {
    stopArpRatchet();
    arpStrumCount_ = 0;
    arpPendingStepMode_ = ARP_STEP_MODE_NORM;
    arpSwingStepScale_ = 1.0f;
    while (event_scheduler_.size()) {
        SendScheduledNotes();
    }
}

void Timbre::Tick() {
    ++tick_;

    if (note_stack_.size()) {
        idle_ticks_ = 0;
    }
    ++idle_ticks_;
    if (idle_ticks_ >= 96) {
        idle_ticks_ = 96;
        if (sharedParams().engineArp1.clock == CLOCK_INTERNAL) {
            running_ = 0;
            FlushQueue();
        }
    }

    SendScheduledNotes();

    int stepTicks = getArpStepTicks();
    if (tick_ >= stepTicks) {
        tick_ = 0;
        arpStrumCount_ = 0;

        const ArpAdvancedParams& adv = sharedParams().arpAdvanced;
        uint16_t pattern = getArpeggiatorPattern();
        uint16_t accentMask = getArpeggiatorAccentMask();
        int patternLen = getArpeggiatorPatternLength();
        uint16_t endMask = (uint16_t)(1u << (patternLen - 1));
        if (bitmask_ == 0 || bitmask_ > endMask) {
            bitmask_ = 1;
        }
        int curPatternStep = 0;
        {
            uint16_t m = bitmask_;
            while ((m & 1u) == 0 && curPatternStep < 15) {
                m >>= 1;
                curPatternStep++;
            }
        }
        int nextPatternStep = (curPatternStep + 1 < patternLen) ? (curPatternStep + 1) : 0;

        int grooveIdx = arpAdvancedGetGroove(adv);
        int humanize = arpAdvancedGetHumanize(adv);
        bool curSwung = isArpSwingStep(curPatternStep, grooveIdx);
        bool nextSwung = isArpSwingStep(nextPatternStep, grooveIdx);

        float swingNorm = (float)arpAdvancedGetSwing(adv) * 0.01f;
        if (swingNorm < 0.0f) {
            swingNorm = 0.0f;
        } else if (swingNorm > 0.50f) {
            swingNorm = 0.50f;
        }
        if (swingNorm > 0.0f) {
            float curOffset = curSwung ? swingNorm : 0.0f;
            float nextOffset = nextSwung ? swingNorm : 0.0f;
            arpSwingStepScale_ = 1.0f + nextOffset - curOffset;
        } else {
            arpSwingStepScale_ = 1.0f;
        }
        float swingVelScale = (curSwung && swingNorm > 0.0f) ? (1.0f - swingNorm * arpSwingVelAttenCoeff(grooveIdx)) : 1.0f;

        uint16_t swingDelay = 0;
        float blocksPerTick = (sharedParams().engineArp1.clock == CLOCK_EXTERNAL)
                                  ? extBlocksPerTick_
                                  : ((ticksEveryNCalls_ > 2.0f) ? ticksEveryNCalls_ : 31.25f);
        float stepBlocks = (float)stepTicks * blocksPerTick;
        if (curSwung && swingNorm > 0.0f) {
            int d = (int)roundf(swingNorm * stepBlocks);
            swingDelay = (uint16_t)((d < 1) ? 1 : d);
        }
        if (humanize > 0 && curPatternStep > 0) {
            int hd = (int)roundf(fabsf(noise[0]) * stepBlocks * ((float)humanize * 0.0030f));
            if (hd > 0) {
                int maxTotal = (int)stepBlocks - 4;
                int totalDelay = (int)swingDelay + hd;
                if (maxTotal > 1 && totalDelay > maxTotal) {
                    totalDelay = maxTotal;
                }
                swingDelay = (uint16_t)totalDelay;
            }
        }
        int humnVel = (humanize > 0) ? (int)roundf(noise[1] * ((float)humanize * 0.40f)) : 0;
        uint8_t has_arpeggiator_note = (bitmask_ & pattern) ? 255 : 0;
        bool has_accent = (bitmask_ & accentMask) != 0;
        int stepMode = getArpeggiatorStepMode();
        const int num_notes = note_stack_.size();
        const int direction = sharedParams().engineArp1.direction;

        if (num_notes == 0) {
            stopArpRatchet();
        }
        if (num_notes && has_arpeggiator_note) {
            if (swingDelay == 0) {
                stopArpRatchet();
            }
            arpPendingStepMode_ = (uint8_t)stepMode;
            if (ARPEGGIO_DIRECTION_CHORD != direction && ARPEGGIO_DIRECTION_STRIDE != direction && stepMode != ARP_STEP_MODE_CHORD) {
                StepArpeggio();
                int step, transpose = 0;
                if (direction == ARPEGGIO_DIRECTION_CONVERGE) {
                    int pos = current_step_;
                    step = (pos & 1) ? ((num_notes - 1) - (pos >> 1)) : (pos >> 1);
                } else if (direction == ARPEGGIO_DIRECTION_DIVERGE) {
                    int revPos = (num_notes - 1) - current_step_;
                    step = (revPos & 1) ? ((num_notes - 1) - (revPos >> 1)) : (revPos >> 1);
                } else if (direction == ARPEGGIO_DIRECTION_THUMB && num_notes > 2) {
                    step = (current_step_ & 1) ? (1 + (current_step_ >> 1)) : 0;
                } else if (direction == ARPEGGIO_DIRECTION_PINKY && num_notes > 2) {
                    step = (current_step_ & 1) ? (num_notes - 1) : (current_step_ >> 1);
                } else if (direction == ARPEGGIO_DIRECTION_LEAP) {
                    if (num_notes > 2) {
                        int half = (num_notes + 1) >> 1;
                        step = (current_step_ & 1) ? (half + (current_step_ >> 1)) : (current_step_ >> 1);
                        if (step >= num_notes) {
                            step = num_notes - 1;
                        }
                    } else {
                        step = current_step_;
                    }
                } else if (current_direction_ > 0) {
                    step = start_step_ + current_step_;
                    if (step >= num_notes) {
                        step -= num_notes;
                        transpose = 12;
                    }
                } else {
                    step = (num_notes - 1) - (start_step_ + current_step_);
                    if (step < 0) {
                        step += num_notes;
                        transpose = -12;
                    }
                }
                const NoteEntry& noteEntry =
                    ARPEGGIO_DIRECTION_PLAYED == direction ? note_stack_.played_note(step) : note_stack_.sorted_note(step);

                uint8_t note = noteEntry.note;
                int effVel = (int)roundf((float)noteEntry.velocity * swingVelScale) + (has_accent ? 32 : 0) + humnVel;
                if (effVel < 1) {
                    effVel = 1;
                } else if (effVel > 127) {
                    effVel = 127;
                }
                uint8_t velocity = (uint8_t)effVel;
                note += 12 * current_octave_;
                if (__canTranspose(direction))
                    note += transpose;
                if (stepMode == ARP_STEP_MODE_OCT_UP) {
                    note += 12;
                } else if (stepMode == ARP_STEP_MODE_OCT_DOWN && note >= 12) {
                    note -= 12;
                }

                while (note > 127) {
                    note -= 12;
                }

                uint8_t compNote = 0;
                bool hasDyad = false;
                if (stepMode == ARP_STEP_MODE_DYAD) {
                    if (num_notes >= 2) {
                        int compIdx = (num_notes >= 3) ? ((step + 2) % num_notes) : ((step + 1) & 1);
                        const NoteEntry& compEntry =
                            (ARPEGGIO_DIRECTION_PLAYED == direction) ? note_stack_.played_note(compIdx) : note_stack_.sorted_note(compIdx);
                        int cn = (int)compEntry.note + 12 * current_octave_ + (__canTranspose(direction) ? transpose : 0);
                        while (cn > 127) {
                            cn -= 12;
                        }
                        if (cn >= 0 && cn != note) {
                            compNote = (uint8_t)cn;
                            hasDyad = true;
                        }
                    } else {
                        compNote = (note <= 120) ? (uint8_t)(note + 7) : (uint8_t)(note - 5);
                        hasDyad = true;
                    }
                }

                if (swingDelay == 0) {
                    SendNote(note, velocity, stepMode);
                    if (hasDyad) {
                        uint8_t compVel = (uint8_t)((velocity * 7) >> 3);
                        if (compVel < 1) {
                            compVel = 1;
                        }
                        SendNote(compNote, compVel, stepMode);
                    }
                } else {
                    arpStrumNotes_[0] = note;
                    arpStrumVelocities_[0] = velocity;
                    arpStrumDelay_[0] = swingDelay;
                    arpStrumCount_ = 1;
                    if (hasDyad) {
                        uint8_t compVel = (uint8_t)((velocity * 7) >> 3);
                        if (compVel < 1) {
                            compVel = 1;
                        }
                        arpStrumNotes_[1] = compNote;
                        arpStrumVelocities_[1] = compVel;
                        arpStrumDelay_[1] = (uint16_t)(swingDelay + 3);
                        arpStrumCount_ = 2;
                    }
                }
            } else {
                uint32_t seed = (uint32_t)chord_cycle_ * 2654435761u;
                bool strumDown = has_accent ? (((seed >> 8) % 5u) == 0u) : (((seed >> 8) & 1u) != 0u);
                bool highPhase = (chord_cycle_ & 1u) != 0u;
                chord_cycle_++;

                int chordIndices[16];
                int chordCount = 0;
                if (direction == ARPEGGIO_DIRECTION_STRIDE && stepMode != ARP_STEP_MODE_CHORD) {
                    if (num_notes == 1) {
                        chordIndices[chordCount++] = 0;
                    } else if (num_notes == 2) {
                        chordIndices[chordCount++] = highPhase ? 1 : 0;
                    } else if (!highPhase) {
                        chordIndices[chordCount++] = 0;
                    } else if (num_notes == 3) {
                        chordIndices[chordCount++] = 1;
                        chordIndices[chordCount++] = 2;
                    } else {
                        chordIndices[chordCount++] = num_notes - 2;
                        chordIndices[chordCount++] = num_notes - 1;
                    }
                } else if (num_notes <= 3) {
                    for (int idx = 0; idx < num_notes && idx < 16; ++idx) {
                        chordIndices[chordCount++] = idx;
                    }
                } else if (!highPhase) {
                    chordIndices[chordCount++] = 0;
                    chordIndices[chordCount++] = 1;
                } else {
                    chordIndices[chordCount++] = num_notes - 2;
                    chordIndices[chordCount++] = num_notes - 1;
                }

                bool hasRatchet = (stepMode == ARP_STEP_MODE_RATCHET2) ||
                                  (numberOfVoices_ > 0.0f && fabsf(engineImpl_->getMatrixDestination(MTX_DEST_ARP_RATCHET)) >= 0.04f);
                // Tight, organic strum spacing: ~2-5 ms per note (3-7 blocks at 1500 Hz).
                int baseStrum = has_accent ? (3 + (int)((seed >> 2) % 3u)) : (4 + (int)((seed >> 2) % 4u));
                int accumDelay = 0;

                for (int idx = 0; idx < chordCount; ++idx) {
                    int i = strumDown ? chordIndices[chordCount - 1 - idx] : chordIndices[idx];
                    const NoteEntry& noteEntry = note_stack_.sorted_note(i);
                    int effVel = (int)roundf((float)noteEntry.velocity * swingVelScale) + (has_accent ? 24 : 0) + humnVel;
                    if (chordCount > 1) {
                        int velJitter = ((int)((seed >> (12 + (idx & 3) * 3)) & 7u) - 3) * 2;
                        effVel += (-4 * idx) + velJitter;
                    }
                    if (effVel < 32) {
                        effVel = 32;
                    } else if (effVel > 127) {
                        effVel = 127;
                    }
                    uint8_t chordNote = noteEntry.note;
                    if (stepMode == ARP_STEP_MODE_OCT_UP && chordNote <= 115) {
                        chordNote += 12;
                    } else if (stepMode == ARP_STEP_MODE_OCT_DOWN && chordNote >= 12) {
                        chordNote -= 12;
                    }

                    if ((swingDelay == 0 && (idx == 0 || hasRatchet)) || arpStrumCount_ >= 8) {
                        SendNote(chordNote, (uint8_t)effVel, stepMode);
                    } else {
                        uint16_t noteDelay = swingDelay;
                        if (idx > 0 && !hasRatchet) {
                            int jitter = (int)((seed >> (4 + (idx & 3) * 2)) & 3u) - 1;
                            int stepDelay = baseStrum + jitter;
                            if (stepDelay < 2) {
                                stepDelay = 2;
                            }
                            accumDelay += stepDelay;
                            if (accumDelay > 48) {
                                accumDelay = 48;
                            }
                            noteDelay = (uint16_t)(swingDelay + accumDelay);
                        }
                        arpStrumNotes_[arpStrumCount_] = chordNote;
                        arpStrumVelocities_[arpStrumCount_] = (uint8_t)effVel;
                        arpStrumDelay_[arpStrumCount_] = noteDelay;
                        arpStrumCount_++;
                    }
                }
            }
        }
        if (bitmask_ >= endMask) {
            bitmask_ = 1;
        } else {
            bitmask_ <<= 1;
        }
    }
}

void Timbre::StepArpeggio() {

    if (current_octave_ == 127) {
        StartArpeggio();
        return;
    }

    int direction = sharedParams().engineArp1.direction;
    uint8_t num_notes = note_stack_.size();
    if (direction == ARPEGGIO_DIRECTION_RANDOM) {
        uint8_t random_byte = *(uint8_t*)noise;
        current_octave_ = random_byte & 0xf;
        current_step_ = (random_byte & 0xf0) >> 4;
        while (current_octave_ >= sharedParams().engineArp1.octave) {
            current_octave_ -= sharedParams().engineArp1.octave;
        }
        while (current_step_ >= num_notes) {
            current_step_ -= num_notes;
        }
    } else if (direction == ARPEGGIO_DIRECTION_RANDOM_WALK) {
        uint8_t random_byte = *(uint8_t*)noise;
        int delta = (random_byte % 3) - 1;
        int totalNotes = num_notes * (int)sharedParams().engineArp1.octave;
        int flat = (int)current_octave_ * num_notes + (int)current_step_ + delta;
        if (flat < 0) {
            flat = (totalNotes > 1) ? 1 : 0;
        } else if (flat >= totalNotes) {
            flat = (totalNotes > 1) ? (totalNotes - 2) : 0;
        }
        current_octave_ = (uint8_t)(flat / num_notes);
        current_step_ = (uint8_t)(flat % num_notes);
    } else if (direction == ARPEGGIO_DIRECTION_OCTAVE_NOTE) {
        if (++current_octave_ >= sharedParams().engineArp1.octave) {
            current_octave_ = 0;
            ++current_step_;
        }
        if (current_step_ >= num_notes) {
            current_step_ = 0;
        }
    } else {
        // NOTE: We always count [0 - cycleLen) here; the actual handling of direction is in Tick()
        uint8_t cycleLen = num_notes;
        if ((direction == ARPEGGIO_DIRECTION_THUMB || direction == ARPEGGIO_DIRECTION_PINKY) && num_notes > 2) {
            cycleLen = (uint8_t)(2 * (num_notes - 1));
        }

        uint8_t trigger_change = 0;
        if (++current_step_ >= cycleLen) {
            current_step_ = 0;
            trigger_change = 1;
        }

        // special case the 'ROTATE' and 'SHIFT' modes, they might not change the octave until the cycle is through
        if (trigger_change && (direction >= ARPEGGIO_DIRECTION_ROTATE_UP && direction <= ARPEGGIO_DIRECTION_SHIFT_UP_DOWN)) {
            if (++start_step_ >= num_notes)
                start_step_ = 0;
            else
                trigger_change = 0;
        }

        if (trigger_change) {
            current_octave_ += current_direction_;
            if (current_octave_ >= sharedParams().engineArp1.octave || current_octave_ < 0) {
                if (__canChangeDir(direction)) {
                    current_direction_ = -current_direction_;
                    StartArpeggio();
                    if (direction != ARPEGGIO_DIRECTION_UP_DOWN_INC && (num_notes > 1 || sharedParams().engineArp1.octave > 1)) {
                        StepArpeggio();
                    }
                } else {
                    StartArpeggio();
                }
            }
        }
    }
}

void Timbre::StartArpeggio() {

    current_step_ = 0;
    start_step_ = 0;
    if (current_direction_ == 1) {
        current_octave_ = 0;
    } else {
        current_octave_ = sharedParams().engineArp1.octave - 1;
    }
}

void Timbre::resetArpPatternState() {
    bitmask_ = 1;
    chord_cycle_ = 0;
    arpStrumCount_ = 0;
    arpPendingStepMode_ = ARP_STEP_MODE_NORM;
    arpSwingStepScale_ = 1.0f;
    current_octave_ = 127;
    current_direction_ = __getDirection(sharedParams().engineArp1.direction);
}

void Timbre::Start() {
    resetArpPatternState();
    recording_ = 0;
    running_ = 1;
    tick_ = getArpStepTicks() - 1;
}

void Timbre::arpeggiatorSetHoldPedal(uint8_t value) {
    if (ignore_note_off_messages_ && !value) {
        // Pedal was released, kill all pending arpeggios.
        note_stack_.Clear();
    }
    ignore_note_off_messages_ = value;
}

void Timbre::setLatchMode(uint8_t value) {
    // When disabling latch mode, clear the note stack.
    latch_ = value;
    if (value == 0) {
        note_stack_.Clear();
        recording_ = 0;
    }
}

void Timbre::setDirection(uint8_t value) {
    // When changing the arpeggio direction, reset the pattern.
    current_direction_ = __getDirection(value);
    StartArpeggio();
}

void Timbre::lfoValueChange(int currentRow, int encoder, float newValue) {
    engineImpl_->lfoValueChange(currentRow, encoder, newValue);
}

void Timbre::updateMidiNoteScale(int scale) {
    engineImpl_->updateMidiNoteScale(scale);
}

void Timbre::midiClockContinue(int songPosition) {
    engineImpl_->midiClockContinue(songPosition);
}

void Timbre::midiClockStart() {
    engineImpl_->midiClockStart();
}

void Timbre::midiClockSongPositionStep(int songPosition) {
    engineImpl_->midiClockSongPositionStep(songPosition);
}

void Timbre::resetMatrixDestination(float oldValue) {
    engineImpl_->resetMatrixDestination(oldValue);
}

void Timbre::setMatrixSource(enum SourceEnum source, float newValue) {
    engineImpl_->setMatrixSource(source, newValue);
}

void Timbre::setMatrixSourceMPE(uint8_t channel, enum SourceEnum source, float newValue) {
    engineImpl_->setMatrixSourceMPE(channel, source, newValue);
}

void Timbre::setMatrixPolyAfterTouch(uint8_t note, float newValue) {
    engineImpl_->setMatrixPolyAfterTouch(note, newValue);
}

void Timbre::verifyLfoUsed(int encoder, float oldValue, float newValue) {
    engineImpl_->verifyLfoUsed(encoder, oldValue, newValue);
}
