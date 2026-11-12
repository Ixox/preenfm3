/*
 * Copyright 2026 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FMDISPLAY_MSFA_EDITOR
#define FMDISPLAY_MSFA_EDITOR

#include "Common.h"
#include "FMDisplayEditor.h"

class Timbre;

/*
 * MSFA (DX7) editor pages. FMDisplayEditor owns one instance and delegates
 * pages 0..3 (MSFA, Oper, EG, Scale) to it. Pages 4 and 5 (Arp/Fx, Perf)
 * stay on the PFM3 parameter path because they are shared with OneSynthParams.
 */
class FMDisplayMsfaEditor {
public:
    FMDisplayMsfaEditor();

    void init(FMDisplayEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft);

    const struct Pfm3EditMenu* getEditMenu(int page) const;

    void refreshEditorByStep(int &refreshStatus, int &endRefreshStatus);
    void displayParamValue(int encoder, TFT_COLOR color);
    void encoderTurned(int encoder6, int ticks);
    void encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button);
    void buttonPressed(int button);
    void buttonLongPressed(int instrument, int button);
    void tempoClick();

private:
    uint8_t getX(int encoder);
    int currentTimbre() const;
    Timbre* timbre();

    uint8_t paramValue(uint8_t paramId);
    void setParamValue(uint8_t paramId, uint8_t value);
    void displayValue(uint8_t paramId, uint8_t value);
    void applyProgramChange();

    FMDisplayEditor *parent_;
    SynthState *synthState_;
    FirmwareTftDisplay *tft_;
    uint8_t valueChangedCounter_[6];
    uint8_t copyOperator_[21];
    bool copyOperatorValid_;
};

#endif
