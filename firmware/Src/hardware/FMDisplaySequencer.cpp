/*
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

#include "FMDisplaySequencer.h"
#include "SynthState.h"
#include "TftDisplay.h"
#include "TftAlgo.h"
#include "Sequencer.h"
#include "preenfm3.h"

#define Y_START_SEQ  72

#define X_START_STEP   28
#define Y_START_STEP  (Y_START_SEQ + 55)
#define Y_HEIGHT 30
#define Y_STEP_LEVEL_METTER 252

const char* buttonLabel[SEQ_MOD_LAST]= {
        "Seq", "Rec", "Play", "Clear", "Step"
};

FMDisplaySequencer::FMDisplaySequencer() {
    seqMode = SEQ_MODE_NORMAL;
    stepSize = 16;
}

void FMDisplaySequencer::setSequencer(Sequencer* sequencer) {
    this->sequencer = sequencer;
}


void FMDisplaySequencer::newTimbre(int instrument, int &refreshStatus, int &endRefreshStatus) {
    if (seqMode == SEQ_MODE_STEP) {
        refreshStatus = 16;
        endRefreshStatus = 13;
    } else {
        refreshStatus = 14;
        endRefreshStatus = 9;    }

}

void FMDisplaySequencer::refreshSequencerByStep(int instrument, int &refreshStatus, int &endRefreshStatus) {

    if (refreshStatus <= 16 && seqMode == SEQ_MODE_STEP) {
        refreshStepSequencerByStep(instrument, refreshStatus, endRefreshStatus);
        return;
    }

    switch (refreshStatus) {
    case 20:
        tft->pauseRefresh();
        tft->clear();
        tft->fillArea(0, 0, 240, 21, COLOR_DARK_YELLOW);
        tft->setCursorInPixel(2, 2);
        tft->print("SEQ ", COLOR_YELLOW, COLOR_DARK_YELLOW);
        tft->print(sequencer->getSequenceName(), COLOR_WHITE, COLOR_DARK_YELLOW);

        tft->fillArea(0, Y_START_SEQ, 240, 2, COLOR_DARK_YELLOW);
        tft->fillArea(0, Y_START_SEQ + 168, 240, 2, COLOR_DARK_YELLOW);

        tft->fillArea(0, Y_START_SEQ + 2, 2, 166, COLOR_DARK_YELLOW);
        tft->fillArea(238, Y_START_SEQ + 2, 2, 166, COLOR_DARK_YELLOW);

        break;
    case 19:
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCharColor(COLOR_WHITE);

        tft->setCursor(2, 2);
        if (sequencer->isExternalClockEnabled()) {
            tft->print("Ext");
            tft->setCursor(6, 2);
            tft->print("   ");
        } else {
            tft->print("Int");
            tft->setCursor(6, 2);
            tft->print(sequencer->getTempo());
            if (sequencer->getTempo() < 100) {
                tft->print(' ');
            }
        }
        break;
    case 18: {
        int mem = sequencer->getMemory();
        tft->setCharBackgroundColor(COLOR_BLACK);
        if (mem < 90) {
            tft->setCharColor(COLOR_GRAY);
        } else {
            tft->setCharColor(COLOR_RED);
        }
        tft->setCursor(18, 2);
        if (mem < 10) {
            tft->print(' ');
        }
        tft->print(mem);
        tft->print('%');
        break;
    }
    case 17:
        displayBeat();
        break;
    case 16:
        tft->fillArea(2, Y_START_SEQ + 2, 236, 166, COLOR_BLACK);
        break;
    case 15:
        tft->setCharBackgroundColor(COLOR_BLACK);

        for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
            tft->setCursorInPixel(44, Y_START_SEQ + 7 + i * 27);

            if (sequencer->isSeqActivated(i) || sequencer->isStepActivated(i)) {
                if (!sequencer->isSeqActivated(i)) {
                    tft->setCharColor(COLOR_CYAN);
                } else if (!sequencer->isStepActivated(i)) {
                    tft->setCharColor(COLOR_GREEN);
                } else {
                    tft->setCharColor(COLOR_RED);
                }
                tft->print((char)146);
            } else {
                tft->print(' ');
            }
        }
        break;
    case 14:
    case 13:
    case 12:
    case 11:
    case 10:
    case 9:{
        uint8_t inst = 14 - refreshStatus;

        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCursorInPixel(15, Y_START_SEQ + 7 + inst * 27);
        if (inst == instrument) {
            tft->setCharColor(COLOR_YELLOW);
            tft->print('>');
        } else {
            tft->setCharColor(COLOR_WHITE);
            tft->print(' ');
        }
        tft->setCursorInPixel(30, Y_START_SEQ + 7 + inst * 27);
        tft->print(inst + 1);

        tft->setCursorInPixel(60, Y_START_SEQ + 7 + inst * 27);
        if (!tft->print(synthState->getTimbreName(inst), 6)) {
            tft->print('.');
        }

        tft->setCursorInPixel(160, Y_START_SEQ + 7 + inst * 27);
        tft->setCharColor(sequencer->isRecording(inst) ? COLOR_RED: COLOR_GRAY);
        tft->print((char)145);

        tft->setCursorInPixel(185, Y_START_SEQ + 7 + inst * 27);
        tft->setCharColor(sequencer->isMuted(inst) ? COLOR_GRAY: COLOR_GREEN);
        tft->print((char)144);

        tft->setCharColor(COLOR_WHITE);
        tft->setCursorInPixel(210, Y_START_SEQ + 7 + inst * 27);
        tft->print(sequencer->getNumberOfBars(inst));


        break;
    }
    case 7 :
        tft->fillArea(100, 247, 55, 20, COLOR_BLACK);
        if (seqMode != SEQ_MODE_NORMAL) {
            tft->setCharColor(COLOR_WHITE);
            tft->setCursorInPixel(100, 247);
            tft->print(buttonLabel[seqMode]);
        }
        break;
    case 6:
        if (seqMode == SEQ_MODE_NORMAL) {
            tft->drawSimpleButton("", 270, 29, 0, COLOR_GREEN, COLOR_DARK_YELLOW);
            tft->setCharBackgroundColor(COLOR_DARK_YELLOW);
            tft->setCursorInPixel(24, 271);
            tft->setCharColor(COLOR_GREEN);
            tft->print((char)0x90);
            tft->print(' ');
            tft->setCharColor(COLOR_RED);
            tft->print((char)0x91);
            tft->setCharBackgroundColor(COLOR_BLACK);
        } else {
            tft->drawSimpleButton("1", 270, 29, 0, COLOR_WHITE, COLOR_DARK_YELLOW);
        }
        break;
    case 5:
        if (seqMode == SEQ_MODE_NORMAL) {
            tft->drawSimpleButton("", 270, 29, 1, COLOR_GREEN, COLOR_DARK_YELLOW);
        } else {
            tft->drawSimpleButton("2", 270, 29, 1, COLOR_WHITE, COLOR_DARK_YELLOW);
        }
        break;
    case 4:
        if (seqMode == SEQ_MODE_NORMAL) {
            tft->drawSimpleButton("Step>", 270, 29, 2, COLOR_WHITE, COLOR_DARK_YELLOW);
        } else {
            tft->drawSimpleButton("3", 270, 29, 2, COLOR_WHITE, COLOR_DARK_YELLOW);
        }
        break;
    case 3:
        if (seqMode == SEQ_MODE_NORMAL) {
            tft->drawSimpleButton("Clear", 270, 29, 3, COLOR_WHITE, COLOR_DARK_YELLOW);
        } else {
            tft->drawSimpleButton("4", 270, 29, 3, COLOR_WHITE, COLOR_DARK_YELLOW);
        }
        break;
    case 2:
        if (seqMode == SEQ_MODE_NORMAL) {
            tft->drawSimpleButton(" ", 270, 29, 4, COLOR_WHITE, COLOR_DARK_YELLOW);
        } else {
            tft->drawSimpleButton("5", 270, 29, 4, COLOR_WHITE, COLOR_DARK_YELLOW);
        }
        break;
    case 1:
        if (seqMode == SEQ_MODE_NORMAL) {
            refreshPlayButton();
        } else {
            tft->drawSimpleButton("6", 270, 29, 5, COLOR_WHITE, COLOR_DARK_YELLOW);
        }
        break;
    }

    if (refreshStatus == endRefreshStatus) {
        endRefreshStatus = 0;
        refreshStatus = 0;
    }
}


void FMDisplaySequencer::refreshStepSequencerByStep(int instrument, int &refreshStatus, int &endRefreshStatus) {
    switch (refreshStatus) {
    case 16: {
        tft->pauseRefresh();
        tft->fillArea(2, Y_START_SEQ + 2, 236, 166, COLOR_BLACK);
        int numberOfBars = sequencer->getNumberOfBars(instrument);
        for (int m = 0; m < 4; m++) {

            uint8_t x = X_START_STEP;
            uint16_t y = Y_START_STEP + m * Y_HEIGHT;
            uint8_t color = m < numberOfBars ? COLOR_WHITE : COLOR_GRAY;
            tft->fillArea(X_START_STEP, y, 194, 2, color);

            //tft->fillArea((uint8_t)(xStart + 200), y - 6, 2, 6, COLOR_WHITE);
            for (int b = 0; b < 4 ; b++) {
                int x2 = x + b * 48;
                tft->fillArea(x2, y + 2, 2, 4, color);
            }
        }
        break;
    }
    case 15: {

        tft->setCharColor(COLOR_YELLOW);
        tft->setCharBackgroundColor(COLOR_BLACK);
        tft->setCursorInPixel(X_START_STEP, Y_START_SEQ + 10);
        tft->print(instrument + 1);
        // Let's store current instrument here (when it's displayed)
        stepCurrentInstrument = instrument;
        tft->setCharColor(COLOR_CYAN);
        int seqNumber = sequencer->getInstrumentStepSeq(instrument) + 1;
        tft->setCursorInPixel(200 + (seqNumber < 10 ? TFT_BIG_CHAR_WIDTH : 0), Y_START_SEQ + 10);
        tft->print(seqNumber);

        stepRedrawSequenceIndex = 0;
        break;
    }
    case 14: {
        // We divide the sequence drawing
        // because it creates up to 512 entries in the tft action buffer
        if (!refreshSequence(instrument)) {
            // If not finish, come back here
            refreshStatus ++;
        }
        break;
    }
    case 13: {
        for (int m = 0; m < 4; m++) {
            tft->fillArea(X_START_STEP, Y_START_STEP + m * Y_HEIGHT - 15, 194, 4, COLOR_BLACK);
        }
        int x = X_START_STEP + (stepCursor % 64) * 3 + 1;
        int y = Y_START_STEP + (stepCursor / 64) * Y_HEIGHT  ;
        // Draw cursor
        tft->fillArea(x, y - 15, stepSize * 3, 4, COLOR_YELLOW);
        break;
    }
    case 6:
        tft->drawSimpleButton("_" , 270, 29, 0, COLOR_WHITE, COLOR_DARK_YELLOW);
        break;
    case 5:
        tft->drawSimpleButton(" ", 270, 29, 1, COLOR_WHITE, COLOR_DARK_YELLOW);
        break;
    case 4:
        tft->drawSimpleButton("<Seq", 270, 29, 2, COLOR_WHITE, COLOR_DARK_YELLOW);
        break;
    case 3:
        tft->drawSimpleButton("Clear", 270, 29, 3, COLOR_WHITE, COLOR_DARK_YELLOW);
        break;
    case 2:
        tft->drawSimpleButton(" ", 270, 29, 4, COLOR_WHITE, COLOR_DARK_YELLOW);
        break;
    case 1:
        refreshPlayButton();
        break;
    }

    if (refreshStatus == endRefreshStatus) {
        endRefreshStatus = 0;
        refreshStatus = 0;
    }
}

void FMDisplaySequencer::refreshPlayButton() {
    tft->drawSimpleButton("\x93", 270, 29, 5,
        sequencer->isRunning() ? COLOR_BLACK : COLOR_LIGHT_GRAY,
        sequencer->isRunning() ? COLOR_YELLOW : COLOR_DARK_YELLOW);
}

// stepRedrawSequenceIndex must have been initialized before the first call
bool FMDisplaySequencer::refreshSequence(int instrument) {
    StepSeqValue* sequence = sequencer->stepGetSequence(instrument);
    int numberOfBlocks = 0;
    int numberOfBars = sequencer->getNumberOfBars(instrument);
    while (stepRedrawSequenceIndex < 256 && numberOfBlocks < 10) {
        if ((sequence[stepRedrawSequenceIndex].full & 0xffffff00) == 0l) {
            stepRedrawSequenceIndex++;
        } else {
            int startBlock = stepRedrawSequenceIndex;
            bool moreThanOneNote = (sequence[stepRedrawSequenceIndex].values[4] > 0);
            int size = 0;
            uint16_t unique = sequence[startBlock].unique;
            while (sequence[stepRedrawSequenceIndex].unique == unique && stepRedrawSequenceIndex < 256 ) {
                size += 3;
                stepRedrawSequenceIndex++;
            }
            size -=2;

            int x = X_START_STEP + (startBlock % 64) * 3 + 1;
            int y = Y_START_STEP + (startBlock / 64) * Y_HEIGHT  ;

            // Draw block
            if ((startBlock / 64) < numberOfBars) {
            	tft->fillArea(x, y - 10, size, 10, moreThanOneNote ? COLOR_CYAN : COLOR_BLUE);
            } else {
            	tft->fillArea(x, y - 10, size, 10, COLOR_GRAY);
            }
            numberOfBlocks ++ ;
        }
    }
    if (stepRedrawSequenceIndex == 256) {
        // finished
        return true;
    } else {
        return false;
    }
}

void FMDisplaySequencer::newNoteInSequence(int instrument, int start, int end, bool moreThanOneNote) {
    int x = X_START_STEP + (start % 64) * 3 + 1;
    int y = Y_START_STEP + (start / 64) * Y_HEIGHT  ;
    int size = (end - start) * 3 - 2;
    int numberOfBars = sequencer->getNumberOfBars(instrument);

    if ((start / 64) < numberOfBars) {
        tft->fillArea(x, y - 10, size, 10, moreThanOneNote ? COLOR_CYAN : COLOR_BLUE);
    } else {
        tft->fillArea(x, y - 10, size, 10, COLOR_GRAY);
    }

    tft->fillArea(x + size, y - 10, 2, 10, COLOR_BLACK);
    tft->fillArea(x -2, y - 10, 2, 10, COLOR_BLACK);
}

void FMDisplaySequencer::clearSequence(int start, int end) {
    int x = X_START_STEP + (start % 64) * 3;
    int y = Y_START_STEP + (start / 64) * Y_HEIGHT  ;
    int size = (end - start) * 3;

    tft->fillArea(x, y - 10, size, 10, COLOR_BLACK);
}

void FMDisplaySequencer::noteOn(int instrument, bool show) {
    if (likely(seqMode != SEQ_MODE_STEP)) {
        tft->setCursorInPixel(160, Y_START_SEQ + 7 + instrument * 27);
        if (sequencer->isRecording(instrument)) {
            tft->setCharColor(COLOR_RED);
        } else {
            tft->setCharColor(COLOR_GRAY);
        }
        tft->print((char)(145 + (show ? 1 : 0)));
    }
}

void FMDisplaySequencer::displayBeat() {
    if (unlikely(synthState->fullState.synthMode == SYNTH_MODE_SEQUENCER)) {
        float precount = sequencer->getPrecount();
        uint8_t measure;
        uint8_t beat ;
        tft->setCharBackgroundColor(COLOR_BLACK);
        if (precount > 0) {
            tft->setCharColor(COLOR_RED);
            measure = precount / 1024;
            beat = (precount - measure * 1024) / 256;
            tft->fillArea(0,              62, 240, 4, COLOR_DARK_GRAY);
            tft->fillArea(180- beat * 60, 62,  60, 4, COLOR_RED);
        } else {
            measure = sequencer->getMeasure();
            beat = sequencer->getBeat();

            tft->fillArea(0,               62, 240, 4, COLOR_DARK_GRAY);
            tft->fillArea((beat - 1) * 60, 62, 60, 4, COLOR_BLUE);
            tft->fillArea(measure * 60,    62, 20, 4, COLOR_CYAN);

            tft->setCharBackgroundColor(COLOR_BLACK);
            if (sequencer->isRunning()) {
                tft->setCharColor(COLOR_WHITE);
            } else {
                tft->setCharColor(COLOR_GRAY);
            }
        }
        tft->setCursor(11, 2);
        tft->print(measure + 1);
        tft->print(':');
        tft->print(beat);
    }
}

void FMDisplaySequencer::encoderTurned(int instrument, int encoder, int ticks) {
    switch (encoder) {
    case 0: {
        bool isExternalClockEnabled = sequencer->isExternalClockEnabled();
        bool newIsExternalClockEnabled = ticks < 0;
        if (isExternalClockEnabled != newIsExternalClockEnabled) {
            sequencer->setExternalClock(newIsExternalClockEnabled);
            refresh(19, 0);
        }
    }
    break;
    case 1: {
        if (!sequencer->isExternalClockEnabled()) {
            int tempo = sequencer->getTempo();
            tempo += ticks;
            if (tempo < 10) {
                tempo = 10;
            }
            if (tempo > 255) {
                tempo = 255;
            }
            sequencer->setTempo((float)tempo);
            refresh(19, 19);
        }
    }
    break;
    // encoder 2 change the current instrument
    // and is managed in synthstate.cpp
    case 3: {
        if (seqMode == SEQ_MODE_STEP) {

            stepCursor += stepSize * ticks;
            while (stepCursor < 0) {
                stepCursor += 256;
            }
            while (stepCursor > 255) {
                stepCursor -= 256;
            }
            refreshStepCursor();
        } else {
            bool isRecording = sequencer->isRecording(instrument);
            bool newIsRecording = ticks > 0;
            if (isRecording != newIsRecording) {
                sequencer->setRecording(instrument, newIsRecording);
                refreshInstrument(instrument);
            }
        }
    }
    break;
    case 4:
        if (seqMode == SEQ_MODE_STEP) {
            if (ticks > 0) {
                stepSize *= 2;
                if (stepSize > 64) {
                    stepSize = 64;
                } else {
                    // Move to beginnig of stepSize
                    stepCursor = stepCursor / stepSize * stepSize;
                    refreshStepCursor();
                }
            } else {
                stepSize /= 2;
                if (stepSize == 0) {
                    stepSize = 1;
                } else {
                    refreshStepCursor();
                }
            }
        } else {
            bool isMuted = sequencer->isMuted(instrument);
            bool newIsMuted = ticks < 0;
            if (isMuted != newIsMuted) {
                sequencer->setMuted(instrument, newIsMuted);
                refreshInstrument(instrument);
            }
        }
    break;
    case 5:
        if (seqMode == SEQ_MODE_STEP) {
			int index = sequencer->getInstrumentStepSeq(instrument) + (ticks > 0 ? 1 : -1);
			if (index < 0) {
				index = 0;
			} else if (index >= NUMBER_OF_STEP_SEQUENCES) {
				index = NUMBER_OF_STEP_SEQUENCES - 1;
			}
			if (index != sequencer->getInstrumentStepSeq(instrument)) {
				sequencer->setInstrumentStepSeq(instrument, index);
				tft->pauseRefresh();
				this->refresh(16, 13);
			}
        } else {
            int bars = sequencer->getNumberOfBars(instrument);
            int newBars = bars + (ticks > 0 ? 1 : -1);
            newBars = newBars < 1 ? 1 : newBars;
            newBars = newBars > 4 ? 4 : newBars;
            // We don't want 3
            if (newBars == 3) {
                if (bars == 2) {
                    newBars = 4;
                } else {
                    newBars = 2;
                }
            }

            if (newBars != bars) {
                sequencer->setNumberOfBars(instrument, newBars);
            }
            refreshInstrument(instrument);
        }
    break;
    }
}


void FMDisplaySequencer::buttonLongPressed(int instrument, int button) {
    // Action for all modes
    switch (button)  {
    case BUTTON_PFM3_6:
        sequencer->stop();
        sequencer->rewind();
        refresh(17, 17);
        refreshPlayButton();
        break;
    }

    if (seqMode == SEQ_MODE_NORMAL) {
        switch (button)  {
        case BUTTON_PFM3_1:
            // Long press on button 1 : record select
            seqMode = SEQ_MODE_REC;
            refresh(7, 1);
            break;
        case BUTTON_PFM3_4:
            // Long press on button 4 : Clear sequencer
            sequencer->stop();
            sequencer->reset(true);
            refresh(19,0);
            break;
        }
    } else if (seqMode == SEQ_MODE_STEP) {
        switch (button)  {
        case BUTTON_PFM3_4:
            // Long press on button 4 : Clear step sequencer
            stepCursor = 0;
            sequencer->stepClearAll(instrument);
            refreshStepSeq();
            break;
        }
    }

}

void FMDisplaySequencer::buttonPressed(int instrument, int button) {
    // Action for SEQ and STEP mode
    if (seqMode == SEQ_MODE_NORMAL || seqMode == SEQ_MODE_STEP) {
        switch (button) {
        case BUTTON_PFM3_6:
            if (sequencer->isRunning()) {
                sequencer->stop();
            } else {
                sequencer->start();
            }
            refreshPlayButton();
            refresh(17, 17);
            return;
        }
    }

    if (seqMode == SEQ_MODE_NORMAL) {
        switch (button) {
        case BUTTON_PFM3_1:
            seqMode = SEQ_MODE_MUTE;
            refresh(7, 1);
            break;
        case BUTTON_PFM3_2:
            break;
        case BUTTON_PFM3_3:
            seqMode = SEQ_MODE_STEP;
            sequencer->setStepMode(true);
            refresh(19, 0);
            break;
        case BUTTON_PFM3_4:
            seqMode = SEQ_MODE_CLEAR;
            refresh(7, 1);
            break;
        case BUTTON_PFM3_5:
            break;
        }
    } else {
        if (button == BUTTON_PFM3_MENU) {
            if (seqMode != SEQ_MODE_STEP) {
                refresh(7, 0);
                seqMode = SEQ_MODE_NORMAL;
                return;
            }
        }
        if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6) {
            int actionInstrument = button - BUTTON_PFM3_1;
            if (seqMode == SEQ_MODE_MUTE) {
                sequencer->toggleMuted(actionInstrument);
                refreshInstrument(actionInstrument);
            } else if (seqMode == SEQ_MODE_REC) {
                sequencer->toggleRecording(actionInstrument);
                refreshInstrument(actionInstrument);
            } else if (seqMode == SEQ_MODE_CLEAR) {
                sequencer->clear(actionInstrument);
                // Refresh memory and activated
                refresh(18, 0);
            } else if (seqMode == SEQ_MODE_STEP) {
                switch (button) {
                case BUTTON_PFM3_1:
                    sequencer->stepClearPart(instrument, stepCursor, stepSize);
                    clearSequence(stepCursor, stepCursor + stepSize);
                    stepCursor += stepSize;
                    if (stepCursor > 255) {
                        stepCursor -= 256;
                    }
                    refreshStepCursor();
                    break;
                case BUTTON_PFM3_3:
                    seqMode = SEQ_MODE_NORMAL;
                    sequencer->setStepMode(false);
                    refresh(19, 0);
                    break;
                }
            }
        }

    }

}


void FMDisplaySequencer::newNoteEntered(int instrumentNO) {
	// We use current instrument and not the one we receive
    bool moreThanOneNote = sequencer->stepRecordNotes(stepCurrentInstrument, stepCursor, stepSize);
    newNoteInSequence(stepCurrentInstrument, stepCursor, stepCursor + stepSize, moreThanOneNote);
    stepCursor += stepSize;
    if (stepCursor > 255) {
        stepCursor -= 256;
    }
    refreshStepCursor();
}

const char* FMDisplaySequencer::getSequenceName() {
	return sequencer->getSequenceName();
}


void FMDisplaySequencer::tempoClick() {

    static float lastVolume[NUMBER_OF_TIMBRES];
    static float lastGainReduction[NUMBER_OF_TIMBRES];


    if (seqMode != SEQ_MODE_STEP) {
        for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {

            if (this->synthState->mixerState.instrumentState[timbre].numberOfVoices > 0) {
                float volume = getCompInstrumentVolume(timbre);
                float gr = getCompInstrumentGainReduction(timbre);
                // gr tend to slower reach 0
                // Let'as accelerate when we don't want to draw the metter anymore
                if (gr > -.1f) {
                    gr = 0;
                }
                if ((volume != lastVolume[timbre] || gr != lastGainReduction[timbre])) {

                    lastVolume[timbre] = volume;
                    lastGainReduction[timbre] = gr;

                    bool isCompressed = synthState->mixerState.instrumentState[timbre].compressorType > 0;
                    tft->drawLevelMetter(30, Y_START_SEQ + 7 + timbre * 27 + 20,
                        208, 3, 5, volume, isCompressed, gr);
                }
            }
        }
    }
    // Not sure it's a good idea to add level metter in the step seq
    //    else {
    //        if (this->synthState->mixerState.instrumentState[stepCurrentInstrument].numberOfVoices > 0) {
    //            float volume = getCompInstrumentVolume(stepCurrentInstrument);
    //            float gr = getCompInstrumentGainReduction(stepCurrentInstrument);
    //            // gr tend to slower reach 0
    //            // Let'as accelerate when we don't want to draw the metter anymore
    //            if (gr > -.1f) {
    //                gr = 0;
    //            }
    //            if ((volume != lastVolume[stepCurrentInstrument] || gr != lastGainReduction[stepCurrentInstrument])) {
    //
    //                lastVolume[stepCurrentInstrument] = volume;
    //                lastGainReduction[stepCurrentInstrument] = gr;
    //
    //                bool isCompressed = synthState->mixerState.instrumentState[stepCurrentInstrument].compressorType > 0;
    //                tft->drawLevelMetter(5, Y_STEP_LEVEL_METTER,
    //                    235, 3, 5, volume, isCompressed, gr);
    //            }
    //        }
    //    }
}
