/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
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

#ifndef SYNTH_H_
#define SYNTH_H_

#include "Timbre.h"
#include "Voice.h"
#include "LfoOsc.h"
#include "LfoEnv.h"
#include "LfoStepSeq.h"

#include "SynthParamListener.h"
#include "SynthStateAware.h"
#include "dwt.h"

#include "SimpleComp.h"

#define UINT_MAX  4294967295
#define NUMBER_OF_STORED_NOTES 6

class Sequencer;

class Synth: public SynthParamListener, public SynthStateAware {
public:
    Synth(void);
    virtual ~Synth(void);

    void setSynthState(SynthState *synthState) {
        SynthStateAware::setSynthState(synthState);
        init(synthState);
    }

    void setSequencer(Sequencer *sequencer) {
        this->sequencer_ = sequencer;
    }

    void noteOn(int timbre, char note, char velocity);
    void noteOff(int timbre, char note);
    void noteOnFromSequencer(int timbre, char note, char velocity);
    void noteOffFromSequencer(int timbre, char note);
    void stopArpegiator(int timbre);
    void allNoteOff(int timbre);
    void allNoteOffQuick(int timbre);
    void allSoundOff();
    void allSoundOff(int timbre);
    bool isPlaying();
    uint8_t buildNewSampleBlock(int32_t *buffer1, int32_t *buffer2, int32_t *buffer3);

    // Overide SynthParamListener
    void playNote(int timbreNumber, char note, char velocity) {
        noteOn(timbreNumber, note, velocity);
    }
    void stopNote(int timbreNumber, char note) {
        if (note != 0) {
            noteOff(timbreNumber, note);
        }
    }

    void newParamValueFromExternal(int timbre, int currentRow, int encoder, ParameterDisplay *param, float oldValue, float newValue) {
        newParamValue(timbre, currentRow, encoder, param, oldValue, newValue);
    }

    void newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
        newMixerValue(mixerValueType, timbre, oldValue, newValue);
    }

    void newParamValue(int timbre, int currentRow, int encoder, ParameterDisplay *param, float oldValue, float newValue);
    void newMixerValue(uint8_t mixerValue, uint8_t timbre, float oldValue, float newValue);
    void newMixerEdit(int oldButton, int newButton) {
    }

    int getFreeVoice();
    void rebuidVoiceAllTimbre();

    int getNumberOfFreeVoicesForThisTimbre(int timbre);

    void newTimbre(int timbre);

    void beforeNewParamsLoad(int timbre);
    void afterNewParamsLoad(int timbre);
    void afterNewMixerLoad();
    void showAlgo() {
    }
    void showIMInformation() {
    }

    void midiClockSetSongPosition(int songPosition, bool tellSequencer);
    void midiClockContinue(int songPosition, bool tellSequencer);
    void midiClockStart(bool tellSequencer);
    void midiClockStop(bool tellSequencer);
    void midiTick(bool tellSequencer);

    void midiClockSongPositionStep(int songPosition) {
        for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
            timbres_[t].midiClockSongPositionStep(songPosition);
        }
    }

    Timbre* getTimbre(int timbre) {
        return &timbres_[timbre];
    }

    Timbre* getTimbres() {
        return timbres_;
    }

    void setNewValueFromMidi(int timbre, int row, int encoder, float newValue);
    void setNewMixerValueFromMidi(int timbre, int mixerValue, float newValue);
    void setNewStepValueFromMidi(int timbre, int whichStepSeq, int step, int value);
    void setNewSymbolInPresetName(int timbre, int index, char newchar);
    void loadPreenFMPatchFromMidi(int timbre, int bank, int bankLSB, int patchNumber);
    void setHoldPedal(int timbre, int value);

    void setCurrentInstrument(int value);

    // For Oscilloscope to sync refresh
    uint8_t getLowerNote(int t) {
        return this->timbres_[t].getLowerNote();
    }

    float getLowerNoteFrequency(int t) {
        return this->timbres_[t].getLowerNoteFrequency();
    }

    uint8_t getNumberOfPlayingVoices() {
        return numberOfPlayingVoices_;
    }

    float getCpuUsage() {
        return cpuUsage_;
    }

    chunkware_simple::SimpleComp& getCompInstrument(int t) {
        return instrumentCompressor_[t];
    }

private:
    // Called by setSynthState
    void init(SynthState *synthState);
    void mixAndPan(int32_t *dest, float *source, float &pan, float sampleMultipler);

    Voice voices_[MAX_NUMBER_OF_VOICES];
    Timbre timbres_[NUMBER_OF_TIMBRES];

    // Cpu usage
    uint32_t totalCyclesUsedInSynth_;
    uint16_t cptCpuUsage_;
    uint8_t numberOfPlayingVoices_;
    float cpuUsage_;
    float totalNumberofCyclesInv_;
    CYCCNT_buffer cycles_all_;

    // Sequencer
    Sequencer *sequencer_;

    // remember notes before changing timbre
    char noteBeforeNewParalsLoad_[NUMBER_OF_STORED_NOTES];
    char velocityBeforeNewParalsLoad_[NUMBER_OF_STORED_NOTES];

    float smoothPan_[NUMBER_OF_TIMBRES];
    float smoothVolume_[NUMBER_OF_TIMBRES];
    float *fxSample;

    chunkware_simple::SimpleComp instrumentCompressor_[NUMBER_OF_TIMBRES];
};

#endif

