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

#include "stm32h7xx_hal.h"
#include "Synth.h"
#include "Menu.h"
#include "Sequencer.h"

extern RNG_HandleTypeDef hrng;
extern float noise[32];

Synth::Synth(void) {
}

Synth::~Synth(void) {
}

inline
float sqrt3(const float x)
{
  union
  {
    int i;
    float x;
  } u;

  u.x = x;
  u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
  return u.x;
}

void Synth::init(SynthState *synthState) {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        for (uint16_t k = 0; k < (sizeof(struct OneSynthParams) / sizeof(float)); k++) {
            ((float*) &timbres_[t].params_)[k] = ((float*) &preenMainPreset)[k];
        }
        timbres_[t].init(synthState, t);
        for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
            timbres_[t].initVoicePointer(v, &voices_[v]);
        }
    }

    newTimbre(0);
    for (int k = 0; k < MAX_NUMBER_OF_VOICES; k++) {
        voices_[k].init();
    }
    rebuidVoiceAllTimbre();
    // Here synthState is already initialized
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres_[t].numberOfVoicesChanged(this->synthState_->mixerState.instrumentState_[t].numberOfVoices);
        smoothVolume_[t] = 0.0f;
        smoothPan_[t] = 0.0f;

        // Default is No compressor
        // We set the sample rate /32 because we update the env only once per BLOCK
        instrumentCompressor_[t].setSampleRate(PREENFM_FREQUENCY / 32.0f);
        instrumentCompressor_[t].setRatio(1.0f);
        instrumentCompressor_[t].setThresh(1000.0f);
        instrumentCompressor_[t].setAttack(10.0);
        instrumentCompressor_[t].setRelease(100.0);
    }

    // Cpu usage
    cptCpuUsage_ = 0;
    totalCyclesUsedInSynth_ = 0;
    numberOfPlayingVoices_ = 0;
    cpuUsage_ = 0.0f;
    totalNumberofCyclesInv_ = 1 / (SystemCoreClock * 32.0f * PREENFM_FREQUENCY_INVERSED);

}

void Synth::noteOn(int timbre, char note, char velocity) {
    if (synthState_->fullState.synthMode == SYNTH_MODE_SEQUENCER) {
        sequencer_->insertNote(timbre, note, velocity);
    }
    timbres_[timbre].noteOn(note, velocity);

}

void Synth::noteOff(int timbre, char note) {
    if (synthState_->fullState.synthMode == SYNTH_MODE_SEQUENCER) {
        sequencer_->insertNote(timbre, note, 0);
    }
    timbres_[timbre].noteOff(note);
}

void Synth::noteOnFromSequencer(uint8_t timbre, int16_t note, uint8_t velocity) {
    if (likely(note > 0 & note < 127)) {
        timbres_[timbre].noteOn(note, velocity);
    }
}

void Synth::noteOffFromSequencer(uint8_t timbre, int16_t note) {
    if (likely(note > 0 & note < 127)) {
        timbres_[timbre].noteOff(note);
    }
}

void Synth::midiClockContinue(int songPosition, bool tellSequencer) {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres_[t].midiClockContinue(songPosition);
    }
    if (tellSequencer) {
        sequencer_->onMidiContinue(songPosition);
    }
}

void Synth::midiClockStart(bool tellSequencer) {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres_[t].midiClockStart();
    }
    if (tellSequencer) {
        sequencer_->onMidiStart();
    }
}

void Synth::midiClockStop(bool tellSequencer) {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres_[t].midiClockStop();
    }
    if (tellSequencer) {
        sequencer_->onMidiStop();
    }
}

void Synth::midiTick(bool tellSequencer) {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres_[t].OnMidiClock();
    }
    if (tellSequencer) {
        sequencer_->onMidiClock();
    }
}

void Synth::midiClockSetSongPosition(int songPosition, bool tellSequencer) {
    if (tellSequencer) {
        sequencer_->midiClockSetSongPosition(songPosition);
    }
}

void Synth::setHoldPedal(int timbre, int value) {
    timbres_[timbre].setHoldPedal(value);
}

void Synth::allNoteOffQuick(int timbre) {
    int numberOfVoices = this->synthState_->mixerState.instrumentState_[timbre].numberOfVoices;
    for (int k = 0; k < numberOfVoices; k++) {
        // voice number k of timbre
        int n = timbres_[timbre].voiceNumber_[k];
        if (voices_[n].isPlaying()) {
            voices_[n].noteOffQuick();
        }
    }
}

void Synth::stopArpegiator(int timbre) {
    timbres_[timbre].resetArpeggiator();
}


void Synth::allNoteOff(int timbre) {
    int numberOfVoices = this->synthState_->mixerState.instrumentState_[timbre].numberOfVoices;
    for (int k = 0; k < numberOfVoices; k++) {
        // voice number k of timbre
        int n = timbres_[timbre].voiceNumber_[k];
        if (voices_[n].isPlaying() && !voices_[n].isReleased()) {
            voices_[n].noteOff();
        }
    }
}

void Synth::allSoundOff(int timbre) {
    int numberOfVoices = this->synthState_->mixerState.instrumentState_[timbre].numberOfVoices;
    for (int k = 0; k < numberOfVoices; k++) {
        // voice number k of timbre
        int n = timbres_[timbre].voiceNumber_[k];
        voices_[n].killNow();
    }
}

void Synth::allSoundOff() {
    for (int k = 0; k < MAX_NUMBER_OF_VOICES; k++) {
        voices_[k].killNow();
    }
}

bool Synth::isPlaying() {
    for (int k = 0; k < MAX_NUMBER_OF_VOICES; k++) {
        if (voices_[k].isPlaying()) {
            return true;
        }
    }
    return false;
}

void Synth::mixAndPan(int32_t *dest, float *source, float &pan, float sampleMultipler) {
    float sampleR, sampleL;
    if (likely(pan == 0)) {
        for (int s = 0; s < BLOCK_SIZE; s++) {
            sampleL = *(source++);
            sampleR = *(source++);

            *dest++ += sampleR * sampleMultipler;
            *dest++ += sampleL * sampleMultipler;
        }
        // No need for final calcul
        return;
    } else if (likely(pan > 0)) {
        float oneMinusPan = 1 - pan;
        for (int s = 0; s < BLOCK_SIZE; s++) {
            sampleL = *(source++);
            sampleR = *(source++);

            *dest++ += (sampleR + sampleL * pan) * sampleMultipler;
            *dest++ += sampleL * oneMinusPan * sampleMultipler;
        }
    } else if (pan < 0) {
        float onePlusPan = 1 + pan;
        float minusPan = -pan;
        for (int s = 0; s < BLOCK_SIZE; s++) {
            sampleL = *(source++);
            sampleR = *(source++);
            *dest++ += sampleR * onePlusPan * sampleMultipler;
            *dest++ += (sampleL + sampleR * minusPan) * sampleMultipler;
        }
    }
    //     final calcul
    //     pan never become null because of smoothing
    int closeToZero = pan * 100000;
    if (closeToZero == 0) {
        pan = 0.0f;
    }
}

/**
 * return : outputSaturated bit field
 */

uint8_t Synth::buildNewSampleBlock(int32_t *buffer1, int32_t *buffer2, int32_t *buffer3) {
    uint8_t outputSaturated = 0;

    CYCLE_MEASURE_START(cycles_all_)
    ;

    // We consider the random number is always ready here...
    uint32_t random32bit;
    if (unlikely(HAL_RNG_GenerateRandomNumber(&hrng, &random32bit) != HAL_OK)) {
        // Recreate on rnd from previous pass
        random32bit = noise[31] * 0x7fffffff;
    }

    noise[0] = (random32bit & 0xffff) * .000030518f - 1.0f; // value between -1 and 1.
    noise[1] = (random32bit >> 16) * .000030518f - 1.0f; // value between -1 and 1.
    for (int noiseIndex = 2; noiseIndex < 32;) {
        random32bit = 214013 * random32bit + 2531011;
        noise[noiseIndex++] = (random32bit & 0xffff) * .000030518f - 1.0f; // value between -1 and 1.
        noise[noiseIndex++] = (random32bit >> 16) * .000030518f - 1.0f; // value between -1 and 1.
    }

    numberOfPlayingVoices_ = 0;
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {

        // timbres_[t].cleanNextBlock();
        if (likely(this->synthState_->mixerState.instrumentState_[t].numberOfVoices > 0)) {
            timbres_[t].updateArpegiatorInternalClock();
            // optionally glide
            timbres_[t].glide();
            //
            timbres_[t].prepareMatrixForNewBlock();
            // render all voices in their own buffer
            numberOfPlayingVoices_ += timbres_[t].voicesNextBlock();
        }
    }

    // Dispatch the timbres on the different out !!

    int32_t *cb1 = buffer1;
    const int32_t *endcb1 = buffer1 + 64;
    int32_t *cb2 = buffer2;
    const int32_t *endcb2 = buffer2 + 64;
    int32_t *cb3 = buffer3;
    const int32_t *endcb3 = buffer3 + 64;

    while (cb1 < endcb1) {
        *cb1++ = 0;
        *cb1++ = 0;
        *cb2++ = 0;
        *cb2++ = 0;
        *cb3++ = 0;
        *cb3++ = 0;
    }

    // fxBus - prepare empty mixing block
    FxBus* fxBus = &this->synthState_->mixerState.fxBus_;
    fxBus->mixSumInit();

    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        // numberOfVoices = 0; => timbre is disabled
        if (this->synthState_->mixerState.instrumentState_[timbre].numberOfVoices == 0) {
            continue;
        }
        //Gate and mixer are per timbre
        if (abs(smoothVolume_[timbre] - synthState_->mixerState.instrumentState_[timbre].volume) < .0005f) {
            smoothVolume_[timbre] = synthState_->mixerState.instrumentState_[timbre].volume;
        } else {
            smoothVolume_[timbre] = smoothVolume_[timbre] * .9f
                + synthState_->mixerState.instrumentState_[timbre].volume * .1f;
        }

        // We divide by 5 to have headroom before saturating (>1.0f)
        timbres_[timbre].voicesToTimbre(smoothVolume_[timbre] * .2f);
        timbres_[timbre].gateFx();
        timbres_[timbre].fxAfterBlock();

        // Smooth pan to avoid audio noise
        smoothPan_[timbre] = smoothPan_[timbre] * .95f
            + ((float) synthState_->mixerState.instrumentState_[timbre].pan / 63.0f) * .05f;

        float *sampleFromTimbre = timbres_[timbre].getSampleBlock();

        // Even without compressor we call this meethod
        // It allows to retrieve the volume in DB
        instrumentCompressor_[timbre].processPfm3(sampleFromTimbre);

        // Send to bus fx, to mix with other timbres
        fxBus->mixAdd(timbres_[timbre].getSampleBlock(), synthState_->mixerState.instrumentState_[timbre].send, synthState_->mixerState.reverbLevel_);
    }

    // fxBus - mixing block process
    switch (synthState_->mixerState.reverbOutput_) {
    case 0:
        fxBus->processBlock(buffer1);
        break;
    case 1:
        fxBus->processBlock(buffer2);
        break;
    case 2:
        fxBus->processBlock(buffer3);
        break;
    }

    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        float *sampleFromTimbre = timbres_[timbre].getSampleBlock();

        // Max is 0x7fffff * [-1:1]
        //float sampleMultipler = (float) 0x7fffff;
        float sampleMultipler = panTable[(int)((1 - synthState_->mixerState.instrumentState_[timbre].send) * 255)] * (float) 0x7fffff;

        switch (synthState_->mixerState.instrumentState_[timbre].out) {
            // 0 => out1+out2, 1 => out1, 2=> out2
            // 3 => out3+out4, 4 => out3, 5=> out4
            // 6 => out5+out6, 7 => out5, 8=> out8
            case 0:
                cb1 = buffer1;
                while (cb1 < endcb1) {
                    cb1++;
                    *cb1++ += (int32_t) ((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                }
                break;
            case 1:
                mixAndPan(buffer1, sampleFromTimbre, smoothPan_[timbre], sampleMultipler);
                break;
            case 2:
                cb1 = buffer1;
                while (cb1 < endcb1) {
                    *cb1++ += (int32_t) ((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                    cb1++;
                }
                break;
            case 3:
                cb2 = buffer2;
                while (cb2 < endcb2) {
                    cb2++;
                    *cb2++ += (int32_t) ((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                }
                break;
            case 4:
                mixAndPan(buffer2, sampleFromTimbre, smoothPan_[timbre], sampleMultipler);
                break;
            case 5:
                cb2 = buffer2;
                while (cb2 < endcb2) {
                    *cb2++ += (int32_t) ((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                    cb2++;
                }
                break;
            case 6:
                cb3 = buffer3;
                while (cb3 < endcb3) {
                    cb3++;
                    *cb3++ += (int32_t) ((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                }
                break;
            case 7:
                mixAndPan(buffer3, sampleFromTimbre, smoothPan_[timbre], sampleMultipler);
                break;
            case 8:
                cb3 = buffer3;
                while (cb3 < endcb3) {
                    *cb3++ += (int32_t) ((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                    cb3++;
                }
                break;
        }
    }
    /*
     * Let's check the clipping
     */
    cb1 = buffer1;
    cb2 = buffer2;
    cb3 = buffer3;

    while (cb1 < endcb1) {
        if (unlikely(*cb1 > 0x7FFFFF)) {
            *cb1 = 0x7FFFFF;
            outputSaturated |= 0b001;
        }
        if (unlikely(*cb1 < -0x7FFFFF)) {
            *cb1 = -0x7FFFFF;
            outputSaturated |= 0b001;
        }
        *cb1 <<= 8;
        cb1++;

        if (unlikely(*cb2 > 0x7FFFFF)) {
            *cb2 = 0x7FFFFF;
            outputSaturated |= 0b010;
        }
        if (unlikely(*cb2 < -0x7FFFFF)) {
            *cb2 = -0x7FFFFF;
            outputSaturated |= 0b010;
        }
        *cb2 <<= 8;
        cb2++;

        if (unlikely(*cb3 > 0x7FFFFF)) {
            *cb3 = 0x7FFFFF;
            outputSaturated |= 0b100;
        }
        if (unlikely(*cb3 < -0x7FFFFF)) {
            *cb3 = -0x7FFFFF;
            outputSaturated |= 0b100;
        }
        *cb3 <<= 8;
        cb3++;
    }

    CYCLE_MEASURE_END();

    // Take 100 of them to have a 0-100 percent value
    totalCyclesUsedInSynth_ += cycles_all_.remove();
    if (cptCpuUsage_++ == 100) {
        cpuUsage_ = totalNumberofCyclesInv_ * ((float) totalCyclesUsedInSynth_);
        cptCpuUsage_ = 0;
        totalCyclesUsedInSynth_ = 0;
    }

    return outputSaturated;
}

int Synth::getNumberOfFreeVoicesForThisTimbre(int timbre) {
    int maxNumberOfFreeVoice = 0;
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        if (t != timbre) {
            maxNumberOfFreeVoice += this->synthState_->mixerState.instrumentState_[t].numberOfVoices;
        }
    }
    return MAX_NUMBER_OF_VOICES - maxNumberOfFreeVoice;

}

void Synth::beforeNewParamsLoad(int timbre) {

    for (int k = 0; k < NUMBER_OF_STORED_NOTES; k++) {
        noteBeforeNewParalsLoad_[k] = 0;
        velocityBeforeNewParalsLoad_[k] = 0;
    }

    if (this->synthState_->params->engineArp1.clock > 0) {
        // Arpegiator : we store pressed key
        int numberOfPressedNote = timbres_[timbre].note_stack_.size();
        for (int k = 0; k < numberOfPressedNote && k < NUMBER_OF_STORED_NOTES; k++) {
            const NoteEntry &noteEntry = timbres_[timbre].note_stack_.played_note(k);
            noteBeforeNewParalsLoad_[k] = noteEntry.note;
            velocityBeforeNewParalsLoad_[k] = noteEntry.velocity;
        }
    } else {
        int numberOfVoices = this->synthState_->mixerState.instrumentState_[timbre].numberOfVoices;
        for (int k = 0; k < numberOfVoices && k < NUMBER_OF_STORED_NOTES; k++) {
            // voice number k of timbre
            int n = timbres_[timbre].voiceNumber_[k];
            if (voices_[n].isPlaying() && !voices_[n].isReleased()) {
                noteBeforeNewParalsLoad_[k] = voices_[n].getNote();
                velocityBeforeNewParalsLoad_[k] = voices_[n].getMidiVelocity();
            }
        }
    }

    timbres_[timbre].resetArpeggiator();
    // Stop all voices
    allNoteOffQuick(timbre);
    // Let's allow the buffer to catch up
    // We can do that because we're in the lower priority thread
    HAL_Delay(5);
}
;

void Synth::afterNewParamsLoad(int timbre) {

    timbres_[timbre].afterNewParamsLoad();
    // values to force check lfo used
    timbres_[timbre].verifyLfoUsed(ENCODER_MATRIX_SOURCE, 0.0f, 1.0f);

    for (int k = 0; k < NUMBER_OF_STORED_NOTES; k++) {
        if (noteBeforeNewParalsLoad_[k] != 0) {
            timbres_[timbre].noteOn(noteBeforeNewParalsLoad_[k], velocityBeforeNewParalsLoad_[k]);
            noteBeforeNewParalsLoad_[k] = 0;
        }
    }

}

void Synth::afterNewMixerLoad() {

    // Set to 0 before rebuilding all voices/timbre relation
    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        timbres_[timbre].numberOfVoicesChanged(0);
    }

    rebuidVoiceAllTimbre();

    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        synthState_->scalaSettingsChanged(timbre);
        timbres_[timbre].numberOfVoicesChanged(this->synthState_->mixerState.instrumentState_[timbre].numberOfVoices);
        timbres_[timbre].afterNewParamsLoad();
        // values to force check lfo used
        timbres_[timbre].verifyLfoUsed(ENCODER_MATRIX_SOURCE, 0.0f, 1.0f);
        // Update compressor
        newMixerValue(MIXER_VALUE_COMPRESSOR, timbre, -1,
            this->synthState_->mixerState.instrumentState_[timbre].compressorType);
    }

    // Update MPE
    newMixerValue(MIXER_VALUE_GLOBAL_SETTINGS_1, 0, -1, this->synthState_->mixerState.MPE_inst1_);

    // Update Reverb local variables
    this->synthState_->mixerState.fxBus_.paramChanged();
}

int Synth::getFreeVoice() {
    // Loop on all voices
    bool used[MAX_NUMBER_OF_VOICES];
    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        used[v] = false;
    }

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        // Must be different from 0 and -1
        int nv = this->synthState_->mixerState.instrumentState_[t].numberOfVoices;

        for (int v = 0; v < nv; v++) {
            used[timbres_[t].voiceNumber_[v]] = true;
        }
    }

    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        if (!used[v]) {
            return v;
        }
    }

    return -1;
}

void Synth::newParamValue(int timbre, int currentRow, int encoder, ParameterDisplay *param, float oldValue,
    float newValue) {
    switch (currentRow) {
        case ROW_ARPEGGIATOR1:
            switch (encoder) {
                case ENCODER_ARPEGGIATOR_CLOCK:
                    allNoteOff(timbre);
                    timbres_[timbre].setArpeggiatorClock((uint8_t) newValue);
                    break;
                case ENCODER_ARPEGGIATOR_BPM:
                    timbres_[timbre].setNewBPMValue((uint8_t) newValue);
                    break;
                case ENCODER_ARPEGGIATOR_DIRECTION:
                    timbres_[timbre].setDirection((uint8_t) newValue);
                    break;
            }
            break;
        case ROW_ARPEGGIATOR2:
            if (unlikely(encoder == ENCODER_ARPEGGIATOR_LATCH)) {
                timbres_[timbre].setLatchMode((uint8_t) newValue);
            }
            break;
        case ROW_ARPEGGIATOR3:
            break;
        case ROW_EFFECT1:
            timbres_[timbre].setNewEffecParam(encoder);
            break;
        case ROW_ENV1_TIME:
        case ROW_ENV1_LEVEL:
            timbres_[timbre].env1_.reloadADSR(encoder);
            break;
        case ROW_ENV2_TIME:
        case ROW_ENV2_LEVEL:
            timbres_[timbre].env2_.reloadADSR(encoder);
            break;
        case ROW_ENV3_TIME:
        case ROW_ENV3_LEVEL:
            timbres_[timbre].env3_.reloadADSR(encoder);
            break;
        case ROW_ENV4_TIME:
        case ROW_ENV4_LEVEL:
            timbres_[timbre].env4_.reloadADSR(encoder);
            break;
        case ROW_ENV5_TIME:
        case ROW_ENV5_LEVEL:
            timbres_[timbre].env5_.reloadADSR(encoder);
            break;
        case ROW_ENV6_TIME:
        case ROW_ENV6_LEVEL:
            timbres_[timbre].env6_.reloadADSR(encoder);
            break;
        case ROW_MATRIX_FIRST ... ROW_MATRIX_LAST:
            timbres_[timbre].verifyLfoUsed(encoder, oldValue, newValue);
            if (encoder == ENCODER_MATRIX_DEST1 || encoder == ENCODER_MATRIX_DEST2) {
                // Reset old destination
                timbres_[timbre].resetMatrixDestination(oldValue);
            }
            break;
        case ROW_LFOOSC1 ... ROW_LFOOSC3:
        case ROW_LFOENV1 ... ROW_LFOENV2:
        case ROW_LFOSEQ1 ... ROW_LFOSEQ2:
            // timbres[timbre].lfo[currentRow - ROW_LFOOSC1]->valueChanged(encoder);
            timbres_[timbre].lfoValueChange(currentRow, encoder, newValue);
            break;
        case ROW_PERFORMANCE1:
            timbres_[timbre].setMatrixSource((enum SourceEnum) (MATRIX_SOURCE_CC1 + encoder), newValue);
            break;
        case ROW_MIDINOTE1CURVE:
            timbres_[timbre].updateMidiNoteScale(0);
            break;
        case ROW_MIDINOTE2CURVE:
            timbres_[timbre].updateMidiNoteScale(1);
            break;
        case ROW_ENV1_CURVE:
            timbres_[timbre].env1_.applyCurves();
            break;
        case ROW_ENV2_CURVE:
            timbres_[timbre].env2_.applyCurves();
            break;
        case ROW_ENV3_CURVE:
            timbres_[timbre].env3_.applyCurves();
            break;
        case ROW_ENV4_CURVE:
            timbres_[timbre].env4_.applyCurves();
            break;
        case ROW_ENV5_CURVE:
            timbres_[timbre].env5_.applyCurves();
            break;
        case ROW_ENV6_CURVE:
            timbres_[timbre].env6_.applyCurves();
            break;

    }
}

// synth is the only one who knows timbres
void Synth::newTimbre(int timbre) {
    this->synthState_->setParamsAndTimbre(&timbres_[timbre].params_, timbre);
}

void Synth::newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    switch (valueType) {
        case MIXER_VALUE_GLOBAL_SETTINGS_1: {
            // Timbre is the settings number we Set
            uint8_t setting = timbre;
            // Did we change MPE Inst1
            if (setting == 0) {
                // Only Inst1 is impacted
                timbres_[0].setMPESetting((uint8_t) newValue);
            }
            break;
        }
        case MIXER_VALUE_GLOBAL_SETTINGS_3:
        	// Did we change the reverb preset
        	if(timbre == 0) {
        	    this->synthState_->mixerState.fxBus_.presetChanged(this->synthState_->mixerState.reverbPreset_);
        	} else if (timbre >= 3 && timbre <= 5) {
        	    this->synthState_->mixerState.fxBus_.paramChanged();
        	}
        	break;
        case MIXER_VALUE_GLOBAL_SETTINGS_4:
        case MIXER_VALUE_GLOBAL_SETTINGS_5:
            this->synthState_->mixerState.fxBus_.paramChanged();
        	break;
        case MIXER_VALUE_MIDI_CHANNEL:
        case MIXER_VALUE_MIDI_FIRST_NOTE:
        case MIXER_VALUE_MIDI_LAST_NOTE:
        case MIXER_VALUE_MIDI_SHIFT_NOTE:
            timbres_[timbre].resetArpeggiator();
            allNoteOff(timbre);
            break;
        case MIXER_VALUE_NUMBER_OF_VOICES:
            // If unison stop sound !
            if (timbres_[timbre].isUnisonMode()) {
                timbres_[timbre].stopPlayingNow();
            }

            if (newValue == oldValue) {
                return;
            } else if (newValue > oldValue) {
                for (int v = (int) oldValue; v < (int) newValue; v++) {
                    timbres_[timbre].setVoiceNumber(v, getFreeVoice());
                }
            } else {
                for (int v = (int) newValue; v < (int) oldValue; v++) {
                    voices_[timbres_[timbre].voiceNumber_[v]].killNow();
                    timbres_[timbre].setVoiceNumber(v, -1);
                }
            }

            timbres_[timbre].numberOfVoicesChanged(newValue);
            break;
        case MIXER_VALUE_COMPRESSOR:
            switch ((int) newValue) {
                // No
                case 0:
                    instrumentCompressor_[timbre].setRatio(1.0f);
                    // Must never be reached
                    instrumentCompressor_[timbre].setThresh(1000.0f);
                    instrumentCompressor_[timbre].setAttack(10.0);
                    instrumentCompressor_[timbre].setRelease(100.0);
                    break;
                case 1:
                    // Slow2
                    instrumentCompressor_[timbre].setRatio(.33f);
                    instrumentCompressor_[timbre].setThresh(-12.0f);
                    instrumentCompressor_[timbre].setAttack(100.0);
                    instrumentCompressor_[timbre].setRelease(1000.0);
                    break;
                case 2:
                    // Medium2
                    instrumentCompressor_[timbre].setRatio(.33f);
                    instrumentCompressor_[timbre].setThresh(-12.0f);
                    instrumentCompressor_[timbre].setAttack(8.0);
                    instrumentCompressor_[timbre].setRelease(200.0);
                    break;
                case 3:
                    // Fast4
                    instrumentCompressor_[timbre].setRatio(.33f);
                    instrumentCompressor_[timbre].setThresh(-12.0f);
                    instrumentCompressor_[timbre].setAttack(1.0);
                    instrumentCompressor_[timbre].setRelease(50.0);
                    break;
            }
            break;
    }
}

void Synth::rebuidVoiceAllTimbre() {
    int voiceNumber = 0;

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        int nv = this->synthState_->mixerState.instrumentState_[t].numberOfVoices;

        for (int v = 0; v < nv; v++) {
            timbres_[t].setVoiceNumber(v, voiceNumber++);
        }

        for (int v = nv; v < MAX_NUMBER_OF_VOICES; v++) {
            timbres_[t].setVoiceNumber(v, -1);
        }
    }
}

void Synth::loadPreenFMPatchFromMidi(int timbre, int bank, int bankLSB, int patchNumber) {
    this->synthState_->loadPresetFromMidi(timbre, bank, bankLSB, patchNumber, &timbres_[timbre].params_);
}

void Synth::setNewValueFromMidi(int timbre, int row, int encoder, float newValue) {
    struct ParameterDisplay *param = &(allParameterRows.row[row]->params[encoder]);
    int index = row * NUMBER_OF_ENCODERS_PFM2 + encoder;
    float oldValue = ((float*) this->timbres_[timbre].getParamRaw())[index];

    this->timbres_[timbre].setNewValue(index, param, newValue);
    float newNewValue = ((float*) this->timbres_[timbre].getParamRaw())[index];
    if (oldValue != newNewValue) {
        this->synthState_->propagateNewParamValueFromExternal(timbre, row, encoder, param, oldValue, newNewValue);
    }
}

void Synth::setNewMixerValueFromMidi(int timbre, int mixerValue, float newValue) {
    switch (mixerValue) {
        case MIXER_VALUE_VOLUME: {
            float oldValue = this->synthState_->mixerState.instrumentState_[timbre].volume;
            this->synthState_->mixerState.instrumentState_[timbre].volume = newValue;
            if (oldValue != newValue) {
                this->synthState_->propagateNewMixerValueFromExternal(timbre, mixerValue, oldValue, newValue);
            }
            break;
        }
        case MIXER_VALUE_PAN: {
            float oldValue = this->synthState_->mixerState.instrumentState_[timbre].pan;
            // Can be 64
            if (newValue > 63.0f) {
                newValue = 63.0f;
            }
            this->synthState_->mixerState.instrumentState_[timbre].pan = newValue;
            if (oldValue != newValue) {
                this->synthState_->propagateNewMixerValueFromExternal(timbre, mixerValue, oldValue, newValue);
            }
            break;
        }
        case MIXER_VALUE_SEND: {
            float oldValue = this->synthState_->mixerState.instrumentState_[timbre].send;
            this->synthState_->mixerState.instrumentState_[timbre].send = newValue;
            if (oldValue != newValue) {
                this->synthState_->propagateNewMixerValueFromExternal(timbre, mixerValue, oldValue, newValue);
            }
            break;
       }
    }
}


void Synth::setNewSeqValueFromMidi(uint8_t timbre, uint8_t seqValue, uint8_t newValue) {
    sequencer_->setNewSeqValueFromMidi(timbre, seqValue, newValue);
}

void Synth::setNewStepValueFromMidi(int timbre, int whichStepSeq, int step, int newValue) {

    if (whichStepSeq < 0 || whichStepSeq > 1 || step < 0 || step > 15 || newValue < 0 || newValue > 15) {
        return;
    }

    int oldValue = this->timbres_[timbre].getSeqStepValue(whichStepSeq, step);

    if (oldValue != newValue) {
        int oldStep = this->synthState_->stepSelect[whichStepSeq];
        this->synthState_->stepSelect[whichStepSeq] = step;
        if (oldStep != step) {
            this->synthState_->propagateNewParamValueFromExternal(timbre, ROW_LFOSEQ1 + whichStepSeq, 2, NULL, oldStep,
                step);
        }
        this->timbres_[timbre].setSeqStepValue(whichStepSeq, step, newValue);
        int newNewValue = this->timbres_[timbre].getSeqStepValue(whichStepSeq, step);
        if (oldValue != newNewValue) {
            this->synthState_->propagateNewParamValueFromExternal(timbre, ROW_LFOSEQ1 + whichStepSeq, 3, NULL, oldValue,
                newValue);
        }
    }
}

void Synth::setNewSymbolInPresetName(int timbre, int index, char newchar) {
    this->timbres_[timbre].getParamRaw()->presetName[index] = newchar;
}

void Synth::setCurrentInstrument(int value) {
    if (value >= 1 && value <= (NUMBER_OF_TIMBRES + 1)) {
        this->synthState_->setCurrentInstrument(value);
    }
}
