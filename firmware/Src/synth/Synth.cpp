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



void Synth::init(SynthState* synthState) {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        for (uint16_t k = 0; k < (sizeof(struct OneSynthParams) / sizeof(float)); k++) {
            ((float*) &timbres[t].params)[k] = ((float*) &preenMainPreset)[k];
        }
        timbres[t].init(synthState, t);
        for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
            timbres[t].initVoicePointer(v, &voices[v]);
        }
    }

    newTimbre(0);
    this->writeCursor = 0;
    this->readCursor = 0;
    for (int k = 0; k < MAX_NUMBER_OF_VOICES; k++) {
        voices[k].init();
    }
    rebuidVoiceAllTimbre();
    // Here synthState is already initialized
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres[t].numberOfVoicesChanged(this->synthState->mixerState.instrumentState[t].numberOfVoices);
        smoothVolume[t] = 0.0f;
        smoothPan[t] = 0.0f;


        // Default is No compressor
        // We set the sample rate /32 because we update the env only once per BLOCK
        compInstrument[t].setSampleRate(PREENFM_FREQUENCY / 32.0f);
        compInstrument[t].setRatio(1.0f);
        compInstrument[t].setThresh(1000.0f);
        compInstrument[t].setAttack(10.0);
        compInstrument[t].setRelease(100.0);
    }

    // Cpu usage
    cptCpuUsage = 0;
    totalCyclesUsedInSynth = 0;
    numberOfPlayingVoices = 0;
    cpuUsage = 0.0f;
    totalNumberofCyclesInv = 1 / (SystemCoreClock * 32.0f * PREENFM_FREQUENCY_INVERSED);

}

void Synth::noteOn(int timbre, char note, char velocity) {
    sequencer->insertNote(timbre, note, velocity);
    timbres[timbre].noteOn(note, velocity);

}

void Synth::noteOff(int timbre, char note) {
    sequencer->insertNote(timbre, note, 0);
    timbres[timbre].noteOff(note);
}

void Synth::noteOnFromSequencer(int timbre, char note, char velocity) {
    timbres[timbre].noteOn(note, velocity);
}

void Synth::noteOffFromSequencer(int timbre, char note) {
    timbres[timbre].noteOff(note);
}

void Synth::midiClockContinue(int songPosition) {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres[t].midiClockContinue(songPosition);
    }
    sequencer->onMidiContinue(songPosition);
}


void Synth::midiClockStart() {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres[t].midiClockStart();
    }
    sequencer->onMidiStart();
}

void Synth::midiClockStop() {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres[t].midiClockStop();
    }
    sequencer->onMidiStop();
}

void Synth::midiTick() {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        timbres[t].OnMidiClock();
    }
    sequencer->onMidiClock();
}

void Synth::midiClockSetSongPosition(int songPosition) {
    sequencer->midiClockSetSongPosition(songPosition);
}

void Synth::setHoldPedal(int timbre, int value) {
    timbres[timbre].setHoldPedal(value);
}

void Synth::allNoteOffQuick(int timbre) {
    int numberOfVoices = this->synthState->mixerState.instrumentState[timbre].numberOfVoices;
    for (int k = 0; k < numberOfVoices; k++) {
        // voice number k of timbre
        int n = timbres[timbre].voiceNumber[k];
        if (voices[n].isPlaying()) {
            voices[n].noteOffQuick();
        }
    }
}

void Synth::allNoteOff(int timbre) {
    int numberOfVoices = this->synthState->mixerState.instrumentState[timbre].numberOfVoices;
    for (int k = 0; k < numberOfVoices; k++) {
        // voice number k of timbre
        int n = timbres[timbre].voiceNumber[k];
        if (voices[n].isPlaying() && !voices[n].isReleased()) {
            voices[n].noteOff();
        }
    }
}

void Synth::allSoundOff(int timbre) {
    int numberOfVoices = this->synthState->mixerState.instrumentState[timbre].numberOfVoices;
    for (int k = 0; k < numberOfVoices; k++) {
        // voice number k of timbre
        int n = timbres[timbre].voiceNumber[k];
        voices[n].killNow();
    }
}

void Synth::allSoundOff() {
    for (int k = 0; k < MAX_NUMBER_OF_VOICES; k++) {
        voices[k].killNow();
    }
}

bool Synth::isPlaying() {
    for (int k = 0; k < MAX_NUMBER_OF_VOICES; k++) {
        if (voices[k].isPlaying()) {
            return true;
        }
    }
    return false;
}


void Synth::mixAndPan(int32_t *dest, float* source, float &pan, float sampleMultipler) {
    float sampleR, sampleL;
    if (likely(pan == 0)) {
        for (int s = 0 ; s < BLOCK_SIZE ; s++) {
            sampleL = *(source++);
            sampleR = *(source++);

            *dest++ += sampleR * sampleMultipler;
            *dest++ += sampleL * sampleMultipler;
        }
        // No need for final calcul
        return;
    } else if (likely(pan > 0)) {
        float oneMinusPan = 1 - pan;
        for (int s = 0 ; s < BLOCK_SIZE ; s++) {
            sampleL = *(source++);
            sampleR = *(source++);

            *dest++ += (sampleR + sampleL * pan) * sampleMultipler;
            *dest++ += sampleL * oneMinusPan * sampleMultipler;
        }
    } else if (pan < 0) {
        float onePlusPan = 1 + pan;
        float minusPan = - pan;
        for (int s = 0 ; s < BLOCK_SIZE  ; s++) {
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

	CYCLE_MEASURE_START(cycles_all);

    // We consider the random number is always ready here...
	uint32_t random32bit;
	if (unlikely(HAL_RNG_GenerateRandomNumber(&hrng, &random32bit) != HAL_OK)) {
	    // Recreate on rnd from previous pass
	    random32bit = noise[31] *  0x7fffffff;
	}

	noise[0] =  (random32bit & 0xffff) * .000030518f - 1.0f; // value between -1 and 1.
    noise[1] = (random32bit >> 16) * .000030518f - 1.0f; // value between -1 and 1.
    for (int noiseIndex = 2; noiseIndex < 32; ) {
        random32bit = 214013 * random32bit + 2531011;
        noise[noiseIndex++] =  (random32bit & 0xffff) * .000030518f - 1.0f; // value between -1 and 1.
        noise[noiseIndex++] = (random32bit >> 16) * .000030518f - 1.0f; // value between -1 and 1.
    }




    for (int t=0; t<NUMBER_OF_TIMBRES; t++) {
        timbres[t].cleanNextBlock();
        if (likely(this->synthState->mixerState.instrumentState[t].numberOfVoices > 0)) {
            timbres[t].updateArpegiatorInternalClock();
            // eventually glide
            if (timbres[t].voiceNumber[0] != -1 && this->voices[timbres[t].voiceNumber[0]].isGliding()) {
                this->voices[timbres[t].voiceNumber[0]].glide();
            }
        }
        timbres[t].prepareMatrixForNewBlock();
    }

    // render all voices in their own sample block...
    numberOfPlayingVoices = 0;
    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        if (likely(this->voices[v].isPlaying())) {
            this->voices[v].nextBlock();
            this->voices[v].fxAfterBlock();
            numberOfPlayingVoices++;
        } else {
            this->voices[v].emptyBuffer();
        }
    }

	// Dispatch the timbres ont the different out !!

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


    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        // numberOfVoices = 0; => timbre is disabled
    	if (this->synthState->mixerState.instrumentState[timbre].numberOfVoices == 0) {
    		continue;
    	}
        //Gate and mixer are per timbre
    	if (abs(smoothVolume[timbre] - synthState->mixerState.instrumentState[timbre].volume) < .0005f)  {
    	    smoothVolume[timbre] = synthState->mixerState.instrumentState[timbre].volume;
    	} else {
    	    smoothVolume[timbre] = smoothVolume[timbre] * .9f + synthState->mixerState.instrumentState[timbre].volume * .1f;
    	}

        // We divide by 4 to have headroom before saturating (>1.0f)
        timbres[timbre].voicesToTimbre(smoothVolume[timbre] * .25f);
        timbres[timbre].gateFx();

        // Smooth pan to avoid audio noise
        smoothPan[timbre] = smoothPan[timbre] * .95f + ((float)synthState->mixerState.instrumentState[timbre].pan / 63.0f) * .05f;

        float *sampleFromTimbre = timbres[timbre].getSampleBlock();

        // Even without compressor we call this meethod
        // It allows to retrieve the volume in DB
        compInstrument[timbre].processPfm3(sampleFromTimbre);

        // Max is 0x7fffff * [-1:1]
        float sampleMultipler = (float)0x7fffff;

        switch (synthState->mixerState.instrumentState[timbre].out) {
        // 0 => out1+out2, 1 => out1, 2=> out2
        // 3 => out3+out4, 4 => out3, 5=> out4
        // 6 => out5+out6, 7 => out5, 8=> out8
        case 0:
            cb1 = buffer1;
            while (cb1 < endcb1) {
                cb1++;
                *cb1++ += (int32_t)((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
            }
        	break;
        case 1:
            mixAndPan(buffer1, sampleFromTimbre, smoothPan[timbre], sampleMultipler);
        	break;
        case 2:
            cb1 = buffer1;
            while (cb1 < endcb1) {
                *cb1++ += (int32_t)((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                cb1++;
            }
        	break;
        case 3:
            cb2 = buffer2;
            while (cb2 < endcb2) {
                cb2++;
                *cb2++ += (int32_t)((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
            }
        	break;
        case 4:
            mixAndPan(buffer2, sampleFromTimbre, smoothPan[timbre], sampleMultipler);
        	break;
        case 5:
            cb2 = buffer2;
            while (cb2 < endcb2) {
                *cb2++ += (int32_t)((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
                cb2++;
            }
        	break;
        case 6:
            cb3 = buffer3;
            while (cb3 < endcb3) {
                cb3++;
                *cb3++ += (int32_t)((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f * sampleMultipler);
            }
        	break;
        case 7:
            mixAndPan(buffer3, sampleFromTimbre, smoothPan[timbre], sampleMultipler);
        	break;
        case 8:
            cb3 = buffer3;
            while (cb3 < endcb3) {
                *cb3++ += (int32_t)((*sampleFromTimbre++ + *sampleFromTimbre++) * .5f  * sampleMultipler);
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
    totalCyclesUsedInSynth += cycles_all.remove();
    if (cptCpuUsage++ == 100) {
        cpuUsage = totalNumberofCyclesInv * ((float)totalCyclesUsedInSynth);
        cptCpuUsage = 0;
        totalCyclesUsedInSynth = 0;
    }

    return outputSaturated;
}

int Synth::getNumberOfFreeVoicesForThisTimbre(int timbre) {
    int maxNumberOfFreeVoice = 0;
    for (int t=0 ; t< NUMBER_OF_TIMBRES; t++) {
        if (t != timbre) {
            maxNumberOfFreeVoice += this->synthState->mixerState.instrumentState[t].numberOfVoices;
        }
    }
    return MAX_NUMBER_OF_VOICES - maxNumberOfFreeVoice;

}


void Synth::beforeNewParamsLoad(int timbre) {

    for (int k = 0; k < NUMBER_OF_STORED_NOT; k++) {
        noteBeforeNewParalsLoad[k] = 0;
        velocityBeforeNewParalsLoad[k] = 0;
    }


    if (this->synthState->params->engineArp1.clock > 0) {
        // Arpegiator : we store pressed key
        int numberOfPressedNote = timbres[timbre].note_stack.size();
        for (int k = 0; k < numberOfPressedNote && k < NUMBER_OF_STORED_NOT; k++) {
            const NoteEntry &noteEntry = timbres[timbre].note_stack.played_note(k);
            noteBeforeNewParalsLoad[k] = noteEntry.note;
            velocityBeforeNewParalsLoad[k] = noteEntry.velocity;
        }
    } else {
        int numberOfVoices = this->synthState->mixerState.instrumentState[timbre].numberOfVoices;
        for (int k = 0; k < numberOfVoices && k < NUMBER_OF_STORED_NOT; k++) {
            // voice number k of timbre
            int n = timbres[timbre].voiceNumber[k];
            if (voices[n].isPlaying() && !voices[n].isReleased()) {
                noteBeforeNewParalsLoad[k] = voices[n].getNote();
                velocityBeforeNewParalsLoad[k] = voices[n].getMidiVelocity();
            }
        }
    }

    timbres[timbre].resetArpeggiator();
    // Stop all voices
    allNoteOffQuick(timbre);
    // Let's allow the buffer to catch up
    // We can do that because we're in the lower priority thread
    HAL_Delay(2);
};


void Synth::afterNewParamsLoad(int timbre) {
    // Reset to 0 the number of voice then try to set the right value
	if (timbres[timbre].params.engine1.polyMono == 1) {
        if (this->synthState->mixerState.instrumentState[timbre].numberOfVoices == 1) {
            // Set number of voice from 1 to 3 (or max available)
            int voicesMax = getNumberOfFreeVoicesForThisTimbre(timbre);
            this->synthState->mixerState.instrumentState[timbre].numberOfVoices = 3 < voicesMax ? 3 : voicesMax;
            rebuidVoiceTimbre(timbre);
            timbres[timbre].numberOfVoicesChanged(this->synthState->mixerState.instrumentState[timbre].numberOfVoices);
        }
	} else {
	    if (this->synthState->mixerState.instrumentState[timbre].numberOfVoices != 1) {
	        this->synthState->mixerState.instrumentState[timbre].numberOfVoices = 1;
	        rebuidVoiceTimbre(timbre);
	        timbres[timbre].numberOfVoicesChanged(this->synthState->mixerState.instrumentState[timbre].numberOfVoices);
	    }
	}

    timbres[timbre].afterNewParamsLoad();
    // values to force check lfo used
    timbres[timbre].verifyLfoUsed(ENCODER_MATRIX_SOURCE, 0.0f, 1.0f);


    for (int k = 0; k < NUMBER_OF_STORED_NOT; k++) {
        if (noteBeforeNewParalsLoad[k] != 0) {
            timbres[timbre].noteOn(noteBeforeNewParalsLoad[k], velocityBeforeNewParalsLoad[k]);
            noteBeforeNewParalsLoad[k] = 0;
        }
    }


}

void Synth::afterNewMixerLoad() {

    rebuidVoiceAllTimbre();

    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES ; timbre++) {
        synthState->scalaSettingsChanged(timbre);
        timbres[timbre].numberOfVoicesChanged(this->synthState->mixerState.instrumentState[timbre].numberOfVoices);
        timbres[timbre].afterNewParamsLoad();
        // values to force check lfo used
        timbres[timbre].verifyLfoUsed(ENCODER_MATRIX_SOURCE, 0.0f, 1.0f);
        //
    }

}


int Synth::getFreeVoice() {
    // Loop on all voices
    bool used[MAX_NUMBER_OF_VOICES];
    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        used[v] = false;
    }

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        // Must be different from 0 and -1
        int nv = this->synthState->mixerState.instrumentState[t].numberOfVoices;

        for (int v = 0; v < nv; v++) {
            used[timbres[t].voiceNumber[v]] = true;
        }
    }

    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        if (!used[v]) {
            return v;
        }
    }

    return -1;
}


void Synth::newParamValue(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue) {
    switch (currentRow) {
    case ROW_ARPEGGIATOR1:
        switch (encoder) {
        case ENCODER_ARPEGGIATOR_CLOCK:
            allNoteOff(timbre);
            timbres[timbre].setArpeggiatorClock((uint8_t) newValue);
            break;
        case ENCODER_ARPEGGIATOR_BPM:
            timbres[timbre].setNewBPMValue((uint8_t) newValue);
            break;
        case ENCODER_ARPEGGIATOR_DIRECTION:
            timbres[timbre].setDirection((uint8_t) newValue);
            break;
        }
        break;
    case ROW_ARPEGGIATOR2:
        if (unlikely(encoder == ENCODER_ARPEGGIATOR_LATCH)) {
            timbres[timbre].setLatchMode((uint8_t) newValue);
        }
        break;
    case ROW_ARPEGGIATOR3:
        break;
    case ROW_EFFECT:
        timbres[timbre].setNewEffecParam(encoder);
        break;
    case ROW_ENV1a:
        timbres[timbre].env1.reloadADSR(encoder);
        break;
    case ROW_ENV1b:
        timbres[timbre].env1.reloadADSR(encoder + 4);
        break;
    case ROW_ENV2a:
        timbres[timbre].env2.reloadADSR(encoder);
        break;
    case ROW_ENV2b:
        timbres[timbre].env2.reloadADSR(encoder + 4);
        break;
    case ROW_ENV3a:
        timbres[timbre].env3.reloadADSR(encoder);
        break;
    case ROW_ENV3b:
        timbres[timbre].env3.reloadADSR(encoder + 4);
        break;
    case ROW_ENV4a:
        timbres[timbre].env4.reloadADSR(encoder);
        break;
    case ROW_ENV4b:
        timbres[timbre].env4.reloadADSR(encoder + 4);
        break;
    case ROW_ENV5a:
        timbres[timbre].env5.reloadADSR(encoder);
        break;
    case ROW_ENV5b:
        timbres[timbre].env5.reloadADSR(encoder + 4);
        break;
    case ROW_ENV6a:
        timbres[timbre].env6.reloadADSR(encoder);
        break;
    case ROW_ENV6b:
        timbres[timbre].env6.reloadADSR(encoder + 4);
        break;
    case ROW_MATRIX_FIRST ... ROW_MATRIX_LAST:
        timbres[timbre].verifyLfoUsed(encoder, oldValue, newValue);
        if (encoder == ENCODER_MATRIX_DEST1 || encoder == ENCODER_MATRIX_DEST2) {
            // Reset old destination
            timbres[timbre].resetMatrixDestination(oldValue);
        }
        break;
    case ROW_LFOOSC1 ... ROW_LFOOSC3:
    case ROW_LFOENV1 ... ROW_LFOENV2:
    case ROW_LFOSEQ1 ... ROW_LFOSEQ2:
        // timbres[timbre].lfo[currentRow - ROW_LFOOSC1]->valueChanged(encoder);
        timbres[timbre].lfoValueChange(currentRow, encoder, newValue);
        break;
    case ROW_PERFORMANCE1:
        timbres[timbre].setMatrixSource((enum SourceEnum)(MATRIX_SOURCE_CC1 + encoder), newValue);
        break;
    case ROW_MIDINOTE1CURVE:
        timbres[timbre].updateMidiNoteScale(0);
        break;
    case ROW_MIDINOTE2CURVE:
        timbres[timbre].updateMidiNoteScale(1);
        break;
    }
}


// synth is the only one who knows timbres
void Synth::newTimbre(int timbre)  {
    this->synthState->setParamsAndTimbre(&timbres[timbre].params, timbre);
}

void Synth::newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    switch (valueType) {
        case MIXER_VALUE_NUMBER_OF_VOICES:
            if (newValue == oldValue) {
                return;
            } else if (newValue > oldValue) {
                for (int v = (int) oldValue; v < (int) newValue; v++) {
                    timbres[timbre].setVoiceNumber(v, getFreeVoice());
                }
            } else {
                for (int v = (int) newValue; v < (int) oldValue; v++) {
                    voices[timbres[timbre].voiceNumber[v]].killNow();
                    timbres[timbre].setVoiceNumber(v, -1);
                }
            }
            timbres[timbre].numberOfVoicesChanged(newValue);
            break;
        case MIXER_VALUE_COMPRESSOR:
            switch ((int)newValue) {
                // No
                case 0:
                    compInstrument[timbre].setRatio(1.0f);
                    // Must never be reached
                    compInstrument[timbre].setThresh(1000.0f);
                    compInstrument[timbre].setAttack(10.0);
                    compInstrument[timbre].setRelease(100.0);
                    break;
                case 1:
                    // Slow2
                    compInstrument[timbre].setRatio(.33f);
                    compInstrument[timbre].setThresh(-12.0f);
                    compInstrument[timbre].setAttack(100.0);
                    compInstrument[timbre].setRelease(500.0);
                    break;
                case 2:
                    // Medium2
                    compInstrument[timbre].setRatio(.33f);
                    compInstrument[timbre].setThresh(-12.0f);
                    compInstrument[timbre].setAttack(10.0);
                    compInstrument[timbre].setRelease(100.0);
                    break;
                case 3:
                    // Fast4
                    compInstrument[timbre].setRatio(.33f);
                    compInstrument[timbre].setThresh(-12.0f);
                    compInstrument[timbre].setAttack(2.0);
                    compInstrument[timbre].setRelease(20.0);
                    break;
            }
            break;
    }
}


void Synth::rebuidVoiceTimbre(int timbre) {

    int nv = this->synthState->mixerState.instrumentState[timbre].numberOfVoices;

    for (int v = 0; v < MAX_NUMBER_OF_VOICES; v++) {
        timbres[timbre].setVoiceNumber(v, -1);
    }

    for (int v = 0; v < nv; v++) {
        timbres[timbre].setVoiceNumber(v, getFreeVoice());
    }
}

void Synth::rebuidVoiceAllTimbre() {
    int voiceNumber = 0;

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        int nv = this->synthState->mixerState.instrumentState[t].numberOfVoices;

        for (int v = 0; v < nv; v++) {
            timbres[t].setVoiceNumber(v, voiceNumber++);
        }

        for (int v = nv; v < MAX_NUMBER_OF_VOICES; v++) {
            timbres[t].setVoiceNumber(v, -1);
        }
}
}


void Synth::loadPreenFMPatchFromMidi(int timbre, int bank, int bankLSB, int patchNumber) {
    this->synthState->loadPresetFromMidi(timbre, bank, bankLSB, patchNumber, &timbres[timbre].params);
}


void Synth::setNewValueFromMidi(int timbre, int row, int encoder, float newValue) {
    struct ParameterDisplay* param = &(allParameterRows.row[row]->params[encoder]);
    int index = row * NUMBER_OF_ENCODERS + encoder;
    float oldValue = ((float*)this->timbres[timbre].getParamRaw())[index];

    this->timbres[timbre].setNewValue(index, param, newValue);
    float newNewValue = ((float*)this->timbres[timbre].getParamRaw())[index];
    if (oldValue != newNewValue) {
        this->synthState->propagateNewParamValueFromExternal(timbre, row, encoder, param, oldValue, newNewValue);
    }
}


void Synth::setNewMixerValueFromMidi(int timbre, int mixerValue, float newValue) {
    switch (mixerValue) {
        case MIXER_VALUE_VOLUME: {
            float oldValue = this->synthState->mixerState.instrumentState[timbre].volume;
            this->synthState->mixerState.instrumentState[timbre].volume = newValue;
            if (oldValue != newValue) {
                this->synthState->propagateNewMixerValueFromExternal(timbre, mixerValue, oldValue, newValue);
            }
            break;
        }
        case MIXER_VALUE_PAN: {
            float oldValue = this->synthState->mixerState.instrumentState[timbre].pan;
            // Can be 64
            if (newValue > 63.0f) {
                newValue = 63.0f;
            }
            this->synthState->mixerState.instrumentState[timbre].pan = newValue;
            if (oldValue != newValue) {
                this->synthState->propagateNewMixerValueFromExternal(timbre, mixerValue, oldValue, newValue);
            }
            break;
        }
    }
}

void Synth::setNewStepValueFromMidi(int timbre, int whichStepSeq, int step, int newValue) {

    if (whichStepSeq < 0 || whichStepSeq > 1 || step < 0 || step > 15 || newValue < 0 || newValue > 15) {
        return;
    }

    int oldValue = this->timbres[timbre].getSeqStepValue(whichStepSeq, step);

    if (oldValue !=  newValue) {
        int oldStep = this->synthState->stepSelect[whichStepSeq];
        this->synthState->stepSelect[whichStepSeq] = step;
        if (oldStep != step) {
            this->synthState->propagateNewParamValueFromExternal(timbre, ROW_LFOSEQ1 + whichStepSeq, 2, NULL, oldStep, step);
        }
        this->timbres[timbre].setSeqStepValue(whichStepSeq, step, newValue);
        int newNewValue = this->timbres[timbre].getSeqStepValue(whichStepSeq, step);
        if (oldValue != newNewValue) {
            this->synthState->propagateNewParamValueFromExternal(timbre, ROW_LFOSEQ1 + whichStepSeq, 3, NULL, oldValue, newValue);
        }
    }
}



void Synth::setNewSymbolInPresetName(int timbre, int index, int value) {
    this->timbres[timbre].getParamRaw()->presetName[index] = value;
}



void Synth::setCurrentInstrument(int value) {
    if (value >=1 && value <= (NUMBER_OF_TIMBRES + 1)) {
        this->synthState->setCurrentInstrument(value);
    }
}


#ifdef DEBUG__KO

// ========================== DEBUG ========================
void Synth::debugVoice() {

    lcd.setRealTimeAction(true);
    lcd.clearActions();
    lcd.clear();
    int numberOfVoices = timbres[0].params.engine1.numberOfVoice;
    // HARDFAULT !!! :-)
    //    for (int k = 0; k <10000; k++) {
    //    	numberOfVoices += timbres[k].params.engine1.numberOfVoice;
    //    	timbres[k].params.engine1.numberOfVoice = 100;
    //    }

    for (int k = 0; k < numberOfVoices && k < 4; k++) {

        // voice number k of timbre
        int n = timbres[0].voiceNumber[k];

        lcd.setCursor(0, k);
        lcd.print((int)voices[n].getNote());

        lcd.setCursor(4, k);
        lcd.print((int)voices[n].getNextPendingNote());

        lcd.setCursor(8, k);
        lcd.print(n);

        lcd.setCursor(12, k);
        lcd.print((int)voices[n].getIndex());

        lcd.setCursor(18, k);
        lcd.print((int) voices[n].isReleased());
        lcd.print((int) voices[n].isPlaying());
    }
    lcd.setRealTimeAction(false);
}

void Synth::showCycles() {
    lcd.setRealTimeAction(true);
    lcd.clearActions();
    lcd.clear();

    float max = SystemCoreClock * 32.0f * PREENFM_FREQUENCY_INVERSED;
    int cycles = cycles_all.remove();
    float percent = (float)cycles * 100.0f / max;
    lcd.setCursor(10, 0);
    lcd.print('>');
    lcd.print(cycles);
    lcd.print('<');
    lcd.setCursor(10, 1);
    lcd.print('>');
    lcd.print(percent);
    lcd.print('%');
    lcd.print('<');

    /*
    lcd.setCursor( 0, 0 );
    lcd.print( "RNG: " );
    lcd.print( cycles_rng.remove() );

    lcd.setCursor( 0, 1 );
    lcd.print( "VOI: " );  lcd.print( cycles_voices1.remove() );
    lcd.print( " " ); lcd.print( cycles_voices2.remove() );

    lcd.setCursor( 0, 2 );
    lcd.print( "FX : " );
    lcd.print( cycles_fx.remove() );

    lcd.setCursor( 0, 3 );
    lcd.print( "TIM: " );
    lcd.print( cycles_timbres.remove() );

    lcd.setRealTimeAction(false);
     */
}

#endif
