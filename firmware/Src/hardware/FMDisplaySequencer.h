/*
 * Copyright 2019 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
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

#ifndef FMDISPLAY_SEQUENCER
#define FMDISPLAY_SEQUENCER

#include "Common.h"
#include "FMDisplay.h"

class SynthState;
class TftDisplay;
class Sequencer;

enum SequencerButtonMode {
    SEQ_MODE_NORMAL = 0,
    SEQ_MODE_REC,   // 1
    SEQ_MODE_MUTE,  // 2
    SEQ_MODE_CLEAR, // 3
    SEQ_MODE_STEP,   // 4
    SEQ_MOD_LAST // 5
};

class FMDisplaySequencer {
public:
    FMDisplaySequencer();

    void init(SynthState *synthState, TftDisplay *tft) {
        synthState_ = synthState;
        tft_ = tft;
    }
    void newTimbre(int instrument, int &refreshStatus, int &endRefreshStatus);
    void setSequencer(Sequencer *sequencer);
    void refreshSequencerByStep(int instrument, int &refreshStatus, int &endRefreshStatus);
    void displayBeat();
    void tempoClick();
    void noteOn(int instrument, bool show);
    void encoderTurned(int instrument, int encoder, int ticks);
    void buttonPressed(int instrument, int button);
    void buttonLongPressed(int instrument, int button);
    void* getValuePointer(int valueType, int encoder);
    void setRefreshStatusPointer(int *refreshStatus, int *endRefreshStatus) {
        refreshStatusP_ = refreshStatus;
        endRefreshStatusP_ = endRefreshStatus;
    }
    void refresh(int startRefreshStatus, int endRefreshStatus) {
        // if we're already refreshing we keep the endRefreshStatus
        int rs = *refreshStatusP_;
        int ers = *endRefreshStatusP_;
        if (unlikely(rs != 0)) {
            *endRefreshStatusP_ = MIN(ers, endRefreshStatus);
        } else {
            *endRefreshStatusP_ = endRefreshStatus;
        }
        *refreshStatusP_ = startRefreshStatus;
    }

    void refreshAllInstruments() {
        refresh(14, 7);
    }


    void refreshInstrument(int instrument) {
    	if(seqMode_ != SEQ_MODE_STEP) {
    		refresh(14 - instrument, 14 - instrument);
    	}
    }

    void refreshActivated() {
        refresh(15, 15);
    }

    void refreshMemory() {
        refresh(18, 18);
    }

    void refreshStepSeq() {
        refresh(16, 0);
    }

    void refreshStepCursor() {
        refresh(13, 13);
    }

    void refreshPlayButton();

    void newNoteEntered(int instrument);

    SequencerButtonMode getSequencerMode() {
        return seqMode_;
    }

    const char* getSequenceName();

    void cleanCurrentState();

    void sequencerWasUpdated(uint8_t timbre, uint8_t seqValue, uint8_t newValue);

private:
    void refreshStepSequencerByStep(int instrument, int &refreshStatus, int &endRefreshStatus);
    bool refreshSequence(int instrument);
    void clearSequence(int start, int end);
    void newNoteInSequence(int instrument, int start, int end, bool moreThanOneNote);

    TftDisplay *tft_;
    SynthState *synthState_;
    uint8_t valueChangedCounter_[NUMBER_OF_ENCODERS];
    Sequencer *sequencer_;
    int *refreshStatusP_;
    int *endRefreshStatusP_;
    SequencerButtonMode seqMode_;
    int stepCursor_;
    int stepSize_;
    int stepRedrawSequenceIndex_;
    int stepCurrentInstrument_;
};

#endif
