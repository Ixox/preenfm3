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

#ifndef SCREEN_MSFA_EDITOR
#define SCREEN_MSFA_EDITOR

#include "Common.h"
#include "ScreenEditor.h"

class Timbre;

/*
 * MSFA (DX7) editor pages. ScreenEditor owns one instance and delegates
 * pages 0..3 (MSFA, Oper, EG, Scale) to it. Pages 4 and 5 (Arp/Fx, Perf)
 * stay on the PFM3 parameter path because they are shared with OneSynthParams.
 */
class ScreenMsfaEditor : public EngineEditor {
public:
    ScreenMsfaEditor();

    void init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) override;

    const struct EditorEditMenu* getEditMenu(int page) const override;

    void refreshEditorByStep(int &refreshStatus, int &endRefreshStatus) override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void encoderTurned(int encoder6, int ticks);
    void newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
        ParameterDisplay *param, float oldValue, float newValue) override;
    void encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button);
    void buttonPressed(int button);
    void buttonLongPressed(int instrument, int button);
    void refreshOscillatorOperatorShape() override;
    void refreshLfoOscillator() override;
    void refreshLfoEnv() override;
    void tempoClick() override;

private:
    uint8_t getX(int encoder);
    int currentTimbre() const;
    Timbre* timbre();

    uint8_t paramValue(uint8_t paramId);
    void setParamValue(uint8_t paramId, uint8_t value);
    void displayValue(uint8_t paramId, uint8_t value);
    void applyProgramChange();

    ScreenEditor *parent_;
    SynthState *synthState_;
    FirmwareTftDisplay *tft_;
    uint8_t valueChangedCounter_[6];
    uint8_t operatorChangedCounter_[6];
    uint8_t feedbackChangedCounter_;
    uint8_t highlightedOperator_[6];
    uint8_t feedbackSource_;
    uint8_t feedbackDest_;
    uint8_t copyOperator_[21];
    bool copyOperatorValid_;
};

#endif
