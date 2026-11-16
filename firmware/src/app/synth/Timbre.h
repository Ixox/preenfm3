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
#include "TimbreParams.h"
#include "SynthEngine.h"
#include "MixerState.h"
#include "note_stack.h"
#include "event_scheduler.h"
#include "Pfm3Preset.h"
#include "SharedParams.h"

extern float panTable[];
class Voice;
class VoicePool;

enum {
    CLOCK_OFF,
    CLOCK_INTERNAL,
    CLOCK_EXTERNAL
};


class Timbre {
    friend class Dx7SynthEngine;
    friend class Preenfm3Engine;
    friend class ByosEngine;
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

    // PFM3 implementations called by Preenfm3Engine.
    void afterNewParamsLoad();
    void setParamValue(int index, float oldValue, float newValue);
    void setFlatParamFromMidi(int row, int encoder, float newValue);
    bool getSharedParamRowEncoder(SharedParamId param, int &row, int &encoder) const;
    bool decodeSharedControlChange(uint8_t cc, uint8_t value, SharedParamId &param, float &scaledValue) const;
    void syncArpUserPatterns();
    void setStepSequencerSteps(int whichStepSeq);
    void handleNullFlatParam(int row);
    void resetPfm3PresetToDefault();
    void setPresetNameChar(int index, char c);
    void engineParamChanged(int paramId, int context, float oldValue, float newValue);
    void serializePfm3Preset(struct Pfm3Preset &out) const;
    void deserializePfm3Preset(const struct Pfm3Preset &in);
    uint32_t getStorageMagic() const;
    uint32_t getStorageBlob(uint8_t *out, uint32_t maxSize);
    bool loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic);
    int getStorageNameOffset(uint32_t magic) const;
    const char* getStorageDefaultName(uint32_t magic) const;
    void applyPfm3Preset(const struct Pfm3Preset &flat);
    void setSharedParams(const TimbreSharedParams &shared);
    void setNewEffectParam(int encoder);
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

    void setEngine(SynthEngineType engine);
    SynthEngineType getEngine() const {
        return engine_;
    }

    int8_t getTimbreNumber() const {
        return timbreNumber_;
    }

    bool supportsDeferredSteal() const {
        return engineImpl_->supportsDeferredSteal();
    }

    bool acceptsFlatParamFromMidi(int row) const {
        return engineImpl_->acceptsFlatParamFromMidi(row);
    }

    bool supportsStepSequencer() const {
        return engineImpl_->supportsStepSequencer();
    }

    bool midiControlChange(uint8_t cc, uint8_t value);

    bool midiNrpn(uint16_t param, uint16_t value) {
        return engineImpl_->midiNrpn(timbreNumber_, param, value);
    }

    bool midiPfm3ParamFeedback(int row, int encoder, float newValue, struct MidiParamMessage &out) const {
        return engineImpl_->midiPfm3ParamFeedback(row, encoder, newValue, out);
    }

    bool midiPatchParam(int timbre, int index, struct MidiParamMessage &out) const {
        return engineImpl_->midiPatchParam(timbre, index, out);
    }

    bool midiParamFeedback(int paramId, int context, float newValue, struct MidiParamMessage &out) const {
        return engineImpl_->midiParamFeedback(paramId, context, newValue, out);
    }

    void loadDx7Patch(const uint8_t *packedPatch) {
        engineImpl_->loadDx7Patch(packedPatch);
    }
    uint8_t* getPatchMemory() {
        return engineImpl_->getPatchMemory();
    }
    void patchMemoryChanged() {
        engineImpl_->patchMemoryChanged();
    }

    void noteOnMPE(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOffMPE(uint8_t channel, uint8_t note, uint8_t velocityOff);

    inline void engineNoteOn(char note, char velocity) {
        engineImpl_->noteOn(note, velocity);
    }

    inline void engineNoteOff(char note) {
        engineImpl_->noteOff(note);
    }

    void initVoicePool(VoicePool *pool) {
        voicePool_ = pool;
    }

    void numberOfVoicesChanged(uint8_t newNumberOfVoices);
    void trimActiveVoices(int count);

    int getMaxVoices() const {
        return maxVoices_;
    }
    NoteStack& getNoteStack() {
        return note_stack_;
    }
    void processMidiClock() {
        OnMidiClock();
    }
    void applyTimbreFx() {
        fxAfterBlock();
    }
    bool adoptVoiceFromPool(Voice *voice, int voiceIndex) {
        return adoptVoice(voice, voiceIndex);
    }
    void dropVoiceToPool(int voiceIndex) {
        dropVoice(voiceIndex);
    }
    int getNumberOfVoices() const {
        return (int) numberOfVoices_;
    }
    int getVoiceNumber(int index) const {
        return voiceNumber_[index];
    }

    void removeActiveVoice(int voiceIndex);
    void clearActiveVoices();

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

    struct Pfm3Preset* getPfm3Preset() {
        return &pfm3Preset_;
    }

    TimbreSharedParams& sharedParams() {
        return sharedParams_[timbreNumber_];
    }
    const TimbreSharedParams& sharedParams() const {
        return sharedParams_[timbreNumber_];
    }


    void updateVoiceMatrix(Voice *voice, int note, int velocity) {
        engineImpl_->updateVoiceMatrix(voice, note, velocity);
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
        return engineImpl_->isLfoUsed(lfo);
    }

    uint8_t getLowerNote() {
        return lowerNote_;
    }
    float getLowerNoteFrequency() {
        return lowerNoteFrequency;
    }

    char* getPresetName() {
        return pfm3Preset_.presetName;
    }

    float getNumberOfVoiceInverse() {
        return numberOfVoiceInverse_;
    }

    bool isUnisonMode() {
    	return engineImpl_->isUnisonMode();
    }

    void stopPlayingNow();
    void allNoteOff();
    void allNoteOffQuick();
    void allSoundOff();
    void noteOffVoice(Voice *voice);
    void noteOffVoiceQuick(Voice *voice);
    void killVoice(Voice *voice);
    void startVoiceAfterTransfer(Voice *voice, int note, float noteFrequency, uint8_t velocity, float phase, bool alreadyOff);
    void completeVoiceTransfer(Voice *voice, int poolIndex, VoicePool *pool);


    void setMPESetting(uint8_t mpeSetting) {
        mpeSetting_ = mpeSetting;
    }

    uint8_t getMPESetting() {
        return mpeSetting_;
    }

private:

    int allocateVoice(int note);
    int reuseVoice(int voiceIndex);
    bool adoptVoice(Voice *voice, int voiceIndex);
    void dropVoice(int voiceIndex);
    int countPendingVoices() const;
    bool ensureActiveVoices(int count);
    void clearMpeMapping(int voiceIndex);

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

    /** --------------FX conf--------------  */
    void fxAfterBlock();
    float delayInterpolation(float readPos, float buffer[], int bufferLenM1);
    float delayInterpolation2(float readPos, float buffer[], int bufferLenM1, int offset);
    float iirFilter(float x, float a0, float *yn1, float *yn2, float *xn1, float *xn2) ;

    int prevFx2Type         = 0;

    #define delayBufferSize 2048

    static float delayBuffer[NUMBER_OF_TIMBRES][delayBufferSize];
    float *delayBuffer_;

    float param1S = 0;
    float matrixFilterFrequencyS = 0;
    float param2S = 0;
    float delaySize1 = 0, delaySize2 = 0, delaySize3 = 0;
    float feedback            = 0;
    float shift = 0,   shift2 = 0;
    int delayWritePos         = 0;
    float delayWritePosF      = 0;
    float delayReadPos        = 0;
    float delayReadPos2       = 0;
    float delayReadFrac       = 0;
    float delayReadFrac2      = 0;
     
    float low1 = 0, band1 = 0;
    float low2 = 0, band2 = 0;
    float low3 = 0, band3 = 0;
    float low4 = 0, band4 = 0;
    float low5 = 0, band5 = 0;
    float low6 = 0, band6 = 0;

    float feedbackInput = 0;

    // hp filter
    float hp_in_y0 = 0;
    float hp_in_y1 = 0;
    float hp_in_x1 = 0;
    float hp_in2_y0 = 0;
    float hp_in2_y1 = 0;
    float hp_in2_x1 = 0;

    // allpass filters
    float _ly1 = 0;
    float _lx1 = 0;
    float _ly2 = 0;
    float _lx2 = 0;

    // frequency shifter / lowpass filter (hb1 - hb3) / allpass hb4 / hipass hb5
    float hb1_x1 = 0, hb1_x2 = 0, hb1_y1 = 0, hb1_y2 = 0;
    float hb2_x1 = 0, hb2_x2 = 0, hb2_y1 = 0, hb2_y2 = 0;
    float hb3_x1 = 0, hb3_x2 = 0, hb3_y1 = 0, hb3_y2 = 0;
    float hb4_x1 = 0, hb4_x2 = 0, hb4_y1 = 0, hb4_y2 = 0;
    float hb5_x1 = 0, hb5_x2 = 0, hb5_y1 = 0, hb5_y2 = 0;
    float hb6_x1 = 0, hb6_x2 = 0, hb6_y1 = 0, hb6_y2 = 0;
    float hb7_x1 = 0, hb7_x2 = 0, hb7_y1 = 0, hb7_y2 = 0;
    float hb8_x1 = 0, hb8_x2 = 0, hb8_y1 = 0, hb8_y2 = 0;
    float phase1 = 0;
    float samplen1 = 0;

    // diffuser
    int inputWritePos1     = 0;
    int inputWritePos2     = 0;
    int inputWritePos3     = 0;
    int inputWritePos4     = 0;
    int inputWritePos5     = 0;
    const int inputBufferLen1 = 126;
    const int inputBufferLen2 = 180;
    const int inputBufferLen3 = 204;
    const int inputBufferLen4 = 245;
    const int inputBufferLen5 = 269;

    // granulator
    enum GRAIN_PARAMS {
        GRAIN_RAMP = 0,
        GRAIN_POS,
        GRAIN_SIZE,
        GRAIN_CURRENT_SHIFT,
        GRAIN_NEXT_SHIFT,
        GRAIN_INC,
        GRAIN_VOL,
        GRAIN_PAN
    };

    uint8_t grainNext = 0, grainPrev = 0;
    float grainTable[3][8] = {
        {1, 1, 1, 0, 0, 0, 1, 0.5f},
        {1, 1, 1, 0, 0, 0, 1, 0.5f},
        {1, 1, 1, 0, 0, 0, 1, 0.5f}
    };
    float lockA, lockB, loopSize = 20;
    
    /** --------------end of FX conf--------------  */

    int8_t timbreNumber_;
    SynthEngineType engine_;
    SynthEngine *engineImpl_;
    // Flat Pfm3Preset mirror for the editor and storage.
    // The PFM3 DSP state is owned by Pfm3TimbreParams in Preenfm3Engine.
    struct Pfm3Preset pfm3Preset_;
    // Engine-neutral per-timbre parameters: arpeggiator, FX1/FX2 rows and
    // preset name. Static because Timbre lives in DTCMRAM and this block is
    // too large to inline; the array itself is placed in .ram_d1.
    static TimbreSharedParams sharedParams_[NUMBER_OF_TIMBRES];
    // Note-to-frequency scale (MTS) table for this instrument. Engines need
    // it to start notes, so it is the only MixerState they read.
    struct MixerState *mixerState_;
    SynthState *synthState_;
    float sampleBlock_[BLOCK_SIZE * 2];
    float *sbMax_;
    float numberOfVoiceInverse_;
    // numberOfVoices_ is the number of voices currently owned by this timbre.
    // maxVoices_ is the configured polyphony limit from the mixer.
    float numberOfVoices_;
    int maxVoices_;
    VoicePool *voicePool_;

    float mixerGain_;
    Voice *voices_[MAX_NUMBER_OF_VOICES];
    int8_t mpeChannelVoice_[16];
    bool holdPedal_;
    int8_t lastPlayedNote_;

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

    uint8_t lowerNote_;
    float lowerNoteFrequency;
    bool lowerNoteReleased_;
    // static
    static uint32_t voiceIndex_;

    uint8_t mpeSetting_;

};

#endif /* TIMBRE_H_ */
