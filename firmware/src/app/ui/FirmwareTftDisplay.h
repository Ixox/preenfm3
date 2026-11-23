/*
 * Copyright 2022 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HARDWARE_FIRMWARETFTDISPLAY_H_
#define HARDWARE_FIRMWARETFTDISPLAY_H_

#include "TftDisplay.h"

// 14 is the number of op waveforms
enum { TFT_NUMBER_OF_WAVEFORM_EXT = 14, TFT_DRAW_ENVELOPPE, TFT_DRAW_LFO, TFT_DRAW_ARP_STEPS };

// Arp step grid drawn in the oscilloscope background.
#define ARP_STEP_BAR_MAX_HEIGHT 20
#define ARP_STEP_BAR_MODE_SIZE  4
#define ARP_STEP_PITCH          10
#define ARP_STEP_BAR_WIDTH      9

// Dirty description for the arp grid drawn in the oscilloscope background.
struct ArpBgUpdate {
    uint16_t stepMask;   // bit k: On/Off/Acc fill of step k
    uint16_t modeMask;   // bit k: 4 px mode strip of step k
    uint16_t cursorMask; // bits: arrow positions to clear
    uint16_t lengthMask; // bits L-1: end-line positions to clear
    bool full;           // full clear + redraw

    ArpBgUpdate() : stepMask(0), modeMask(0), cursorMask(0), lengthMask(0), full(false) {
    }
};

struct WaveFormExt {
    const float* waveForms;
    int size;
};

/*
 * Add specific firmware display features
 * Added 12/2022 to access env user waveforms
 */
class FirmwareTftDisplay : public TftDisplay {
public:
    FirmwareTftDisplay();
    virtual ~FirmwareTftDisplay();

    void initWaveFormExt(int index, const float* waveform, int size);

    void oscilloBgActionOperatorShape(int wfNumber);
    void oscilloBgActionEnvelope();
    void oscilloBgActionLfo();
    void oscilloBgArp(const ArpBgUpdate& update);
    void oscilloBgActionClear();

    void oscilloBgSetArpSteps(const uint8_t levels[16], const uint8_t colors[16], const uint8_t modeColors[16], uint16_t modeMask,
                              int selected, int length);

    void oscilloBgSetEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL, int8_t aCurve, int8_t dCurve,
                              int8_t sCurve, int8_t rCurve);
    void oscilloBgSetLfoEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL);
    void oscilloBgSetLfo(float shape, float freq, float kSyn, float bias, float phase);

    void additionalActions();
    void clearActions();

private:
    void oscilloBgDrawOperatorShape(const float* waveForm, int size);
    void oscilloBgDrawEnvelope();
    void oscilloBgDrawLfo();
    void queueArpSteps(bool full);
    void drawArpStepsFull();
    void drawArpStepsPartial();
    void drawArpStepCell(int k);
    void drawArpModeCell(int k);
    void drawArpEndLine(int length, bool draw);
    void clearArpArrow(int k);
    void drawArpArrow(int k);
    void drawArpBaselineSeparators();
    void oscilloFillWithRand(int randtype);
    void oscilloBgDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    void oscilloBgDrawVerticalLine(int16_t x1, int16_t y1, int16_t y2, uint16_t color);
    void oscilloBgDrawEnvStep(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, int8_t curve);

    bool oscilloIsClean_;
    WaveFormExt waveForm_[TFT_NUMBER_OF_WAVEFORM_EXT];
    float oscilParams1_[6];
    float oscilParams2_[6];
    uint8_t envCurve_[4];

    uint8_t arpStepLevels_[16];
    uint8_t arpStepColors_[16];
    uint8_t arpStepModeColors_[16];
    uint16_t arpStepModeMask_;
    uint8_t arpStepSelected_;
    uint8_t arpStepLength_;

    ArpBgUpdate arpPending_;
    int envInQueue_;
    int lfoInQueue_;
    int operatorInQueue_;
    int arpFullInQueue_;
    int arpPartialInQueue_;
};

#endif /* HARDWARE_FIRMWARETFTDISPLAY_H_KO_ */
