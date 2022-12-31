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
// Regular memory
float midiNoteScale[2][NUMBER_OF_TIMBRES][128] __attribute__((section(".ram_d1")));
float Timbre::unisonPhase[14] = { .37f, .11f, .495f, .53f, .03f, .19f, .89f, 0.23f, .71f, .19f, .31f, .43f, .59f, .97f };


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
