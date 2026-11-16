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
#include "VoicePool.h"

#include "SynthParamListener.h"
#include "SynthStateAware.h"
#include "dwt.h"

#include "SimpleComp.h"

#define UINT_MAX               4294967295
#define NUMBER_OF_STORED_NOTES 6

class Sequencer;

class Synth : public SynthParamListener, public SynthStateAware {
public:
    Synth(void);
    virtual ~Synth(void);

    void setSynthState(SynthState* synthState) {
        SynthStateAware::setSynthState(synthState);
        init(synthState);
    }

    void setSequencer(Sequencer* sequencer) {
        this->sequencer_ = sequencer;
    }

    void noteOn(int timbre, char note, char velocity);
    void noteOff(int timbre, char note);
    void noteOnFromSequencer(uint8_t timbre, int16_t note, uint8_t velocity);
    void noteOffFromSequencer(uint8_t timbre, int16_t note);
    void stopArpegiator(int timbre);
    void allNoteOff(int timbre);
    void allNoteOffQuick(int timbre);
    void allSoundOff();
    void allSoundOff(int timbre);
    bool isPlaying();
    uint8_t buildNewSampleBlock(int32_t* buffer1, int32_t* buffer2, int32_t* buffer3);

    // Overide SynthParamListener
    void playNote(int timbreNumber, char note, char velocity) {
        noteOn(timbreNumber, note, velocity);
    }
    void stopNote(int timbreNumber, char note) {
        if (note != 0) {
            noteOff(timbreNumber, note);
        }
    }

    void newPfm3ParamValueFromExternal(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue) {
        newPfm3ParamValue(timbre, currentRow, encoder, param, oldValue, newValue);
    }

    void newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
        newMixerValue(mixerValueType, timbre, oldValue, newValue);
    }

    void newPfm3ParamValue(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue);
    void newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue);
    void newEngineParamValueFromExternal(int timbre, int paramId, int context, float oldValue, float newValue) {
        newEngineParamValue(timbre, paramId, context, oldValue, newValue);
    }
    void newMixerValue(uint8_t mixerValue, uint8_t timbre, float oldValue, float newValue);
    void newMixerEdit(int oldButton, int newButton) {
    }

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

    void setNewMixerValueFromMidi(int timbre, int mixerValue, float newValue);
    void setNewSeqValueFromMidi(uint8_t timbre, uint8_t seqValue, uint8_t newValue);
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
    void init(SynthState* synthState);
    void mixAndPan(int32_t* dest, float* source, float& pan, float sampleMultipler);
    void processPendingTransfers();

    Voice voices_[MAX_NUMBER_OF_VOICES];
    VoicePool voicePool_;
    Timbre timbres_[NUMBER_OF_TIMBRES];

    // Cpu usage
    uint32_t totalCyclesUsedInSynth_;
    uint16_t cptCpuUsage_;
    uint8_t numberOfPlayingVoices_;
    float cpuUsage_;
    float totalNumberofCyclesInv_;
    CYCCNT_buffer cycles_all_;

    // Sequencer
    Sequencer* sequencer_;

    // remember notes before changing timbre
    char noteBeforeNewParalsLoad_[NUMBER_OF_STORED_NOTES];
    char velocityBeforeNewParalsLoad_[NUMBER_OF_STORED_NOTES];

    float smoothPan_[NUMBER_OF_TIMBRES];
    float smoothVolume_[NUMBER_OF_TIMBRES];
    float* fxSample_;

    chunkware_simple::SimpleComp instrumentCompressor_[NUMBER_OF_TIMBRES];
};

#endif
