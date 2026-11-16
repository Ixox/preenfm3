/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SYNTH_ENGINE_H_
#define SYNTH_ENGINE_H_

#include <stdint.h>

#include "Common.h"

class Timbre;
class Voice;
class SynthState;

/*
 * Per-timbre sound engine interface.
 * Calls are per note or per block, never per sample.
 */
class SynthEngine {
public:
    virtual ~SynthEngine() {}

    virtual void init(Timbre *timbre, SynthState *synthState) = 0;
    virtual void afterNewParamsLoad() = 0;
    virtual void bindVoice(Voice *voice) = 0;

    virtual void noteOn(char note, char velocity) = 0;
    virtual void noteOff(char note) = 0;

    virtual uint8_t voicesNextBlock() = 0;

    virtual void prepareBlock() = 0;
    virtual void glide() = 0;
    virtual void gateFx() = 0;

    virtual void setMatrixSource(SourceEnum source, float value) = 0;
    virtual void setMatrixSourceMPE(uint8_t channel, SourceEnum source, float value) = 0;
    virtual void resetMatrixDestination(float oldValue) = 0;
    virtual void setMatrixPolyAfterTouch(uint8_t note, float value) = 0;
    virtual void noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) = 0;

    virtual void newParamValue(int row, int encoder) {}
    virtual void prepareFxMatrix(Voice *voice) {}
    virtual void updateVoiceMatrix(Voice *voice, int note, int velocity) {}
    virtual bool supportsDeferredSteal() const { return false; }

    virtual void lfoValueChange(int row, int encoder, float value) {}
    virtual void updateMidiNoteScale(int scale) {}
    virtual void verifyLfoUsed(int encoder, float oldValue, float newValue) {}
    virtual bool isLfoUsed(int lfo) { return false; }
    virtual void midiClockContinue(int songPosition) {}
    virtual void midiClockStart() {}
    virtual void midiClockStop() {}
    virtual void midiClockSongPositionStep(int songPosition) {}
    virtual float getMatrixDestination(int destination) { return 0.0f; }
};

#endif
