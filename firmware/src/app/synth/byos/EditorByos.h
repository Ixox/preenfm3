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

    void init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) override;
    SynthEngineType getEngineType() const override;
    const char* getEngineName() const override;
    const struct EditorEditMenu* getEngineMenu(int page) const override;
    int getCommonPage(int page) const override;
    void drawEditorButton(int button, const struct EditorEditMenu *editMenu,
        const struct EditorOneButton *page) override;
    void drawParameterName(int button, const struct RowEncoder &rowEncoder,
        const struct EditorEditMenu *editMenu, const struct EditorOneButton *page) override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void encoderTurned(int encoder6, int ticks) override;
    void newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) override;
    void buttonPressed(int button) override;
    void tempoClick() override;
    void refreshOscilloBG() override;

private:
    int currentTimbre() const;
    Timbre* timbre();
};

#endif
