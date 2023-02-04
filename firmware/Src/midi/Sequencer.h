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

/**
 * unique / values[0..1] = position
 * values[2] = velocity
 * values[3...8] 6 notes
 */
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
    void getFullDefaultState(uint8_t* buffer, uint32_t *size, uint8_t seqNumber);
    void setFullState(uint8_t* buffer);

    void setTempo(float newTempo);

    bool isRunning() { return running_; }
    void start();
    void stop();

    void setNewSeqValueFromMidi(uint8_t timbre, uint8_t seqValue, uint8_t newValue);

    void rewind() {
        millisTimer_ = 0;
        current16bitTimer_ = 0;
        midiClockTimer_ = 0;
    }

    void reset(bool synthNoteOff);
    void clear(uint8_t instrument);
    void ticMillis();

    void onMidiContinue(int songPosition);
    void onMidiStart();
    void onMidiStop();
    void onMidiClock();
    void midiClockSetSongPosition(int songPosition);

    void mainSequencerTic(uint16_t counter);
    void insertNote(uint8_t instrument, uint8_t note, uint8_t velocity);
    void changeCurrentNote(int instrument, int stepCursor, int stepSize, int ticks);
    void changeCurrentVelocity(int instrument, int stepCursor, int stepSize, int ticks);
    uint64_t getStepData(int instrument, int stepCursor);

    bool isMuted(uint8_t instrument) {
        return muted_[instrument];
    }
    void setMuted(uint8_t instrument, bool mute);

    void toggleMuted(uint8_t instrument);
    void setTranspose(uint8_t instrument, int16_t newValue);

    bool isRecording(uint8_t instrument) {
        return recording_[instrument];
    }
    void setRecording(uint8_t instrument, bool record) {
        recording_[instrument] = record;
    }
    void toggleRecording(uint8_t instrument) {
        recording_[instrument] = !recording_[instrument];
    }
    bool setSeqActivated(uint8_t instrument);

    bool isSeqActivated(uint8_t instrument) {
        return seqActivated_[instrument];
    }

    bool isStepActivated(uint8_t instrument) {
    	int seqNumber = instrumentStepSeq_[instrument];
        return stepActivated_[seqNumber];
    }

    float getPrecount() {
        return precount_;
    }
    uint8_t getMeasure() {
        return (current16bitTimer_ >> (BITS_PER_BEAT + 2));
    }
    uint8_t getBeat()  {
        return ((current16bitTimer_ >> BITS_PER_BEAT) & 0x3) + 1;
    }
    uint8_t getTempo() {
        return tempo_;
    }
    uint8_t getMemory() {
        return 100.0f * ((float)lastFreeAction_) * SEQ_ACTION_SIZE_INV;
    }

    uint8_t getNumberOfBars(int instrument) {
        return (instrumentTimerMask_[instrument] >> (BITS_PER_BEAT + 2)) + 1;
    }

    void setNumberOfBars(int instrument, uint8_t bars);

    bool isExternalClockEnabled() {
        return externalClock_;
    }

    void setExternalClock(bool enable);

    void setStepMode(bool stepMode) {
        cleanCurrentState();
        this->stepMode_ = stepMode;
    }
    void stepClearAll(int instrument);
    bool stepRecordNotes(int instrument, int stepCursor, int stepSize);
    void stepClearPart(int instrument, int stepCursor, int stepSize);

    StepSeqValue* stepGetSequence(int instrument);
    const char* getSequenceName() {
        return sequenceName_;
    }
    void setSequenceName(const char* newName);
    char* getSequenceNameInBuffer(char* buffer);
    uint8_t getInstrumentStepSeq(int instrument) {
    	return instrumentStepSeq_[instrument];
    }
    void setInstrumentStepSeq(int instrument, int index);

    void cleanCurrentState() {
        // Clean possibly not null value
        tmpStepValue_.full = 0l;
        stepNumberOfNotesOn_ = 0;
    }


private:
    void processActionBetwen(int instrument, uint16_t startTimer, uint16_t endTimer);
    void resyncNextAction(int instrument, uint16_t newInstrumentTimer);
    void loadStateVersion1(uint8_t* buffer);
    void loadStateVersion2(uint8_t* buffer);
    bool createNewNoteIfNeeded(int instrument, int stepCursor, int stepSize);
    char sequenceName_[13];
    uint16_t lastFreeAction_;
    Synth * synth_;
    FMDisplaySequencer* displaySequencer_;

    bool externalClock_;
    float tempo_;
    float oneBitInMillis_;
    float millisInBits_;
    uint32_t millisTimer_;
    float midiClockTimer_;
    uint16_t current16bitTimer_;
    uint16_t previousCurrent16bitTimer_;
    uint32_t sequenceDuration_;
    bool running_;
    bool extMidiRunning_;
    float precount_;
    uint16_t lastBeat_;
    uint32_t ledTimer_;

    // Send midi tick and songPosition_ to synth
    // to allow LFO and arpeggiator sync
    float midiTickCounter_;
    int midiTickCounterLastSent_;
    float midiTickCounterInc_;
    int songPosition_;

    // Step sequencer
    bool stepMode_;
    int stepNumberOfNotesOn_;

    // Per instrument
    uint16_t stepUniqueValue_[NUMBER_OF_TIMBRES];
    bool seqActivated_[NUMBER_OF_TIMBRES];
    // Per sequence
    bool stepActivated_[NUMBER_OF_STEP_SEQUENCES];
    uint16_t nextActionIndex_[NUMBER_OF_TIMBRES];
    uint16_t previousActionIndex_[NUMBER_OF_TIMBRES];
    bool nextActionTimerOutOfSync_[NUMBER_OF_TIMBRES];
    bool recording_[NUMBER_OF_TIMBRES];
    bool muted_[NUMBER_OF_TIMBRES];
    int8_t transpose_[NUMBER_OF_TIMBRES];
    //uint16_t nextLastActionTimer[NUMBER_OF_TIMBRES];;
    uint16_t instrumentTimerMask_[NUMBER_OF_TIMBRES];
    uint16_t lastInstrument16bitTimer_[NUMBER_OF_TIMBRES];
    uint8_t instrumentStepIndex_[NUMBER_OF_TIMBRES];
    uint8_t instrumentStepLastUnique_[NUMBER_OF_TIMBRES];
    // Each instrument points to its own step seq
    uint8_t instrumentStepSeq_[NUMBER_OF_TIMBRES];

    // Tmp step seq to store current pressed midi keys
    StepSeqValue tmpStepValue_;
};

#endif /* MIDI_SEQUENCER_H_ */
