#include "usb_audio.h"

#include <string.h>

#include "Common.h"

#define USB_AUDIO_RING_FRAMES 1024
#define USB_AUDIO_RING_MASK   (USB_AUDIO_RING_FRAMES - 1)

/* Q32 resampling phase. */
#define USB_AUDIO_PHASE_ONE ((uint64_t)1 << 32)
/* Servo gain and clamp: keep each ring near half full. */
#define USB_AUDIO_SERVO_GAIN      8192
#define USB_AUDIO_SERVO_CLAMP_DIV 500

#define RAM_D2B_SECTION __attribute__((section(".ram_d2b")))

/* ---- capture: synth -> host, 24-bit left-justified int32, 48 kHz ---- */
static int32_t capRing[USB_AUDIO_RING_FRAMES * 2] RAM_D2B_SECTION;
static volatile uint32_t capWrite = 0;
static volatile uint32_t capRead = 0;
static volatile uint8_t capStreaming = 0;
static uint64_t capPhase = 0;
static uint64_t capBaseStep = 0;
static int32_t capPrevL = 0;
static int32_t capPrevR = 0;
static uint8_t capPrimed = 0;

/* ---- playback: host -> synth, 24-bit left-justified int32, 48 kHz ---- */
static int32_t pbRing[USB_AUDIO_RING_FRAMES * 2] RAM_D2B_SECTION;
static volatile uint32_t pbWrite = 0;
static volatile uint32_t pbRead = 0;
static volatile uint8_t pbStreaming = 0;
static uint64_t pbPhase = 0;
static uint64_t pbBaseStep = 0;
static int32_t pbPrevL = 0;
static int32_t pbPrevR = 0;
static int32_t pbCurL = 0;
static int32_t pbCurR = 0;
static uint8_t pbPrimed = 0;

static inline int32_t clamp24(int64_t v) {
    if (v > 0x7FFFFF00) {
        return 0x7FFFFF00;
    }
    if (v < -0x7FFFFF00) {
        return -0x7FFFFF00;
    }
    return (int32_t)v;
}

static inline int64_t servoStep(uint64_t base, uint32_t w, uint32_t r) {
    int32_t fill = (int32_t)(w - r);
    int32_t err = fill - USB_AUDIO_RING_FRAMES / 2;
    int64_t corr = (int64_t)err * USB_AUDIO_SERVO_GAIN;
    const int64_t maxCorr = (int64_t)(base / USB_AUDIO_SERVO_CLAMP_DIV);
    if (corr > maxCorr) {
        corr = maxCorr;
    } else if (corr < -maxCorr) {
        corr = -maxCorr;
    }
    int64_t step = (int64_t)base + corr;
    if (step < (int64_t)(USB_AUDIO_PHASE_ONE / 4)) {
        step = (int64_t)(USB_AUDIO_PHASE_ONE / 4);
    }
    return step;
}

void usbAudioReset(void) {
    capWrite = 0;
    capRead = 0;
    capPhase = 0;
    capPrevL = 0;
    capPrevR = 0;
    capPrimed = 0;
    capBaseStep = (uint64_t)((double)PREENFM_FREQUENCY / 48000.0 * (double)USB_AUDIO_PHASE_ONE + 0.5);
}

void usbAudioSetStreaming(int on) {
    capStreaming = on ? 1 : 0;
}

void usbAudioPush(const int32_t* w1, const int32_t* w2, const int32_t* w3, int frames) {
    if (!capStreaming) {
        return;
    }
    if (capBaseStep == 0) {
        capBaseStep = (uint64_t)((double)PREENFM_FREQUENCY / 48000.0 * (double)USB_AUDIO_PHASE_ONE + 0.5);
    }

    uint32_t w = capWrite;
    uint32_t r = capRead;
    uint64_t step = (uint64_t)servoStep(capBaseStep, w, r);

    for (int i = 0; i < frames; i++) {
        int32_t l = clamp24((int64_t)w1[2 * i] + w2[2 * i] + w3[2 * i]);
        int32_t rr = clamp24((int64_t)w1[2 * i + 1] + w2[2 * i + 1] + w3[2 * i + 1]);

        if (!capPrimed) {
            capPrevL = l;
            capPrevR = rr;
            capPrimed = 1;
        }

        while (capPhase < USB_AUDIO_PHASE_ONE) {
            uint32_t frac = (uint32_t)(capPhase >> 16);
            int32_t ol = capPrevL + (int32_t)((((int64_t)l - capPrevL) * frac) >> 16);
            int32_t orr = capPrevR + (int32_t)((((int64_t)rr - capPrevR) * frac) >> 16);

            uint32_t nw = w + 1;
            if ((uint32_t)(nw - r) <= USB_AUDIO_RING_FRAMES) {
                uint32_t idx = (w & USB_AUDIO_RING_MASK) * 2;
                capRing[idx] = ol;
                capRing[idx + 1] = orr;
                w = nw;
            }
            capPhase += step;
        }
        capPhase -= USB_AUDIO_PHASE_ONE;
        capPrevL = l;
        capPrevR = rr;
    }

    capWrite = w;
}

static inline void pack24(uint8_t* d, int32_t v) {
    uint32_t u = (uint32_t)v;
    d[0] = (uint8_t)(u >> 8);
    d[1] = (uint8_t)(u >> 16);
    d[2] = (uint8_t)(u >> 24);
}

void usbAudioPullPacked(uint8_t* dst, int frames) {
    uint32_t r = capRead;
    uint32_t w = capWrite;

    if ((uint32_t)(w - r) >= (uint32_t)frames) {
        for (int i = 0; i < frames; i++) {
            uint32_t idx = (r & USB_AUDIO_RING_MASK) * 2;
            pack24(dst, capRing[idx]);
            pack24(dst + 3, capRing[idx + 1]);
            dst += 6;
            r++;
        }
        capRead = r;
    } else {
        memset(dst, 0, (size_t)frames * 6);
        capRead = w;
    }
}

void usbAudioPlaybackReset(void) {
    pbWrite = 0;
    pbRead = 0;
    pbPhase = 0;
    pbPrimed = 0;
    pbPrevL = 0;
    pbPrevR = 0;
    pbCurL = 0;
    pbCurR = 0;
    pbBaseStep = (uint64_t)(48000.0 / (double)PREENFM_FREQUENCY * (double)USB_AUDIO_PHASE_ONE + 0.5);
}

void usbAudioSetPlaybackStreaming(int on) {
    pbStreaming = on ? 1 : 0;
}

void usbAudioPlaybackPush(const uint8_t* src, int frames) {
    uint32_t w = pbWrite;
    uint32_t r = pbRead;

    for (int i = 0; i < frames; i++) {
        uint32_t u = (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16);
        int32_t v = (int32_t)u;
        if (v & 0x800000) {
            v |= (int32_t)0xFF000000;
        }
        int32_t l = (int32_t)((uint32_t)v << 8);
        src += 3;

        u = (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16);
        v = (int32_t)u;
        if (v & 0x800000) {
            v |= (int32_t)0xFF000000;
        }
        int32_t rr = (int32_t)((uint32_t)v << 8);
        src += 3;

        uint32_t nw = w + 1;
        if ((uint32_t)(nw - r) <= USB_AUDIO_RING_FRAMES) {
            uint32_t idx = (w & USB_AUDIO_RING_MASK) * 2;
            pbRing[idx] = l;
            pbRing[idx + 1] = rr;
            w = nw;
        }
    }

    pbWrite = w;
}

static int pbPop(int32_t* l, int32_t* r) {
    uint32_t rd = pbRead;
    if (rd == pbWrite) {
        return 0;
    }
    uint32_t idx = (rd & USB_AUDIO_RING_MASK) * 2;
    *l = pbRing[idx];
    *r = pbRing[idx + 1];
    pbRead = rd + 1;
    return 1;
}

void usbAudioPlaybackPull(int32_t* dst, int frames) {
    if (!pbStreaming) {
        memset(dst, 0, (size_t)frames * 2 * sizeof(int32_t));
        return;
    }

    if (!pbPrimed) {
        if (!pbPop(&pbPrevL, &pbPrevR) || !pbPop(&pbCurL, &pbCurR)) {
            memset(dst, 0, (size_t)frames * 2 * sizeof(int32_t));
            pbRead = pbWrite;
            return;
        }
        pbPrimed = 1;
        pbPhase = 0;
    }

    uint64_t step = (uint64_t)servoStep(pbBaseStep, pbWrite, pbRead);

    for (int i = 0; i < frames; i++) {
        while (pbPhase >= USB_AUDIO_PHASE_ONE) {
            pbPrevL = pbCurL;
            pbPrevR = pbCurR;
            if (!pbPop(&pbCurL, &pbCurR)) {
                memset(dst + 2 * i, 0, (size_t)(frames - i) * 2 * sizeof(int32_t));
                pbPrimed = 0;
                pbRead = pbWrite;
                return;
            }
            pbPhase -= USB_AUDIO_PHASE_ONE;
        }

        uint32_t frac = (uint32_t)(pbPhase >> 16);
        dst[2 * i] = pbPrevL + (int32_t)((((int64_t)pbCurL - pbPrevL) * frac) >> 16);
        dst[2 * i + 1] = pbPrevR + (int32_t)((((int64_t)pbCurR - pbPrevR) * frac) >> 16);
        pbPhase += step;
    }
}
