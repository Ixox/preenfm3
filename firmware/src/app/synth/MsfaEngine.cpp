/*
 * MSFA engine state and adapters.
 *
 * The MSFA headers are only included here so their Lfo/Env classes do not
 * collide with the PFM3 classes in the rest of the firmware.
 */

#include <new>

#include "MsfaEngine.h"
#include "MsfaDebug.h"
#include "controllers.h"
#include "dx7note.h"
#include "fm_core.h"
#include "lfo.h"
#include "synth.h"
#include "tuning.h"

FmCore msfaCore;

static std::shared_ptr<TuningState> msfaTuning;

struct MsfaTimbreState {
    Controllers controllers;
    Lfo lfo;
    int32_t lfoValue;
    int32_t lfoDelay;
    uint8_t blockPhase;
};

struct MsfaVoiceState {
    Dx7Note noteObj;
    int32_t buffer[N];
    uint8_t bufferPos;
    bool active;
    bool released;
    uint8_t note;
    uint8_t velocity;
    uint8_t debugBlocks;

    MsfaVoiceState();
};

alignas(MsfaVoiceState) static uint8_t msfaVoiceStorage[MAX_NUMBER_OF_VOICES][sizeof(MsfaVoiceState)]
    __attribute__((section(".ram_d1")));
alignas(MsfaTimbreState) static uint8_t msfaTimbreStorage[NUMBER_OF_TIMBRES][sizeof(MsfaTimbreState)]
    __attribute__((section(".ram_d1")));

MsfaVoiceState *msfaVoiceStates = reinterpret_cast<MsfaVoiceState *>(msfaVoiceStorage);
MsfaTimbreState *msfaTimbreStates = reinterpret_cast<MsfaTimbreState *>(msfaTimbreStorage);

MsfaVoiceState *msfaGetVoiceState(int index) {
    return msfaVoiceStates + index;
}

MsfaTimbreState *msfaGetTimbreState(int index) {
    return msfaTimbreStates + index;
}

MsfaVoiceState::MsfaVoiceState()
    : noteObj(msfaTuning, nullptr),
      bufferPos(N),
      active(false),
      released(false),
      note(0),
      velocity(0),
      debugBlocks(0) {
    for (int i = 0; i < N; i++) {
        buffer[i] = 0;
    }
}

void msfaTimbreInit(MsfaTimbreState *state) {
    Controllers &controllers = state->controllers;

    controllers.pitch_cc = 0x2000;
    controllers.pitch_range_up = 3;
    controllers.pitch_range_dn = 3;
    controllers.pitch_step = 0;
    controllers.amp_mod = 0;
    controllers.pitch_mod = 0;
    controllers.eg_mod = 127;
    controllers.aftertouch_cc = 0;
    controllers.breath_cc = 0;
    controllers.foot_cc = 0;
    controllers.modwheel_cc = 0;
    controllers.portamento_enable_cc = false;
    controllers.portamento_cc = 0;
    controllers.portamento_gliss_cc = false;
    controllers.masterTune = 0;
    controllers.mpeEnabled = false;
    controllers.mpePitchBendRange = 24;
    controllers.core = &msfaCore;
    controllers.refresh();

    state->lfoValue = 1 << 23;
    state->lfoDelay = 1 << 24;
    state->blockPhase = 0;
}

void msfaTimbreKeyDown(MsfaTimbreState *state) {
    state->lfo.keydown();
}

void msfaTimbreNextBlock(MsfaTimbreState *state) {
    if (state->blockPhase == 0) {
        state->lfoValue = state->lfo.getsample();
        state->lfoDelay = state->lfo.getdelay();
    }
    state->blockPhase ^= 1;
}

void msfaTimbreLoadProgram(MsfaTimbreState *state, const uint8_t *program) {
    state->lfo.reset(program + 137);
}

void msfaTimbreSetController(MsfaTimbreState *state, enum SourceEnum source, float value) {
    Controllers &controllers = state->controllers;

    switch (source) {
        case MATRIX_SOURCE_PITCHBEND:
            controllers.pitch_cc = 8192 + (int) (value * 8192.0f);
            break;
        case MATRIX_SOURCE_MODWHEEL:
            controllers.modwheel_cc = (int) (value * 127.0f);
            break;
        case MATRIX_SOURCE_AFTERTOUCH:
            controllers.aftertouch_cc = (int) (value * 127.0f);
            break;
        case MATRIX_SOURCE_BREATH:
            controllers.breath_cc = (int) (value * 127.0f);
            break;
        default:
            return;
    }
    controllers.refresh();
}

void msfaVoiceStart(MsfaVoiceState *state, const uint8_t *program, uint8_t note,
                    uint8_t velocity, uint8_t channel, MsfaTimbreState *ts) {
    state->noteObj.tuning_state_ = msfaTuning;
    state->noteObj.init(program, note, velocity, channel, &ts->controllers);

    for (int i = 0; i < N; i++) {
        state->buffer[i] = 0;
    }
    state->bufferPos = N;
    state->active = true;
    state->released = false;
    state->note = note;
    state->velocity = velocity;
    state->debugBlocks = 0;

    msfaDebugLog("S note=%d vel=%d play=%d fin=%d b0=%d ops=%02x alg=%d\n",
        note, velocity, state->noteObj.isPlaying(), state->noteObj.isFinished(),
        (int) state->noteObj.debugBasePitch(0), state->noteObj.debugOpSwitch(),
        state->noteObj.debugAlgorithm());
}

void msfaVoiceKeyUp(MsfaVoiceState *state) {
    if (state == nullptr || !state->active) {
        return;
    }
    state->noteObj.keyup();
    state->released = true;
}

bool msfaVoiceIsActive(MsfaVoiceState *state) {
    return state != nullptr && state->active;
}

bool msfaVoiceRender(MsfaVoiceState *state, MsfaTimbreState *ts, float *out) {
    if (state == nullptr || !state->active) {
        return false;
    }

    if (state->bufferPos >= N) {
        for (int i = 0; i < N; i++) {
            state->buffer[i] = 0;
        }
        state->noteObj.compute(state->buffer, ts->lfoValue, ts->lfoDelay, &ts->controllers);
        state->bufferPos = 0;
    }

    int32_t raw0 = state->buffer[state->bufferPos];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        int32_t val = state->buffer[state->bufferPos + i] >> 4;
        int32_t clip = val < -(1 << 24) ? -32768 : (val >= (1 << 24) ? 32767 : (val >> 9));
        float f = (float) clip * (1.0f / 32768.0f);
        *out++ = f;
        *out++ = f;
    }
    state->bufferPos += BLOCK_SIZE;

    if (state->debugBlocks < 4) {
        msfaDebugLog("R note=%d b=%d play=%d fin=%d act=%d raw=%d out=%d eL=%d eO=%d st=%d dn=%d\n",
            state->note, state->bufferPos, state->noteObj.isPlaying(), state->noteObj.isFinished(),
            state->active, (int) raw0, (int) (out[-64] * 1000000.0f),
            state->noteObj.debugEnvLevel(0), state->noteObj.debugEnvOutLevel(0),
            state->noteObj.debugEnvStage(0), state->noteObj.debugEnvDown(0));
        state->debugBlocks++;
    }

    if (!state->noteObj.isPlaying() || (state->released && state->noteObj.isFinished())) {
        state->active = false;
        return false;
    }
    return true;
}

void msfaVoiceUpdateProgram(MsfaVoiceState *state, const uint8_t *program) {
    if (state == nullptr || !state->active) {
        return;
    }
    state->noteObj.update(program, state->note, state->velocity, 1);
}

void msfaVoiceReset(MsfaVoiceState *state) {
    if (state == nullptr) {
        return;
    }
    state->active = false;
    state->released = false;
    state->bufferPos = N;
    state->note = 0;
    state->velocity = 0;
    state->debugBlocks = 0;
    for (int i = 0; i < N; i++) {
        state->buffer[i] = 0;
    }
}

void msfaEngineInit() {
    msfaDebugInit();
    msfaTuning = createStandardTuning();

    for (int i = 0; i < MAX_NUMBER_OF_VOICES; i++) {
        new (&msfaVoiceStorage[i][0]) MsfaVoiceState();
    }
    for (int i = 0; i < NUMBER_OF_TIMBRES; i++) {
        new (&msfaTimbreStorage[i][0]) MsfaTimbreState();
        msfaTimbreInit(msfaTimbreStates + i);
    }
}
