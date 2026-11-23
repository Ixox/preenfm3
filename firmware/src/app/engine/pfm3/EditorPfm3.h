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

#ifndef EDITOR_PFM3
#define EDITOR_PFM3

#include "Common.h"
#include "EditorParamPage.h"

/*
 * PFM3 engine pages, parameter handling and menu tables.
 *
 * Shared pages (Arp/Fx) are handled by EditorCommon; the Perf page
 * stays PFM3-only.
 */
class EditorPfm3 : public EditorParamPage {
public:
    EditorPfm3();

    void init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) override;
    SynthEngineType getEngineType() const override;
    const char* getEngineName() const override;
    const struct EditorMenu* getEngineMenu(int page) const override;
    int getCommonPage(int page) const override;
    void drawAlgorithm() override;
    void printPageHeader() override;
    void drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) override;
    EditorSelectorKind selectorKind(int button) const override;
    void printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                            const struct EditorPageButton* page) override;
    void preparePage() override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void encoderTurned(int encoder, int ticks) override;
    void newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) override;
    void slowTick() override;
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) override;
    void buttonPressed(int button) override;
    void buttonLongPressed(int instrument, int button) override;
    void refreshOscilloBG() override;
    void newTimbre() override;

private:
    void updatePage() const override;
    uint8_t currentElement() const;

    void checkOperatorNumber();
    void refreshOscillatorOperatorShape();
    void refreshOscillatorOperatorEnvelope();
    void refreshLfoOscillator();
    void refreshLfoEnv();
    bool isInOperatorPage(uint8_t pageNumber) const;
    bool isInMatrixPage(uint8_t pageNumber) const;
    bool isInModulatorPage(uint8_t pageNumber);

    uint8_t imChangedCounter_[NUMBER_OF_ENCODERS];
    uint8_t mixChangedCounter_[NUMBER_OF_ENCODERS];
    uint8_t mixChangeOperatorNumber_[NUMBER_OF_ENCODERS];
    bool multipleEdition_;
    // Remember previous lfo freq value
    // Buffer to copy and past
    int8_t bufferPageNumber_;
    uint8_t bufferEditNumber_;
    float bufferParam_[6];
};

#endif
