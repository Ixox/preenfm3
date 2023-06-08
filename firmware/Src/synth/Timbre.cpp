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


#include <math.h>
#include "Timbre.h"
#include "Voice.h"

#define INV127 .00787401574803149606f
#define INV16 .0625
#define INV_BLOCK_SIZE (1.0f / BLOCK_SIZE)

// Regular memory
float midiNoteScale[2][NUMBER_OF_TIMBRES][128] __attribute__((section(".ram_d1")));
float Timbre::unisonPhase[14] = { .37f, .11f, .495f, .53f, .03f, .19f, .89f, 0.23f, .71f, .19f, .31f, .43f, .59f, .97f };
float Timbre::delayBuffer[NUMBER_OF_TIMBRES][delayBufferSize] __attribute__ ((section(".ram_d2b")));

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
    ARPEGGIO_DIRECTION_ROTATE_UP, ARPEGGIO_DIRECTION_ROTATE_DOWN, ARPEGGIO_DIRECTION_ROTATE_UP_DOWN,
   /*
    * SHIFT modes rotate and extend with transpose, e.g. UP: C-E-G -> E-G-C1 -> G-C1-E1 -> repeat
    */
    ARPEGGIO_DIRECTION_SHIFT_UP, ARPEGGIO_DIRECTION_SHIFT_DOWN, ARPEGGIO_DIRECTION_SHIFT_UP_DOWN,

    ARPEGGIO_DIRECTION_COUNT
};

// TODO Maybe add something like struct ArpDirectionParams { dir, can_change, use_start_step }

inline static int __getDirection( int _direction ) {
    switch( _direction ) {
    case ARPEGGIO_DIRECTION_DOWN:
    case ARPEGGIO_DIRECTION_ROTATE_DOWN:
    case ARPEGGIO_DIRECTION_SHIFT_DOWN:
        return -1;
    default:
        return 1;
    }
}

inline static int __canChangeDir( int _direction ) {
    switch( _direction ) {
    case ARPEGGIO_DIRECTION_UP_DOWN:
    case ARPEGGIO_DIRECTION_ROTATE_UP_DOWN:
    case ARPEGGIO_DIRECTION_SHIFT_UP_DOWN:
        return 1;
    default:
        return 0;
    }
}

inline static int __canTranspose( int _direction ) {
    switch( _direction ) {
    case ARPEGGIO_DIRECTION_SHIFT_UP:
    case ARPEGGIO_DIRECTION_SHIFT_DOWN:
    case ARPEGGIO_DIRECTION_SHIFT_UP_DOWN:
        return 1;
    default:
        return 0;
    }
}

enum NewNoteType {
    NEW_NOTE_FREE = 0,
    NEW_NOTE_RELEASE,
    NEW_NOTE_OLD,
    NEW_NOTE_NONE
};


arp_pattern_t lut_res_arpeggiator_patterns[ ARPEGGIATOR_PRESET_PATTERN_COUNT ]  = {
  ARP_PATTERN(21845), ARP_PATTERN(62965), ARP_PATTERN(46517), ARP_PATTERN(54741),
  ARP_PATTERN(43861), ARP_PATTERN(22869), ARP_PATTERN(38293), ARP_PATTERN(2313),
  ARP_PATTERN(37449), ARP_PATTERN(21065), ARP_PATTERN(18761), ARP_PATTERN(54553),
  ARP_PATTERN(27499), ARP_PATTERN(23387), ARP_PATTERN(30583), ARP_PATTERN(28087),
  ARP_PATTERN(22359), ARP_PATTERN(28527), ARP_PATTERN(30431), ARP_PATTERN(43281),
  ARP_PATTERN(28609), ARP_PATTERN(53505)
};

uint16_t Timbre::getArpeggiatorPattern() const
{
  const int pattern = (int)params_.engineArp2.pattern;
  if ( pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT )
    return ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[ pattern ]);
  else
    return ARP_PATTERN_GETMASK( params_.engineArpUserPatterns.patterns[ pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT ] );
}

const uint8_t midi_clock_tick_per_step[17]  = {
  192, 144, 96, 72, 64, 48, 36, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1
};

extern float noise[32];

float panTable[]  = {
        0.0000, 0.0007, 0.0020, 0.0036, 0.0055, 0.0077, 0.0101, 0.0128, 0.0156, 0.0186,
        0.0218, 0.0252, 0.0287, 0.0324, 0.0362, 0.0401, 0.0442, 0.0484, 0.0527, 0.0572,
        0.0618, 0.0665, 0.0713, 0.0762, 0.0812, 0.0863, 0.0915, 0.0969, 0.1023, 0.1078,
        0.1135, 0.1192, 0.1250, 0.1309, 0.1369, 0.1430, 0.1492, 0.1554, 0.1618, 0.1682,
        0.1747, 0.1813, 0.1880, 0.1947, 0.2015, 0.2085, 0.2154, 0.2225, 0.2296, 0.2369,
        0.2441, 0.2515, 0.2589, 0.2664, 0.2740, 0.2817, 0.2894, 0.2972, 0.3050, 0.3129,
        0.3209, 0.3290, 0.3371, 0.3453, 0.3536, 0.3619, 0.3703, 0.3787, 0.3872, 0.3958,
        0.4044, 0.4131, 0.4219, 0.4307, 0.4396, 0.4485, 0.4575, 0.4666, 0.4757, 0.4849,
        0.4941, 0.5034, 0.5128, 0.5222, 0.5316, 0.5411, 0.5507, 0.5604, 0.5700, 0.5798,
        0.5896, 0.5994, 0.6093, 0.6193, 0.6293, 0.6394, 0.6495, 0.6597, 0.6699, 0.6802,
        0.6905, 0.7009, 0.7114, 0.7218, 0.7324, 0.7430, 0.7536, 0.7643, 0.7750, 0.7858,
        0.7967, 0.8076, 0.8185, 0.8295, 0.8405, 0.8516, 0.8627, 0.8739, 0.8851, 0.8964,
        0.9077, 0.9191, 0.9305, 0.9420, 0.9535, 0.9651, 0.9767, 0.9883, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
        1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000
} ;


inline
float modulo2(float readPos, int bufferLen) {
    if(unlikely(readPos < 0) )
        readPos += bufferLen;
    return readPos;
}
inline
float modulo(float d, float max) {
    return unlikely(d >= max) ? d - max : d;
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
float foldAbs(float x) {
    x *= 0.5f;
    float f = (fabsf(x - roundf(x)));
    return f + f;
}
inline
float fold(float x4) {
    return fabsf(x4 + 0.25f - roundf(x4 + 0.25f)) - 0.25f;
}
inline
float sigmoid(float x)
{
    return x * (1.5f - 0.5f * x * x);
}
inline
float sigmoidPos(float x)
{
    //x : 0 -> 1
    return (sigmoid((x * 2) - 1) + 1) * 0.5f;
}
inline
float sigmoid2(float x)
{
    return x * (1.8f - 0.864f * x * x );
}
inline
float sigmoid3(float x)
{
    return x * (1.4f - 0.4065f * x * x);
}
inline
float tanh3(float x)
{
    return 1.5f * x / (1.7f + fabsf(0.34f * x * x));
}
inline
float tanh4(float x)
{
    return x / sqrt3(x * x + 1);
}
inline 
float fastSin(float x) {
    return 3.9961f * x * ( 1 - fabsf(x) );
}
inline 
float hann(float x) {
    float s = sqrt3(x * (1 - x));
    return s + s;
}
Timbre::Timbre() {

    recomputeNext_ = true;
    currentGate_ = 0;
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

void Timbre::init(SynthState *synthState, int timbreNumber) {

    mixerState_ = &synthState->mixerState;

    env1_.init(&params_.env1Time, &params_.env1Level, 0, &params_.engine1.algo, &params_.env1Curve);
    env2_.init(&params_.env2Time, &params_.env2Level, 1, &params_.engine1.algo, &params_.env2Curve);
    env3_.init(&params_.env3Time, &params_.env3Level, 2, &params_.engine1.algo, &params_.env3Curve);
    env4_.init(&params_.env4Time, &params_.env4Level, 3, &params_.engine1.algo, &params_.env4Curve);
    env5_.init(&params_.env5Time, &params_.env5Level, 4, &params_.engine1.algo, &params_.env5Curve);
    env6_.init(&params_.env6Time, &params_.env6Level, 5, &params_.engine1.algo, &params_.env6Curve);

    osc1_.init(synthState, &params_.osc1, OSC1_FREQ);
    osc2_.init(synthState, &params_.osc2, OSC2_FREQ);
    osc3_.init(synthState, &params_.osc3, OSC3_FREQ);
    osc4_.init(synthState, &params_.osc4, OSC4_FREQ);
    osc5_.init(synthState, &params_.osc5, OSC5_FREQ);
    osc6_.init(synthState, &params_.osc6, OSC6_FREQ);

    timbreNumber_ = timbreNumber;

    for (int s = 0; s < 2; s++) {
        for (int n = 0; n < 128; n++) {
            midiNoteScale[s][timbreNumber][n] = INV127 * (float) n;
        }
    }
    for (int lfo = 0; lfo < NUMBER_OF_LFO; lfo++) {
        lfoUSed_[lfo] = 0;
    }

    lowerNote_ = 64;
    lowerNoteReleased_ = true;

    /** --------------FX init--------------  */

    delayBuffer_ = delayBuffer[timbreNumber_];

    for (int s = 0; s < delayBufferSize; s++) {
        delayBuffer_[s] = 0;
    }

    // all pass params
    float f1 = 0.0156f;
    apcoef1 = (1.0f - f1) / (1.0f + f1);
    float f2 = clamp(0.17f + f1, 0.01f, 0.99f);
    apcoef2 = (1.0f - f2) / (1.0f + f2);
    float f3 = clamp(0.17f + f2, 0.01f, 0.99f);
    apcoef3 = (1.0f - f3) / (1.0f + f3);
    float f4 = clamp(0.17f + f3, 0.01f, 0.99f);
    apcoef4 = (1.0f - f4) / (1.0f + f4);
    
}

void Timbre::setVoiceNumber(int v, int n) {
    voiceNumber_[v] = n;
    if (n >= 0) {
        voices_[n]->setCurrentTimbre(this);
    }
}

void Timbre::initVoicePointer(int n, Voice *voice) {
    voices_[n] = voice;
}

void Timbre::noteOn(char note, char velocity) {
    if (params_.engineArp1.clock) {
        arpeggiatorNoteOn(note, velocity);
    } else {
        preenNoteOn(note, velocity);
    }
}

void Timbre::noteOff(char note) {
    if (params_.engineArp1.clock) {
        arpeggiatorNoteOff(note);
    } else {
        preenNoteOff(note);
    }
}

void Timbre::noteOnMPE(uint8_t channel, uint8_t note, uint8_t velocity)  {
	int voiceToUse = voiceNumber_[channel -1];

    if (unlikely(voiceToUse == -1)) {
        return;
    }

    preenNoteOnUpdateMatrix(voiceToUse, note, velocity);
    float noteFrequency = mixerState_->instrumentState_[0].scaleFrequencies[(int) note];

	if (voices_[voiceToUse]->isPlaying()) {
        voices_[voiceToUse]->noteOnWithoutPop(note, noteFrequency, velocity, voiceIndex_++);
	} else {
        voices_[voiceToUse]->noteOn(note, noteFrequency, velocity, voiceIndex_++);
	}
}


void Timbre::noteOffMPE(uint8_t channel, uint8_t note, uint8_t velocityOff) {
    int voiceToUse = voiceNumber_[channel -1];

    if (unlikely(voiceToUse == -1)) {
		return;
	}

	if (voices_[voiceToUse]->isPlaying()) {
		voices_[voiceToUse]->noteOff();
	}
}


void Timbre::preenNoteOn(char note, char velocity) {
    // NumberOfVoice = 0 or no mapping in scala frequencies
    if (unlikely(numberOfVoices_ == 0 || mixerState_->instrumentState_[timbreNumber_].scaleFrequencies[(int) note] == 0.0f)) {
        return;
    }

    note &= 0x7f;

    int iNov = params_.engine1.playMode == PLAY_MODE_POLY ? (int) numberOfVoices_ : 1;

    // Frequency depends on the current instrument scale
    float noteFrequency = mixerState_->instrumentState_[timbreNumber_].scaleFrequencies[(int) note];

    uint32_t indexMin = UINT32_MAX;
    int voiceToUse = -1;

    int newNoteType = NEW_NOTE_NONE;

    for (int k = 0; k < iNov; k++) {
        // voice number k of timbre
        int n = voiceNumber_[k];

        if (unlikely(voices_[n]->isNewNotePending())) {
            continue;
        }

        // same note = priority 1 : take the voice immediatly
        if (unlikely(voices_[n]->isPlaying() && voices_[n]->getNote() == note)) {
            if (likely(params_.engine1.playMode != PLAY_MODE_UNISON)) {
                preenNoteOnUpdateMatrix(n, note, velocity);
                voices_[n]->noteOnWithoutPop(note, noteFrequency, velocity, voiceIndex_++);
            } else {
                // Unison !!
                float noteFrequencyUnison = noteFrequency  + noteFrequency * params_.engine2.unisonDetune * .05f;
                float noteFrequencyUnisonInc = noteFrequency * params_.engine2.unisonDetune * numberOfVoiceInverse_ * .1f;
                for (int k = 0; k < numberOfVoices_; k++) {
                    int n = voiceNumber_[k];
                    preenNoteOnUpdateMatrix(n, note, velocity);
                    voices_[n]->noteOnWithoutPop(note, noteFrequencyUnison, velocity, voiceIndex_++, unisonPhase[k]);
                    noteFrequencyUnison += noteFrequencyUnisonInc;
                }
            }
            lastPlayedNote_ = n;
            // Just in case we tune Osc Freq/Ftune while playing the same note
            lowerNoteFrequency = voices_[n]->getNoteRealFrequencyEstimation(noteFrequency);
            return;
        }

        // unlikely because if it true, CPU is not full
        if (unlikely(newNoteType > NEW_NOTE_FREE)) {
            if (!voices_[n]->isPlaying()) {
                voiceToUse = n;
                newNoteType = NEW_NOTE_FREE;
            } else if (voices_[n]->isReleased()) {
                uint32_t indexVoice = voices_[n]->getIndex();
                if (indexVoice < indexMin) {
                    indexMin = indexVoice;
                    voiceToUse = n;
                    newNoteType = NEW_NOTE_RELEASE;
                }
            }
        }
    }

    if (voiceToUse == -1) {
        for (int k = 0; k < iNov; k++) {
            // voice number k of timbre
            int n = voiceNumber_[k];
            unsigned int indexVoice = voices_[n]->getIndex();
            if (indexVoice < indexMin && !voices_[n]->isNewNotePending()) {
                newNoteType = NEW_NOTE_OLD;
                indexMin = indexVoice;
                voiceToUse = n;
            }
        }
    }
    // All voices in newnotepending state ?
    if (voiceToUse != -1) {

        if (likely(params_.engine1.playMode != PLAY_MODE_UNISON)) {
            preenNoteOnUpdateMatrix(voiceToUse, note, velocity);

            switch (newNoteType) {
                case NEW_NOTE_FREE:
                    voices_[voiceToUse]->noteOn(note, noteFrequency, velocity, voiceIndex_++);
                    break;
                case NEW_NOTE_OLD:
                case NEW_NOTE_RELEASE:
                    voices_[voiceToUse]->noteOnWithoutPop(note, noteFrequency, velocity, voiceIndex_++);
                    break;
            }
        } else {
            // Unisons : we start all voices with different frequency
            float noteFrequencyUnison = noteFrequency  + noteFrequency * params_.engine2.unisonDetune * .05f;
            float noteFrequencyUnisonInc = noteFrequency * params_.engine2.unisonDetune * numberOfVoiceInverse_ * .1f;

            for (int k = 0; k < numberOfVoices_; k++) {
                int n = voiceNumber_[k];

                preenNoteOnUpdateMatrix(n, note, velocity);

                switch (newNoteType) {
                    case NEW_NOTE_FREE:
                        voices_[n]->noteOn(note, noteFrequencyUnison, velocity, voiceIndex_++, unisonPhase[k]);
                        break;
                    case NEW_NOTE_OLD:
                    case NEW_NOTE_RELEASE:
                        voices_[n]->noteOnWithoutPop(note, noteFrequencyUnison, velocity, voiceIndex_++, unisonPhase[k]);
                        break;
                }
                noteFrequencyUnison += noteFrequencyUnisonInc;
            }
        }

        lastPlayedNote_ = voiceToUse;
        if (note <= lowerNote_ || lowerNoteReleased_ || iNov == 1) {
            lowerNote_ = note;
            lowerNoteReleased_ = false;
            lowerNoteFrequency = voices_[voiceToUse]->getNoteRealFrequencyEstimation(noteFrequency);
        }

    }
}

void Timbre::preenNoteOnUpdateMatrix(int voiceToUse, int note, int velocity) {
    // Update voice matrix with midi note and velocity
    voices_[voiceToUse]->matrix.setSource(MATRIX_SOURCE_NOTE1, midiNoteScale[0][timbreNumber_][note]);
    voices_[voiceToUse]->matrix.setSource(MATRIX_SOURCE_NOTE2, midiNoteScale[1][timbreNumber_][note]);
    voices_[voiceToUse]->matrix.setSource(MATRIX_SOURCE_VELOCITY, INV127 * velocity);
    voices_[voiceToUse]->matrix.setSource(MATRIX_SOURCE_RANDOM, noise[voiceToUse]);
}

void Timbre::preenNoteOff(char note) {
    bool isUnison = params_.engine1.playMode == PLAY_MODE_UNISON;


    if (note == lowerNote_) {
        lowerNoteReleased_ = true;
    }

    // Unison we must check all voices.... (different from noteOn)
    int iNov = params_.engine1.playMode == PLAY_MODE_MONO ? 1 : (int) numberOfVoices_;
    for (int k = 0; k < iNov; k++) {
        // voice number k of timbre
        int n = voiceNumber_[k];

        // Not playing = free CPU
        if (unlikely(!voices_[n]->isPlaying())) {
            continue;
        }

        if (likely(voices_[n]->getNextGlidingNote() == 0 || voices_[n]->isNewGlide())) {
            if (voices_[n]->getNote() == note) {
                if (unlikely(holdPedal_)) {
                    voices_[n]->setHoldedByPedal(true);
                    if (likely(!isUnison)) {
                        return;
                    }
                } else {
                    voices_[n]->noteOff();
                    if (likely(!isUnison)) {
                        return;
                    }
                }
            }
        } else {
            // if gliding and releasing first note
            if (voices_[n]->getNote() == note) {
                voices_[n]->glideFirstNoteOff();
                if (likely(!isUnison)) {
                    return;
                }
            }
            // if gliding and releasing next note
            if (voices_[n]->getNextGlidingNote() == note) {
                voices_[n]->glideToNote(voices_[n]->getNote(), voices_[n]->getNoteFrequency());
                voices_[n]->glideFirstNoteOff();
                // Sync Osccilo
                lowerNoteFrequency = voices_[n]->getNoteRealFrequencyEstimation(voices_[n]->getNoteFrequency());
                if (likely(!isUnison)) {
                    return;
                }
            }
        }
    }
}


void Timbre::stopPlayingNow() {
    for (int k = 0; k < numberOfVoices_; k++) {
        // voice number k of timbre
        int n = voiceNumber_[k];
        if (n != -1) {
            voices_[n]->killNow();
        }
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
                voices_[n]->noteOff();
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
    ticksEveyNCallsInteger_ = (int) ticksEveryNCalls_;
}

void Timbre::setArpeggiatorClock(float clockValue) {
    if (clockValue == CLOCK_OFF) {
        FlushQueue();
        note_stack_.Clear();
    }
    if (clockValue == CLOCK_INTERNAL) {
        setNewBPMValue(params_.engineArp1.BPM);
    }
    if (clockValue == CLOCK_EXTERNAL) {
        // Let's consider we're running
        running_ = 1;
    }
}

void Timbre::updateArpegiatorInternalClock() {

    // Apeggiator clock : internal
    if (params_.engineArp1.clock == CLOCK_INTERNAL) {
        arpegiatorStep_ += 1.0f;
        if (unlikely((arpegiatorStep_) > ticksEveryNCalls_)) {
            arpegiatorStep_ -= ticksEveyNCallsInteger_;
            Tick();
        }
    }
}

void Timbre::cleanNextBlock() {

    float *sp = sampleBlock_;
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
    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->prepareMatrixForNewBlock();
    }
}


void Timbre::glide() {
    if (unlikely(params_.engine1.playMode == PLAY_MODE_UNISON)) {
        for (int vv = 0; vv < numberOfVoices_; vv++) {
            int v = voiceNumber_[vv];
            if (v != -1 && voices_[v]->isGliding()) {
                voices_[v]->glide();
            }
        }
    } else {
        if (voiceNumber_[0] != -1 && voices_[voiceNumber_[0]]->isGliding()) {
            voices_[voiceNumber_[0]]->glide();
        }
    }
}

uint8_t Timbre::voicesNextBlock() {
    uint8_t numberOfPlayingVoices_ = 0;
    if (unlikely(params_.engine1.playMode == PLAY_MODE_UNISON)) {
        cleanNextBlock();
        // UNISON
        float pansSav[6];
        pansSav[0] = params_.engineMix1.panOsc1;
        pansSav[1] = params_.engineMix1.panOsc2;
        pansSav[2] = params_.engineMix2.panOsc3;
        pansSav[3] = params_.engineMix2.panOsc4;
        pansSav[4] = params_.engineMix3.panOsc5;
        pansSav[5] = params_.engineMix3.panOsc6;

        float* pans[] = {
            &params_.engineMix1.panOsc1,
            &params_.engineMix1.panOsc2,
            &params_.engineMix2.panOsc3,
            &params_.engineMix2.panOsc4,
            &params_.engineMix3.panOsc5,
            &params_.engineMix3.panOsc6
        };

        int currentAlgo = (int)params_.engine1.algo;
        float algoNumberOfMix = algoInformation[currentAlgo].mix;
        float numberOfCarrierOp = numberOfVoices_ * algoNumberOfMix;
        float opPan = - params_.engine2.unisonSpread;
        float opPanInc = 2.0f / numberOfCarrierOp * params_.engine2.unisonSpread;

        if (likely(voices_[voiceNumber_[0]]->isPlaying())) {
            for (int vv = 0; vv < numberOfVoices_; vv++) {
                int v = voiceNumber_[vv];
                if (unlikely(v < 0)) {
                    continue;
                }
                bool otherSide = false;
                for (int op = 0; op < 6; op ++) {
                    if (algoOpInformation[currentAlgo][op] == 1) {
                        if (otherSide) {
                            *pans[op] = -opPan;
                        } else {
                            *pans[op] = opPan;
                        }
                        opPan += opPanInc;
                        otherSide =! otherSide;
                    }
                }
                voices_[v]->nextBlock();

                if (vv > 0) {
                    // We accumulate in the first voice buffer
                    float *source = voices_[v]->getSampleBlock();
                    float *voice0SampleBlock = voices_[voiceNumber_[0]]->getSampleBlock();
                    for (int s = 0; s < BLOCK_SIZE; s++) {
                        *voice0SampleBlock++ += *source++;
                        *voice0SampleBlock++ += *source++;
                    }
                }
                numberOfPlayingVoices_++;
            }
            voices_[voiceNumber_[0]]->fxAfterBlock();

        } else {
            voices_[voiceNumber_[0]]->emptyBuffer();
        }

        params_.engineMix1.panOsc1 = pansSav[0];
        params_.engineMix1.panOsc2 = pansSav[1];
        params_.engineMix2.panOsc3 = pansSav[2];
        params_.engineMix2.panOsc4 = pansSav[3];
        params_.engineMix3.panOsc5 = pansSav[4];
        params_.engineMix3.panOsc6 = pansSav[5];
    } else {
        for (int k = 0; k < numberOfVoices_; k++) {
            int v = voiceNumber_[k];
            if (likely(voices_[v]->isPlaying())) {
                voices_[v]->nextBlock();
                voices_[v]->fxAfterBlock();
                numberOfPlayingVoices_++;
            } else {
                voices_[v]->emptyBuffer();
            }
        }
    }

    return numberOfPlayingVoices_;
}

void Timbre::fxAfterBlock() {

	int fx2Type = params_.effect2.type;
    
    if(!voices_[lastPlayedNote_]->isPlaying()) {
        // hack : this voice is not playing but still need to calculate lfo
        voices_[lastPlayedNote_]->matrix.computeAllDestinations();
    }

    float matrixFilterFrequency = voices_[lastPlayedNote_]->matrix.getDestination(FILTER2_PARAM1);
    float matrixFilterParam2    = voices_[lastPlayedNote_]->matrix.getDestination(FILTER2_PARAM2);
    float matrixFilterAmp       = voices_[lastPlayedNote_]->matrix.getDestination(FILTER2_AMP);
    float matrixFilterPan       = clamp( voices_[lastPlayedNote_]->matrix.getDestination(ALL_PAN), -1, 1);
    float gainTmp               = clamp(this->params_.effect2.param3 + matrixFilterAmp, 0, 16);

    switch (fx2Type) {
        case FILTER2_FLANGE: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 0.5f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            param1S = 0.02f * this->params_.effect2.param1 + .98f * param1S;
            float fxParamTmp = foldAbs(param1S * (param1S + matrixFilterFrequency) * 0.125f) * 0.5f;
            delayReadFrac = (fxParamTmp + 99 * delayReadFrac) * 0.01f; // smooth change
            
            float currentDelaySize1 = delaySize1;
            delaySize1 = clamp(1 + delayBufStereoSize * delayReadFrac, 0, delayBufStereoSizeM1);
            float delaySizeInc1 = (delaySize1 - currentDelaySize1) * INV_BLOCK_SIZE;

            float currentFeedback = feedback;
            feedback = clamp( this->params_.effect2.param2 + matrixFilterParam2, -1, 1) * 0.95f;
            float feedbackInc = (feedback - currentFeedback) * INV_BLOCK_SIZE;

            float *sp  = sampleBlock_;
            
            float delayReadPos90;

            float filterB2 = 0.1f;
            float filterB = (filterB2 * filterB2 * 0.5f);

            float _in3_b1 = (1 - filterB);
            float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            float _in3_a1 = -_in3_a0;

            const float f = 0.75f;
            const float f2 = 0.74f;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                
                low3  += f * band3;
                band3 += f * (*sp - low3 - band3);

                low4  += f * band4;
                band4 += f * (*(sp + 1) - low4 - band4);

                // feedback
                float feedL   = (low1) * currentFeedback;
                float feedR   = (low2) * currentFeedback;

                // audio in hp
                hp_in_x0     = tanh4(low3) - feedL;
                hp_in_y0     = _in3_a0 * hp_in_x0 + _in3_a1 * hp_in_x1 + _in3_b1 * hp_in_y1;
                hp_in_y1     = hp_in_y0;
                hp_in_x1     = hp_in_x0;

                hp_in2_x0    = tanh4(low4) - feedR;
                hp_in2_y0    = _in3_a0 * hp_in2_x0 + _in3_a1 * hp_in2_x1 + _in3_b1 * hp_in2_y1;
                hp_in2_y1    = hp_in2_y0;
                hp_in2_x1    = hp_in2_x0;

                delayWritePos = (delayWritePos + 1) & delayBufStereoSizeM1;
                delayBuffer_[delayWritePos] = hp_in_y0;
                delayBuffer_[delayWritePos + delayBufStereoSize] = hp_in2_y0;

                delayReadPos   = modulo2(delayWritePos - currentDelaySize1, delayBufStereoSize);
                delayReadPos90 = modulo2(delayReadPos - 190, delayBufStereoSize);

                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufStereoSizeM1);
                delayOut2 = delayInterpolation2(delayReadPos90, delayBuffer_, delayBufStereoSizeM1, delayBufStereoSize);

                // L

                low1  += f2 * band1;
                band1 += f2 * (delayOut1 - low1 - band1);

                _ly1 = apcoef2 * (_ly1 + low1) - _lx1; // allpass
                _lx1 = low1;

                hb4_y1 = apcoef3 * (hb4_y1 + _ly1) - hb4_x1; // allpass
                hb4_x1 = _ly1;

                // R

                low2  += f2 * band2;
                band2 += f2 * (delayOut2 - low2 - band2);

                _ly2 = apcoef3 * (_ly2 + low2) - _lx2; // allpass 2
                _lx2 = low2;

                hb4_y2 = apcoef4 * (hb4_y2 + _ly2) - hb4_x2; // allpass
                hb4_x2 = _ly2;

                *sp = *sp * dry + (low1 + hb4_y1) * wet;
                sp++;
                *sp = *sp * dry + (low2 + hb4_y2) * wet;
                sp++;

                currentDelaySize1 += delaySizeInc1;
                currentFeedback += feedbackInc;
            }

        }
        break;
        case FILTER2_CHORUS: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 0.5f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            param1S = 0.02f * this->params_.effect2.param1 + .98f * param1S;
            float param1 = 0.1f + param1S * 0.9f;
            float fxParamTmp = foldAbs( 0.125f * param1 * (0.5f + (param1 + matrixFilterFrequency) * 0.5f ) );
            fxParamTmp = sigmoid(fxParamTmp);
            delayReadFrac = (fxParamTmp + 99 * delayReadFrac) * 0.01f; // smooth change

            float readPos1 = delayReadFrac;
            float readPos2 = foldAbs( readPos1 + 0.3333f * param1);
            float readPos3 = foldAbs( readPos1 + 0.6666f * param1);

            float currentDelaySize1 = delaySize1;
            float currentDelaySize2 = delaySize2;
            float currentDelaySize3 = delaySize3;
            delaySize1 = 1 + delayBufferSizeM4 * readPos1;
            delaySize2 = 1 + delayBufferSizeM4 * readPos2;
            delaySize3 = 1 + delayBufferSizeM4 * readPos3;
            float delaySizeInc1 = (delaySize1 - currentDelaySize1) * INV_BLOCK_SIZE;
            float delaySizeInc2 = (delaySize2 - currentDelaySize2) * INV_BLOCK_SIZE;
            float delaySizeInc3 = (delaySize3 - currentDelaySize3) * INV_BLOCK_SIZE;

            float currentFeedback = feedback;
            feedback = clamp(this->params_.effect2.param2 + matrixFilterParam2, -1, 1) * 0.4f;
            float feedbackInc = (feedback - currentFeedback) * INV_BLOCK_SIZE;

            float *sp = sampleBlock_;
            
            float delReadPos1, delReadPos2, delReadPos3, monoIn;

            const float f = 0.8f;
            const float f2 = 0.7f;

            // hi pass params
            float filterB2    = 0.15f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in_b1 = (1 - filterB);
            float _in_a0 = (1 + _in_b1 * _in_b1 * _in_b1) * 0.5f;
            float _in_a1 = -_in_a0;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                monoIn = (*sp + *(sp + 1)) * 0.5f;

                // input lp
                low3  += f * band3;
                band3 += f * ((monoIn - delaySumOut * currentFeedback) - low3 - band3);

                // audio in hp
                hp_in_x0     = low3;
                hp_in_y0     = _in_a0 * hp_in_x0 + _in_a1 * hp_in_x1 + _in_b1 * hp_in_y1;
                hp_in_y1     = hp_in_y0;
                hp_in_x1     = hp_in_x0;

                delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;
                delayBuffer_[delayWritePos] = hp_in_y0;

                delayWritePosF = (float) delayWritePos;

                delReadPos1 = modulo2(delayWritePosF - currentDelaySize1, delayBufferSize);
                delayOut1  = delayInterpolation(delReadPos1, delayBuffer_, delayBufferSizeM1);

                delReadPos2 = modulo2(delayWritePosF - currentDelaySize2, delayBufferSize);
                delayOut2  = delayInterpolation(delReadPos2, delayBuffer_, delayBufferSizeM1);

                delReadPos3 = modulo2(delayWritePosF - currentDelaySize3, delayBufferSize);
                delayOut3  = delayInterpolation(delReadPos3, delayBuffer_, delayBufferSizeM1);

                delaySumOut = (delayOut1 - delayOut3 + delayOut2) ;
                float delaySumOut2 = (delayOut3 - delayOut1 + delayOut2) ;

                low1  += f2 * band1;
                band1 += f2 * (delaySumOut - low1 - band1);

                low2  += f2 * band2;
                band2 += f2 * (low1 - low2 - band2);

                low5  += f2 * band5;
                band5 += f2 * (delaySumOut2 - low5 - band5);

                low6  += f2 * band6;
                band6 += f2 * (low5 - low6 - band6);

                *sp = *sp * dry + low2 * wetL;
                sp++;
                *sp = *sp * dry + low6 * wetR;
                sp++;

                currentDelaySize1 += delaySizeInc1;
                currentDelaySize2 += delaySizeInc2;
                currentDelaySize3 += delaySizeInc3;
                currentFeedback += feedbackInc;
            }

        }
        break;
        case FILTER2_DIMENSION: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255];
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            param1S = 0.02f * matrixFilterFrequency + .98f * param1S;
            float param1 = this->params_.effect2.param1;

            matrixFilterFrequency *= 0.5f;
            
            float fxParamTmp  = sigmoidPos(foldAbs( (0.125f *  (0.5f + (param1 + param1S) * param1 ) ) ));
            delayReadFrac = (fxParamTmp + 99 * delayReadFrac) * 0.01f; // smooth change
            float fxParamTmp2 = sigmoidPos(foldAbs( (0.125f *  (0.5f + (param1 - param1S) * param1 ) ) ));
            delayReadFrac2 = (fxParamTmp2 + 99 * delayReadFrac2) * 0.01f; // smooth change

            float currentDelaySize1 = clamp(delaySize1, 0, delayBufStereoSizeM1);
            float currentDelaySize2 = clamp(delaySize2, 0, delayBufStereoSizeM1);
            delaySize1 = clamp(1 + delayBufStereoSize * delayReadFrac,  0, delayBufStereoSizeM1);
            delaySize2 = clamp(1 + delayBufStereoSize * delayReadFrac2, 0, delayBufStereoSizeM1);
            float delaySizeInc1 = (delaySize1 - currentDelaySize1) * INV_BLOCK_SIZE;
            float delaySizeInc2 = (delaySize2 - currentDelaySize2) * INV_BLOCK_SIZE;

            float currentFeedback = feedback;
            feedback = clamp( (this->params_.effect2.param2 + matrixFilterParam2) , -0.995f, 0.995f);
            feedback *= feedback;
            float feedbackInc = (feedback - currentFeedback) * INV_BLOCK_SIZE;
            
            const float f = 0.82f;
            const float f2 = 0.75f;

            // hi pass params
            float filterB2    = 0.2f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in2_b1 = (1 - filterB);
            float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;
            float _in2_a1 = -_in2_a0;

            float *sp = sampleBlock_;
            
            float delReadPos, delReadPos2;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                // feedback lp
                float feedL   = low3 * currentFeedback;
                float feedR   = low4 * currentFeedback;

                low1  += f * band1;
                band1 += f * (*sp - low1 - band1);

                low2  += f * band2;
                band2 += f * (*(sp + 1) - low2 - band2);

                // audio in hp
                float hp_in_1 = low1 + feedL;
                hp_in_x0     = hp_in_1;
                hp_in_y0     = _in2_a0 * hp_in_x0 + _in2_a1 * hp_in_x1 + _in2_b1 * hp_in_y1;
                hp_in_y1     = hp_in_y0;
                hp_in_x1     = hp_in_x0;

                float hp_in_2 = low2 + feedR;
                hp_in2_x0    = hp_in_2;
                hp_in2_y0    = _in2_a0 * hp_in2_x0 + _in2_a1 * hp_in2_x1 + _in2_b1 * hp_in2_y1;
                hp_in2_y1    = hp_in2_y0;
                hp_in2_x1    = hp_in2_x0;

                float lpc1  = (hp_in_1 - hp_in_y0);
                float lpc2  = (hp_in_2 - hp_in2_y0);

                delayWritePos = (delayWritePos + 1) & delayBufStereoSizeM1;

                delayBuffer_[delayWritePos] = hp_in_y0;
                delayBuffer_[delayWritePos + delayBufStereoSize] = hp_in2_y0;

                delayWritePosF = (float) delayWritePos;

                delReadPos = modulo2(delayWritePosF - currentDelaySize1, delayBufStereoSize);
                delReadPos2 = modulo2(delayWritePosF - currentDelaySize2, delayBufStereoSize);

                delayOut1 = delayInterpolation(delReadPos, delayBuffer_, delayBufStereoSizeM1);

                delayOut3 = delayInterpolation2(delReadPos2, delayBuffer_, delayBufStereoSizeM1, delayBufStereoSize);

                low3  += f2 * band3;
                band3 += f2 * ((delayOut1) - low3 - band3);

                low4  += f2 * band4;
                band4 += f2 * ((low3) - low4 - band4);

                low5  += f2 * band5;
                band5 += f2 * (delayOut3 - low5 - band5);

                low6  += f2 * band6;
                band6 += f2 * (low5 - low6 - band6);


                *sp = (*sp) * dry + (lpc1 + low6 - low4 * 0.3f) * wet;
                sp++;
                *sp = (*sp) * dry + (lpc2 + low4 - low6 * 0.3f) * wet;
                sp++;

                currentDelaySize1 += delaySizeInc1;
                currentDelaySize2 += delaySizeInc2;
                currentFeedback += feedbackInc;
            }

        }
        break;
        case FILTER2_DOUBLER: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 0.75f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            param1S = 0.02f * this->params_.effect2.param1 + .98f * param1S;
            param2S = 0.05f * (this->params_.effect2.param2 + matrixFilterParam2) + .95f * param2S;

            float currentShift = shift;
            shift = clamp(fabsf(param1S * 2 + matrixFilterFrequency * 0.5f), 0, 16);
            float shiftInc = (shift - currentShift) * INV_BLOCK_SIZE;

            float quadrant = fabsf((clamp(shift, 0, 2) - 1));// 2 quadrant for up & down shift
            float feedbackZeroZone = clamp(0.82f + quadrant * 40, 0, 0.999f);

            float feed = clamp(param2S * feedbackZeroZone, 0, 1);
            float currentFeedback = feedback;
            feedback = clamp( feed, -1, 1) * 0.44f;
            float feedbackInc = (feedback - currentFeedback) * INV_BLOCK_SIZE;

            float lpZeroZone  = clamp(quadrant * 50, 0, 1);

            float filterA2    = 0.5f + lpZeroZone * 0.2f;
            float filterA     = (filterA2 * filterA2 * 0.5f);
            float _in_lp_b = 1 - filterA;
            float _in_lp_a = 1 - _in_lp_b;

            const float f = 0.7f;
            const float f2 = 0.38f;
            const float f3 = 0.72f;

            float *sp = sampleBlock_;
            
            float delayReadPos180, level1, level2;

            // hi pass params
            float hpZeroZone  = 1 - clamp(sqrt3(quadrant) * 4.5f, 0, 1);

            float filterB2    = 0.1f + (0.32f - (param1S * 0.2f) + hpZeroZone * 0.3f) * clamp(param2S * param2S * 2, 0, 1);
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in2_b1 = (1 - filterB);
            float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;
            float _in2_a1 = -_in2_a0;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float monoIn = (*sp + *(sp + 1)) * 0.5f;

                // feedback lp
                hb1_x1  = _in_lp_a * delaySumOut + hb1_x1 * _in_lp_b;
                hb1_x2  = _in_lp_b * hb1_x1 + hb1_x2 * _in_lp_b;

                // delay in hp
                hp_in_x0     = clamp(tanh4((monoIn + hb1_x2 * currentFeedback)), -1, 1);
                hp_in_y0     = _in2_a0 * hp_in_x0 + _in2_a1 * hp_in_x1 + _in2_b1 * hp_in_y1;
                hp_in_y1     = hp_in_y0;
                hp_in_x1     = hp_in_x0;

                delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;

                low1  += f * band1;
                band1 += f * (hp_in_y0 - low1 - band1);
                low2  += f * band2;
                band2 += f * (low1 - low2 - band2);

                delayBuffer_[delayWritePos] = low2;

                delayReadPos    = modulo(delayReadPos + currentShift, delayBufferSize);
                delayReadPos180 = modulo2(delayReadPos - delayBufferSize180, delayBufferSize);

                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                delayOut2 = delayInterpolation(delayReadPos180, delayBuffer_, delayBufferSizeM1);

                delayWritePosF = (float) delayWritePos;
                float rwp1 = modulo2(delayWritePosF - delayReadPos, delayBufferSize);
                float rwp3 = modulo2(delayWritePosF - delayReadPos180, delayBufferSize);

                level1 = hann(rwp1 * delayBufferSizeInv);
                level2 = hann(rwp3 * delayBufferSizeInv);

                float out1 = delayOut1 * (level1);
                float out2 = delayOut2 * (level2);

                delaySumOut  = out1 + out2;
                float delaySumOut2 = out1 - out2;

                low5  += f3 * band5;
                band5 += f3 * (delaySumOut - low5 - band5);

                low6  += f3 * band6;
                band6 += f3 * (delaySumOut2 - low6 - band6);

                // bass boost
                low3  += f2 * band3;
                band3 += f2 * (low5 - low3 - band3);
                low4  += f2 * band4;
                band4 += f2 * (low6 - low4 - band4);

                *sp = (*sp * dry + ((low3 + low5) * wetL));
                sp++;
                *sp = (*sp * dry + ((low4 + low6) * wetR));
                sp++;

                currentFeedback += feedbackInc;
                currentShift += shiftInc;
            }
        }
        break;
        case FILTER2_TRIPLER: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 0.75f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            param1S = 0.005f * this->params_.effect2.param1 + .995f * param1S;
            param2S = 0.005f * this->params_.effect2.param2 + .995f * param2S;

            float currentShift = shift;
            shift = clamp(fabsf(param1S * 2 + matrixFilterFrequency * 0.5f), 0, 16);
            float shiftInc = (shift - currentShift) * INV_BLOCK_SIZE;

            float currentShift2 = shift2;
            shift2 = clamp(fabsf(param2S * 2 + matrixFilterParam2 * 0.5f), 0, 16);
            float shiftInc2 = (shift2 - currentShift2) * INV_BLOCK_SIZE;

            const float f = 0.75f;
            const float f2 = 0.72f;

            float *sp = sampleBlock_;
            
            float level1, level2, level3, level4;
            float delayReadPos180;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float monoIn = (*sp + *(sp + 1)) * 0.5f;

                delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;

                low1  += f * band1;
                band1 += f * (monoIn - low1 - band1);

                delayBuffer_[delayWritePos] = low1;

                //--------------- shifter 1

                delayReadPos = modulo(delayReadPos + currentShift, delayBufferSize);
                delayReadPos180 = modulo(delayReadPos + delayBufferSize180, delayBufferSize);

                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                delayOut2 = delayInterpolation(delayReadPos180, delayBuffer_, delayBufferSizeM1);
                
                float delayWritePosF = (float) delayWritePos;
                float rwp1 = modulo2(delayWritePosF - delayReadPos, delayBufferSize);
                float rwp2 = modulo2(delayWritePosF - delayReadPos180, delayBufferSize);

                level1 = hann(rwp1 * delayBufferSizeInv);
                level2 = hann(rwp2 * delayBufferSizeInv);

                float out1 = delayOut1 * (level1);
                float out2 = delayOut2 * (level2);

                delaySumOut  = out1 + out2;

                //--------------- shifter 2

                delayReadPos2 = modulo(delayReadPos2 + currentShift2, delayBufferSize);
                delayReadPos180 = modulo(delayReadPos2 + delayBufferSize90, delayBufferSize);

                delayOut3 = delayInterpolation(delayReadPos2, delayBuffer_, delayBufferSizeM1);
                delayOut4 = delayInterpolation(delayReadPos180, delayBuffer_, delayBufferSizeM1);

                float rwp3 = modulo2(delayWritePosF - delayReadPos2, delayBufferSize);
                float rwp4 = modulo2(delayWritePosF - delayReadPos180, delayBufferSize);

                level3 = hann(rwp3 * delayBufferSizeInv);
                level4 = hann(rwp4 * delayBufferSizeInv);

                float out3 = delayOut3 * (level3);
                float out4 = delayOut4 * (level4);

                delaySumOut -= out3 + out4;

                // lp output 
                low3  += f2 * band3;
                band3 += f2 * ((delaySumOut) - low3 - band3);

                *sp = *sp * dry + low3 * wetL;
                sp++;
                *sp = *sp * dry + low3 * wetR;
                sp++;

                currentShift += shiftInc;
                currentShift2 += shiftInc2;

            }
        }
        break;
        case FILTER2_BODE: {
            // dry wet
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 0.66f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            float param1 = this->params_.effect2.param1;
    
            // mix between freq shift + and - :
            int param255 = 0;
            if (param1 <= 0.4f) {
                param255 = 0;
            } else if (param1 >= 0.6f) {
                param255 = 255;
            } else {
                param255 = 255 * (5.f * (param1 - 0.4f));
            }
            float shiftMinus = panTable[255 - param255];
            float shiftPlus  = panTable[param255];
            
            // shift val
            param1S = 0.05f * param1 + .95f * param1S;
            float quadrant = fabsf(param1S - 0.5f);// 2 quadrant for up & down shift
            float quadrantSq = sqrt3(quadrant);

            // shift increment :
            shift = clamp(shift, 0, 0.1f);
            float currentShift = shift;
            float shiftval = clamp(fabsf(quadrant * 1.34f + matrixFilterFrequency * 0.5f), 0, 0.9999f);
            shiftval *= shiftval * 0.05f;
            shift = shift * 0.96f + 0.04f * shiftval;
            float shiftInc = (shift - currentShift) * INV_BLOCK_SIZE;

            // feedback
            float feedbackZeroZone = clamp(0.8f + (quadrantSq * 8), 0, 1);

            float feedbackParam = clamp(this->params_.effect2.param2 + matrixFilterParam2, 0, 1) * 0.8f;

            feedback = clamp(feedback, 0, 1);

            float currentFeedback = feedback;
            feedback = feedbackParam * feedbackZeroZone;
            float feedbackInc = (feedback - currentFeedback) * INV_BLOCK_SIZE;

            float currentDelaySize1 = clamp(delaySize1, 0, delayBufferSize);
            delaySize1 = clamp(430 + 70 * quadrant,  0, delayBufferSize);
            float delaySizeInc1 = (delaySize1 - currentDelaySize1) * INV_BLOCK_SIZE;

            float *sp = sampleBlock_;
            
            float iirFilter1, iirFilter2, iirFilter3, iirFilter4, iirFilter5, iirFilter6, iirFilter7, iirFilter8;
            float cos, sin;
            float phase2;
            float shifterIn;
            float shifterOutR = 0, shifterOutI = 0, shifterOut = 0, shifterOut2 = 0;

            const float f = 0.73f;
            const float f2 = 0.75f;
            const float f3 = 0.25f;

            // hi pass params
            float filterB2    = 0.2f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in2_b1 = (1 - filterB);
            float _in2_a0 = (1 + _in2_b1 * _in2_b1 * _in2_b1) * 0.5f;
            float _in2_a1 = -_in2_a0;

            float hpZeroZone  = clamp( (1.3f - param1S) * sqrt3(param1 * quadrantSq * 16), 0, 1);
            filterB2    = 0.32f - 0.3f * hpZeroZone * (1 - (feedbackZeroZone * fabsf(feedbackParam) * 0.125f));
            filterB     = (filterB2 * filterB2 * 0.5f);

            float _in3_b1 = (1 - filterB);
            float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            float _in3_a1 = -_in3_a0;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float monoIn = (*sp + *(sp + 1)) * 0.5f;

                // monoIn lp
                low1  += f * band1;
                band1 += f * (monoIn - low1 - band1);

                // feedback hp
                float feedbackIn = tanh4(shifterOutMix * 1.35f) *  currentFeedback;

                _ly1 = apcoef3 * (_ly1 + feedbackIn) - _lx1; // allpass
                _lx1 = feedbackIn;

                _ly2 = apcoef4 * (_ly2 + _ly1) - _lx2; // allpass
                _lx2 = _ly1;

                hp_in_x0     = (feedbackIn + _ly2) * 0.5f;
                hp_in_y0     = _in3_a0 * hp_in_x0 + _in3_a1 * hp_in_x1 + _in3_b1 * hp_in_y1;
                hp_in_y1     = hp_in_y0;
                hp_in_x1     = hp_in_x0;
               
                // feedback lp
                low2  += f2 * band2;
                band2 += f2 * (hp_in_y0 - low2 - band2);
                low3  += f2 * band3;
                band3 += f2 * ( low2 - low3 - band3);

                delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;
                delayBuffer_[delayWritePos] = low3;

                delayReadPos = modulo2(delayWritePos - currentDelaySize1, delayBufferSize);
                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);

                shifterIn = clamp(low1 - delayOut1, -1.f, 1.f);

                hp_in2_x0    = shifterIn;
                hp_in2_y0    = _in2_a0 * hp_in2_x0 + _in2_a1 * hp_in2_x1 + _in2_b1 * hp_in2_y1;
                hp_in2_y1    = hp_in2_y0;
                hp_in2_x1    = hp_in2_x0;

                low4  += f * band4;
                band4 += f * (hp_in2_y0 - low4 - band4);

                // Frequency shifter 

                //     Phase reference path
                iirFilter1 = iirFilter(samplen1,    0.48645677879491144857f, &hb1_x1,  &hb1_x2,  &hb1_y1, &hb1_y2);
                iirFilter2 = iirFilter(iirFilter1,  0.88068726735639790704f, &hb2_x1,  &hb2_x2,  &hb2_y1, &hb2_y2);
                iirFilter3 = iirFilter(iirFilter2,  0.97790456293916316888f, &hb3_x1,  &hb3_x2,  &hb3_y1, &hb3_y2);
                iirFilter4 = iirFilter(iirFilter3,  0.99767037906310385154f, &hb4_x1,  &hb4_x2,  &hb4_y1, &hb4_y2);

                //     +90 deg path
                iirFilter5 = iirFilter(low4,         0.16507919004304125177f, &hb5_x1,  &hb5_x2,  &hb5_y1, &hb5_y2);
                iirFilter6 = iirFilter(iirFilter5,   0.73969068299070206418f, &hb6_x1,  &hb6_x2,  &hb6_y1, &hb6_y2);
                iirFilter7 = iirFilter(iirFilter6,   0.94788883423814862539f, &hb7_x1,  &hb7_x2,  &hb7_y1, &hb7_y2);
                iirFilter8 = iirFilter(iirFilter7,   0.99119752093109647628f, &hb8_x1,  &hb8_x2,  &hb8_y1, &hb8_y2);

                samplen1 = low4;

                //     sin
                phase1 = phase1 + currentShift;
                if(phase1 >= 1) {
                    phase1 -= 2.f;
                }
                //     cos = sin( x + 90°)
                phase2 = phase1 + 0.5f;
                if (phase2 >= 1)  {
                    phase2 -= 2.f;
                }

                sin = fastSin(phase1);
                cos = fastSin(phase2);

                shifterOutR = sin * iirFilter4;
                shifterOutI = cos * iirFilter8;
                shifterOut  = shifterOutI + shifterOutR;
                shifterOut2 = shifterOutI - shifterOutR;
                float shifterOutMixA = shifterOut2 * shiftPlus;
                float shifterOutMixB = shifterOut * shiftMinus;
                shifterOutMix = shifterOutMixA + shifterOutMixB;
                float shifterOutMix2 = shifterOutMixA - shifterOutMixB;

                // bass boost
                low5  += f3 * band5;
                band5 += f3 * (shifterOutMix - low5 - band5);
                
                *sp = (*sp * dry) - (shifterOutMix + low5) * wetL;
                sp++;
                *sp = (*sp * dry) - (shifterOutMix2 + low5) * wetR;
                sp++;

                currentFeedback += feedbackInc;
                currentShift += shiftInc;
                currentDelaySize1 += delaySizeInc1;
            }
        }
        break;
        case FILTER2_WIDE: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 0.3333f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;
            
            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            param1S = 0.02f * fabsf(this->params_.effect2.param1 + matrixFilterFrequency) + .98f * param1S;
            param2S = 0.02f * clamp(this->params_.effect2.param2 + matrixFilterParam2, 0.f, 1.f) + .98f * param2S;

            float detune = param1S * param1S * 0.03125f;

            float currentShift = shift;
            shift =  clamp(1 + detune, 0, 16);
            float shiftInc = (shift - currentShift) * INV_BLOCK_SIZE;

            float currentShift2 = shift2;
            shift2 = clamp(1 - detune, 0, 16);
            float shiftInc2 = (shift2 - currentShift2) * INV_BLOCK_SIZE;

            const int delaySizeInt = 500;

            const float f = 0.72f;
            const float f2 = 0.7f + f * 0.1f;
            const float f3 = 0.32f;

            // hi pass params
            float filterB2    = clamp(param2S * param2S * 0.93f, 0, 1);
            float filterB     = (filterB2 * filterB2 * 0.5f);

            const float _hp_b1 = (1 - filterB);
            const float _hp_a0 = (1 + _hp_b1 * _hp_b1 * _hp_b1) * 0.5f;
            const float _hp_a1 = -_hp_a0;

            float *sp = sampleBlock_;
            
            float level1, level2, level3, level4;
            float delayReadPos180;

            for (int k = 0; k < BLOCK_SIZE; k++) {
                float monoIn = (*sp + *(sp + 1)) * 0.5f;
                
                low1  += f * band1;
                band1 += f * (monoIn - low1 - band1);

                delayWritePos = (delayWritePos + 1) & delayBufStereoSizeM1;
                delayWritePosF = (float) delayWritePos;

                // predelay
                delayBuffer_[delayBufStereoSize + delayWritePos] = low1;
                int wp = (delayWritePos - delaySizeInt) & delayBufStereoSizeM1;

                // hp
                hp_in_x0     = delayBuffer_[delayBufStereoSize + wp];
                hp_in_y0     = _hp_a0 * hp_in_x0 + _hp_a1 * hp_in_x1 + _hp_b1 * hp_in_y1;
                hp_in_y1     = hp_in_y0;
                hp_in_x1     = hp_in_x0;

                delayBuffer_[delayWritePos] = hp_in_y0;

                float hpComplement = hp_in_x0 - hp_in_y0;

                //--------------- shifter 1
                
                delayReadPos = modulo(delayReadPos + currentShift, delayBufStereoSize);
                delayReadPos180 = modulo(delayReadPos + delayBufferSize90, delayBufStereoSize);

                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufStereoSizeM1);
                delayOut2 = delayInterpolation(delayReadPos180, delayBuffer_, delayBufStereoSizeM1);

                float rwp1 = modulo2(delayWritePosF - delayReadPos, delayBufStereoSize);
                float rwp2 = modulo2(delayWritePosF - delayReadPos180, delayBufStereoSize);

                level1 = hann(rwp1 * delayBufStereoSizeInv);
                level2 = hann(rwp2 * delayBufStereoSizeInv);

                float out1 = delayOut1 * (level1);
                float out2 = delayOut2 * (level2);

                //--------------- shifter 2

                delayReadPos2 = modulo(delayReadPos2 + currentShift2, delayBufStereoSize);
                delayReadPos180 = modulo(delayReadPos2 + delayBufferSize90, delayBufStereoSize);

                delayOut3 = delayInterpolation(delayReadPos2, delayBuffer_, delayBufStereoSizeM1);
                delayOut4 = delayInterpolation(delayReadPos180, delayBuffer_, delayBufStereoSizeM1);

                float rwp3 = modulo2(delayWritePosF - delayReadPos2, delayBufStereoSize);
                float rwp4 = modulo2(delayWritePosF - delayReadPos180, delayBufStereoSize);

                level3 = hann(rwp3 * delayBufStereoSizeInv);
                level4 = hann(rwp4 * delayBufStereoSizeInv);

                float out3 = delayOut3 * (level3);
                float out4 = delayOut4 * (level4);
   
                // lp output 
                low3  += f2 * band3;
                band3 += f2 * ((hpComplement + out1 + out2) - low3 - band3);

                low4  += f2 * band4;
                band4 += f2 * ((hpComplement + out3 + out4) - low4 - band4);

                // bass boost
                low5  += f3 * band5;
                band5 += f3 * (low3 - low5 - band5);
                low6  += f3 * band6;
                band6 += f3 * (low4 - low6 - band6);

                *sp = *sp * dry + (low3 + low3 + low5) * wetL;
                sp++;
                *sp = *sp * dry + (low4 + low4 + low6) * wetR;
                sp++;

                currentShift += shiftInc;
                currentShift2 += shiftInc2;
            }
        }
        break;
        case FILTER2_DELAYCRUNCH: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 1.2f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            param1S = 0.005f * (this->params_.effect2.param1) + .995f * param1S;
            matrixFilterFrequencyS = 0.02f * (matrixFilterFrequency) + .98f * matrixFilterFrequencyS;
            param2S = 0.05f * (this->params_.effect2.param2 + matrixFilterParam2) + .95f * param2S;

            param2S = clamp(param2S, 0, 1.f);

            feedback = param2S * 1.25f;

            const float sampleRateDivide = 4;
            const float sampleRateDivideInv = 1 / sampleRateDivide;
            float inputIncCount = 0;

            float currentDelaySize1 = clamp(delaySize1, 0, delayBufferSize);
            delaySize1 = 1.f + (delayBufferSize - 16) * clamp(param1S + (matrixFilterFrequencyS * 0.0625f), 0.f, 1.f);
            float delaySizeInc1 = (delaySize1 - currentDelaySize1) * sampleRateDivideInv * INV_BLOCK_SIZE;

            float filterB2     = 0.15f + param2S * 0.33f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in3_b1 = (1 - filterB);
            float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            float _in3_a1 = -_in3_a0;

            const float f = 0.72f;
            const float f2 = 0.75f;

            const float fnotch = 1.03f;

            float *sp = sampleBlock_;
 
            for (int k = 0; k < BLOCK_SIZE; k++) {

                if(++inputIncCount >= sampleRateDivide) {
                    float monoIn = (*sp + *(sp + 1)) * 0.5f;

                    inputIncCount = 0;
                    delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;
                    delayWritePosF = (float) delayWritePos;

                    low1  += f * band1;
                    band1 += f * ((delayOut1 * feedback) - low1 - band1);
                    
                    low2  += f * band2;
                    band2 += f * (low1 - low2 - band2);

                    // hp
                    hp_in_x0     = low2;
                    hp_in_y0     = _in3_a0 * hp_in_x0 + _in3_a1 * hp_in_x1 + _in3_b1 * hp_in_y1;
                    hp_in_y1     = hp_in_y0;
                    hp_in_x1     = hp_in_x0;

                    hp_in2_x0    = hp_in_y0;
                    hp_in2_y0    = _in3_a0 * hp_in2_x0 + _in3_a1 * hp_in2_x1 + _in3_b1 * hp_in2_y1;
                    hp_in2_y1    = hp_in2_y0;
                    hp_in2_x1    = hp_in2_x0;

                    hb5_x1    = hp_in2_y0 + monoIn;
                    hb5_y1    = _in3_a0 * hb5_x1 + _in3_a1 * hb5_x2 + _in3_b1 * hb5_y2;
                    hb5_y2    = hb5_y1;
                    hb5_x2    = hb5_x1;

                    delayBuffer_[delayWritePos] = hb5_y1;
                }

                delayReadPos = modulo2(delayWritePosF - currentDelaySize1, delayBufferSize);
                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                delayWritePosF += sampleRateDivideInv;

                // lp 
                low3 += f2 * band3;
                band3 += f2 * (delayOut1 - low3 - band3);
                low4 += f2 * band4;
                band4 += f2 * (low3 - low4 - band4);
                low5 += f2 * band5;
                band5 += f2 * (low4 - low5 - band5);
                low6 += f2 * band6;
                band6 += f2 * (low5 - low6 - band6);

                // notch 
                hb1_x1 += fnotch * hb1_y1;
                float high7 = low6 - hb1_x1 - hb1_y1;
                hb1_y1 += fnotch * high7;
                float notch = (high7 + hb1_x1);

                *sp = *sp * dry + notch * wetL;
                sp++;
                *sp = *sp * dry + notch * wetR;
                sp++;

                currentDelaySize1 += delaySizeInc1;
            }
        }
        break;
        case FILTER2_PINGPONG: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 1.25f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            param1S = 0.005f * (this->params_.effect2.param1) + .995f * param1S;
            matrixFilterFrequencyS = 0.02f * (matrixFilterFrequency) + .98f * matrixFilterFrequencyS;
            param2S = 0.05f * (this->params_.effect2.param2 + matrixFilterParam2) + .95f * param2S;
            
            param2S = clamp(param2S, 0, 1.f);

            feedback = param2S* 1.25f;

            const float sampleRateDivide = 4;
            const float sampleRateDivideInv = 1 / sampleRateDivide;
            float inputIncCount = 0;

            float currentDelaySize1 = clamp(delaySize1, 0, delayBufferSizeF);
            delaySize1 = 1.f + (delayBufferSize - 16) * clamp(param1S + (matrixFilterFrequencyS * 0.0625f), 0.f, 1.f);
            float delaySizeInc1 = (delaySize1 - currentDelaySize1) * sampleRateDivideInv * INV_BLOCK_SIZE;

            float currentDelaySize2 = clamp(delaySize2, 0, delayBufferSizeF);
            delaySize2 = delaySize1 * 0.5f;
            float delaySizeInc2 = (delaySize2 - currentDelaySize2) * sampleRateDivideInv * INV_BLOCK_SIZE;

            float filterB2     = 0.15f + param2S * 0.33f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in3_b1 = (1 - filterB);
            float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            float _in3_a1 = -_in3_a0;

            float f = 0.72f;
            float f2 = 0.72;
            float f3 = 0.75;
            const float fnotch = 1.03f;

            float *sp = sampleBlock_;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                if(++inputIncCount >= sampleRateDivide) {
                    float monoIn = (*sp + *(sp + 1)) * 0.5f;

                    inputIncCount = 0;
                    delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;
                    delayWritePosF = (float) delayWritePos;

                    low1  += f * band1;
                    band1 += f * ((delayOut1 * feedback) - low1 - band1);
                    
                    // hp
                    hp_in_x0     = low1;
                    hp_in_y0     = _in3_a0 * hp_in_x0 + _in3_a1 * hp_in_x1 + _in3_b1 * hp_in_y1;
                    hp_in_y1     = hp_in_y0;
                    hp_in_x1     = hp_in_x0;

                    hp_in2_x0    = hp_in_y0;
                    hp_in2_y0    = _in3_a0 * hp_in2_x0 + _in3_a1 * hp_in2_x1 + _in3_b1 * hp_in2_y1;
                    hp_in2_y1    = hp_in2_y0;
                    hp_in2_x1    = hp_in2_x0;

                    hb5_x1    = hp_in2_y0 + monoIn;
                    hb5_y1    = _in3_a0 * hb5_x1 + _in3_a1 * hb5_x2 + _in3_b1 * hb5_y2;
                    hb5_y2    = hb5_y1;
                    hb5_x2    = hb5_x1;

                    low2  += f * band2;
                    band2 += f * (hb5_y1 - low2 - band2);

                    delayBuffer_[delayWritePos] = low2;
                }

                delayReadPos = modulo2(delayWritePosF - currentDelaySize1, delayBufferSize);
                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                delayReadPos = modulo2(delayWritePosF - currentDelaySize2, delayBufferSize);
                delayOut2 = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);

                delayWritePosF += sampleRateDivideInv;

                // lp L
                low3  += f2 * band3;
                band3 += f2 * (delayOut1 - low3 - band3);
                low4  += f2 * band4;
                band4 += f2 * (low3 - low4 - band4);
                low5  += f2 * band5;
                band5 += f2 * (low4 - low5 - band5);
                low6  += f2 * band6;
                band6 += f2 * (low5 - low6 - band6);

                // lp R
                hb2_x1  += f3 * hb2_y1;
                hb2_y1 += f3 * (delayOut2 - hb2_x1 - hb2_y1);
                hb2_x2 += f3 * hb2_y2;
                hb2_y2+= f3 * (hb2_x1 - hb2_x2 - hb2_y2);
                hb3_x1 += f3 * hb3_y1;
                hb3_y1+= f3 * (hb2_x2 - hb3_x1 - hb3_y1);
                hb3_x2 += f3 * hb3_y2;
                hb3_y2+= f3 * (hb3_x1 - hb3_x2 - hb3_y2);

                // notch L
                hb1_x1 += fnotch * hb1_y1;
                float high7 = low6 - hb1_x1 - hb1_y1;
                hb1_y1 += fnotch * high7;
                float notchL = (high7 + hb1_x1);

                // notch R
                hb1_x2 += fnotch * hb1_y2;
                float high8 = hb3_x2 - hb1_x2 - hb1_y2;
                hb1_y2 += fnotch * high8;
                float notchR = (high8 + hb1_x2);

                *sp = *sp * dry + notchL * wetL;
                sp++;
                *sp = *sp * dry + notchR * wetR;
                sp++;

                currentDelaySize1 += delaySizeInc1;
                currentDelaySize2 += delaySizeInc2;
            }
        }
        break;
        case FILTER2_DIFFUSER: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 1.5f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            param1S = 0.005f * (this->params_.effect2.param1) + .995f * param1S;
            matrixFilterFrequencyS = 0.01f * (matrixFilterFrequency) + .99f * matrixFilterFrequencyS;
            param2S = 0.05f * (this->params_.effect2.param2 + matrixFilterParam2) + .95f * param2S;

            param1S = clamp(param1S, 0, 1);
            param2S = clamp(param2S, 0, 1);

            feedback = param2S * 0.4999f;

            const float sampleRateDivide = 4;
            const float sampleRateDivideInv = 1 / sampleRateDivide;
            float inputIncCount = 0;

            float currentDelaySize1 = clamp(delaySize1, 0, delayBufStereoSize);
            delaySize1 = 1.f + (delayBufStereoSize - 120) * clamp(param1S + (matrixFilterFrequencyS * 0.125f), 0.f, 1.f) * 0.5f;
            float delaySizeInc1 = (delaySize1 - currentDelaySize1) * sampleRateDivideInv * INV_BLOCK_SIZE;

            float filterB2    = 0.15f + param2S * 0.1f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in3_b1 = (1 - filterB);
            float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            float _in3_a1 = -_in3_a0;

            float f = 0.7f - clamp(feedback - (param1S * 0.3f), 0, 1) * 0.6f;
            float f2 = 0.75f - param1S * 0.1f;
            const float fnotch = 1.03f;

            const float inputCoef1 = 0.7f;
            const float inputCoef2 = 0.625f;

            float diff1Out = 0, diff2Out = 0, diff3Out = 0, diff4Out = 0, diff5Out = 0;

            float sizeParamInpt = 0.2f + param1S * 0.2f;
            float inputBuffer1ReadLen = inputBufferLen1 * sizeParamInpt;
            float inputBuffer2ReadLen = inputBufferLen2 * sizeParamInpt;
            float inputBuffer3ReadLen = inputBufferLen3 * sizeParamInpt;
            float inputBuffer4ReadLen = inputBufferLen4 * sizeParamInpt;
            float inputBuffer5ReadLen = inputBufferLen5 * sizeParamInpt;

            float *sp = sampleBlock_;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                if(++inputIncCount >= sampleRateDivide) {
                    float monoIn = (*sp + *(sp + 1)) * 0.5f;
    
                    inputIncCount = 0;
                    delayWritePos = (delayWritePos + 1) & delayBufStereoSizeM1;
                    delayWritePosF = (float) delayWritePos;
                    inputWritePos1        = modulo(inputWritePos1 + 1 , inputBufferLen1);
                    inputWritePos2        = modulo(inputWritePos2 + 1 , inputBufferLen2);
                    inputWritePos3        = modulo(inputWritePos3 + 1 , inputBufferLen3);
                    inputWritePos4        = modulo(inputWritePos4 + 1 , inputBufferLen4);
                    inputWritePos5        = modulo(inputWritePos5 + 1 , inputBufferLen5);

                    // ---- feedback lp
                    low1  += f * band1;
                    band1 += f * ((delayOut1 * feedback) - low1 - band1);
                    
                    // ---- diffuser 1
                    int inputReadPos1     = delayBufStereoSize + modulo2(inputWritePos1 - inputBuffer1ReadLen, inputBufferLen1);
                    float in_apSum1 = (monoIn - low1) + delayBuffer_[inputReadPos1] * inputCoef1;
                    diff1Out        = delayBuffer_[inputReadPos1] - in_apSum1 * inputCoef1;
                    delayBuffer_[delayBufStereoSize + inputWritePos1] = in_apSum1;

                    // ---- diffuser 2
                    int bufferStart = delayBufStereoSize + inputBufferLen1;
                    int inputReadPos2     = bufferStart + modulo2(inputWritePos2 - inputBuffer2ReadLen, inputBufferLen2);
                    float in_apSum2 = diff1Out + delayBuffer_[inputReadPos2] * inputCoef2;
                    diff2Out        = delayBuffer_[inputReadPos2] - in_apSum2 * inputCoef2;
                    delayBuffer_[bufferStart + inputWritePos2] = in_apSum2;

                    // ---- diffuser 3
                    bufferStart += inputBufferLen2;
                    int inputReadPos3     = bufferStart + modulo2(inputWritePos3 - inputBuffer3ReadLen, inputBufferLen3);
                    float in_apSum3 = -diff2Out + delayBuffer_[inputReadPos3] * inputCoef2;
                    diff3Out        = delayBuffer_[inputReadPos3] - in_apSum3 * inputCoef2;
                    delayBuffer_[bufferStart + inputWritePos3] = in_apSum3;

                    // ---- diffuser 4
                    bufferStart += inputBufferLen3;
                    int inputReadPos4     = bufferStart + modulo2(inputWritePos4 - inputBuffer4ReadLen, inputBufferLen4);
                    float in_apSum4 = diff3Out + delayBuffer_[inputReadPos4] * inputCoef2;
                    diff4Out        = delayBuffer_[inputReadPos4] - in_apSum4 * inputCoef2;
                    delayBuffer_[bufferStart + inputWritePos4] = in_apSum4;

                    // ---- diffuser 5
                    bufferStart += inputBufferLen4;
                    int inputReadPos5     = bufferStart + modulo2(inputWritePos5 - inputBuffer5ReadLen, inputBufferLen5);
                    float in_apSum5 = -diff4Out + delayBuffer_[inputReadPos5] * inputCoef2;
                    diff5Out        = delayBuffer_[inputReadPos5] - in_apSum5 * inputCoef2;
                    delayBuffer_[bufferStart + inputWritePos5] = in_apSum5;

                    low1  += f2 * band1;
                    band1 += f2 * (diff5Out - low1 - band1);

                    // hp
                    hp_in_x0     = diff5Out;
                    hp_in_y0     = _in3_a0 * hp_in_x0 + _in3_a1 * hp_in_x1 + _in3_b1 * hp_in_y1;
                    hp_in_y1     = hp_in_y0;
                    hp_in_x1     = hp_in_x0;

                    hp_in2_x0    = hp_in_y0;
                    hp_in2_y0    = _in3_a0 * hp_in2_x0 + _in3_a1 * hp_in2_x1 + _in3_b1 * hp_in2_y1;
                    hp_in2_y1    = hp_in2_y0;
                    hp_in2_x1    = hp_in2_x0;

                    hb5_x1    = hp_in2_y0;
                    hb5_y1    = _in3_a0 * hb5_x1 + _in3_a1 * hb5_x2 + _in3_b1 * hb5_y2;
                    hb5_y2    = hb5_y1;
                    hb5_x2    = hb5_x1;

                    low2  += f * band2;
                    band2 += f * (hb5_y1 - low2 - band2);

                    delayBuffer_[delayWritePos] = low1 + low2;
                }

                delayReadPos = modulo2(delayWritePosF - currentDelaySize1, delayBufStereoSize);
                delayOut1 = delayInterpolation(delayReadPos, delayBuffer_, delayBufStereoSizeM1);

                delayReadPos = modulo2(delayReadPos - 512, delayBufStereoSize);
                delayOut2 = delayInterpolation(delayReadPos, delayBuffer_, delayBufStereoSizeM1);
               
                delayWritePosF += sampleRateDivideInv;

                // lp L
                low3  += f2 * band3;
                band3 += f2 * (delayOut1 - low3 - band3);
                low4  += f2 * band4;
                band4 += f2 * (low3 - low4 - band4);
                low5  += f2 * band5;
                band5 += f2 * (low4 - low5 - band5);
                low6  += f2 * band6;
                band6 += f2 * (low5 - low6 - band6);

                // lp R
                hb2_x1  += f2 * hb2_y1;
                hb2_y1 += f2 * (delayOut2 - hb2_x1 - hb2_y1);
                hb2_x2 += f2 * hb2_y2;
                hb2_y2+= f2 * (hb2_x1 - hb2_x2 - hb2_y2);
                hb3_x1 += f2 * hb3_y1;
                hb3_y1+= f2 * (hb2_x2 - hb3_x1 - hb3_y1);
                hb3_x2 += f2 * hb3_y2;
                hb3_y2+= f2 * (hb3_x1 - hb3_x2 - hb3_y2);

                // notch L
                hb1_x1 += fnotch * hb1_y1;
                float high7 = low6 - hb1_x1 - hb1_y1;
                hb1_y1 += fnotch * high7;
                float notchL = (high7 + hb1_x1);

                // notch R
                hb1_x2 += fnotch * hb1_y2;
                float high8 = hb3_x2 - hb1_x2 - hb1_y2;
                hb1_y2 += fnotch * high8;
                float notchR = (high8 + hb1_x2);

                *sp = *sp * dry + notchL * wetL;
                sp++;
                *sp = *sp * dry + notchR * wetR;
                sp++;

                currentDelaySize1 += delaySizeInc1;
            }
        }
        break;
        case FILTER2_GRAIN1: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 1.75f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            lockA = lockA * 0.98f + ((param2S > 0.99f) ? 0 : 1) * 0.02f;
            lockB = (1 - lockA);

            param1S = 0.05f * (this->params_.effect2.param1 + matrixFilterFrequency) + .95f * param1S;
            param2S = 0.05f * clamp( fabsf(this->params_.effect2.param2 + matrixFilterParam2), 0, 1) + 0.95f * param2S;

            const float sampleRateDivide = 4;
            const float sampleRateDivideInv = 1 / sampleRateDivide;
            float inputIncCount = 0;

            bool grainProb = clamp(param1S * 3, 0.05f, 0.95f) >= fabs(noise[0]);


            float filterB2    = 0.15f + param2S * 0.1f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in3_b1 = (1 - filterB);
            float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            float _in3_a1 = -_in3_a0;

            if(grainProb) {
                if(grainTable[grainNext][GRAIN_RAMP] >= 1 && grainTable[grainPrev][GRAIN_RAMP] > 0.33f) {
                    //grain done, compute another one
                    float jitter = param2S * lockA;
                    float grainRate = sampleRateDivideInv * (1 + jitter * noise[4] * 0.0025f);
                    grainTable[grainNext][GRAIN_RAMP] = 0;
                    grainTable[grainNext][GRAIN_SIZE] = clamp((1800 + (noise[2]) * 40 * jitter * jitter) * param1S * param1S, 432, delayBufferSize - 100);
                    grainTable[grainNext][GRAIN_POS] = modulo2(delayWritePosF - (400 + jitter * noise[5] * 290), delayBufferSize);
                    float invGrainSize = 1 / grainTable[grainNext][GRAIN_SIZE];
                    grainTable[grainNext][GRAIN_CURRENT_SHIFT] = grainTable[grainNext][GRAIN_NEXT_SHIFT];
                    grainTable[grainNext][GRAIN_NEXT_SHIFT] = grainRate * invGrainSize;
                    grainTable[grainNext][GRAIN_INC] = clamp( (grainTable[grainNext][GRAIN_NEXT_SHIFT] - grainTable[grainNext][GRAIN_CURRENT_SHIFT]) * invGrainSize, 0, 0.5f);
                    grainTable[grainNext][GRAIN_VOL] = sqrt3(fabsf(noise[3]));
                    grainTable[grainNext][GRAIN_PAN] = clamp(1 + (noise[6]) * sqrt3(param2S), 0, 2) * 0.5f;
                    grainPrev = grainNext;

                    float filterB2     = 0.35f - sqrt3(fabsf(param1S)) * 0.2f;
                    float filterB     = (filterB2 * filterB2 * 0.5f);
                    _in3_b1 = (1 - filterB);
                    _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
                    _in3_a1 = -_in3_a0;

                    if(lockB < 0.02f) {
                        loopSize = clamp((1 - param1S) * 1400, 48, 1800);
                    }
                }
                if(++grainNext > 2) {
                    grainNext = 0;
                }
            }

            float env;

            const float f = 0.85f;
            const float f2 = 0.9f;
            const float fnotch = 1.03f;

            float grain1, grain2, grain3;
            float grain1L, grain1R;
            float grain2L, grain2R;
            float grain3L, grain3R;
            float grainSumR, grainSumL;

            float *sp = sampleBlock_;
            float monoIn;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                if (++inputIncCount >= sampleRateDivide)
                {
                    inputIncCount = 0;
                    delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;
                    delayWritePosF = (float)delayWritePos;

                    delayReadPos = modulo(delayWritePos + loopSize, delayBufferSize);
                    float feedbackIn = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);

                    monoIn = (*sp + *(sp + 1)) * 0.5f;
                    // hp
                    hp_in_x0     = monoIn;
                    hp_in_y0     = _in3_a0 * hp_in_x0 + _in3_a1 * hp_in_x1 + _in3_b1 * hp_in_y1;
                    hp_in_y1     = hp_in_y0;
                    hp_in_x1     = hp_in_x0;

                    delayBuffer_[delayWritePos] = hp_in_y0 * lockA + feedbackIn * lockB;
                }

                ///-------- grain 1
                grain1L = grain1R = 0;
                if (grainTable[0][GRAIN_RAMP] < 1)
                {
                    delayReadPos = modulo(grainTable[0][GRAIN_POS] + grainTable[0][GRAIN_RAMP] * grainTable[0][GRAIN_SIZE], delayBufferSize);
                    env = hann(grainTable[0][GRAIN_RAMP]) * grainTable[0][GRAIN_VOL];
                    grain1 = env * delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                    grain1L = grain1 * grainTable[0][GRAIN_PAN];
                    grain1R = grain1 - grain1L;
                }
                grainTable[0][GRAIN_RAMP] += grainTable[0][GRAIN_CURRENT_SHIFT];
                grainTable[0][GRAIN_CURRENT_SHIFT] += grainTable[0][GRAIN_INC];

                ///-------- grain 2
                grain2L = grain2R = 0;
                if (grainTable[1][GRAIN_RAMP] < 1)
                {
                    delayReadPos = modulo(grainTable[1][GRAIN_POS] + grainTable[1][GRAIN_RAMP] * grainTable[1][GRAIN_SIZE], delayBufferSize);
                    env = hann(grainTable[1][GRAIN_RAMP]) * grainTable[1][GRAIN_VOL];
                    grain2 = env * delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                    grain2L = grain2 * grainTable[1][GRAIN_PAN];
                    grain2R = grain2 - grain2L;
                }
                grainTable[1][GRAIN_RAMP] += grainTable[1][GRAIN_CURRENT_SHIFT];
                grainTable[1][GRAIN_CURRENT_SHIFT] += grainTable[1][GRAIN_INC];

                ///-------- grain 3
                grain3L = grain3R = 0;
                if (grainTable[2][GRAIN_RAMP] < 1)
                {
                    delayReadPos = modulo(grainTable[2][GRAIN_POS] + grainTable[2][GRAIN_RAMP] * grainTable[2][GRAIN_SIZE], delayBufferSize);
                    env = hann(grainTable[2][GRAIN_RAMP]) * grainTable[2][GRAIN_VOL];
                    grain3 = env * delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                    grain3L = grain3 * grainTable[2][GRAIN_PAN];
                    grain3R = grain3 - grain3L;
                }
                grainTable[2][GRAIN_RAMP] += grainTable[2][GRAIN_CURRENT_SHIFT];
                grainTable[2][GRAIN_CURRENT_SHIFT] += grainTable[2][GRAIN_INC];

                grainSumL = grain1L + grain2L + grain3L;
                grainSumR = grain1R + grain2R + grain3R;

                // lp L
                low3  += f2 * band3;
                band3 += f2 * ((grainSumL) - low3 - band3);
                low5  += f2 * band5;
                band5 += f2 * ((low3) - low5 - band5);

                // lp R
                low4  += f2 * band4;
                band4 += f2 * ((grainSumR) - low4 - band4);
                low6  += f2 * band6;
                band6 += f2 * ((low4) - low6 - band6);

                // notch L
                hb1_x1 += fnotch * hb1_y1;
                float high7 = low5 - hb1_x1 - hb1_y1;
                hb1_y1 += fnotch * high7;
                float notchL = (high7 + hb1_x1);
      
                // notch R
                hb1_x2 += fnotch * hb1_y2;
                float high8 = low6 - hb1_x2 - hb1_y2;
                hb1_y2 += fnotch * high8;
                float notchR = (high8 + hb1_x2);

                *sp = *sp * dry + notchL * wetL;
                sp++;
                *sp = *sp * dry + notchR * wetR;
                sp++;
            }
        }

        break;
        case FILTER2_GRAIN2: {
            mixerGain_ = 0.02f * gainTmp + .98f * mixerGain_;
            float mixerGain_01 = clamp(mixerGain_, 0, 1);
            int mixerGain255 = mixerGain_01 * 255;
            float dry = panTable[255 - mixerGain255];
            float wet = panTable[mixerGain255] * 1.75f;
            float extraAmp = clamp(mixerGain_ - 1, 0, 1);
            wet += extraAmp;

            float wetL = wet * (1 + matrixFilterPan);
            float wetR = wet * (1 - matrixFilterPan);

            lockA = lockA * 0.98f + ((param2S > 0.99f) ? 0 : 1) * 0.02f;
            lockB = (1 - lockA);

            if(lockB > 0) {
                param1S = 0.005f * (this->params_.effect2.param1 + matrixFilterFrequency) + .995f * param1S;
            } else {
                param1S = 0.00005f * (this->params_.effect2.param1 + matrixFilterFrequency) + .99995f * param1S;
            }
            param2S = 0.05f * clamp( fabsf(this->params_.effect2.param2 + matrixFilterParam2), 0, 1) + 0.95f * param2S;

            const float sampleRateDivide = 4;
            const float sampleRateDivideInv = 1 / sampleRateDivide;
            float inputIncCount = 0;

            bool grainProb = clamp(param1S * 3, 0.05f, 0.95f) >= fabs(noise[0]);

            float filterB2    = 0.15f + param2S * 0.1f;
            float filterB     = (filterB2 * filterB2 * 0.5f);

            float _in3_b1 = (1 - filterB);
            float _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
            float _in3_a1 = -_in3_a0;

            if(grainProb) {
                if(grainTable[grainNext][GRAIN_RAMP] >= 1 && grainTable[grainPrev][GRAIN_RAMP] > 0.33f) {
                    //grain done, compute another one
                    float jitter = param2S * lockA;
                    float grainRate = sampleRateDivideInv * (1 + jitter * noise[4] * 0.025f);
                    grainTable[grainNext][GRAIN_RAMP] = 0;
                    grainTable[grainNext][GRAIN_SIZE] = clamp((1800 + (noise[2]) * 40 * jitter * jitter) * param1S * param1S, 432, delayBufferSize - 100);
                    grainTable[grainNext][GRAIN_POS] = modulo2(delayWritePosF - (800 + jitter * noise[5] * 790), delayBufferSize);
                    float invGrainSize = 1 / grainTable[grainNext][GRAIN_SIZE];
                    grainTable[grainNext][GRAIN_CURRENT_SHIFT] = grainTable[grainNext][GRAIN_NEXT_SHIFT];
                    grainTable[grainNext][GRAIN_NEXT_SHIFT] = grainRate * invGrainSize;
                    grainTable[grainNext][GRAIN_INC] = clamp( (grainTable[grainNext][GRAIN_NEXT_SHIFT] - grainTable[grainNext][GRAIN_CURRENT_SHIFT]) * invGrainSize, 0, 0.5f);
                    grainTable[grainNext][GRAIN_VOL] = sqrt3(fabsf(noise[3]));
                    grainTable[grainNext][GRAIN_PAN] = clamp(1 + (noise[6]) * sqrt3(param2S), 0, 2) * 0.5f;
                    grainPrev = grainNext;

                    float filterB2    = (0.35f - sqrt3(fabsf(param1S)) * 0.2f) * lockA + (0.05f) * lockB;
                    float filterB     = (filterB2 * filterB2 * 0.5f);
                    _in3_b1 = (1 - filterB);
                    _in3_a0 = (1 + _in3_b1 * _in3_b1 * _in3_b1) * 0.5f;
                    _in3_a1 = -_in3_a0;

                    if(lockB < 0.02f) {
                        loopSize = clamp((1 - param1S) * 1400, 48, 1800);
                    }
                }
                if(++grainNext > 2) {
                    grainNext = 0;
                }
            }

            float env;

            const float f = 0.85f;
            const float f2 = 0.8f;
            const float fnotch = 1.03f;

            float grain1, grain2, grain3;
            float grain1L, grain1R;
            float grain2L, grain2R;
            float grain3L, grain3R;
            float grainSumR, grainSumL;

            float *sp = sampleBlock_;
            float monoIn;

            for (int k = 0; k < BLOCK_SIZE; k++) {

                if (++inputIncCount >= sampleRateDivide)
                {
                    inputIncCount = 0;
                    delayWritePos = (delayWritePos + 1) & delayBufferSizeM1;
                    delayWritePosF = (float)delayWritePos;

                    delayReadPos = modulo(delayWritePos + loopSize, delayBufferSize);
                    float feedbackIn = delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);

                    monoIn = (*sp + *(sp + 1)) * 0.5f;
                    // hp
                    hp_in_x0    = monoIn;
                    hp_in_y0    = _in3_a0 * hp_in_x0 + _in3_a1 * hp_in_x1 + _in3_b1 * hp_in_y1;
                    hp_in_y1    = hp_in_y0;
                    hp_in_x1    = hp_in_x0;

                    delayBuffer_[delayWritePos] = hp_in_y0 * lockA + feedbackIn * lockB;
                }

                ///-------- grain 1
                grain1L = grain1R = 0;
                if (grainTable[0][GRAIN_RAMP] < 1)
                {
                    delayReadPos = modulo(grainTable[0][GRAIN_POS] + grainTable[0][GRAIN_RAMP] * grainTable[0][GRAIN_SIZE], delayBufferSize);
                    env = hann(grainTable[0][GRAIN_RAMP]) * grainTable[0][GRAIN_VOL];
                    grain1 = env * delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                    grain1L = grain1 * grainTable[0][GRAIN_PAN];
                    grain1R = grain1 - grain1L;
                }
                grainTable[0][GRAIN_RAMP] += grainTable[0][GRAIN_CURRENT_SHIFT];
                grainTable[0][GRAIN_CURRENT_SHIFT] += grainTable[0][GRAIN_INC];

                ///-------- grain 2
                grain2L = grain2R = 0;
                if (grainTable[1][GRAIN_RAMP] < 1)
                {
                    delayReadPos = modulo(grainTable[1][GRAIN_POS] + grainTable[1][GRAIN_RAMP] * grainTable[1][GRAIN_SIZE], delayBufferSize);
                    env = hann(grainTable[1][GRAIN_RAMP]) * grainTable[1][GRAIN_VOL];
                    grain2 = env * delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                    grain2L = grain2 * grainTable[1][GRAIN_PAN];
                    grain2R = grain2 - grain2L;
                }
                grainTable[1][GRAIN_RAMP] += grainTable[1][GRAIN_CURRENT_SHIFT];
                grainTable[1][GRAIN_CURRENT_SHIFT] += grainTable[1][GRAIN_INC];

                ///-------- grain 3
                grain3L = grain3R = 0;
                if (grainTable[2][GRAIN_RAMP] < 1)
                {
                    delayReadPos = modulo(grainTable[2][GRAIN_POS] + grainTable[2][GRAIN_RAMP] * grainTable[2][GRAIN_SIZE], delayBufferSize);
                    env = hann(grainTable[2][GRAIN_RAMP]) * grainTable[2][GRAIN_VOL];
                    grain3 = env * delayInterpolation(delayReadPos, delayBuffer_, delayBufferSizeM1);
                    grain3L = grain3 * grainTable[2][GRAIN_PAN];
                    grain3R = grain3 - grain3L;
                }
                grainTable[2][GRAIN_RAMP] += grainTable[2][GRAIN_CURRENT_SHIFT];
                grainTable[2][GRAIN_CURRENT_SHIFT] += grainTable[2][GRAIN_INC];

                grainSumL = grain1L + grain2L + grain3L;
                grainSumR = grain1R + grain2R + grain3R;

                // lp L
                low3  += f2 * band3;
                band3 += f2 * ((grainSumL) - low3 - band3);
                low5  += f2 * band5;
                band5 += f2 * ((low3) - low5 - band5);

                // lp R
                low4  += f2 * band4;
                band4 += f2 * ((grainSumR) - low4 - band4);
                low6  += f2 * band6;
                band6 += f2 * ((low4) - low6 - band6);

                // notch L
                hb1_x1 += fnotch * hb1_y1;
                float high7 = low5 - hb1_x1 - hb1_y1;
                hb1_y1 += fnotch * high7;
                float notchL = (high7 + hb1_x1);
      
                // notch R
                hb1_x2 += fnotch * hb1_y2;
                float high8 = low6 - hb1_x2 - hb1_y2;
                hb1_y2 += fnotch * high8;
                float notchR = (high8 + hb1_x2);

                *sp = *sp * dry + notchL * wetL;
                sp++;
                *sp = *sp * dry + notchR * wetR;
                sp++;
            }
        }

        break;
        default:
            // NO EFFECT
            break;
    }
}

float Timbre::iirFilter(float x, float a1, float *xn1, float *xn2, float *yn1, float *yn2) {
    // https://dsp.stackexchange.com/a/59157
    // 𝑦[𝑘] = 𝑐( 𝑥[𝑘] + 𝑦[𝑘−2] ) − 𝑥[𝑘−2]
    float y = a1 * (x + *yn2) - *xn2;
    *yn2 = *yn1;
    *yn1 = y;
    *xn2 = *xn1;
    *xn1 = x;

    return y;
}

float Timbre::delayInterpolation(float readPos, float buffer[], int bufferLenM1) {
    int readPosInt = readPos;
    float y1 = buffer[readPosInt];
    float y0 = buffer[(readPosInt - 1) & bufferLenM1];
    float x = 1 - (readPos - floorf(readPos));
    return (y0 - y1) * x + y1;
}

float Timbre::delayInterpolation2(float readPos, float buffer[], int bufferLenM1, int offset) {
    int readPosInt = readPos;
    float y1 = buffer[offset + readPosInt];
    float y0 = buffer[offset + ((readPosInt - 1) & bufferLenM1)];
    float x = 1 - (readPos - floorf(readPos));
    return (y0 - y1) * x + y1;
}

void Timbre::voicesToTimbre(float volumeGain) {

    int numberOfVoicesToCopy = numberOfVoices_;

    // If unison : all voices have already been added to the first one
    if (unlikely(params_.engine1.playMode == PLAY_MODE_UNISON)) {
        numberOfVoicesToCopy = 1;
    }

    for (int k = 0; k < numberOfVoicesToCopy; k++) {
        float *timbreBlock = sampleBlock_;
        const float *voiceBlock = voices_[voiceNumber_[k]]->getSampleBlock();

        if (unlikely(k == 0)) {
            if (unlikely(numberOfVoicesToCopy == 1.0f)) {
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
    // Gate algo !!
    float gate = voices_[lastPlayedNote_]->matrix.getDestination(MAIN_GATE);
    if (unlikely(gate > 0 || currentGate_ > 0)) {
        gate *= .72547132656922730694f; // 0 < gate < 1.0
        if (gate > 1.0f) {
            gate = 1.0f;
        }
        float incGate = (gate - currentGate_) * .03125f; // ( *.03125f = / 32)
        // limit the speed.
        if (incGate > 0.002f) {
            incGate = 0.002f;
        } else if (incGate < -0.002f) {
            incGate = -0.002f;
        }

        float *sp = sampleBlock_;
        float coef;
        for (int k = 0; k < BLOCK_SIZE; k++) {
            currentGate_ += incGate;
            coef = 1.0f - currentGate_;
            *sp = *sp * coef;
            sp++;
            *sp = *sp * coef;
            sp++;
        }
        //    currentGate = gate;
    }
}

void Timbre::afterNewParamsLoad() {
    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->afterNewParamsLoad();
    }

    for (int j = 0; j < NUMBER_OF_ENCODERS_PFM2; j++) {
        env1_.reloadADSR(j);
        env2_.reloadADSR(j);
        env3_.reloadADSR(j);
        env4_.reloadADSR(j);
        env5_.reloadADSR(j);
        env6_.reloadADSR(j);
    }

    resetArpeggiator();
    for (int k = 0; k < NUMBER_OF_ENCODERS_PFM2; k++) {
        setNewEffecParam(k);
    }
    // Update midi note scale
    updateMidiNoteScale(0);
    updateMidiNoteScale(1);
}

void Timbre::resetArpeggiator() {
    // Reset Arpeggiator
    FlushQueue();
    note_stack_.Clear();
    setArpeggiatorClock(params_.engineArp1.clock);
    setLatchMode(params_.engineArp2.latche);
}

void Timbre::setNewValue(int index, struct ParameterDisplay *param, float newValue) {
    if (newValue > param->maxValue) {
        // in v2, matrix target were removed so some values are > to max value but we need to accept it
        bool mustConstraint = true;
        if (param->valueNameOrder != NULL) {
            for (int v = 0; v < param->numberOfValues; v++) {
                if ((int) param->valueNameOrder[v] == (int) (newValue + .01)) {
                    mustConstraint = false;
                }
            }
        }
        if (mustConstraint) {
            newValue = param->maxValue;
        }
    } else if (newValue < param->minValue) {
        newValue = param->minValue;
    }
    ((float*) &params_)[index] = newValue;
}

int Timbre::getSeqStepValue(int whichStepSeq, int step) {

    if (whichStepSeq == 0) {
        return params_.lfoSteps1.steps[step];
    } else {
        return params_.lfoSteps2.steps[step];
    }
}

void Timbre::setSeqStepValue(int whichStepSeq, int step, int value) {

    if (whichStepSeq == 0) {
        params_.lfoSteps1.steps[step] = value;
    } else {
        params_.lfoSteps2.steps[step] = value;
    }
}

void Timbre::setNewEffecParam(int encoder) {

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
    if (params_.engineArp1.clock == CLOCK_INTERNAL) {
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
    if (params_.engineArp1.clock == CLOCK_EXTERNAL) {
        running_ = 1;
    }
}

void Timbre::OnMidiStart() {
    if (params_.engineArp1.clock == CLOCK_EXTERNAL) {
        Start();
    }
}

void Timbre::OnMidiStop() {
    if (params_.engineArp1.clock == CLOCK_EXTERNAL) {
        running_ = 0;
        FlushQueue();
    }
}

void Timbre::OnMidiClock() {
    if (params_.engineArp1.clock == CLOCK_EXTERNAL && running_) {
        Tick();
    }
}

void Timbre::SendNote(uint8_t note, uint8_t velocity) {

    // If there are some Note Off messages for the note about to be triggeered
    // remove them from the queue and process them now.
    if (event_scheduler_.Remove(note, 0)) {
        preenNoteOff(note);
    }

    // Send a note on and schedule a note off later.
    preenNoteOn(note, velocity);
    event_scheduler_.Schedule(note, 0, midi_clock_tick_per_step[(int) params_.engineArp2.duration] - 1, 0);
}

void Timbre::SendLater(uint8_t note, uint8_t velocity, uint8_t when, uint8_t tag) {
    event_scheduler_.Schedule(note, velocity, when, tag);
}

void Timbre::SendScheduledNotes() {
    uint8_t current = event_scheduler_.root();
    while (current) {
        const SchedulerEntry &entry = event_scheduler_.entry(current);
        if (entry.when) {
            break;
        }
        if (entry.note != kZombieSlot) {
            if (entry.velocity == 0) {
                preenNoteOff(entry.note);
            } else {
                preenNoteOn(entry.note, entry.velocity);
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
        if (params_.engineArp1.clock == CLOCK_INTERNAL) {
            running_ = 0;
            FlushQueue();
        }
    }

    SendScheduledNotes();

    if (tick_ >= midi_clock_tick_per_step[(int) params_.engineArp2.division]) {
        tick_ = 0;
        uint16_t pattern = getArpeggiatorPattern();
        uint8_t has_arpeggiator_note = (bitmask_ & pattern) ? 255 : 0;
        const int num_notes = note_stack_.size();
        const int direction = params_.engineArp1.direction;

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
                const NoteEntry &noteEntry = ARPEGGIO_DIRECTION_PLAYED == direction ? note_stack_.played_note(step) : note_stack_.sorted_note(step);

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
                    const NoteEntry &noteEntry = note_stack_.sorted_note(i);
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

    int direction = params_.engineArp1.direction;
    uint8_t num_notes = note_stack_.size();
    if (direction == ARPEGGIO_DIRECTION_RANDOM) {
        uint8_t random_byte = *(uint8_t*) noise;
        current_octave_ = random_byte & 0xf;
        current_step_ = (random_byte & 0xf0) >> 4;
        while (current_octave_ >= params_.engineArp1.octave) {
            current_octave_ -= params_.engineArp1.octave;
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
            if (current_octave_ >= params_.engineArp1.octave || current_octave_ < 0) {
                if (__canChangeDir(direction)) {
                    current_direction_ = -current_direction_;
                    StartArpeggio();
                    if (num_notes > 1 || params_.engineArp1.octave > 1) {
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
        current_octave_ = params_.engineArp1.octave - 1;
    }
}

void Timbre::Start() {
    bitmask_ = 1;
    recording_ = 0;
    running_ = 1;
    tick_ = midi_clock_tick_per_step[(int) params_.engineArp2.division] - 1;
    current_octave_ = 127;
    current_direction_ = __getDirection(params_.engineArp1.direction);
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
    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->lfoValueChange(currentRow, encoder, newValue);
    }
}

void Timbre::updateMidiNoteScale(int scale) {

    int intBreakNote;
    int curveBefore;
    int curveAfter;
    if (scale == 0) {
        intBreakNote = params_.midiNote1Curve.breakNote;
        curveBefore = params_.midiNote1Curve.curveBefore;
        curveAfter = params_.midiNote1Curve.curveAfter;
    } else {
        intBreakNote = params_.midiNote2Curve.breakNote;
        curveBefore = params_.midiNote2Curve.curveBefore;
        curveAfter = params_.midiNote2Curve.curveAfter;
    }
    float floatBreakNote = intBreakNote;
    float multiplier = 1.0f;

    switch (curveBefore) {
        case MIDI_NOTE_CURVE_FLAT:
            for (int n = 0; n < intBreakNote; n++) {
                midiNoteScale[scale][timbreNumber_][n] = 0;
            }
            break;
        case MIDI_NOTE_CURVE_M_LINEAR:
            multiplier = -1.0f;
            goto linearBefore;
        case MIDI_NOTE_CURVE_M_LINEAR2:
            multiplier = -8.0f;
            goto linearBefore;
        case MIDI_NOTE_CURVE_LINEAR2:
            multiplier = 8.0f;
            goto linearBefore;
        case MIDI_NOTE_CURVE_LINEAR:
linearBefore: for (int n = 0; n < intBreakNote; n++) {
                float fn = (floatBreakNote - n);
                midiNoteScale[scale][timbreNumber_][n] = fn * INV127 * multiplier;
            }
            break;
        case MIDI_NOTE_CURVE_M_EXP:
            multiplier = -1.0f;
        case MIDI_NOTE_CURVE_EXP:
            if (floatBreakNote == 0) {
                floatBreakNote = 1;
            }
            for (int n = 0; n < intBreakNote; n++) {
                float fn = (floatBreakNote - n);
                fn = fn * fn / floatBreakNote;
                midiNoteScale[scale][timbreNumber_][n] = fn * INV16 * multiplier;
            }
            break;
    }

    // BREAK NOTE = 0;
    midiNoteScale[scale][timbreNumber_][intBreakNote] = 0;

    switch (curveAfter) {
        case MIDI_NOTE_CURVE_FLAT:
            for (int n = intBreakNote + 1; n < 128; n++) {
                midiNoteScale[scale][timbreNumber_][n] = 0;
            }
            break;
        case MIDI_NOTE_CURVE_M_LINEAR:
            multiplier = -1.0f;
            goto linearAfter;
        case MIDI_NOTE_CURVE_M_LINEAR2:
            multiplier = -8.0f;
            goto linearAfter;
        case MIDI_NOTE_CURVE_LINEAR2:
            multiplier = 8.0f;
            goto linearAfter;
        case MIDI_NOTE_CURVE_LINEAR:
linearAfter: for (int n = intBreakNote + 1; n < 128; n++) {
                float fn = n - floatBreakNote;
                midiNoteScale[scale][timbreNumber_][n] = fn * INV127 * multiplier;
            }
            break;
        case MIDI_NOTE_CURVE_M_EXP:
            multiplier = -1.0f;
        case MIDI_NOTE_CURVE_EXP:
            if (floatBreakNote == 0) {
                floatBreakNote = 1;
            }
            for (int n = intBreakNote + 1; n < 128; n++) {
                float fn = n - floatBreakNote;
                fn = fn * fn / floatBreakNote;
                midiNoteScale[scale][timbreNumber_][n] = fn * INV16 * multiplier;
            }
            break;
    }
    /*
     lcd.setCursor(0,0);
     lcd.print((int)(midiNoteScale[timbreNumber][25] * 127.0f));
     lcd.print(" ");
     lcd.setCursor(10,0);
     lcd.print((int)(midiNoteScale[timbreNumber][intBreakNote - 5] * 127.0f));
     lcd.print(" ");
     lcd.setCursor(0,1);
     lcd.print((int)(midiNoteScale[timbreNumber][intBreakNote + 5] * 127.0f));
     lcd.print(" ");
     lcd.setCursor(10,1);
     lcd.print((int)(midiNoteScale[timbreNumber][102] * 127.0f));
     lcd.print(" ");
     */

}

void Timbre::midiClockContinue(int songPosition) {

    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->midiClockContinue(songPosition);
    }

    recomputeNext_ = ((songPosition & 0x1) == 0);
    OnMidiContinue();
}

void Timbre::midiClockStart() {

    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->midiClockStart();
    }

    recomputeNext_ = true;
    OnMidiStart();
}

void Timbre::midiClockSongPositionStep(int songPosition) {

    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->midiClockSongPositionStep(songPosition, recomputeNext_);
    }

    if ((songPosition & 0x1) == 0) {
        recomputeNext_ = true;
    }
}

void Timbre::resetMatrixDestination(float oldValue) {
    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->matrix.resetDestination(oldValue);
    }
}

void Timbre::setMatrixSource(enum SourceEnum source, float newValue) {
    for (int k = 0; k < numberOfVoices_; k++) {
        voices_[voiceNumber_[k]]->matrix.setSource(source, newValue);
    }
}

void Timbre::setMatrixSourceMPE(uint8_t channel, enum SourceEnum source, float newValue) {
    int voiceToUse = voiceNumber_[channel -1];

    if (unlikely(voiceToUse == -1)) {
        return;
    }

	voices_[voiceToUse]->matrix.setSource(source, newValue);
}


void Timbre::setMatrixPolyAfterTouch(uint8_t note, float newValue) {
    for (int k = 0; k < numberOfVoices_; k++) {
        int voiceIndex = voiceNumber_[k];
        if (unlikely(voices_[voiceIndex]->isPlaying() && voices_[voiceIndex]->getNote() == note)) {
            voices_[voiceNumber_[k]]->matrix.setSource(MATRIX_SOURCE_POLYPHONIC_AFTERTOUCH, newValue);
            break;
        }
    }
}



void Timbre::verifyLfoUsed(int encoder, float oldValue, float newValue) {
    // No need to recompute
    if (numberOfVoices_ == 0.0f) {
        return;
    }
    // We used it and still use it
    if (encoder == ENCODER_MATRIX_MUL && oldValue != 0.0f && newValue != 0.0f) {
        return;
    }

    for (int lfo = 0; lfo < NUMBER_OF_LFO; lfo++) {
        lfoUSed_[lfo] = 0;
    }

    MatrixRowParams *matrixRows = &params_.matrixRowState1;

    for (int r = 0; r < MATRIX_SIZE; r++) {
        if ((matrixRows[r].source >= MATRIX_SOURCE_LFO1 && matrixRows[r].source <= MATRIX_SOURCE_LFOSEQ2) && matrixRows[r].mul != 0.0f
            && (matrixRows[r].dest1 != 0.0f || matrixRows[r].dest2 != 0.0f)) {
            lfoUSed_[(int) matrixRows[r].source - MATRIX_SOURCE_LFO1]++;
        }

        // Check if we have a Mtx* that would require LFO even if mul is 0
        // http://ixox.fr/forum/index.php?topic=69220.0
        if (matrixRows[r].mul != 0.0f && matrixRows[r].source != 0.0f) {
            if (matrixRows[r].dest1 >= MTX1_MUL && matrixRows[r].dest1 <= MTX4_MUL) {
                int index = matrixRows[r].dest1 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed_[(int) matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
                }
            }
            // same test for dest2
            if (matrixRows[r].dest2 >= MTX1_MUL && matrixRows[r].dest2 <= MTX4_MUL) {
                int index = matrixRows[r].dest2 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed_[(int) matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
                }
            }
        }
    }

    /*
     lcd.setCursor(11, 1);
     lcd.print('>');
     for (int lfo=0; lfo < NUMBER_OF_LFO; lfo++) {
     lcd.print((int)lfoUSed[lfo]);
     }
     lcd.print('<');
     */

}
