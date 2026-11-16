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

#ifndef SCREEN_PFM3_EDITOR
#define SCREEN_PFM3_EDITOR

#include "Common.h"
#include "ScreenEditor.h"

/*
 * PFM3 editor pages, parameter handling and menu tables.
 *
 * Shared pages (Arp/Fx, Perf) still use the common ScreenEditor helpers.
 */
class ScreenPfm3Editor : public EngineEditor {
public:
    ScreenPfm3Editor();

    void init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) override;
    const struct EditorEditMenu* getEditMenu(int page) const override;
    void refreshEditorByStep(int &refreshStatus, int &endRefreshStatus) override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
        ParameterDisplay *param, float oldValue, float newValue) override;
    void tempoClick() override;
    void encoderTurned(int encoder, int ticks) override;
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) override;
    void buttonPressed(int button) override;
    void buttonLongPressed(int instrument, int button) override;
    void refreshOscillatorOperatorShape() override;
    void refreshLfoOscillator() override;
    void refreshLfoEnv() override;
    void checkOperatorNumber() override;

private:
    void encoderTurnedPfm2(int row, int encoder4, int ticks, bool specialOpCase = true);
    void buttonPressedEditPage(int button);
    void updateEncoderValueWithoutCursor(int row, int encoder, ParameterDisplay *param,
        float oldValue, float newFloatValue);
    void resetHideParams();
    bool isInOperatorPage(uint8_t pageNumber);
    bool isInMatrixPage(uint8_t pageNumber);
    bool isInModulatorPage(uint8_t pageNumber);
    uint8_t getX(int encoder);

    ScreenEditor *parent_;
    SynthState *synthState_;
    FirmwareTftDisplay *tft_;

    bool hideParam_[NUMBER_OF_ENCODERS];
    // Value Changed info
    uint8_t valueChangedCounter_[NUMBER_OF_ENCODERS];
    uint8_t imChangedCounter_[NUMBER_OF_ENCODERS];
    uint8_t mixChangedCounter_[NUMBER_OF_ENCODERS];
    uint8_t mixChangeOperatorNumber_[NUMBER_OF_ENCODERS];
    bool multipleEdition_;
    // Remember previous lfo freq value
    float lfoFreqPreviousValue[5];
    // Buffer to copy and past
    int8_t bufferPageNumber;
    uint8_t bufferEditNumber;
    float bufferParam[6];
};

#endif
