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

#define UINT_MAX  4294967295
#define NUMBER_OF_STORED_NOT 6

class Sequencer;

class Synth : public SynthParamListener, public SynthStateAware
{
public:
    Synth(void);
    virtual ~Synth(void);

    void setSynthState(SynthState* synthState) {
        SynthStateAware::setSynthState(synthState);
        init(synthState);
    }

    void setSequencer(Sequencer* sequencer) {
        this->sequencer = sequencer;
    }

    void noteOn(int timbre, char note, char velocity);
    void noteOff(int timbre, char note);
    void noteOnFromSequencer(int timbre, char note, char velocity);
    void noteOffFromSequencer(int timbre, char note);
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


    void newParamValueFromExternal(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue) {
        newParamValue(timbre, currentRow, encoder, param, oldValue, newValue);
    }

    void newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
        newMixerValue(mixerValueType, timbre, oldValue, newValue);
    }

    void newParamValue(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue);
    void newMixerValue(uint8_t mixerValue, uint8_t timbre, float oldValue, float newValue);
    void newMixerEdit(int oldButton, int newButton) {};


    int getFreeVoice();
    void rebuidVoiceAllTimbre();
    void rebuidVoiceTimbre(int timbre);

    int getNumberOfFreeVoicesForThisTimbre(int timbre);

    void newTimbre(int timbre);

    void beforeNewParamsLoad(int timbre);
    void afterNewParamsLoad(int timbre);
    void afterNewMixerLoad();
    void showAlgo() { }
    void showIMInformation() {}

    void midiClockSetSongPosition(int songPosition);
    void midiClockContinue(int songPosition);
    void midiClockStart();
    void midiClockStop();
    void midiTick();

    void midiClockSongPositionStep(int songPosition) {
        for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
            timbres[t].midiClockSongPositionStep(songPosition);
        }
    }

    inline int leftSampleAtReadCursor() const {
        return this->samples[this->readCursor];
    }

    inline int rightSampleAtReadCursor() const {
        return this->samples[this->readCursor + 1];
    }

    void incReadCursor() {
        this->readCursor = (this->readCursor + 2) & 255;
    }

    inline int getSampleCount() {
        if (this->readCursor > this->writeCursor) {
            return this->writeCursor - this->readCursor + 256;
        } else {
            return this->writeCursor - this->readCursor;
        }
    }

    Timbre* getTimbre(int timbre) {
        return &timbres[timbre];
    }

    Timbre* getTimbres() {
        return timbres;
    }

    void setNewValueFromMidi(int timbre, int row, int encoder, float newValue);
    void setNewMixerValueFromMidi(int timbre, int mixerValue, float newValue);
    void setNewStepValueFromMidi(int timbre, int whichStepSeq, int step, int newValue);
    void setNewSymbolInPresetName(int timbre, int index, int value);
    void loadPreenFMPatchFromMidi(int timbre, int bank, int bankLSB, int patchNumber);
    void setHoldPedal(int timbre, int value);

    void setCurrentInstrument(int value);

#ifdef DEBUG
    void debugVoice();
    void showCycles();
#endif

    // For Oscilloscope to sync refresh
    uint8_t getLowerNote(int t) {
    	return this->timbres[t].getLowerNote();
    }

    float getLowerNoteFrequency(int t) {
        return this->timbres[t].getLowerNoteFrequency();
    }

    uint8_t getNumberOfPlayingVoices() {
        return numberOfPlayingVoices;
    }

    float getCpuUsage() {
        return cpuUsage;
    }


private:
    // Called by setSynthState
    void init(SynthState* synthState);
    void mixAndPan(int32_t *dest, float* source, float &pan, float sampleMultipler);
    float filteredVolume[NUMBER_OF_TIMBRES];
    Voice voices[MAX_NUMBER_OF_VOICES];
    Timbre timbres[NUMBER_OF_TIMBRES];

    // 4 buffer or 32 stero int = 64*4 = 256
    // sample Buffer
    volatile int readCursor;
    volatile int writeCursor;
    int samples[256];

    // gate
    float currentGate;

    // Cpu usage
    uint32_t totalCyclesUsedInSynth;
    uint16_t cptCpuUsage;
    uint8_t numberOfPlayingVoices;
    float cpuUsage;
    CYCCNT_buffer cycles_all;
    float totalNumberofCyclesInv;

    // Sequencer
    Sequencer* sequencer;

    // remember notes before changing timbre
    char noteBeforeNewParalsLoad[NUMBER_OF_STORED_NOT];
    char velocityBeforeNewParalsLoad[NUMBER_OF_STORED_NOT];

    float smoothPan[NUMBER_OF_TIMBRES];
    float smoothVolume[NUMBER_OF_TIMBRES];

};



#endif

