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
#include "Pfm3Param.h"
#include "MidiControlChange.h"
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

    ARPEGGIO_DIRECTION_COUNT
};

// TODO Maybe add something like struct ArpDirectionParams { dir, can_change, use_start_step }

inline static int __getDirection(int _direction) {
    switch (_direction) {
    case ARPEGGIO_DIRECTION_DOWN:
    case ARPEGGIO_DIRECTION_ROTATE_DOWN:
    case ARPEGGIO_DIRECTION_SHIFT_DOWN:
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
    ARP_PATTERN(21845), ARP_PATTERN(62965), ARP_PATTERN(46517), ARP_PATTERN(54741), ARP_PATTERN(43861), ARP_PATTERN(22869),
    ARP_PATTERN(38293), ARP_PATTERN(2313),  ARP_PATTERN(37449), ARP_PATTERN(21065), ARP_PATTERN(18761), ARP_PATTERN(54553),
    ARP_PATTERN(27499), ARP_PATTERN(23387), ARP_PATTERN(30583), ARP_PATTERN(28087), ARP_PATTERN(22359), ARP_PATTERN(28527),
    ARP_PATTERN(30431), ARP_PATTERN(43281), ARP_PATTERN(28609), ARP_PATTERN(53505)};

uint16_t Timbre::getArpeggiatorPattern() const {
    const int pattern = (int)sharedParams().engineArp2.pattern;
    if (pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT)
        return ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[pattern]);
    else
        return ARP_PATTERN_GETMASK(sharedParams().engineArpUserPatterns.patterns[pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT]);
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
        return;
    }
    stopPlayingNow();
    engine_ = engine;
    engineImpl_ = SynthEngineFactory::get(engine_, timbreNumber_);
    engineImpl_->afterNewParamsLoad();
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
            arpegiatorStep_ -= ticksEveyNCallsInteger_;
            Tick();
        }
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
    // CLOCK_MODE_INTERNAL
    if (sharedParams().engineArp1.clock == CLOCK_INTERNAL) {
        if (idle_ticks_ >= 96 || !running_) {
            Start();
        }
        idle_ticks_ = 0;
    }

    if (latch_ && !recording_) {
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
    if (sharedParams().engineArp1.clock == CLOCK_EXTERNAL && running_) {
        Tick();
    }
}

void Timbre::SendNote(uint8_t note, uint8_t velocity) {

    // If there are some Note Off messages for the note about to be triggeered
    // remove them from the queue and process them now.
    if (event_scheduler_.Remove(note, 0)) {
        engineNoteOff(note);
    }

    // Send a note on and schedule a note off later.
    engineNoteOn(note, velocity);
    event_scheduler_.Schedule(note, 0, midi_clock_tick_per_step[(int)sharedParams().engineArp2.duration] - 1, 0);
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

    if (tick_ >= midi_clock_tick_per_step[(int)sharedParams().engineArp2.division]) {
        tick_ = 0;
        uint16_t pattern = getArpeggiatorPattern();
        uint8_t has_arpeggiator_note = (bitmask_ & pattern) ? 255 : 0;
        const int num_notes = note_stack_.size();
        const int direction = sharedParams().engineArp1.direction;

        if (num_notes && has_arpeggiator_note) {
            if (ARPEGGIO_DIRECTION_CHORD != direction) {
                StepArpeggio();
                int step, transpose = 0;
                if (current_direction_ > 0) {
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
                uint8_t velocity = noteEntry.velocity;
                note += 12 * current_octave_;
                if (__canTranspose(direction))
                    note += transpose;

                while (note > 127) {
                    note -= 12;
                }

                SendNote(note, velocity);
            } else {
                for (int i = 0; i < note_stack_.size(); ++i) {
                    const NoteEntry& noteEntry = note_stack_.sorted_note(i);
                    SendNote(noteEntry.note, noteEntry.velocity);
                }
            }
        }
        bitmask_ <<= 1;
        if (!bitmask_) {
            bitmask_ = 1;
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
    } else {
        // NOTE: We always count [0 - num_notes) here; the actual handling of direction is in Tick()

        uint8_t trigger_change = 0;
        if (++current_step_ >= num_notes) {
            current_step_ = 0;
            trigger_change = 1;
        }

        // special case the 'ROTATE' and 'SHIFT' modes, they might not change the octave until the cycle is through
        if (trigger_change && (direction >= ARPEGGIO_DIRECTION_ROTATE_UP)) {
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
                    if (num_notes > 1 || sharedParams().engineArp1.octave > 1) {
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

void Timbre::Start() {
    bitmask_ = 1;
    recording_ = 0;
    running_ = 1;
    tick_ = midi_clock_tick_per_step[(int)sharedParams().engineArp2.division] - 1;
    current_octave_ = 127;
    current_direction_ = __getDirection(sharedParams().engineArp1.direction);
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
