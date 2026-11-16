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

#ifndef EDITOR_PARAM_PAGE
#define EDITOR_PARAM_PAGE

#include "Common.h"
#include "EditorEngine.h"

/*
 * Generic menu/encoder page mechanics over the flat Pfm3Preset model.
 * Used by the PFM3 editor and by the shared Arp/Fx page. Engine-specific
 * pages override the drawing/editing hooks they need.
 */
class EditorParamPage : public EditorEngine {
public:
    EditorParamPage();

    void drawEditorButton(int button, const struct EditorEditMenu *editMenu,
        const struct EditorOneButton *page) override;
    void drawParameterName(int button, const struct RowEncoder &rowEncoder,
        const struct EditorEditMenu *editMenu, const struct EditorOneButton *page) override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void newPfm3ParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
        ParameterDisplay *param, float oldValue, float newValue) override;
    void tempoClick() override;
    void encoderTurned(int encoder, int ticks) override;
    void buttonPressed(int button) override;
    void preparePage() override;
    void refreshOscilloBG() override;

protected:
    void encoderTurnedPfm2(int row, int encoder4, int ticks, bool specialOpCase = true);
    void updateEncoderValueWithoutCursor(int row, int encoder, ParameterDisplay *param,
        float oldValue, float newFloatValue);
    void resetHideParams();

    bool hideParam_[NUMBER_OF_ENCODERS];
    uint8_t valueChangedCounter_[NUMBER_OF_ENCODERS];
    float lfoFreqPreviousValue[5];
};

#endif
