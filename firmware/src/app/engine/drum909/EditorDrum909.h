/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef EDITOR_DRUM909_H_
#define EDITOR_DRUM909_H_

#include "EditorEngine.h"

class Timbre;

/*
 * 9W9 editor. Phase 1: one empty "KIT" page so the engine can be selected.
 * The drum pages and parameters come in Phase 2.
 */
class EditorDrum909 : public EditorEngine {
public:
    EditorDrum909();

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
    Timbre* timbre();
};

#endif
