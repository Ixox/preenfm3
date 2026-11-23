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

// Ids of the `Action` slots handled by EditorParamPage.
enum class EditorActionId : uint8_t { LfoSync = 0, StepSync = 1 };

// `Custom` slot families, selected by EditorSlot::context.
enum class EditorCustomContext : uint8_t { ArpPattern = 0, ArpAdvanced = 1 };

/*
 * Generic menu/encoder page mechanics over the flat Pfm3Preset model.
 * Used by the PFM3 editor and by the shared Arp/Fx page. Engine-specific
 * pages override the drawing/editing hooks they need.
 */
class EditorParamPage : public EditorEngine {
public:
    EditorParamPage();

    void drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) override;
    void printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                            const struct EditorPageButton* page) override;
    void displayParamValue(int encoder, TFT_COLOR color) override;
    void newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) override;
    void slowTick() override;
    void encoderTurned(int encoder, int ticks) override;
    void buttonPressed(int button) override;
    void buttonLongPressed(int instrument, int button) override;
    void preparePage() override;
    void refreshOscilloBG() override;
    int getStepSelection(int which) const override;
    int getPatternSelection() const override {
        return patternSelect_;
    }

    // Bridge from the physical encoder index to the generic slot model.
    EditorParamInfo paramInfoFor(int encoder) const;

protected:
    void encoderTurnedForStepSequencer(int whichStepSeq, int encoder, int ticks);
    void encoderTurnedForCustom(int encoder, const EditorSlot& slot, int ticks);
    void displayCustomValue(int encoder, const EditorSlot& slot, TFT_COLOR color);
    void getArpStepDisplay(int index, bool factory, uint16_t& mask, uint16_t& accent, int& length, int modes[16]) const;
    bool isInArpPatternPage() const;
    bool copyArpUserPattern();
    bool pasteArpUserPattern();
    void queueArpStepsBg();
    void queueArpStepsBg(const ArpBgUpdate& update);
    struct Pfm3Preset* pfm3Preset() const;
    void encoderTurnedFlat(int flatIndex, int ticks);
    void encoderTurnedCustomAction(EditorActionId action, int ticks);
    void applyTicks(int flatIndex, int ticks);
    void updateEncoderValueWithoutCursor(int encoderInRow, int element, const EditorParamInfo& info, float oldValue, float newFloatValue);
    void resetHideParams();

    bool hideParam_[NUMBER_OF_ENCODERS];
    uint8_t valueChangedCounter_[NUMBER_OF_ENCODERS];
    float lfoFreqPreviousValue_[5];
    char patternSelect_;
    arp_pattern_t arpPatternBuffer_;
    uint8_t arpStepModesBuffer_[16];
    uint8_t arpPatternLengthBuffer_;
    bool arpPatternBufferValid_;
};

#endif
