/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
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
#ifndef MIDI_SEQUENCER_H_
#define MIDI_SEQUENCER_H_


#include <stdint.h>
#include "Common.h"

class Synth;
class FMDisplaySequencer;

#define SEQ_ACTION_SIZE 2048
#define NUMBER_OF_STEP_SEQUENCES 12

#define SEQ_ACTION_SIZE_INV (1.0f/(float)SEQ_ACTION_SIZE)
#define SEQ_LAST_ACTION_INDEX SEQ_LAST_ACTION_SIZE-1

#define BITS_PER_BEAT 8
#define MAX_PER_BEAT (0xff)
#define MAX_TIME (0xfff)

enum SEQ_VERSION {
    SEQ_VERSION1 = 1,
    SEQ_VERSION2
};

#define SEQ_CURRENT_VERSION SEQ_VERSION2

enum SeqMidiActionType {
    SEQ_ACTION_NONE = 0,
    SEQ_ACTION_NOTE,
    SEQ_ACTION_CC,
    SEQ_ACTION_CLEAR
};


struct SeqMidiAction {
    uint16_t when; // 2 bits measure + 2 bits beat + 8 bits
    uint16_t nextIndex;
    uint8_t actionType;
    uint8_t param1;
    uint8_t param2;
    uint8_t unused; // It takes memory even if it does not exist
};

union StepSeqValue {
    uint64_t full;
    uint8_t values[8];
    uint16_t unique;
};


class Sequencer {
public:
    Sequencer();
    virtual ~Sequencer();
    void setSynth(Synth* synth);
    void setDisplaySequencer(FMDisplaySequencer* displaySequencer);

    void getFullState(uint8_t* buffer, uint32_t *size);
    void getFullDefaultState(uint8_t* buffer, uint32_t *size);
    void setFullState(uint8_t* buffer);

    void setTempo(float newTempo);

    bool isRunning() { return running; }
    void start();
    void stop();

    void rewind() {
        millisTimer = 0;
        current16bitTimer = 0;
        midiClockTimer = 0;
    }

    void reset(bool synthNoteOff);
    void clear(uint8_t instrument);
    void ticMillis();

    void onMidiContinue(int songPosition);
    void onMidiStart();
    void onMidiStop();
    void onMidiClock();
    void midiClockSetSongPosition(int songPosition);

    void tic(uint16_t counter);
    void calculateSequenceDuration(int instrument);
    void insertNote(uint8_t instrument, uint8_t note, uint8_t velocity);

    bool isMuted(uint8_t instrument) {
        return muted[instrument];
    }
    void setMuted(uint8_t instrument, bool mute);

    void toggleMuted(uint8_t instrument);

    bool isRecording(uint8_t instrument) {
        return recording[instrument];
    }
    void setRecording(uint8_t instrument, bool record) {
        recording[instrument] = record;
    }
    void toggleRecording(uint8_t instrument) {
        recording[instrument] = !recording[instrument];
    }
    bool setSeqActivated(uint8_t instrument);


    bool isSeqActivated(uint8_t instrument) {
        return seqActivated[instrument];
    }

    bool isStepActivated(uint8_t instrument) {
    	int seqNumber = instrumentStepSeq[instrument];
        return stepActivated[seqNumber];
    }

    float getPrecount() {
        return precount;
    }
    uint8_t getMeasure() {
        return (current16bitTimer >> (BITS_PER_BEAT + 2));
    }
    uint8_t getBeat()  {
        return ((current16bitTimer >> BITS_PER_BEAT) & 0x3) + 1;
    }
    uint8_t getTempo() {
        return tempo;
    }
    uint8_t getMemory() {
        return 100.0f * ((float)lastFreeAction) * SEQ_ACTION_SIZE_INV;
    }

    uint8_t getNumberOfBars(int instrument) {
        return (instrumentTimerMask[instrument] >> (BITS_PER_BEAT + 2)) + 1;
    }

    void setNumberOfBars(int instrument, uint8_t bars);

    bool isExternalClockEnabled() {
        return externalClock;
    }

    void setExternalClock(bool enable);

    void setStepMode(bool stepMode) {
        this->stepMode = stepMode;
    }
    void stepClearAll(int instrument);
    bool stepRecordNotes(int instrument, int stepCursor, int stepSize);
    void stepClearPart(int instrument, int stepCursor, int stepSize);

    StepSeqValue* stepGetSequence(int instrument);
    const char* getSequenceName() {
        return sequenceName;
    }
    void setSequenceName(const char* newName);
    char* getSequenceNameInBuffer(char* buffer);
    uint8_t getInstrumentStepSeq(int instrument) {
    	return instrumentStepSeq[instrument];
    }
    void setInstrumentStepSeq(int instrument, int index) {
    	instrumentStepSeq[instrument] = (uint8_t)index;
    }

private:
    void processActionBetwen(int instrument, uint16_t startTimer, uint16_t endTimer);
    void resyncNextAction(int instrument, uint16_t newInstrumentTimer);
    void loadStateVersion1(uint8_t* buffer);
    void loadStateVersion2(uint8_t* buffer);

    char sequenceName[13];
    uint16_t lastFreeAction;
    Synth * synth;
    FMDisplaySequencer* displaySequencer;

    bool externalClock;
    float tempo;
    float oneBitInMillis;
    float millisInBits;
    uint32_t millisTimer;
    float midiClockTimer;
    uint16_t current16bitTimer;
    uint16_t previousCurrent16bitTimer;
    uint32_t sequenceDuration;
    bool running;
    bool extMidiRunning;
    float precount;
    uint16_t lastBeat;
    uint32_t ledTimer;

    // Step sequencer
    int stepCurrentInstrumentNONO;
    bool stepMode;
    int stepNumberOfNotesOn;

    // Per instrument
    uint16_t stepUniqueValue[NUMBER_OF_TIMBRES];
    bool seqActivated[NUMBER_OF_TIMBRES];
    // Per sequence
    bool stepActivated[NUMBER_OF_STEP_SEQUENCES];
    uint16_t nextActionIndex[NUMBER_OF_TIMBRES];
    uint16_t previousActionIndex[NUMBER_OF_TIMBRES];
    bool nextActionTimerOutOfSync[NUMBER_OF_TIMBRES];
    bool recording[NUMBER_OF_TIMBRES];
    bool muted[NUMBER_OF_TIMBRES];
    //uint16_t nextLastActionTimer[NUMBER_OF_TIMBRES];;
    uint16_t instrumentTimerMask[NUMBER_OF_TIMBRES];
    uint16_t lastInstrument16bitTimer[NUMBER_OF_TIMBRES];
    uint8_t instrumentStepIndex[NUMBER_OF_TIMBRES];
    uint8_t instrumentStepLastUnique[NUMBER_OF_TIMBRES];
    // Each instrument points to its own step seq
    uint8_t instrumentStepSeq[NUMBER_OF_TIMBRES];

    // Tmp step seq to store current pressed midi keys
    StepSeqValue tmpStepValue;
};

#endif /* MIDI_SEQUENCER_H_ */
