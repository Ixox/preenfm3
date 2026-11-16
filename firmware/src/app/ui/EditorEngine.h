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

#ifndef EDITOR_ENGINE_H
#define EDITOR_ENGINE_H

#include "FirmwareTftDisplay.h"
#include "ScreenTypes.h"
#include "Common.h"

class SynthState;
class ScreenEditor;

enum CommonPage {
    COMMON_PAGE_NONE = -1,
    COMMON_PAGE_ARP_FX = 0,
    COMMON_PAGE_COUNT
};

/*
 * Common interface for editor pages.
 * ScreenEditor owns one implementation per engine plus one shared-page
 * editor, and dispatches to the active one. Calls are per page/note/block,
 * never per sample.
 */
class EditorEngine {
public:
    // Virtual destructor: ScreenEditor deletes through this interface.
    virtual ~EditorEngine() {}

    // Engine implemented by this editor.
    virtual SynthEngineType getEngineType() const = 0;
    // Display name of the engine.
    virtual const char *getEngineName() const = 0;
    // Menu for an engine-owned main page, or nullptr.
    virtual const struct EditorEditMenu* getEngineMenu(int page) const = 0;
    // Map an engine main page to a shared page, or COMMON_PAGE_NONE.
    virtual int getCommonPage(int page) const { return COMMON_PAGE_NONE; }

    // Bind the editor to its owner, shared state and display.
    virtual void init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) = 0;
    // Draw the timbre's algorithm (editors without one do nothing).
    virtual void drawAlgorithm() {}
    // Page header prefix ("Op N" on operator pages, ...).
    virtual void drawPageHeader() {}
    // Draw one page button.
    virtual void drawEditorButton(int button, const struct EditorEditMenu *editMenu,
        const struct EditorOneButton *page) = 0;
    // Draw one parameter name.
    virtual void drawParameterName(int button, const struct RowEncoder &rowEncoder,
        const struct EditorEditMenu *editMenu, const struct EditorOneButton *page) = 0;
    // Extra work after the page header (operator highlight, hidden params, ...).
    virtual void preparePage() {}
    // Print the value of encoder `encoder`.
    virtual void displayParamValue(int encoder, TFT_COLOR color) = 0;
    // An engine-specific parameter changed (engine-defined paramId); update UI feedback.
    virtual void newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {}
    // Tempo clock tick: refresh time-synced value feedback.
    virtual void tempoClick() = 0;
    // Encoder `encoder` moved by `ticks`.
    virtual void encoderTurned(int encoder, int ticks) = 0;
    // Encoder moved while `button` is held (fine edit / page change).
    virtual void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {}
    // Button pressed.
    virtual void buttonPressed(int button) = 0;
    // Button held: copy/paste and other long-press actions.
    virtual void buttonLongPressed(int instrument, int button) {}
    // Redraw the oscilloscope background for the current page.
    virtual void refreshOscilloBG() = 0;
    // Called when the UI switches to another timbre.
    virtual void newTimbre() {}

    // PFM3 only: a flat parameter changed (editor/MIDI path); update UI feedback.
    virtual void newPfm3ParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
        ParameterDisplay *param, float oldValue, float newValue) {}

    // Linked list of registered editors (owned by ScreenEditor).
    EditorEngine *nextEditor = nullptr;

protected:
    uint8_t getEncoderX(int encoder) const {
        uint8_t x = encoder * 8;
        if (x >= 24) {
            x -= 24;
        }
        return x;
    }

    // Helpers for the current page; defined in EditorEngine.cpp.
    const struct EditorEditMenu* currentEditMenu() const;
    const struct EditorOneButton* currentPage() const;
    struct RowEncoder rowEncoderFor(int encoder) const;
    int currentTimbre() const;

    ScreenEditor *parent_ = nullptr;
    SynthState *synthState_ = nullptr;
    FirmwareTftDisplay *tft_ = nullptr;
};

#endif
