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

#ifndef EDITOR_DX7
#define EDITOR_DX7

#include "Common.h"
#include "EditorEngine.h"

class Timbre;

/*
 * DX7 (DX7) editor pages. ScreenEditor owns one instance and delegates
 * pages 0..1 (Global, Oper) to it. Page 2 reuses the PFM3 Arp/Fx menu;
 * pages 3..5 are hidden.
 */
class EditorDx7 : public EditorEngine {
public:
    EditorDx7();

    void init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) override;
    SynthEngineType getEngineType() const override;
    const char* getEngineName() const override;
    const struct EditorMenu* getEngineMenu(int page) const override;
    int getCommonPage(int page) const override;
    void drawAlgorithm() override;
    void displayEditorTitle() override;
    void printPageHeader() override;
    void preparePage() override;
    void drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) override;
    EditorSelectorKind selectorKind(int button) const override;
    void printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                            const struct EditorPageButton* page) override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void encoderTurned(int encoder, int ticks) override;
    void newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) override;
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) override;
    void buttonPressed(int button) override;
    void buttonLongPressed(int instrument, int button) override;
    void refreshOscilloBG() override;
    void slowTick() override;

private:
    void refreshOscillatorOperatorEnvelope();
    int currentTimbre() const;
    Timbre* timbre();

    void displayValue(uint8_t paramId, uint8_t value);
    void applyProgramChange();

    uint8_t valueChangedCounter_[6];
    uint8_t feedbackChangedCounter_;
    uint8_t feedbackSource_;
    uint8_t feedbackDest_;
    uint8_t copyOperator_[21];
    bool copyOperatorValid_;
};

#endif
