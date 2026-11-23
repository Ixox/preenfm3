/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DRUM909_ENGINE_H_
#define DRUM909_ENGINE_H_

#include "Drum909Params.h"
#include "SynthEngine.h"

struct drum909;

/*
 * 9W9 TR-909 drum engine.
 *
 * The whole kit is carried by one voice (getMaxVoices() == 1). The carrier is
 * claimed on the first note and reused by later notes; the DSP rendering and
 * the 44.1 -> 47916 resampling come in Phase 5.
 */
class Drum909Engine : public SynthEngine {
public:
    Drum909Engine();

    void init(Timbre* timbre, SynthState* synthState) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice* voice) override;
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    void killVoice(Voice* voice) override;
    void noteOffVoice(Voice* voice) override;
    void noteOffVoiceQuick(Voice* voice) override;
    uint8_t voicesNextBlock() override;
    void prepareBlock() override {
    }
    void glide() override {
    }
    void gateFx() override {
    }
    void setMatrixSource(SourceEnum source, float value) override {
    }
    void setMatrixSourceMPE(uint8_t channel, SourceEnum source, float value) override {
    }
    void resetMatrixDestination(float oldValue) override {
    }
    void setMatrixPolyAfterTouch(uint8_t note, float value) override {
    }
    void noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) override;

    void engineParamChanged(int paramId, int context, float oldValue, float newValue) override;
    uint8_t* getPatchMemory() override;
    void patchMemoryChanged() override;

    uint8_t getMaxVoices() const override {
        return 1;
    }
    bool supportsStepSequencer() const override {
        return false;
    }
    bool reusesSameNoteVoice() const override {
        return false;
    }
    bool supportsDeferredSteal() const override {
        return false;
    }

    uint32_t getStorageMagic() const override;
    int getStorageNameOffset(uint32_t magic) const override;
    const char* getStorageDefaultName() const override;
    uint32_t getStorageBlob(uint8_t* out, uint32_t maxSize, const TimbreSharedParams& shared) override;
    bool loadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic) override;

private:
    static const int kResampleRing = 128;
    static const int kResampleMask = 127;
    static const int kResampleChunk = 32;

    void applyParams();
    void resetResampler();

    Timbre* timbre_;
    SynthState* synthState_;
    int timbreNumber_;
    Drum909Params params_;
    Voice* carrier_;
    struct drum909* kit_;

    // 44.1 kHz 909 -> PREENFM_FREQUENCY resampler state.
    float ringL_[kResampleRing];
    float ringR_[kResampleRing];
    uint32_t ringHead_;
    uint32_t ringTail_;
    float phase_;
};

#endif
