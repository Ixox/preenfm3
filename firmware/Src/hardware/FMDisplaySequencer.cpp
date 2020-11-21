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

const char *buttonLabel[SEQ_MOD_LAST] = { "Seq", "Rec", "Play", "Clear", "Step" };

FMDisplaySequencer::FMDisplaySequencer() {
    seqMode_ = SEQ_MODE_NORMAL;
    stepSize_ = 16;
}

void FMDisplaySequencer::setSequencer(Sequencer *sequencer) {
    sequencer_ = sequencer;
}

void FMDisplaySequencer::newTimbre(int instrument, int &refreshStatus, int &endRefreshStatus) {
    if (seqMode_ == SEQ_MODE_STEP) {
        refreshStatus = 16;
        endRefreshStatus = 13;
    } else {
        refreshStatus = 14;
        endRefreshStatus = 9;
    }

}

void FMDisplaySequencer::refreshSequencerByStep(int instrument, int &refreshStatus, int &endRefreshStatus) {

    if (refreshStatus <= 16 && seqMode_ == SEQ_MODE_STEP) {
        refreshStepSequencerByStep(instrument, refreshStatus, endRefreshStatus);
        return;
    }

    switch (refreshStatus) {
        case 20:
            tft_->pauseRefresh();
            tft_->clear();
            tft_->fillArea(0, 0, 240, 21, COLOR_DARK_YELLOW);
            tft_->setCursorInPixel(2, 2);
            tft_->print("SEQ ", COLOR_YELLOW, COLOR_DARK_YELLOW);
            tft_->print(sequencer_->getSequenceName(), COLOR_WHITE, COLOR_DARK_YELLOW);

            tft_->fillArea(0, Y_START_SEQ, 240, 2, COLOR_DARK_YELLOW);
            tft_->fillArea(0, Y_START_SEQ + 168, 240, 2, COLOR_DARK_YELLOW);

            tft_->fillArea(0, Y_START_SEQ + 2, 2, 166, COLOR_DARK_YELLOW);
            tft_->fillArea(238, Y_START_SEQ + 2, 2, 166, COLOR_DARK_YELLOW);

            break;
        case 19:
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCharColor(COLOR_WHITE);

            tft_->setCursor(2, 2);
            if (sequencer_->isExternalClockEnabled()) {
                tft_->print("Ext");
                tft_->setCursor(6, 2);
                tft_->print("   ");
            } else {
                tft_->print("Int");
                tft_->setCursor(6, 2);
                tft_->print(sequencer_->getTempo());
                if (sequencer_->getTempo() < 100) {
                    tft_->print(' ');
                }
            }
            break;
        case 18: {
            int mem = sequencer_->getMemory();
            tft_->setCharBackgroundColor(COLOR_BLACK);
            if (mem < 90) {
                tft_->setCharColor(COLOR_GRAY);
            } else {
                tft_->setCharColor(COLOR_RED);
            }
            tft_->setCursor(18, 2);
            if (mem < 10) {
                tft_->print(' ');
            }
            tft_->print(mem);
            tft_->print('%');
            break;
        }
        case 17:
            displayBeat();
            break;
        case 16:
            tft_->fillArea(2, Y_START_SEQ + 2, 236, 166, COLOR_BLACK);
            break;
        case 15:
            tft_->setCharBackgroundColor(COLOR_BLACK);

            for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
                tft_->setCursorInPixel(8, Y_START_SEQ + 7 + i * 27);

                if (sequencer_->isSeqActivated(i) || sequencer_->isStepActivated(i)) {
                    if (!sequencer_->isSeqActivated(i)) {
                        tft_->setCharColor(COLOR_CYAN);
                    } else if (!sequencer_->isStepActivated(i)) {
                        tft_->setCharColor(COLOR_GREEN);
                    } else {
                        tft_->setCharColor(COLOR_RED);
                    }
                    tft_->print((char) 146);
                } else {
                    tft_->print(' ');
                }
            }
            break;
        case 14:
        case 13:
        case 12:
        case 11:
        case 10:
        case 9: {
            uint8_t inst = 14 - refreshStatus;

            tft_->setCharBackgroundColor(COLOR_BLACK);
            if (inst == instrument) {
                tft_->setCharColor(COLOR_YELLOW);
            } else {
                tft_->setCharColor(COLOR_LIGHT_GRAY);
            }
            tft_->setCursorInPixel(26, Y_START_SEQ + 7 + inst * 27);
            tft_->print(inst + 1);

            tft_->setCursorInPixel(50, Y_START_SEQ + 7 + inst * 27);
            tft_->print(synthState_->getTimbreName(inst), 8);

            tft_->setCursorInPixel(160, Y_START_SEQ + 7 + inst * 27);
            tft_->setCharColor(sequencer_->isRecording(inst) ? COLOR_RED : COLOR_GRAY);
            tft_->print((char) 145);

            tft_->setCursorInPixel(185, Y_START_SEQ + 7 + inst * 27);
            tft_->setCharColor(sequencer_->isMuted(inst) ? COLOR_GRAY : COLOR_GREEN);
            tft_->print((char) 144);

            tft_->setCharColor(COLOR_WHITE);
            tft_->setCursorInPixel(210, Y_START_SEQ + 7 + inst * 27);
            tft_->print(sequencer_->getNumberOfBars(inst));

            break;
        }
        case 7:
            tft_->fillArea(100, 247, 55, 20, COLOR_BLACK);
            if (seqMode_ != SEQ_MODE_NORMAL) {
                tft_->setCharColor(COLOR_WHITE);
                tft_->setCursorInPixel(100, 247);
                tft_->print(buttonLabel[seqMode_]);
            }
            break;
        case 6:
            if (seqMode_ == SEQ_MODE_NORMAL) {
                tft_->drawSimpleButton("", 270, 29, 0, COLOR_GREEN, COLOR_DARK_YELLOW);
                tft_->setCharBackgroundColor(COLOR_DARK_YELLOW);
                tft_->setCursorInPixel(24, 271);
                tft_->setCharColor(COLOR_GREEN);
                tft_->print((char) 0x90);
                tft_->print(' ');
                tft_->setCharColor(COLOR_RED);
                tft_->print((char) 0x91);
                tft_->setCharBackgroundColor(COLOR_BLACK);
            } else {
                tft_->drawSimpleButton("1", 270, 29, 0, COLOR_WHITE, COLOR_DARK_YELLOW);
            }
            break;
        case 5:
            if (seqMode_ == SEQ_MODE_NORMAL) {
                tft_->drawSimpleButton("", 270, 29, 1, COLOR_GREEN, COLOR_DARK_YELLOW);
            } else {
                tft_->drawSimpleButton("2", 270, 29, 1, COLOR_WHITE, COLOR_DARK_YELLOW);
            }
            break;
        case 4:
            if (seqMode_ == SEQ_MODE_NORMAL) {
                tft_->drawSimpleButton("Step>", 270, 29, 2, COLOR_WHITE, COLOR_DARK_YELLOW);
            } else {
                tft_->drawSimpleButton("3", 270, 29, 2, COLOR_WHITE, COLOR_DARK_YELLOW);
            }
            break;
        case 3:
            if (seqMode_ == SEQ_MODE_NORMAL) {
                tft_->drawSimpleButton("Clear", 270, 29, 3, COLOR_WHITE, COLOR_DARK_YELLOW);
            } else {
                tft_->drawSimpleButton("4", 270, 29, 3, COLOR_WHITE, COLOR_DARK_YELLOW);
            }
            break;
        case 2:
            if (seqMode_ == SEQ_MODE_NORMAL) {
                tft_->drawSimpleButton(" ", 270, 29, 4, COLOR_WHITE, COLOR_DARK_YELLOW);
            } else {
                tft_->drawSimpleButton("5", 270, 29, 4, COLOR_WHITE, COLOR_DARK_YELLOW);
            }
            break;
        case 1:
            if (seqMode_ == SEQ_MODE_NORMAL) {
                refreshPlayButton();
            } else {
                tft_->drawSimpleButton("6", 270, 29, 5, COLOR_WHITE, COLOR_DARK_YELLOW);
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
            tft_->pauseRefresh();
            tft_->fillArea(2, Y_START_SEQ + 2, 236, 166, COLOR_BLACK);
            int numberOfBars = sequencer_->getNumberOfBars(instrument);
            for (int m = 0; m < 4; m++) {

                uint8_t x = X_START_STEP;
                uint16_t y = Y_START_STEP + m * Y_HEIGHT;
                uint8_t color = m < numberOfBars ? COLOR_WHITE : COLOR_GRAY;
                tft_->fillArea(X_START_STEP, y, 194, 2, color);

                //tft->fillArea((uint8_t)(xStart + 200), y - 6, 2, 6, COLOR_WHITE);
                for (int b = 0; b < 4; b++) {
                    int x2 = x + b * 48;
                    tft_->fillArea(x2, y + 2, 2, 4, color);
                }
            }
            break;
        }
        case 15: {

            tft_->setCharColor(COLOR_YELLOW);
            tft_->setCharBackgroundColor(COLOR_BLACK);
            tft_->setCursorInPixel(X_START_STEP, Y_START_SEQ + 10);
            tft_->print(instrument + 1);
            // Let's store current instrument here (when it's displayed)
            stepCurrentInstrument_ = instrument;
            tft_->setCharColor(COLOR_CYAN);
            int seqNumber = sequencer_->getInstrumentStepSeq(instrument) + 1;
            tft_->setCursorInPixel(200 + (seqNumber < 10 ? TFT_BIG_CHAR_WIDTH : 0), Y_START_SEQ + 10);
            tft_->print(seqNumber);

            stepRedrawSequenceIndex_ = 0;
            break;
        }
        case 14: {
            // We divide the sequence drawing
            // because it creates up to 512 entries in the tft action buffer
            if (!refreshSequence(instrument)) {
                // If not finish, come back here
                refreshStatus++;
            }
            break;
        }
        case 13: {
            for (int m = 0; m < 4; m++) {
                tft_->fillArea(X_START_STEP, Y_START_STEP + m * Y_HEIGHT - 15, 194, 4, COLOR_BLACK);
            }
            int x = X_START_STEP + (stepCursor_ % 64) * 3 + 1;
            int y = Y_START_STEP + (stepCursor_ / 64) * Y_HEIGHT;
            // Draw cursor
            tft_->fillArea(x, y - 15, stepSize_ * 3, 4, COLOR_YELLOW);
            break;
        }
        case 6:
            tft_->drawSimpleButton("_", 270, 29, 0, COLOR_WHITE, COLOR_DARK_YELLOW);
            break;
        case 5:
            tft_->drawSimpleButton(" ", 270, 29, 1, COLOR_WHITE, COLOR_DARK_YELLOW);
            break;
        case 4:
            tft_->drawSimpleButton("<Seq", 270, 29, 2, COLOR_WHITE, COLOR_DARK_YELLOW);
            break;
        case 3:
            tft_->drawSimpleButton("Clear", 270, 29, 3, COLOR_WHITE, COLOR_DARK_YELLOW);
            break;
        case 2:
            tft_->drawSimpleButton(" ", 270, 29, 4, COLOR_WHITE, COLOR_DARK_YELLOW);
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
    tft_->drawSimpleButton("\x93", 270, 29, 5, sequencer_->isRunning() ? COLOR_BLACK : COLOR_LIGHT_GRAY,
        sequencer_->isRunning() ? COLOR_YELLOW : COLOR_DARK_YELLOW);
}

// stepRedrawSequenceIndex must have been initialized before the first call
bool FMDisplaySequencer::refreshSequence(int instrument) {
    StepSeqValue *sequence = sequencer_->stepGetSequence(instrument);
    int numberOfBlocks = 0;
    int numberOfBars = sequencer_->getNumberOfBars(instrument);
    while (stepRedrawSequenceIndex_ < 256 && numberOfBlocks < 10) {
        if ((sequence[stepRedrawSequenceIndex_].full & 0xffffff00) == 0l) {
            stepRedrawSequenceIndex_++;
        } else {
            int startBlock = stepRedrawSequenceIndex_;
            bool moreThanOneNote = (sequence[stepRedrawSequenceIndex_].values[4] > 0);
            int size = 0;
            uint16_t unique = sequence[startBlock].unique;
            while (sequence[stepRedrawSequenceIndex_].unique == unique && stepRedrawSequenceIndex_ < 256) {
                size += 3;
                stepRedrawSequenceIndex_++;
            }
            size -= 2;

            int x = X_START_STEP + (startBlock % 64) * 3 + 1;
            int y = Y_START_STEP + (startBlock / 64) * Y_HEIGHT;

            // Draw block
            if ((startBlock / 64) < numberOfBars) {
                tft_->fillArea(x, y - 10, size, 10, moreThanOneNote ? COLOR_CYAN : COLOR_BLUE);
            } else {
                tft_->fillArea(x, y - 10, size, 10, COLOR_GRAY);
            }
            numberOfBlocks++;
        }
    }
    if (stepRedrawSequenceIndex_ == 256) {
        // finished
        return true;
    } else {
        return false;
    }
}

void FMDisplaySequencer::newNoteInSequence(int instrument, int start, int end, bool moreThanOneNote) {
    int x = X_START_STEP + (start % 64) * 3 + 1;
    int y = Y_START_STEP + (start / 64) * Y_HEIGHT;
    int size = (end - start) * 3 - 2;
    int numberOfBars = sequencer_->getNumberOfBars(instrument);

    if ((start / 64) < numberOfBars) {
        tft_->fillArea(x, y - 10, size, 10, moreThanOneNote ? COLOR_CYAN : COLOR_BLUE);
    } else {
        tft_->fillArea(x, y - 10, size, 10, COLOR_GRAY);
    }

    tft_->fillArea(x + size, y - 10, 2, 10, COLOR_BLACK);
    tft_->fillArea(x - 2, y - 10, 2, 10, COLOR_BLACK);
}

void FMDisplaySequencer::clearSequence(int start, int end) {
    int x = X_START_STEP + (start % 64) * 3;
    int y = Y_START_STEP + (start / 64) * Y_HEIGHT;
    int size = (end - start) * 3;

    tft_->fillArea(x, y - 10, size, 10, COLOR_BLACK);
}

void FMDisplaySequencer::noteOn(int instrument, bool show) {
    if (likely(seqMode_ != SEQ_MODE_STEP)) {
        tft_->setCursorInPixel(160, Y_START_SEQ + 7 + instrument * 27);
        if (sequencer_->isRecording(instrument)) {
            tft_->setCharColor(COLOR_RED);
        } else {
            tft_->setCharColor(COLOR_GRAY);
        }
        tft_->print((char) (145 + (show ? 1 : 0)));
    }
}

void FMDisplaySequencer::displayBeat() {
    if (unlikely(synthState_->fullState.synthMode == SYNTH_MODE_SEQUENCER)) {
        float precount = sequencer_->getPrecount();
        uint8_t measure;
        uint8_t beat;
        tft_->setCharBackgroundColor(COLOR_BLACK);
        if (precount > 0) {
            tft_->setCharColor(COLOR_RED);
            measure = precount / 1024;
            beat = (precount - measure * 1024) / 256;
            tft_->fillArea(0, 62, 240, 4, COLOR_DARK_GRAY);
            tft_->fillArea(180 - beat * 60, 62, 60, 4, COLOR_RED);
        } else {
            measure = sequencer_->getMeasure();
            beat = sequencer_->getBeat();

            tft_->fillArea(0, 62, 240, 4, COLOR_DARK_GRAY);
            tft_->fillArea((beat - 1) * 60, 62, 60, 4, COLOR_BLUE);
            tft_->fillArea(measure * 60, 62, 20, 4, COLOR_CYAN);

            tft_->setCharBackgroundColor(COLOR_BLACK);
            if (sequencer_->isRunning()) {
                tft_->setCharColor(COLOR_WHITE);
            } else {
                tft_->setCharColor(COLOR_GRAY);
            }
        }
        tft_->setCursor(11, 2);
        tft_->print(measure + 1);
        tft_->print(':');
        tft_->print(beat);
    }
}

void FMDisplaySequencer::encoderTurned(int instrument, int encoder, int ticks) {
    switch (encoder) {
        case 0: {
            bool isExternalClockEnabled = sequencer_->isExternalClockEnabled();
            bool newIsExternalClockEnabled = ticks < 0;
            if (isExternalClockEnabled != newIsExternalClockEnabled) {
                sequencer_->setExternalClock(newIsExternalClockEnabled);
                refresh(19, 0);
            }
        }
            break;
        case 1: {
            if (!sequencer_->isExternalClockEnabled()) {
                int tempo = sequencer_->getTempo();
                tempo += ticks;
                if (tempo < 10) {
                    tempo = 10;
                }
                if (tempo > 255) {
                    tempo = 255;
                }
                sequencer_->setTempo((float) tempo);
                refresh(19, 19);
            }
        }
            break;
            // encoder 2 change the current instrument
            // and is managed in synthstate.cpp
        case 3: {
            if (seqMode_ == SEQ_MODE_STEP) {

                stepCursor_ += stepSize_ * ticks;
                while (stepCursor_ < 0) {
                    stepCursor_ += 256;
                }
                while (stepCursor_ > 255) {
                    stepCursor_ -= 256;
                }
                refreshStepCursor();
            } else {
                bool isRecording = sequencer_->isRecording(instrument);
                bool newIsRecording = ticks > 0;
                if (isRecording != newIsRecording) {
                    sequencer_->setRecording(instrument, newIsRecording);
                    refreshInstrument(instrument);
                }
            }
        }
            break;
        case 4:
            if (seqMode_ == SEQ_MODE_STEP) {
                if (ticks > 0) {
                    stepSize_ *= 2;
                    if (stepSize_ > 64) {
                        stepSize_ = 64;
                    } else {
                        // Move to beginnig of stepSize
                        stepCursor_ = stepCursor_ / stepSize_ * stepSize_;
                        refreshStepCursor();
                    }
                } else {
                    stepSize_ /= 2;
                    if (stepSize_ == 0) {
                        stepSize_ = 1;
                    } else {
                        refreshStepCursor();
                    }
                }
            } else {
                bool isMuted = sequencer_->isMuted(instrument);
                bool newIsMuted = ticks < 0;
                if (isMuted != newIsMuted) {
                    sequencer_->setMuted(instrument, newIsMuted);
                    refreshInstrument(instrument);
                }
            }
            break;
        case 5:
            if (seqMode_ == SEQ_MODE_STEP) {
                int index = sequencer_->getInstrumentStepSeq(instrument) + (ticks > 0 ? 1 : -1);
                if (index < 0) {
                    index = 0;
                } else if (index >= NUMBER_OF_STEP_SEQUENCES) {
                    index = NUMBER_OF_STEP_SEQUENCES - 1;
                }
                if (index != sequencer_->getInstrumentStepSeq(instrument)) {
                    sequencer_->setInstrumentStepSeq(instrument, index);
                    tft_->pauseRefresh();
                    refresh(16, 13);
                }
            } else {
                int bars = sequencer_->getNumberOfBars(instrument);
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
                    sequencer_->setNumberOfBars(instrument, newBars);
                }
                refreshInstrument(instrument);
            }
            break;
    }
}

void FMDisplaySequencer::buttonLongPressed(int instrument, int button) {
    // Action for all modes
    switch (button) {
        case BUTTON_PFM3_6:
            sequencer_->stop();
            sequencer_->rewind();
            refresh(17, 17);
            refreshPlayButton();
            break;
    }

    if (seqMode_ == SEQ_MODE_NORMAL) {
        switch (button) {
            case BUTTON_PFM3_1:
                // Long press on button 1 : record select
                seqMode_ = SEQ_MODE_REC;
                refresh(7, 1);
                break;
            case BUTTON_PFM3_4:
                // Long press on button 4 : Clear sequencer
                sequencer_->stop();
                sequencer_->reset(true);
                refresh(19, 0);
                break;
        }
    } else if (seqMode_ == SEQ_MODE_STEP) {
        switch (button) {
            case BUTTON_PFM3_4:
                // Long press on button 4 : Clear step sequencer
                stepCursor_ = 0;
                sequencer_->stepClearAll(instrument);
                refreshStepSeq();
                break;
        }
    }

}

void FMDisplaySequencer::buttonPressed(int instrument, int button) {
    // Action for SEQ and STEP mode
    if (seqMode_ == SEQ_MODE_NORMAL || seqMode_ == SEQ_MODE_STEP) {
        switch (button) {
            case BUTTON_PFM3_6:
                if (sequencer_->isRunning()) {
                    sequencer_->stop();
                } else {
                    sequencer_->start();
                }
                refreshPlayButton();
                refresh(17, 17);
                return;
        }
    }

    if (seqMode_ == SEQ_MODE_NORMAL) {
        switch (button) {
            case BUTTON_PFM3_1:
                seqMode_ = SEQ_MODE_MUTE;
                refresh(7, 1);
                break;
            case BUTTON_PFM3_2:
                break;
            case BUTTON_PFM3_3:
                seqMode_ = SEQ_MODE_STEP;
                sequencer_->setStepMode(true);
                refresh(19, 0);
                break;
            case BUTTON_PFM3_4:
                seqMode_ = SEQ_MODE_CLEAR;
                refresh(7, 1);
                break;
            case BUTTON_PFM3_5:
                break;
        }
    } else {
        if (button == BUTTON_PFM3_MENU) {
            if (seqMode_ != SEQ_MODE_STEP) {
                refresh(7, 0);
                seqMode_ = SEQ_MODE_NORMAL;
                return;
            }
        }
        if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6) {
            int actionInstrument = button - BUTTON_PFM3_1;
            if (seqMode_ == SEQ_MODE_MUTE) {
                sequencer_->toggleMuted(actionInstrument);
                refreshInstrument(actionInstrument);
            } else if (seqMode_ == SEQ_MODE_REC) {
                sequencer_->toggleRecording(actionInstrument);
                refreshInstrument(actionInstrument);
            } else if (seqMode_ == SEQ_MODE_CLEAR) {
                sequencer_->clear(actionInstrument);
                // Refresh memory and activated
                refresh(18, 0);
            } else if (seqMode_ == SEQ_MODE_STEP) {
                switch (button) {
                    case BUTTON_PFM3_1:
                        sequencer_->stepClearPart(instrument, stepCursor_, stepSize_);
                        clearSequence(stepCursor_, stepCursor_ + stepSize_);
                        stepCursor_ += stepSize_;
                        if (stepCursor_ > 255) {
                            stepCursor_ -= 256;
                        }
                        refreshStepCursor();
                        break;
                    case BUTTON_PFM3_3:
                        seqMode_ = SEQ_MODE_NORMAL;
                        sequencer_->setStepMode(false);
                        refresh(19, 0);
                        break;
                }
            }
        }

    }

}

void FMDisplaySequencer::newNoteEntered(int instrumentNO) {
    // We use current instrument and not the one we receive
    bool moreThanOneNote = sequencer_->stepRecordNotes(stepCurrentInstrument_, stepCursor_, stepSize_);
    newNoteInSequence(stepCurrentInstrument_, stepCursor_, stepCursor_ + stepSize_, moreThanOneNote);
    stepCursor_ += stepSize_;
    if (stepCursor_ > 255) {
        stepCursor_ -= 256;
    }
    refreshStepCursor();
}

const char* FMDisplaySequencer::getSequenceName() {
    return sequencer_->getSequenceName();
}

void FMDisplaySequencer::tempoClick() {

    static float lastVolume[NUMBER_OF_TIMBRES];
    static float lastGainReduction[NUMBER_OF_TIMBRES];

    if (seqMode_ != SEQ_MODE_STEP && synthState_->mixerState.levelMeterWhere_ == 2) {
        for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {

            if (synthState_->mixerState.instrumentState_[timbre].numberOfVoices > 0) {
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

                    bool isCompressed = synthState_->mixerState.instrumentState_[timbre].compressorType > 0;
                    tft_->drawLevelMetter(26, Y_START_SEQ + 7 + timbre * 27 + 20, 212, 3, 5, volume, isCompressed, gr);
                }
            }
        }
    }
    // Not sure it's a good idea to add level metter in the step seq
    //    else {
    //        if (synthState->mixerState.instrumentState[stepCurrentInstrument].numberOfVoices > 0) {
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


void FMDisplaySequencer::cleanCurrentState() {
    sequencer_->cleanCurrentState();
}
