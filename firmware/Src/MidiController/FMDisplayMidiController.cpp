/*
 * Copyright 2021 Xavier Hosxe
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

#include "FMDisplayMidiController.h"
#include "MidiControllerState.h"
#include "MidiControllerFile.h"
#include "TftDisplay.h"


#define X_OFFSET 16

FMDisplayMidiController::FMDisplayMidiController() {
    menuPressed_ = false;
    displayMode_ = DISPLAY_MIDI_CONTROLLER_MAIN;
    editLetterPosition_ = 0;
    midiChannel_ = 0;
}

void FMDisplayMidiController::init(TftDisplay *tft, MidiControllerState *midiControllerState, MidiControllerFile *midiControllerFile) {
    tft_ = tft;
    midiControllerState_ = midiControllerState;
    midiControllerFile_ = midiControllerFile;

    midiControllerFile_->loadConfig(midiControllerState_);
}


void FMDisplayMidiController::refreshAllScreenByStep() {

    if (likely(refreshStatus_ != 21)) {
        if (unlikely(displayMode_ == DISPLAY_MIDI_CONTROLLER_EDIT_ENCODER)) {
            refreshAllScreenByStepEditEncoder();
            return;
        } else if (unlikely(displayMode_ == DISPLAY_MIDI_CONTROLLER_EDIT_BUTTON)) {
            refreshAllScreenByStepEditButton();
            return;
        } else if (unlikely(displayMode_ == DISPLAY_MIDI_CONTROLLER_SAVE)) {
            refreshAllScreenByStepSave();
            return;
        }
    }

    switch (refreshStatus_) {
        case 21: {
            tft_->pauseRefresh();
            tft_->clear();
            tft_->fillArea(0, 0, 240, 21, COLOR_DARK_BLUE);
            tft_->setCursorInPixel(8, 2);
            tft_->print("Midi Controller PFM3", COLOR_YELLOW, COLOR_DARK_BLUE);
            break;
        }
        case 20: {
            tft_->fillArea(0, 22, 240, 298, COLOR_BLACK);

            int y = 3 * TFT_BIG_CHAR_HEIGHT - 10;
            tft_->fillArea(0, y, 240, 126, COLOR_DARK_BLUE);
            tft_->fillArea(1, y + 1, 238, 124, COLOR_BLACK);

            y = 10 * TFT_BIG_CHAR_HEIGHT - 10;
            tft_->fillArea(0, y, 240, 126, COLOR_DARK_BLUE);
            tft_->fillArea(1, y + 1, 238, 124, COLOR_BLACK);

            displayMidiChannel();
            break;
        }
        case 19:
        case 18:
        case 17:
        case 16:
        case 15:
        case 14: {
            int encoder = 19 - refreshStatus_;
            uint8_t x = (encoder > 2 ? (encoder - 3) * 7 : encoder * 7) * TFT_BIG_CHAR_WIDTH + X_OFFSET;
            uint16_t y = (encoder > 2 ? 6 : 3) * TFT_BIG_CHAR_HEIGHT;

            tft_->setCharColor(COLOR_WHITE);
            tft_->setCursorInPixel(x, y);
            tft_->print(midiControllerState_->encoder_[encoder].name);

            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursorInPixel(x + 2, y + TFT_BIG_CHAR_HEIGHT - 1);
            tft_->printSmallChars("cc");
            tft_->printSmallChar(midiControllerState_->encoder_[encoder].controller);
            if (midiControllerState_->encoder_[encoder].midiChannel != 16) {
                tft_->setCharColor(COLOR_ORANGE);
                tft_->printSmallChars(" ch");
                tft_->printSmallChar(midiControllerState_->encoder_[encoder].midiChannel + 1);
            }

            displayEncoderValue(encoder);
            break;
        }
        case 13:
        case 12:
        case 11:
        case 10:
        case 9:
        case 8: {
            int button = 13 - refreshStatus_;
            uint8_t x = (button > 2 ? (button - 3) * 7 : button * 7) * TFT_BIG_CHAR_WIDTH + X_OFFSET;
            uint16_t y = (button > 2 ? 13 : 10) * TFT_BIG_CHAR_HEIGHT;

            tft_->setCharColor(COLOR_WHITE);
            tft_->setCursorInPixel(x, y);
            tft_->print(midiControllerState_->button_[button].name);

            tft_->setCharColor(COLOR_LIGHT_GRAY);
            tft_->setCursorInPixel(x + 2, y + TFT_BIG_CHAR_HEIGHT - 1);
            tft_->printSmallChars("cc");
            tft_->printSmallChar(midiControllerState_->button_[button].controller);
            if (midiControllerState_->button_[button].midiChannel != 16) {
                tft_->setCharColor(COLOR_ORANGE);
                tft_->printSmallChars(" ch");
                tft_->printSmallChar(midiControllerState_->button_[button].midiChannel + 1);
            }
            displayButtonValue(button);
            break;
        }

        case 7:
            refreshStatus_ = 0;
            break;
    }

    if (refreshStatus_ > 0) {
        refreshStatus_--;
    }

    if (refreshStatus_ == 0) {
        tft_->restartRefreshTft();
    }
}


void FMDisplayMidiController::refreshAllScreenByStepEditEncoder() {
    switch (refreshStatus_) {
        case 20:
            tft_->fillArea(0, 22, 240, 298, COLOR_BLACK);
            tft_->fillArea(0, 50, 240, 250, COLOR_DARK_RED);
            tft_->fillArea(2, 52, 236, 246, COLOR_BLACK);
        break;
        case 19:
            tft_->setCharColor(COLOR_WHITE);
            tft_->setCursor(3, 4);
            tft_->print("Edit Encoder");
            break;
        case 18:
            tft_->setCursor(1,6);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChars("a,d");
            tft_->setCursor(4, 6);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Name    : ");
            displayEncoderParam(editControl_, ENCODER_PARAM_NAME);
            break;
        case 17:
            tft_->setCursor(1, 8);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('b');
            tft_->setCursor(4, 8);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Midi ch : ");
            displayEncoderParam(editControl_, ENCODER_PARAM_MIDI_CHANNEL);
            break;
        case 16:
            tft_->setCursor(1, 9);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('e');

            tft_->setCursor(4, 9);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("CC      : ");
            displayEncoderParam(editControl_, ENCODER_PARAM_CONTROLLER);
            break;
        case 15:
            tft_->setCursor(1, 11);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('c');

            tft_->setCursor(4, 11);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Min     : ");
            displayEncoderParam(editControl_, ENCODER_PARAM_MIN);
            break;
        case 14:
            tft_->setCursor(1, 12);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('f');

            tft_->setCursor(4, 12);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Max     : ");
            displayEncoderParam(editControl_, ENCODER_PARAM_MAX);
            break;
        case 13:
            refreshStatus_ = 0;
            break;
    }


    if (refreshStatus_ > 0) {
        refreshStatus_--;
    }

    if (refreshStatus_ == 0) {
        tft_->restartRefreshTft();
    }
}



void FMDisplayMidiController::refreshAllScreenByStepEditButton() {
    switch (refreshStatus_) {
        case 20:
            tft_->fillArea(0, 22, 240, 298, COLOR_BLACK);
            tft_->fillArea(0, 50, 240, 250, COLOR_ORANGE);
            tft_->fillArea(2, 52, 236, 246, COLOR_BLACK);
        break;
        case 19:
            tft_->setCharColor(COLOR_WHITE);
            tft_->setCursor(3, 4);
            tft_->print("Edit Button");
            break;
        case 18:
            tft_->setCursor(1,6);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChars("a,d");
            tft_->setCursor(4, 6);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Name    : ");
            displayButtonParam(editControl_, BUTTON_PARAM_NAME);
            break;
        case 17:
            tft_->setCursor(1, 8);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('b');
            tft_->setCursor(4, 8);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Midi ch : ");
            displayButtonParam(editControl_, BUTTON_PARAM_MIDI_CHANNEL);
            break;
        case 16:
            tft_->setCursor(1, 9);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('e');

            tft_->setCursor(4, 9);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("CC      : ");
            displayButtonParam(editControl_, BUTTON_PARAM_CONTROLLER);
            break;
        case 15:
            tft_->setCursor(1, 11);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('c');

            tft_->setCursor(4, 11);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Type    : ");
            displayButtonParam(editControl_, BUTTON_PARAM_BUTTON_TYPE);
            break;
        case 14:
            tft_->setCursor(1, 12);
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->printSmallChar('f');

            tft_->setCursor(4, 12);
            tft_->setCharColor(COLOR_WHITE);
            tft_->print("Max     : ");
            displayButtonParam(editControl_, BUTTON_PARAM_HIGH);
            break;
        case 13:
            refreshStatus_ = 0;
            break;
    }


    if (refreshStatus_ > 0) {
        refreshStatus_--;
    }

    if (refreshStatus_ == 0) {
        tft_->restartRefreshTft();
    }
}

void FMDisplayMidiController::refreshAllScreenByStepSave() {
    switch (refreshStatus_) {
        case 20:
            tft_->fillArea(30, 50, 190, 240, COLOR_RED);
            tft_->fillArea(33, 53, 184, 234, COLOR_BLACK);
        break;
        case 19:
            tft_->setCharColor(COLOR_RED);
            tft_->setCursor(9, 6);
            tft_->print("Save");
        break;
        case 18:
            tft_->setCharColor(COLOR_WHITE);
            tft_->setCursorInPixel(50, 180);
            tft_->printSmallChars("'SEQ' again to save");
            tft_->setCursorInPixel(60, 210);
            tft_->printSmallChars("Any other button");
            tft_->setCursorInPixel(88, 220);
            tft_->printSmallChars("To Cancel");
        break;
        case 17:
            refreshStatus_ = 0;
        break;
    }
    if (refreshStatus_ > 0) {
        refreshStatus_--;
    }
    if (refreshStatus_ == 0) {
        tft_->restartRefreshTft();
    }
}


void FMDisplayMidiController::displayEncoderValue(int encoder) {
    uint8_t x = (encoder > 2 ? (encoder - 3) * 7 : encoder * 7) * TFT_BIG_CHAR_WIDTH + X_OFFSET;
    uint8_t y = (encoder > 2 ? 6 : 3) * TFT_BIG_CHAR_HEIGHT;

    tft_->setCharColor(COLOR_YELLOW);
    tft_->setCursorInPixel(x, y + TFT_BIG_CHAR_HEIGHT + TFT_SMALL_CHAR_HEIGHT);
    if (midiControllerState_->encoder_[encoder].value < 10) {
        tft_->print("  ");
    } else if (midiControllerState_->encoder_[encoder].value < 100) {
        tft_->print(' ');
    }
    tft_->print(midiControllerState_->encoder_[encoder].value);
}

void FMDisplayMidiController::displayButtonValue(int button) {
    uint8_t x = (button > 2 ? (button - 3) * 7 : button * 7) * TFT_BIG_CHAR_WIDTH + X_OFFSET;
    uint16_t y = (button > 2 ? 13 : 10) * TFT_BIG_CHAR_HEIGHT  + TFT_BIG_CHAR_HEIGHT + TFT_SMALL_CHAR_HEIGHT ;
    uint8_t width = 55;

    if (midiControllerState_->button_[button].value == midiControllerState_->button_[button].valueHigh) {
        tft_->fillArea(x, y, width, 22, COLOR_GREEN);
        tft_->setCharBackgroundColor(COLOR_GREEN);
        tft_->setCharColor(COLOR_BLACK);
    } else {
        if (midiControllerState_->button_[button].buttonType == MIDI_BUTTON_TYPE_PUSH) {
            tft_->fillArea(x, y, width, 22, COLOR_DARK_GRAY);
        } else {
            tft_->fillArea(x, y, width, 22, COLOR_GREEN);
        }
        tft_->fillArea(x + 1, y + 1, width - 2, 20, COLOR_BLACK);
        tft_->setCharColor(COLOR_GRAY);
    }

    int len = 16;
    if (midiControllerState_->button_[button].value < 10) {
        len = 6;
    } else if (midiControllerState_->button_[button].value < 100) {
        len = 11;
    }
    tft_->setCursorInPixel(x + (width >> 1) - len, y + 2);
    tft_->print(midiControllerState_->button_[button].value);
    tft_->setCharBackgroundColor(COLOR_BLACK);
}

void FMDisplayMidiController::encoderTurned(int encoder, int ticks) {
    switch (displayMode_) {
        case DISPLAY_MIDI_CONTROLLER_MAIN: {
            if (!menuPressed_) {
                midiControllerState_->encoderDelta(midiChannel_, encoder, ticks);
                displayEncoderValue(encoder);
            } else  {
                if (displayMode_ != DISPLAY_MIDI_CONTROLLER_EDIT_ENCODER) {
                    displayMode_ = DISPLAY_MIDI_CONTROLLER_EDIT_ENCODER;
                    editControl_ = encoder;
                    setResetRefreshStatus();
                }
            }
            break;
        }
        case DISPLAY_MIDI_CONTROLLER_EDIT_ENCODER: {
            switch (encoder) {
            case 0: {
                int newValue = editLetterPosition_ + (ticks > 0 ? 1 : -1);
                newValue = newValue < 0 ? 0 : newValue;
                newValue = newValue > 4 ? 4 : newValue;
                if (editLetterPosition_ != newValue) {
                    editLetterPosition_ = newValue;
                    displayEncoderParam(editControl_, ENCODER_PARAM_NAME);
                }
                break;
            }
            case 3: {
                int newValue = midiControllerState_->encoder_[editControl_].name[editLetterPosition_] + ticks;
                newValue = newValue < 32 ? 32 : newValue;
                newValue = newValue > 127 ? 127 : newValue;
                if (midiControllerState_->encoder_[editControl_].name[editLetterPosition_] != newValue) {
                    midiControllerState_->encoder_[editControl_].name[editLetterPosition_] = newValue;
                    displayEncoderParam(editControl_, ENCODER_PARAM_NAME);
                }
                break;
            }
            case 1: {
                int newValue = midiControllerState_->encoder_[editControl_].midiChannel + (ticks > 0 ? 1 : -1);
                newValue = newValue < 0 ? 0 : newValue;
                // 16 is valid : it means globalMidiChannel
                newValue = newValue > 16 ? 16 : newValue;
                if (midiControllerState_->encoder_[editControl_].midiChannel != newValue) {
                    midiControllerState_->encoder_[editControl_].midiChannel = newValue;
                    displayEncoderParam(editControl_, ENCODER_PARAM_MIDI_CHANNEL);
                }
                break;
            }
            case 4: {
                int newValue = midiControllerState_->encoder_[editControl_].controller + ticks;
                newValue = newValue < 0 ? 0 : newValue;
                newValue = newValue > 127 ? 127 : newValue;
                if (midiControllerState_->encoder_[editControl_].controller != newValue) {
                    midiControllerState_->encoder_[editControl_].controller = newValue;
                    displayEncoderParam(editControl_, ENCODER_PARAM_CONTROLLER);
                }
                break;
            }
            case 2: {
                int newValue = midiControllerState_->encoder_[editControl_].minValue + ticks;
                newValue = newValue < 0 ? 0 : newValue;
                newValue = newValue > midiControllerState_->encoder_[editControl_].maxValue ? midiControllerState_->encoder_[editControl_].maxValue : newValue;
                if (midiControllerState_->encoder_[editControl_].minValue != newValue) {
                    midiControllerState_->encoder_[editControl_].minValue = newValue;
                    displayEncoderParam(editControl_, ENCODER_PARAM_MIN);
                }
                break;
            }
            case 5: {
                int newValue = midiControllerState_->encoder_[editControl_].maxValue + ticks;
                newValue = newValue < midiControllerState_->encoder_[editControl_].minValue ? midiControllerState_->encoder_[editControl_].minValue : newValue;
                newValue = newValue > 127 ? 127 : newValue;
                if (midiControllerState_->encoder_[editControl_].maxValue != newValue) {
                    midiControllerState_->encoder_[editControl_].maxValue = newValue;
                    displayEncoderParam(editControl_, ENCODER_PARAM_MAX);
                }
                break;
            }
            }
            break;
        }
        case DISPLAY_MIDI_CONTROLLER_EDIT_BUTTON: {
            switch (encoder) {
            case 0: {
                int newValue = editLetterPosition_ + (ticks > 0 ? 1 : -1);
                newValue = newValue < 0 ? 0 : newValue;
                newValue = newValue > 4 ? 4 : newValue;
                if (editLetterPosition_ != newValue) {
                    editLetterPosition_ = newValue;
                    displayButtonParam(editControl_, BUTTON_PARAM_NAME);
                }
                break;
            }
            case 3: {
                int newValue = midiControllerState_->button_[editControl_].name[editLetterPosition_] + ticks;
                newValue = newValue < 32 ? 32 : newValue;
                newValue = newValue > 127 ? 127 : newValue;
                if (midiControllerState_->button_[editControl_].name[editLetterPosition_] != newValue) {
                    midiControllerState_->button_[editControl_].name[editLetterPosition_] = newValue;
                    displayButtonParam(editControl_, BUTTON_PARAM_NAME);
                }
                break;
            }
            case 1: {
                int newValue = midiControllerState_->button_[editControl_].midiChannel + (ticks > 0 ? 1 : -1);
                newValue = newValue < 0 ? 0 : newValue;
                // 16 is valid : it means globalMidiChannel
                newValue = newValue > 16 ? 16 : newValue;
                if (midiControllerState_->button_[editControl_].midiChannel != newValue) {
                    midiControllerState_->button_[editControl_].midiChannel = newValue;
                    displayButtonParam(editControl_, BUTTON_PARAM_MIDI_CHANNEL);
                }
                break;
            }
            case 4: {
                int newValue = midiControllerState_->button_[editControl_].controller + ticks;
                newValue = newValue < 0 ? 0 : newValue;
                newValue = newValue > 127 ? 127 : newValue;
                if (midiControllerState_->button_[editControl_].controller != newValue) {
                    midiControllerState_->button_[editControl_].controller = newValue;
                    displayButtonParam(editControl_, BUTTON_PARAM_CONTROLLER);
                }
                break;
            }
            case 2: {
                MidiButtonType newValue = ticks < 0 ? MIDI_BUTTON_TYPE_PUSH : MIDI_BUTTON_TYPE_TOGGLE;
                if (midiControllerState_->button_[editControl_].buttonType != newValue) {
                    midiControllerState_->button_[editControl_].buttonType = newValue;
                    displayButtonParam(editControl_, BUTTON_PARAM_BUTTON_TYPE);
                }
                break;
            }
            case 5: {
                int newValue = midiControllerState_->button_[editControl_].valueHigh + ticks;
                newValue = newValue < 0 ? 0  : newValue;
                newValue = newValue > 127 ? 127 : newValue;
                if (midiControllerState_->button_[editControl_].valueHigh != newValue) {
                    midiControllerState_->button_[editControl_].valueHigh = newValue;
                    displayButtonParam(editControl_, BUTTON_PARAM_HIGH);
                }
                break;
            }
            }

            break;
        }
    }
}


void FMDisplayMidiController::displayMidiChannel() {
    tft_->setCharColor(COLOR_DARK_GRAY);
    tft_->setCursorInPixel(50, 26);
    tft_->printSmallChars("Midi Channel : ");
    tft_->setCharColor(COLOR_WHITE);
    tft_->printSmallChar(midiChannel_ + 1);
    if ((midiChannel_ + 1) < 10) {
        tft_->printSmallChar(' ');
    }
}


void FMDisplayMidiController::displayEncoderParam(int encoder, EncoderParamType paramType) {
    switch (paramType) {
    case ENCODER_PARAM_NAME:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 6);
        tft_->print(midiControllerState_->encoder_[encoder].name);
        tft_->setCursor(14 + editLetterPosition_, 6);
        tft_->setCharColor(COLOR_RED);
        if (midiControllerState_->encoder_[encoder].name[editLetterPosition_] == ' ') {
            tft_->print('_');
        } else {
            tft_->print(midiControllerState_->encoder_[encoder].name[editLetterPosition_]);
        }
        break;
    case ENCODER_PARAM_MIDI_CHANNEL:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 8);
        if (midiControllerState_->encoder_[editControl_].midiChannel == 16) {
            tft_->print("--");
        } else {
            tft_->print(midiControllerState_->encoder_[editControl_].midiChannel + 1);
            if ((midiControllerState_->encoder_[editControl_].midiChannel +1) < 10) {
                tft_->print(' ');
            }
        }
        break;
    case ENCODER_PARAM_CONTROLLER:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 9);
        tft_->print(midiControllerState_->encoder_[editControl_].controller);
        if (midiControllerState_->encoder_[editControl_].controller < 100) {
            tft_->print(' ');
            if (midiControllerState_->encoder_[editControl_].controller < 10) {
                tft_->print(' ');
            }
        }
        break;
    case ENCODER_PARAM_MIN:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 11);
        tft_->print(midiControllerState_->encoder_[editControl_].minValue);
        if (midiControllerState_->encoder_[editControl_].minValue < 100) {
            tft_->print(' ');
            if (midiControllerState_->encoder_[editControl_].minValue < 10) {
                tft_->print(' ');
            }
        }
        break;
    case ENCODER_PARAM_MAX:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 12);
        tft_->print(midiControllerState_->encoder_[editControl_].maxValue);
        if (midiControllerState_->encoder_[editControl_].maxValue < 100) {
            tft_->print(' ');
            if (midiControllerState_->encoder_[editControl_].maxValue < 10) {
                tft_->print(' ');
            }
        }
        break;
    }

}


void FMDisplayMidiController::displayButtonParam(int button, ButtonParamType paramType) {
    switch (paramType) {
    case BUTTON_PARAM_NAME:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 6);
        tft_->print(midiControllerState_->button_[button].name);
        tft_->setCursor(14 + editLetterPosition_, 6);
        tft_->setCharColor(COLOR_RED);
        if (midiControllerState_->button_[button].name[editLetterPosition_] == ' ') {
            tft_->print('_');
        } else {
            tft_->print(midiControllerState_->button_[button].name[editLetterPosition_]);
        }
        break;
    case BUTTON_PARAM_MIDI_CHANNEL:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 8);
        if (midiControllerState_->button_[editControl_].midiChannel == 16) {
            tft_->print("--");
        } else {
            tft_->print(midiControllerState_->button_[editControl_].midiChannel + 1);
            if ((midiControllerState_->button_[editControl_].midiChannel +1) < 10) {
                tft_->print(' ');
            }
        }
        break;
    case BUTTON_PARAM_CONTROLLER:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 9);
        tft_->print(midiControllerState_->button_[editControl_].controller);
        if (midiControllerState_->button_[editControl_].controller < 100) {
            tft_->print(' ');
            if (midiControllerState_->button_[editControl_].controller < 10) {
                tft_->print(' ');
            }
        }
        break;
    case BUTTON_PARAM_BUTTON_TYPE:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 11);
        if (midiControllerState_->button_[editControl_].buttonType == MIDI_BUTTON_TYPE_PUSH) {
            tft_->print("Push");
        } else {
            tft_->print("Togl");
        }
        break;
    case BUTTON_PARAM_HIGH:
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(14, 12);
        tft_->print(midiControllerState_->button_[editControl_].valueHigh);
        if (midiControllerState_->button_[editControl_].valueHigh < 100) {
            tft_->print(' ');
            if (midiControllerState_->button_[editControl_].valueHigh < 10) {
                tft_->print(' ');
            }
        }
        break;
    }

}



void FMDisplayMidiController::buttonUp(int button) {
    if (displayMode_ == DISPLAY_MIDI_CONTROLLER_SAVE) {
        return;
    } else if (displayMode_ == DISPLAY_MIDI_CONTROLLER_MAIN) {
        if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6) {
            if (midiControllerState_->buttonUp(midiChannel_, button)) {
                displayButtonValue(button);
            }
        } else if (button == BUTTON_PFM3_MENU) {
            menuPressed_ = false;
        }
    }
}

void FMDisplayMidiController::buttonDown(int button) {
    if (displayMode_ == DISPLAY_MIDI_CONTROLLER_SAVE) {
        if (button == BUTTON_PFM3_SEQUENCER) {
            tft_->fillArea(30, 50, 190, 240, COLOR_RED);
            tft_->setCharBackgroundColor(COLOR_RED);
            tft_->setCharColor(COLOR_BLACK);
            tft_->setCursorInPixel(94, 120);
            tft_->print("Saved");
            tft_->setCharBackgroundColor(COLOR_BLACK);
            midiControllerFile_->saveConfig(midiControllerState_);
            HAL_Delay(1500);
        }
        displayMode_ = DISPLAY_MIDI_CONTROLLER_MAIN;
        setResetRefreshStatus();
        return;
    } else if (displayMode_ == DISPLAY_MIDI_CONTROLLER_MAIN) {
        if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6) {
            if (!menuPressed_) {
                midiControllerState_->buttonDown(midiChannel_, button);
                displayButtonValue(button);
            } else  {
                displayMode_ = DISPLAY_MIDI_CONTROLLER_EDIT_BUTTON;
                editControl_ = button;
                setResetRefreshStatus();
            }
        } else if (button == BUTTON_PFM3_MENU) {
            menuPressed_ = true;
        } else if (button == BUTTON_PFM3_SEQUENCER) {
            displayMode_ = DISPLAY_MIDI_CONTROLLER_SAVE;
            refreshStatus_ = 20;
            HAL_Delay(100);
        } else if (button == BUTTON_NEXT_INSTRUMENT) {
            if (midiChannel_ < 15) {
                midiChannel_ ++;
                displayMidiChannel();
            }
        } else if (button == BUTTON_PREVIOUS_INSTRUMENT) {
            if (midiChannel_ > 0) {
                midiChannel_ --;
                displayMidiChannel();
            }
        }
    } else {
        if (button == BUTTON_PFM3_MENU) {
            displayMode_ = DISPLAY_MIDI_CONTROLLER_MAIN;
            setResetRefreshStatus();
        }
    }
}


// USELESS
void FMDisplayMidiController::encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {
}

void FMDisplayMidiController::buttonPressed(int button) {
}

void FMDisplayMidiController::buttonLongPressed(int button) {
}

void FMDisplayMidiController::twoButtonsPressed(int button1, int button2) {
}
