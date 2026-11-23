/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef EDITOR_BYOS_H_
#define EDITOR_BYOS_H_

#include "EditorEngine.h"

class Timbre;

/*
 * Minimal BYOS editor: one "Engine" page with OP (op1/op2 rows) and ENV buttons.
 */
class EditorByos : public EditorEngine {
public:
    EditorByos();

    void init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) override;
    SynthEngineType getEngineType() const override;
    const char* getEngineName() const override;
    const struct EditorMenu* getEngineMenu(int page) const override;
    int getCommonPage(int page) const override;
    void drawAlgorithm() override;
    void displayEditorTitle() override;
    void drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) override;
    void printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                            const struct EditorPageButton* page) override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void encoderTurned(int encoder, int ticks) override;
    void newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) override;
    void buttonPressed(int button) override;
    void slowTick() override;
    void refreshOscilloBG() override;

private:
    int currentTimbre() const;
    Timbre* timbre();
};

#endif
