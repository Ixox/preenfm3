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

enum { TFT_NUMBER_OF_WAVEFORM_EXT = 14,
    TFT_DRAW_ENVELOPPE,
    TFT_DRAW_LFO,
    TFT_DRAW_LFO_ENVELOPE};



struct WaveFormExt {
    float* waveForms;
    int size;
};


enum {
    TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM = TFT_STANDARD_LAST_ACTION
};


/*
 * Add specific firmware display features
 * Added 12/2022 to access env user waveforms
 */
class FirmwareTftDisplay : public TftDisplay {
public:
    FirmwareTftDisplay();
    virtual ~FirmwareTftDisplay();

    void initWaveFormExt(int index, float* waveform, int size);

    void oscilloBgActionOperatorShape(int wfNumber);
    void oscilloBgActionEnvelope();
    void oscilloBgActionLfo();
    void oscilloBgActionClear();

    void oscilloBgSetEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL, int8_t aCurve, int8_t dCurve, int8_t sCurve, int8_t rCurve);
    void oscilloBgSetLfoEnvelope(float a, float d, float s, float r, float aL, float dL, float sL, float rL);
    void oscilloBgSetLfo(float shape, float freq, float kSyn, float bias, float phase);

    void additionalActions();

private:
    void oscilloBgDrawOperatorShape(float* waveForm, int size);
    void oscilloBgDrawEnvelope();
    void oscilloBgDrawLfo();
    void oscilloFillWithRand(int randtype);
    void oscilloBgDrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    void oscilloBgDrawVerticalLine(int16_t x1, int16_t y1, int16_t y2, uint16_t color);
    void oscilloBgDrawEnvStep(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, int8_t curve);

    bool oscilloIsClean;
    WaveFormExt waveForm[TFT_NUMBER_OF_WAVEFORM_EXT];
    float oscilParams1[6];
    float oscilParams2[6];
    uint8_t envCurve[4];
};

#endif /* HARDWARE_FIRMWARETFTDISPLAY_H_KO_ */
