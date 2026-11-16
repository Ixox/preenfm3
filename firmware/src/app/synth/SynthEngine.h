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
#include "PersistenceMagic.h"
#include "TimbreParams.h"

class Timbre;
class Voice;
class SynthState;

/*
 * Per-timbre sound engine interface.
 *
 * A Timbre owns one Preenfm3Engine and one MsfaSynthEngine and selects one at
 * runtime. The engine owns its format's parameters, per-voice state, note
 * allocation, block rendering and persistence.
 *
 * All calls are per note or per block, never per sample. MIDI decoding, voice
 * pool arbitration, UI and storage stay in Synth/Timbre.
 *
 * The interface is wider than a clean engine API would be: it had to fit the
 * PFM3 engine, which was originally tightly coupled to Timbre and Voice (matrix
 * sources, note curves, LFO usage, unison, deferred voice steal, ...). Most of
 * these methods only mirror that coupling; a new engine can leave the default
 * no-op implementations and override only the core note/render/persistence
 * methods.
 */
class SynthEngine {
public:
    virtual ~SynthEngine() {}

    // Called once at startup: bind the engine to its timbre and global state.
    virtual void init(Timbre *timbre, SynthState *synthState) = 0;

    // A preset/mixer was loaded or the timbre reset: rebuild derived state
    // (envelopes, context, per-voice data) from the new parameters.
    virtual void afterNewParamsLoad() = 0;

    // Attach the engine's per-voice state to a voice owned by this timbre.
    // Called when a voice is allocated or transferred from another timbre.
    virtual void bindVoice(Voice *voice) = 0;

    // MIDI note on/off (note 0..127, velocity 0..127). The engine allocates or
    // reuses voices, applies mono/unison/glide and updates the matrix.
    virtual void noteOn(char note, char velocity) = 0;
    virtual void noteOff(char note) = 0;

    // External voice lifecycle, dispatched to the engine that owns the voice.
    virtual void killVoice(Voice *voice) {}
    virtual void noteOffVoice(Voice *voice) {}
    virtual void noteOffVoiceQuick(Voice *voice) {}

    // Restart a voice after a deferred cross-timbre transfer.
    virtual void startVoiceAfterTransfer(Voice *voice, int note, float noteFrequency, uint8_t velocity, bool alreadyOff) {}

    // Render one block for all voices of the timbre.
    // Returns the number of voices that were playing.
    virtual uint8_t voicesNextBlock() = 0;

    // Called once per block before voicesNextBlock(): update per-block state
    // such as the voice matrices.
    virtual void prepareBlock() = 0;

    // Advance the glide of voices that are gliding (called each block).
    virtual void glide() = 0;

    // Apply the MAIN_GATE matrix destination to the timbre output block.
    virtual void gateFx() = 0;

    // Set a global matrix source on all voices (aftertouch, CC, ...).
    virtual void setMatrixSource(SourceEnum source, float value) = 0;

    // Set a matrix source on the voice mapped to MPE channel (1..16).
    virtual void setMatrixSourceMPE(uint8_t channel, SourceEnum source, float value) = 0;

    // A matrix destination changed: reset it on all voices.
    virtual void resetMatrixDestination(float oldValue) = 0;

    // Polyphonic aftertouch: update the voice currently playing `note`.
    virtual void setMatrixPolyAfterTouch(uint8_t note, float value) = 0;

    // MPE note on/off: map the channel to a voice, then play/stop it.
    virtual void noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) = 0;

    // A parameter changed: refresh cached/derived state if needed
    // (PFM3 only refreshes on algorithm, pan and filter changes).
    virtual void paramChanged(int row, int encoder, float oldValue, float newValue) {}

    // Write a flat parameter value into the engine's owner storage.
    // `index` is the flat index used by the editor/MIDI path (row * 4 + encoder).
    virtual void setParamValue(int index, float value) {}

    // Engine-specific helpers used by the shared timbre code.
    virtual bool isUnisonMode() const { return false; }
    virtual void setStepSequencerSteps(int which, const struct StepSequencerSteps &steps) {}

    // Flat OneSynthParams mirror (PFM3 UI/storage layout). Engines that do not
    // use the flat mirror keep the default no-op implementations.
    virtual void serializeFlatParams(struct OneSynthParams &out) const {}
    virtual void deserializeFlatParams(const struct OneSynthParams &in) {}

    // Prepare a voice's FX matrix before rendering (once per block).
    virtual void prepareFxMatrix(Voice *voice) {}

    // Re-apply note/velocity matrix sources after a voice was bound/transferred.
    virtual void updateVoiceMatrix(Voice *voice, int note, int velocity) {}

    // True if a stolen voice may finish its release with the old timbre while
    // the new timbre starts (deferred cross-timbre transfer).
    virtual bool supportsDeferredSteal() const { return false; }

    // An LFO/step parameter changed: forward the new value to the voices.
    virtual void lfoValueChange(int row, int encoder, float value) {}

    // Recompute the MIDI note curve table (scale 0 or 1) used by matrix sources.
    virtual void updateMidiNoteScale(int scale) {}

    // Recompute which LFOs the matrix actually uses (CPU optimisation).
    virtual void verifyLfoUsed(int encoder, float oldValue, float newValue) {}

    // Whether an LFO is used by the matrix (skip its processing when unused).
    virtual bool isLfoUsed(int lfo) { return false; }

    // MIDI clock sync for LFOs, step sequencers and arpeggiators.
    virtual void midiClockContinue(int songPosition) {}
    virtual void midiClockStart() {}
    virtual void midiClockStop() {}
    virtual void midiClockSongPositionStep(int songPosition) {}

    // Read a matrix destination from the last played voice (shared FX/filter).
    virtual float getMatrixDestination(int destination) { return 0.0f; }

    /*
     * Persistence.
     *
     * A preset/mixer slot is ALIGNED_PATCH_SIZE (1024) bytes:
     *   [0 .. maxSize)            engine blob (maxSize = ALIGNED_PATCH_SIZE - 5)
     *   [ALIGNED_PATCH_SIZE - 5)  4-byte magic, written by the storage layer
     *
     * The engine owns the blob layout and must store the 12-byte preset name
     * at getStorageNameOffset(magic); storage adds the NUL terminator.
     * `shared` is the timbre-wide state (Arp, FX1, FX2, presetName, see
     * TimbreParams.h); include it in the blob if the format needs it.
     */

    // Unique engine id stored at the end of the slot; selects the engine on load.
    virtual uint32_t getStorageMagic() const = 0;

    // Byte offset of the 12-byte preset name inside the blob for `magic`.
    // Return -1 if `magic` is unknown. Engines may support legacy magics.
    virtual int getStorageNameOffset(uint32_t magic) const = 0;

    // Name displayed when the stored name is empty (e.g. "MSFA"). May be "".
    virtual const char* getStorageDefaultName() const = 0;

    // Write the on-flash engine bytes (without magic) into `out`.
    // Must not write more than `maxSize` bytes.
    // Returns the number of bytes written.
    virtual uint32_t getStorageBlob(uint8_t *out, uint32_t maxSize, const TimbreSharedParams &shared) = 0;

    // Load from `in` (`size` valid bytes; magic already read by storage).
    // `magic` lets the engine accept its legacy formats.
    // Return false on unknown/invalid data; storage then keeps the current preset.
    virtual bool loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic) = 0;
};

#endif
