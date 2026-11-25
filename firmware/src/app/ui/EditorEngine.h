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

#include "Common.h"
#include "FirmwareTftDisplay.h"
#include "ScreenTypes.h"
#include "engine/pfm3/Pfm3Param.h"

class SynthState;
class ScreenEditor;

// Per-engine UI colours. Defaults reproduce the historic PFM3 look.
struct EditorTheme {
    TFT_COLOR pageLabelColor = COLOR_CYAN_LIGHT;
    TFT_COLOR pageLabelBackground = COLOR_NONE;
    TFT_COLOR buttonBackground = COLOR_BLUE_DARK;
    TFT_COLOR buttonText = COLOR_WHITE;
    TFT_COLOR buttonTextInactive = COLOR_GRAY;
    TFT_COLOR buttonIndicatorActive = COLOR_WHITE;
    TFT_COLOR buttonIndicatorInactive = COLOR_GRAY;
    bool buttonSelectorFilled = true;
    TFT_COLOR paramNameColor = COLOR_GRAY_MEDIUM;
    TFT_COLOR paramValueColor = COLOR_WHITE;
    TFT_COLOR paramValueChangedColor = COLOR_YELLOW;
    TFT_COLOR headerBackground = COLOR_BLUE_DARK;
    TFT_COLOR headerInstrumentColor = COLOR_YELLOW;
    TFT_COLOR headerPresetName = COLOR_WHITE;
    TFT_COLOR headerBorderBottom = COLOR_NONE;
    TFT_COLOR backgroundColor = COLOR_BLACK;
};

enum CommonPage { COMMON_PAGE_NONE = -1, COMMON_PAGE_ARP_FX = 0, COMMON_PAGE_COUNT };

struct EditorParamInfo {
    const char* name;
    const char* const* names;
    ParameterDisplayType displayType;
    const char* const* valueNames;
    const unsigned char* valueOrder;
    const unsigned char* valueOrderReversed;
    float min;
    float max;
    float step;
    float incValue;
    uint8_t nameCount;
};

enum class EditorSelectorKind : uint8_t {
    None = 0,
    Operator,
    MatrixRow,
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
    virtual ~EditorEngine() {
    }

    // Generic slot for encoder `encoder` on the current page.
    EditorSlot slotForEncoder(int encoder) const;
    // Engine implemented by this editor.
    virtual SynthEngineType getEngineType() const = 0;
    // Display name of the engine.
    virtual const char* getEngineName() const = 0;
    // Engine UI colours.
    virtual const EditorTheme& getTheme() const {
        return theme_;
    }
    // State of a page button. Engine-page ids are stored per editor so that
    // switching engines cannot reuse a stale, out-of-range state index.
    uint8_t getButtonState(uint8_t buttonId) const;
    // Same, clamped to the page's state count so a stale index from another
    // engine can never be used to index page->states[].
    uint8_t getButtonState(const struct EditorPageButton* page) const;
    // Menu for an engine-owned main page, or nullptr.
    virtual const struct EditorMenu* getEngineMenu(int page) const = 0;
    // Map an engine main page to a shared page, or COMMON_PAGE_NONE.
    virtual int getCommonPage(int page) const {
        return COMMON_PAGE_NONE;
    }
    // Type of bottom-row selector for `button`, if any.
    virtual EditorSelectorKind selectorKind(int button) const {
        (void)button;
        return EditorSelectorKind::None;
    }
    uint8_t editPage() const {
        return editPage_;
    }
    void setEditPage(uint8_t page) {
        editPage_ = page;
        updatePage();
    }
    uint8_t operatorNumber() const {
        return operatorNumber_;
    }
    void setOperatorNumber(uint8_t op) {
        operatorNumber_ = op;
        updatePage();
    }
    void setEditPageSelected(int mainPage, int editPage) {
        if (mainPage >= 0 && mainPage < 6) {
            editPageSelected_[mainPage] = editPage;
        }
    }
    int editPageSelected(int mainPage) const {
        if (mainPage < 0 || mainPage >= 6) {
            return 0;
        }
        return editPageSelected_[mainPage];
    }

    // Bind the editor to its owner, shared state and display.
    virtual void init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) = 0;
    // Draw the timbre's algorithm (editors without one do nothing).
    virtual void drawAlgorithm() {
    }
    // Draw the engine title on the editor main page.
    virtual void displayEditorTitle();
    // Page header prefix ("Op N" on operator pages, ...).
    virtual void printPageHeader() {
    }
    // Draw one page button.
    virtual void drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) = 0;
    // Draw one parameter name.
    virtual void printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                                    const struct EditorPageButton* page) = 0;
    // Extra work after the page header (operator highlight, hidden params, ...).
    virtual void preparePage() {
    }
    // Print the value of encoder `encoder`.
    virtual void displayParamValue(int encoder, TFT_COLOR color) = 0;
    // An engine-specific parameter changed (engine-defined paramId); update UI feedback.
    virtual void newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) {
        (void)refreshStatus;
        (void)timbre;
        (void)paramId;
        (void)context;
        (void)oldValue;
        (void)newValue;
    }
    // Tempo clock tick: refresh time-synced value feedback.
    virtual void slowTick() = 0;
    // Encoder `encoder` moved by `ticks`.
    virtual void encoderTurned(int encoder, int ticks) = 0;
    // Encoder moved while `button` is held (fine edit / page change).
    virtual void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {
    }
    // Button pressed.
    virtual void buttonPressed(int button) = 0;
    // Button held: copy/paste and other long-press actions.
    virtual void buttonLongPressed(int instrument, int button) {
    }
    // Redraw the oscilloscope background for the current page.
    virtual void refreshOscilloBG() = 0;
    // Called when the UI switches to another timbre.
    virtual void newTimbre() {
    }

    // Step-sequencer / arp-pattern cursor positions (PFM3/shared Arp page).
    virtual int getStepSelection(int which) const {
        return 0;
    }
    virtual int getPatternSelection() const {
        return 0;
    }

    // Generic page model (engine-neutral). Older editors may leave these unused.
    virtual const struct EditorMenu* getMenu(int page) const {
        (void)page;
        return nullptr;
    }
    // Update the current page's slots before they are read.
    virtual void updatePage() const {
    }
    virtual EditorParamInfo getParamInfo(EditorParamRef ref) const {
        (void)ref;
        EditorParamInfo info = {};
        return info;
    }
    virtual float getParamValue(EditorParamRef ref) const {
        (void)ref;
        return 0.0f;
    }
    virtual void setParamValue(EditorParamRef ref, float value, ParamSource src) {
        (void)ref;
        (void)value;
        (void)src;
    }
    virtual bool adjustParam(EditorParamRef ref, int ticks, ParamSource src) {
        (void)ref;
        (void)ticks;
        (void)src;
        return false;
    }
    virtual void drawRegion(uint16_t regionId, const Rect& rect) {
        (void)regionId;
        (void)rect;
    }
    virtual void customEncoderTurned(EditorSlot slot, int encoder, int ticks) {
        (void)slot;
        (void)encoder;
        (void)ticks;
    }
    virtual void paramChanged(EditorParamRef ref, float oldValue, float newValue, ParamSource src) {
        (void)ref;
        (void)oldValue;
        (void)newValue;
        (void)src;
    }

    // Linked list of registered editors (owned by ScreenEditor).
    EditorEngine* nextEditor_ = nullptr;

protected:
    uint8_t getEncoderX(int encoder) const {
        uint8_t x = encoder * 8;
        if (x >= 24) {
            x -= 24;
        }
        return x;
    }

    // Helpers for the current page; defined in EditorEngine.cpp.
    const struct EditorMenu* currentEditMenu() const;
    const struct EditorPageButton* currentPage() const;
    int currentTimbre() const;

    void drawOperatorSelector(int button, int maxOperator);
    void drawMatrixSelector(int button, uint8_t pageButtonId, int maxRow);
    void selectOperator(int button, int maxOperator);
    void drawEditorButtonDefault(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page);

    uint8_t& buttonStateRef(uint8_t buttonId);

    ScreenEditor* parent_ = nullptr;
    SynthState* synthState_ = nullptr;
    FirmwareTftDisplay* tft_ = nullptr;

    uint8_t editPage_ = 0;
    uint8_t operatorNumber_ = 0;
    uint8_t editPageSelected_[6] = {0, 0, 0, 0, 0, 0};
    uint8_t enginePageState_[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    EditorTheme theme_;
};

#endif
