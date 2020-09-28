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


static const float calledPerSecond = PREENFM_FREQUENCY / 32.0f;

/*
#include "LiquidCrystal.h"
extern LiquidCrystal lcd;
void myVoiceError(char info, int t, int t2) {
    lcd.setRealTimeAction(true);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print('!');
    lcd.print(info);
    lcd.print(t);
    lcd.print(' ');
    lcd.print(t2);
    while (true) {};
}

...
        if (voiceNumber[k] < 0) myVoiceError('A', voiceNumber[k], k);

*/

//#define DEBUG_ARP_STEP
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
  const int pattern = (int)params.engineArp2.pattern;
  if ( pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT )
    return ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[ pattern ]);
  else
    return ARP_PATTERN_GETMASK( params.engineArpUserPatterns.patterns[ pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT ] );
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



// Static to all 4 timbres
unsigned int voiceIndex;


Timbre::Timbre() {


    this->recomputeNext = true;
	this->currentGate = 0;
    this->sbMax = &this->sampleBlock[64];
    this->holdPedal = false;
    this->lastPlayedNote = 0;
    // arpegiator
    setNewBPMValue(90);
    arpegiatorStep = 0.0;
    idle_ticks_ = 96;
    running_ = 0;
    ignore_note_off_messages_ = 0;
    recording_ = 0;
    note_stack.Init();
    event_scheduler.Init();
    // Arpeggiator start
    Start();


}

Timbre::~Timbre() {
}

void Timbre::init(SynthState* synthState, int timbreNumber) {

    mixerState = &synthState->mixerState;

	env1.init(&params.env1a,  &params.env1b, 0, &params.engine1.algo);
	env2.init(&params.env2a,  &params.env2b, 1, &params.engine1.algo);
	env3.init(&params.env3a,  &params.env3b, 2, &params.engine1.algo);
	env4.init(&params.env4a,  &params.env4b, 3, &params.engine1.algo);
	env5.init(&params.env5a,  &params.env5b, 4, &params.engine1.algo);
	env6.init(&params.env6a,  &params.env6b, 5, &params.engine1.algo);

	osc1.init(synthState, &params.osc1, OSC1_FREQ);
	osc2.init(synthState, &params.osc2, OSC2_FREQ);
	osc3.init(synthState, &params.osc3, OSC3_FREQ);
	osc4.init(synthState, &params.osc4, OSC4_FREQ);
	osc5.init(synthState, &params.osc5, OSC5_FREQ);
	osc6.init(synthState, &params.osc6, OSC6_FREQ);

    this->timbreNumber = timbreNumber;

    for (int s=0; s<2; s++) {
        for (int n=0; n<128; n++) {
            midiNoteScale[s][timbreNumber][n] = INV127 * (float)n;
        }
    }
    for (int lfo=0; lfo<NUMBER_OF_LFO; lfo++) {
        lfoUSed[lfo] = 0;
    }

    lowerNote = 64;
    lowerNoteReleased = true;
}

void Timbre::setVoiceNumber(int v, int n) {
    this->voiceNumber[v] = n;
    if (n >= 0) {
        voices[n]->setCurrentTimbre(this);
    }
}

void Timbre::initVoicePointer(int n, Voice* voice) {
	voices[n] = voice;
}

void Timbre::noteOn(char note, char velocity) {
	if (params.engineArp1.clock) {
		arpeggiatorNoteOn(note, velocity);
	} else {
		preenNoteOn(note, velocity);
	}
}

void Timbre::noteOff(char note) {
	if (params.engineArp1.clock) {
		arpeggiatorNoteOff(note);
	} else {
		preenNoteOff(note);
	}
}


void Timbre::preenNoteOn(char note, char velocity) {
	note &= 0x7f;

	int iNov = params.engine1.polyMono == 0.0f ? 1 : (int) numberOfVoices;
	if (unlikely(iNov == 0)) {
		return;
	}

	// Frequency depends on the current instrument scale
    float noteFrequency = mixerState->instrumentState[timbreNumber].scaleFrequencies[(int)note];

	unsigned int indexMin = UINT32_MAX;
	int voiceToUse = -1;

	int newNoteType = NEW_NOTE_NONE;

	for (int k = 0; k < iNov; k++) {
		// voice number k of timbre
		int n = voiceNumber[k];

        if (unlikely(voices[n]->isNewNotePending())) {
            continue;
        }

		// same note = priority 1 : take the voice immediatly
		if (unlikely(voices[n]->isPlaying() && voices[n]->getNote() == note)) {

            preenNoteOnUpdateMatrix(n, note, velocity);
            voices[n]->noteOnWithoutPop(note, noteFrequency, velocity, voiceIndex++);
            this->lastPlayedNote = n;
            // Just in case we tune Osc Freq/Ftune while playing the same note
            lowerNoteFrequency = voices[n]->getNoteRealFrequencyEstimation(noteFrequency);
			return;
		}

		// unlikely because if it true, CPU is not full
		if (unlikely(newNoteType > NEW_NOTE_FREE)) {
			if (!voices[n]->isPlaying()) {
				voiceToUse = n;
				newNoteType = NEW_NOTE_FREE;
			}

			if (voices[n]->isReleased()) {
				unsigned int indexVoice = voices[n]->getIndex();
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
			int n = voiceNumber[k];
			unsigned int indexVoice = voices[n]->getIndex();
			if (indexVoice < indexMin && !voices[n]->isNewNotePending()) {
				newNoteType = NEW_NOTE_OLD;
				indexMin = indexVoice;
				voiceToUse = n;
			}
		}
	}
	// All voices in newnotepending state ?
	if (voiceToUse != -1) {

		preenNoteOnUpdateMatrix(voiceToUse, note, velocity);

		switch (newNoteType) {
		case NEW_NOTE_FREE:
			voices[voiceToUse]->noteOn(note, noteFrequency, velocity, voiceIndex++);
			break;
		case NEW_NOTE_OLD:
		case NEW_NOTE_RELEASE:
			voices[voiceToUse]->noteOnWithoutPop(note, noteFrequency, velocity, voiceIndex++);
			break;
		}

		this->lastPlayedNote = voiceToUse;
	    if (note <= lowerNote || lowerNoteReleased || iNov == 1) {
	        lowerNote = note;
	        lowerNoteReleased = false;
	        lowerNoteFrequency = voices[voiceToUse]->getNoteRealFrequencyEstimation(noteFrequency);
	    }

	}
}



void Timbre::preenNoteOnUpdateMatrix(int voiceToUse, int note, int velocity) {
    // Update voice matrix with midi note and velocity
    voices[voiceToUse]->matrix.setSource(MATRIX_SOURCE_NOTE1, midiNoteScale[0][timbreNumber][note]);
    voices[voiceToUse]->matrix.setSource(MATRIX_SOURCE_NOTE2, midiNoteScale[1][timbreNumber][note]);
    voices[voiceToUse]->matrix.setSource(MATRIX_SOURCE_VELOCITY, INV127*velocity);
}

void Timbre::preenNoteOff(char note) {

	if (note == lowerNote) {
		lowerNoteReleased = true;
	}

    int iNov = params.engine1.polyMono == 0.0f ? 1 : (int) numberOfVoices;
	for (int k = 0; k < iNov; k++) {
		// voice number k of timbre
		int n = voiceNumber[k];

		// Not playing = free CPU
		if (unlikely(!voices[n]->isPlaying())) {
			continue;
		}

		if (likely(voices[n]->getNextGlidingNote() == 0)) {
			if (voices[n]->getNote() == note) {
				if (unlikely(holdPedal)) {
					voices[n]->setHoldedByPedal(true);
					return;
				} else {
					voices[n]->noteOff();
					return;
				}
			}
		} else {
			// if gliding and releasing first note
			if (voices[n]->getNote() == note) {
				voices[n]->glideFirstNoteOff();
				return;
			}
			// if gliding and releasing next note
			if (voices[n]->getNextGlidingNote() == note) {
				voices[n]->glideToNote(voices[n]->getNote(), voices[n]->getNoteFrequency());
				voices[n]->glideFirstNoteOff();
                // Sync Osccilo
				lowerNoteFrequency = voices[n]->getNoteRealFrequencyEstimation(voices[n]->getNoteFrequency());
				return;
			}
		}
	}
}


void Timbre::setHoldPedal(int value) {
	if (value <64) {
		holdPedal = false;
	    int nVoices = numberOfVoices;
	    for (int k = 0; k < nVoices; k++) {
	        // voice number k of timbre
	        int n = voiceNumber[k];
	        if (voices[n]->isHoldedByPedal()) {
	        	voices[n]->noteOff();
	        }
	    }
	    arpeggiatorSetHoldPedal(0);
	} else {
		holdPedal = true;
	    arpeggiatorSetHoldPedal(127);
	}
}




void Timbre::setNewBPMValue(float bpm) {
	ticksPerSecond = bpm * 24.0f / 60.0f;
	ticksEveryNCalls = calledPerSecond / ticksPerSecond;
	ticksEveyNCallsInteger = (int)ticksEveryNCalls;
}

void Timbre::setArpeggiatorClock(float clockValue) {
	if (clockValue == CLOCK_OFF) {
		FlushQueue();
		note_stack.Clear();
	}
	if (clockValue == CLOCK_INTERNAL) {
	    setNewBPMValue(params.engineArp1.BPM);
	}
	if (clockValue == CLOCK_EXTERNAL) {
		// Let's consider we're running
		running_ = 1;
	}
}


void Timbre::updateArpegiatorInternalClock() {

	// Apeggiator clock : internal
	if (params.engineArp1.clock == CLOCK_INTERNAL) {
		arpegiatorStep+=1.0f;
		if (unlikely((arpegiatorStep) > ticksEveryNCalls)) {
			arpegiatorStep -= ticksEveyNCallsInteger;
			Tick();
		}
	}
}

void Timbre::cleanNextBlock() {

	float *sp = this->sampleBlock;
	while (sp < this->sbMax) {
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
    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->prepareMatrixForNewBlock();
    }
}



void Timbre::voicesToTimbre() {

    for (int k = 0; k < numberOfVoices; k++) {
		float* timbreBlock= this->sampleBlock;
		const float* voiceBlock = voices[voiceNumber[k]]->getSampleBlock();

		if (unlikely(k == 0)) {
            for (int s = 0 ; s < BLOCK_SIZE ; s++) {
                *timbreBlock++ = *voiceBlock++;
                *timbreBlock++ = *voiceBlock++;
            }
		} else if (k == numberOfVoices - 1) {
			// Let's apply the * numberOfVoiceInverse to divide by the number of voices
			for (int s = 0 ; s < BLOCK_SIZE ; s++) {
				*timbreBlock = (*timbreBlock + *voiceBlock++) * numberOfVoiceInverse;
				timbreBlock++;
				*timbreBlock = (*timbreBlock + *voiceBlock++) * numberOfVoiceInverse;
				timbreBlock++;
			}
		} else {
			for (int s = 0 ; s < BLOCK_SIZE ; s++) {
				*timbreBlock++ += *voiceBlock++;
				*timbreBlock++ += *voiceBlock++;
			}
		}
    }
}

void Timbre::gateFx() {
    // Gate algo !!
    float gate = voices[this->lastPlayedNote]->matrix.getDestination(MAIN_GATE);
    if (unlikely(gate > 0 || currentGate > 0)) {
		gate *=.72547132656922730694f; // 0 < gate < 1.0
		if (gate > 1.0f) {
			gate = 1.0f;
		}
		float incGate = (gate - currentGate) * .03125f; // ( *.03125f = / 32)
		// limit the speed.
		if (incGate > 0.002f) {
			incGate = 0.002f;
		} else if (incGate < -0.002f) {
			incGate = -0.002f;
		}

		float *sp = this->sampleBlock;
		float coef;
    	for (int k=0 ; k< BLOCK_SIZE ; k++) {
			currentGate += incGate;
			coef = 1.0f - currentGate;
			*sp = *sp * coef;
			sp++;
			*sp = *sp * coef;
			sp++;
		}
    //    currentGate = gate;
    }
}


void Timbre::afterNewParamsLoad() {
    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->afterNewParamsLoad();
    }

    for (int j=0; j<NUMBER_OF_ENCODERS * 2; j++) {
        this->env1.reloadADSR(j);
        this->env2.reloadADSR(j);
        this->env3.reloadADSR(j);
        this->env4.reloadADSR(j);
        this->env5.reloadADSR(j);
        this->env6.reloadADSR(j);
    }


    resetArpeggiator();
    for (int k=0; k<NUMBER_OF_ENCODERS; k++) {
        setNewEffecParam(k);
    }
    // Update midi note scale
    updateMidiNoteScale(0);
    updateMidiNoteScale(1);
}


void Timbre::resetArpeggiator() {
	// Reset Arpeggiator
	FlushQueue();
	note_stack.Clear();
	setArpeggiatorClock(params.engineArp1.clock);
	setLatchMode(params.engineArp2.latche);
}



void Timbre::setNewValue(int index, struct ParameterDisplay* param, float newValue) {
    if (newValue > param->maxValue) {
        // in v2, matrix target were removed so some values are > to max value but we need to accept it
        bool mustConstraint = true;
		if (param->valueNameOrder != NULL) {
			for (int v = 0; v < param->numberOfValues; v++) {
				if ((int)param->valueNameOrder[v] == (int)(newValue + .01)) {
					mustConstraint = false;
				}
			}
		}
        if (mustConstraint) {
            newValue= param->maxValue;
        }
    } else if (newValue < param->minValue) {
        newValue= param->minValue;
    }
    ((float*)&params)[index] = newValue;
}

int Timbre::getSeqStepValue(int whichStepSeq, int step) {

    if (whichStepSeq == 0) {
        return params.lfoSteps1.steps[step];
    } else {
        return params.lfoSteps2.steps[step];
    }
}

void Timbre::setSeqStepValue(int whichStepSeq, int step, int value) {

    if (whichStepSeq == 0) {
        params.lfoSteps1.steps[step] = value;
    } else {
        params.lfoSteps2.steps[step] = value;
    }
}


void Timbre::setNewEffecParam(int encoder) {

    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->setNewEffectParam(encoder);
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
	if (params.engineArp1.clock == CLOCK_INTERNAL) {
		if (idle_ticks_ >= 96 || !running_) {
			Start();
		}
		idle_ticks_ = 0;
	}

	if (latch_ && !recording_) {
		note_stack.Clear();
		recording_ = 1;
	}
	note_stack.NoteOn(note, velocity);
}


void Timbre::arpeggiatorNoteOff(char note) {
	if (ignore_note_off_messages_) {
		return;
	}
	if (!latch_) {
		note_stack.NoteOff(note);
	} else {
		if (note == note_stack.most_recent_note().note) {
			recording_ = 0;
		}
	}
}


void Timbre::OnMidiContinue() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL) {
		running_ = 1;
	}
}

void Timbre::OnMidiStart() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL) {
		Start();
	}
}

void Timbre::OnMidiStop() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL) {
		running_ = 0;
		FlushQueue();
	}
}


void Timbre::OnMidiClock() {
	if (params.engineArp1.clock == CLOCK_EXTERNAL && running_) {
		Tick();
	}
}


void Timbre::SendNote(uint8_t note, uint8_t velocity) {

	// If there are some Note Off messages for the note about to be triggeered
	// remove them from the queue and process them now.
	if (event_scheduler.Remove(note, 0)) {
		preenNoteOff(note);
	}

	// Send a note on and schedule a note off later.
	preenNoteOn(note, velocity);
	event_scheduler.Schedule(note, 0, midi_clock_tick_per_step[(int)params.engineArp2.duration] - 1, 0);
}

void Timbre::SendLater(uint8_t note, uint8_t velocity, uint8_t when, uint8_t tag) {
	event_scheduler.Schedule(note, velocity, when, tag);
}


void Timbre::SendScheduledNotes() {
  uint8_t current = event_scheduler.root();
  while (current) {
    const SchedulerEntry& entry = event_scheduler.entry(current);
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
  event_scheduler.Tick();
}


void Timbre::FlushQueue() {
  while (event_scheduler.size()) {
    SendScheduledNotes();
  }
}



void Timbre::Tick() {
	++tick_;

	if (note_stack.size()) {
		idle_ticks_ = 0;
	}
	++idle_ticks_;
	if (idle_ticks_ >= 96) {
		idle_ticks_ = 96;
	    if (params.engineArp1.clock == CLOCK_INTERNAL) {
	      running_ = 0;
	      FlushQueue();
	    }
	}

	SendScheduledNotes();

	if (tick_ >= midi_clock_tick_per_step[(int)params.engineArp2.division]) {
		tick_ = 0;
		uint16_t pattern = getArpeggiatorPattern();
		uint8_t has_arpeggiator_note = (bitmask_ & pattern) ? 255 : 0;
		const int num_notes = note_stack.size();
		const int direction = params.engineArp1.direction;

		if (num_notes && has_arpeggiator_note) {
			if ( ARPEGGIO_DIRECTION_CHORD != direction ) {
				StepArpeggio();
				int step, transpose = 0;
				if ( current_direction_ > 0 ) {
					step = start_step_ + current_step_;
					if ( step >= num_notes ) {
						step -= num_notes;
						transpose = 12;
					}
				} else {
					step = (num_notes - 1) - (start_step_ + current_step_);
					if ( step < 0 ) {
						step += num_notes;
						transpose = -12;
					}
				}
				const NoteEntry &noteEntry = ARPEGGIO_DIRECTION_PLAYED == direction
					? note_stack.played_note(step)
					: note_stack.sorted_note(step);

				uint8_t note = noteEntry.note;
				uint8_t velocity = noteEntry.velocity;
				note += 12 * current_octave_;
				if ( __canTranspose( direction ) )
					 note += transpose;

				while (note > 127) {
					note -= 12;
				}

				SendNote(note, velocity);
			} else {
				for (int i = 0; i < note_stack.size(); ++i ) {
					const NoteEntry& noteEntry = note_stack.sorted_note(i);
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

	int direction = params.engineArp1.direction;
	uint8_t num_notes = note_stack.size();
	if (direction == ARPEGGIO_DIRECTION_RANDOM) {
		uint8_t random_byte = *(uint8_t*)noise;
		current_octave_ = random_byte & 0xf;
		current_step_ = (random_byte & 0xf0) >> 4;
		while (current_octave_ >= params.engineArp1.octave) {
			current_octave_ -= params.engineArp1.octave;
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
		if (trigger_change && (direction >= ARPEGGIO_DIRECTION_ROTATE_UP ) ) {
			if ( ++start_step_ >= num_notes )
				start_step_ = 0;
			else
				trigger_change = 0;
		}

		if (trigger_change) {
			current_octave_ += current_direction_;
			if (current_octave_ >= params.engineArp1.octave || current_octave_ < 0) {
				if ( __canChangeDir(direction) ) {
					current_direction_ = -current_direction_;
					StartArpeggio();
					if (num_notes > 1 || params.engineArp1.octave > 1) {
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
		current_octave_ = params.engineArp1.octave - 1;
	}
}

void Timbre::Start() {
	bitmask_ = 1;
	recording_ = 0;
	running_ = 1;
	tick_ = midi_clock_tick_per_step[(int)params.engineArp2.division] - 1;
    current_octave_ = 127;
	current_direction_ = __getDirection( params.engineArp1.direction );
}


void Timbre::arpeggiatorSetHoldPedal(uint8_t value) {
  if (ignore_note_off_messages_ && !value) {
    // Pedal was released, kill all pending arpeggios.
    note_stack.Clear();
  }
  ignore_note_off_messages_ = value;
}


void Timbre::setLatchMode(uint8_t value) {
    // When disabling latch mode, clear the note stack.
	latch_ = value;
    if (value == 0) {
      note_stack.Clear();
      recording_ = 0;
    }
}

void Timbre::setDirection(uint8_t value) {
	// When changing the arpeggio direction, reset the pattern.
	current_direction_ = __getDirection(value);
	StartArpeggio();
}

void Timbre::lfoValueChange(int currentRow, int encoder, float newValue) {
    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->lfoValueChange(currentRow, encoder, newValue);
    }
}

void Timbre::updateMidiNoteScale(int scale) {

    int intBreakNote;
    int curveBefore;
    int curveAfter;
    if (scale == 0) {
        intBreakNote = params.midiNote1Curve.breakNote;
        curveBefore = params.midiNote1Curve.curveBefore;
        curveAfter = params.midiNote1Curve.curveAfter;
    } else {
        intBreakNote = params.midiNote2Curve.breakNote;
        curveBefore = params.midiNote2Curve.curveBefore;
        curveAfter = params.midiNote2Curve.curveAfter;
    }
    float floatBreakNote = intBreakNote;
    float multiplier = 1.0f;


    switch (curveBefore) {
    case MIDI_NOTE_CURVE_FLAT:
        for (int n=0; n < intBreakNote ; n++) {
            midiNoteScale[scale][timbreNumber][n] = 0;
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
        linearBefore:
        for (int n=0; n < intBreakNote ; n++) {
            float fn = (floatBreakNote - n);
            midiNoteScale[scale][timbreNumber][n] = fn * INV127 * multiplier;
        }
        break;
    case MIDI_NOTE_CURVE_M_EXP:
        multiplier = -1.0f;
    case MIDI_NOTE_CURVE_EXP:
        for (int n=0; n < intBreakNote ; n++) {
            float fn = (floatBreakNote - n);
            fn = fn * fn / floatBreakNote;
            midiNoteScale[scale][timbreNumber][n] = fn * INV16 * multiplier;
        }
        break;
    }

    // BREAK NOTE = 0;
    midiNoteScale[scale][timbreNumber][intBreakNote] = 0;


    switch (curveAfter) {
    case MIDI_NOTE_CURVE_FLAT:
        for (int n = intBreakNote + 1; n < 128 ; n++) {
            midiNoteScale[scale][timbreNumber][n] = 0;
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
        linearAfter:
        for (int n = intBreakNote + 1; n < 128 ; n++) {
            float fn = n - floatBreakNote;
            midiNoteScale[scale][timbreNumber][n] = fn  * INV127 * multiplier;
        }
        break;
    case MIDI_NOTE_CURVE_M_EXP:
        multiplier = -1.0f;
    case MIDI_NOTE_CURVE_EXP:
        for (int n = intBreakNote + 1; n < 128 ; n++) {
            float fn = n - floatBreakNote;
            fn = fn * fn / floatBreakNote;
            midiNoteScale[scale][timbreNumber][n] = fn * INV16 * multiplier;
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

    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->midiClockContinue(songPosition);
    }

    this->recomputeNext = ((songPosition&0x1)==0);
    OnMidiContinue();
}


void Timbre::midiClockStart() {

    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->midiClockStart();
    }

    this->recomputeNext = true;
    OnMidiStart();
}

void Timbre::midiClockSongPositionStep(int songPosition) {

    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->midiClockSongPositionStep(songPosition,  this->recomputeNext);
    }

    if ((songPosition & 0x1) == 0) {
        this->recomputeNext = true;
    }
}


void Timbre::resetMatrixDestination(float oldValue) {
    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->matrix.resetDestination(oldValue);
    }
}

void Timbre::setMatrixSource(enum SourceEnum source, float newValue) {
    for (int k = 0; k < numberOfVoices; k++) {
        voices[voiceNumber[k]]->matrix.setSource(source, newValue);
    }
}

void Timbre::verifyLfoUsed(int encoder, float oldValue, float newValue) {
    // No need to recompute
    if (numberOfVoices == 0.0f) {
        return;
    }
    // We used it and still use it
    if (encoder == ENCODER_MATRIX_MUL && oldValue != 0.0f && newValue != 0.0f) {
        return;
    }

    for (int lfo = 0; lfo < NUMBER_OF_LFO; lfo++) {
        lfoUSed[lfo] = 0;
    }



    MatrixRowParams* matrixRows = &params.matrixRowState1;

    for (int r = 0; r < MATRIX_SIZE; r++) {
        if ((matrixRows[r].source >= MATRIX_SOURCE_LFO1 && matrixRows[r].source <= MATRIX_SOURCE_LFOSEQ2)
                && matrixRows[r].mul != 0.0f
                && (matrixRows[r].dest1 != 0.0f || matrixRows[r].dest2 != 0.0f)) {
            lfoUSed[(int)matrixRows[r].source - MATRIX_SOURCE_LFO1]++;
        }

        // Check if we have a Mtx* that would require LFO even if mul is 0
        // http://ixox.fr/forum/index.php?topic=69220.0
        if (matrixRows[r].mul != 0.0f && matrixRows[r].source != 0.0f) {
            if (matrixRows[r].dest1 >= MTX1_MUL && matrixRows[r].dest1 <= MTX4_MUL) {
                int index = matrixRows[r].dest1 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed[(int)matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
                }
            }
            // same test for dest2
            if (matrixRows[r].dest2 >= MTX1_MUL && matrixRows[r].dest2 <= MTX4_MUL) {
                int index = matrixRows[r].dest2 - MTX1_MUL;
                if (matrixRows[index].source >= MATRIX_SOURCE_LFO1 && matrixRows[index].source <= MATRIX_SOURCE_LFOSEQ2) {
                    lfoUSed[(int)matrixRows[index].source - MATRIX_SOURCE_LFO1]++;
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
