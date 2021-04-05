/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <.> hosxe < a t > gmail.com)
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

#ifndef TIMBRE_H_
#define TIMBRE_H_

#include "Common.h"

#include "Osc.h"
#include "Env.h"
#include "Lfo.h"
#include "LfoOsc.h"
#include "LfoEnv.h"
#include "LfoEnv2.h"
#include "LfoStepSeq.h"
#include "Matrix.h"
#include "note_stack.h"
#include "event_scheduler.h"

extern float panTable[];
class Voice;

enum {
    CLOCK_OFF,
    CLOCK_INTERNAL,
    CLOCK_EXTERNAL
};

class Timbre {
    friend class Synth;
    friend class Voice;
public:
    Timbre();
    virtual ~Timbre();
    void init(SynthState *synthState, int timbreNumber);
    void setVoiceNumber(int v, int n);
    void initVoicePointer(int n, Voice *voice);
    void updateArpegiatorInternalClock();
    void cleanNextBlock();
    void prepareMatrixForNewBlock();
    uint8_t voicesNextBlock();
    void glide();
    void voicesToTimbre(float volumeGain);
    void gateFx();
    void afterNewParamsLoad();
    void setNewValue(int index, struct ParameterDisplay *param, float newValue);
    void setNewEffecParam(int encoder);
    int getSeqStepValue(int whichStepSeq, int step);
    void setSeqStepValue(int whichStepSeq, int step, int value);
    // Arpegiator
    void arpeggiatorNoteOn(char note, char velocity);
    void arpeggiatorNoteOff(char note);
    void StartArpeggio();
    void StepArpeggio();
    void Start();
    void arpeggiatorSetHoldPedal(uint8_t value);
    void setLatchMode(uint8_t value);
    void setDirection(uint8_t value);
    void setNewBPMValue(float bpm);
    void setArpeggiatorClock(float bpm);
    void resetArpeggiator();
    uint16_t getArpeggiatorPattern() const;

    void noteOn(char note, char velocity);
    void noteOff(char note);

    void noteOnMPE(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOffMPE(uint8_t channel, uint8_t note, uint8_t velocityOff);

    void preenNoteOn(char note, char velocity);
    inline void preenNoteOnUpdateMatrix(int voiceToUse, int note, int velocity);
    void preenNoteOff(char note);

    void numberOfVoicesChanged(uint8_t newNumberOfVoices) {
        if (likely(newNumberOfVoices > 0)) {
            numberOfVoiceInverse_ = 1.0f / (float) newNumberOfVoices;
        } else {
            numberOfVoiceInverse_ = 1.0f;
        }
        numberOfVoices_ = newNumberOfVoices;
    }

    void lfoValueChange(int currentRow, int encoder, float newValue);

    void setHoldPedal(int value);

    void resetMatrixDestination(float oldValue);

    void setMatrixSource(enum SourceEnum source, float newValue);
    void setMatrixSourceMPE(uint8_t channel, enum SourceEnum source, float newValue);

    void setMatrixPolyAfterTouch(uint8_t note, float newValue);
    void verifyLfoUsed(int encoder, float oldValue, float newValue);

    void midiClockStop() {
        OnMidiStop();
    }

    void midiClockContinue(int songPosition);
    void midiClockStart();
    void midiClockSongPositionStep(int songPosition);

    struct OneSynthParams* getParamRaw() {
        return &params_;
    }

    float* getSampleBlock() {
        return sampleBlock_;
    }

    const float* getSampleBlock() const {
        return sampleBlock_;
    }

    int8_t voiceNumber_[MAX_NUMBER_OF_VOICES];

    // Midi note response
    // Midi Note Scale
    void updateMidiNoteScale(int scale);

    // Do matrix use LFO
    bool isLfoUsed(int lfo) {
        return lfoUSed_[lfo] > 0;
    }

    uint8_t getLowerNote() {
        return lowerNote_;
    }
    float getLowerNoteFrequency() {
        return lowerNoteFrequency;
    }

    char* getPresetName() {
        return params_.presetName;
    }

    float getNumberOfVoiceInverse() {
        return numberOfVoiceInverse_;
    }

    bool isUnisonMode() {
    	return params_.engine1.playMode == PLAY_MODE_UNISON;
    }

    void stopPlayingNow();


private:

    // MiniPal Arpegiator
    void SendLater(uint8_t note, uint8_t velocity, uint8_t when, uint8_t tag);
    void SendScheduledNotes();
    void FlushQueue();
    void Tick();
    void OnMidiContinue();
    void OnMidiStart();
    void OnMidiStop();
    void OnMidiClock();
    void SendNote(uint8_t note, uint8_t velocity);

    int8_t timbreNumber_;
    struct OneSynthParams params_;
    struct MixerState *mixerState_;
    float sampleBlock_[BLOCK_SIZE * 2];
    float *sbMax_;
    float numberOfVoiceInverse_;
    // numberOfVoices is to be used instead of params.engine1.numberOfVoice
    // Because numberOfVoices can be just incremented, and we're not sure the voice is ready.
    // numberOfVoices is incremented after the new voices are initialized and ready to use.
    float numberOfVoices_;

    float mixerGain_;
    Voice *voices_[MAX_NUMBER_OF_VOICES];
    bool holdPedal_;
    int8_t lastPlayedNote_;

    // 6 oscillators Max
    Osc osc1_;
    Osc osc2_;
    Osc osc3_;
    Osc osc4_;
    Osc osc5_;
    Osc osc6_;

    // And their 6 envelopes
    Env env1_;
    Env env2_;
    Env env3_;
    Env env4_;
    Env env5_;
    Env env6_;

    // Must recompute LFO steps ?
    bool recomputeNext_;
    float currentGate_;
    // Arpeggiator

    // TO REFACTOR
    float ticksPerSecond_;
    float ticksEveryNCalls_;
    int ticksEveyNCallsInteger_;

    float arpegiatorStep_;
    NoteStack note_stack_;
    EventScheduler event_scheduler_;

    uint8_t running_;
    uint8_t latch_;
    uint8_t tick_;
    uint8_t idle_ticks_;
    uint16_t bitmask_;
    int8_t current_direction_;
    int8_t current_octave_;
    int8_t current_step_;
    int8_t start_step_;
    uint8_t ignore_note_off_messages_;
    uint8_t recording_;

    // lfoUsed
    uint8_t lfoUSed_[NUMBER_OF_LFO];
    uint8_t lowerNote_;
    float lowerNoteFrequency;
    bool lowerNoteReleased_;
    // static
    static uint32_t voiceIndex_;

    // Unison phase
    static float unisonPhase[14];
};

#endif /* TIMBRE_H_ */
