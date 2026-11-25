#include "usb_audio_tx.h"
#include "main.h"
#include "tusb.h"
#include "usb_audio.h"

#define AUDIO_AS_IN_ITF      3U
#define AUDIO_AS_OUT_ITF     4U
#define AUDIO_FRAMES_PER_MS  48
#define AUDIO_TX_BYTES       288 /* 48 frames * 2 ch * 3 B */
#define AUDIO_RX_FRAME_BYTES 6   /* 2 ch * 3 B */

static volatile uint8_t captureStreaming = 0;
static volatile uint8_t playbackStreaming = 0;

static bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];
static uint16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];
static uint32_t sampFreq;
static uint8_t clkValid;
static audio20_control_range_4_n_t(1) sampleFreqRng;

void usbAudioInit(void) {
    sampFreq = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    clkValid = 1;
    sampleFreqRng.wNumSubRanges = 1;
    sampleFreqRng.subrange[0].bMin = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bMax = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bRes = 0;
    usbAudioPlaybackReset();
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const* req, uint8_t* buff) {
    (void)rhport;
    uint8_t channelNum = TU_U16_LOW(req->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(req->wValue);
    uint8_t entityID = TU_U16_HIGH(req->wIndex);

    if (req->bRequest != AUDIO20_CS_REQ_CUR || entityID != 0x02 || channelNum > CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX) {
        return false;
    }

    switch (ctrlSel) {
    case AUDIO20_FU_CTRL_MUTE:
        if (req->wLength != sizeof(audio20_control_cur_1_t)) {
            return false;
        }
        mute[channelNum] = ((audio20_control_cur_1_t*)buff)->bCur;
        return true;
    case AUDIO20_FU_CTRL_VOLUME:
        if (req->wLength != sizeof(audio20_control_cur_2_t)) {
            return false;
        }
        volume[channelNum] = (uint16_t)((audio20_control_cur_2_t*)buff)->bCur;
        return true;
    default:
        return false;
    }
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const* req) {
    uint8_t channelNum = TU_U16_LOW(req->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(req->wValue);
    uint8_t entityID = TU_U16_HIGH(req->wIndex);

    if (entityID == 0x01 && ctrlSel == AUDIO20_TE_CTRL_CONNECTOR) {
        audio20_desc_channel_cluster_t ret = {
            .bNrChannels = CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX,
            .bmChannelConfig = (audio20_channel_config_t)0,
            .iChannelNames = 0,
        };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &ret, sizeof(ret));
    }

    if (entityID == 0x02 && channelNum <= CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX) {
        switch (ctrlSel) {
        case AUDIO20_FU_CTRL_MUTE:
            return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &mute[channelNum], sizeof(mute[channelNum]));
        case AUDIO20_FU_CTRL_VOLUME:
            switch (req->bRequest) {
            case AUDIO20_CS_REQ_CUR:
                return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &volume[channelNum], sizeof(volume[channelNum]));
            case AUDIO20_CS_REQ_RANGE: {
                audio20_control_range_2_n_t(1) ret;
                ret.wNumSubRanges = 1;
                ret.subrange[0].bMin = -90;
                ret.subrange[0].bMax = 90;
                ret.subrange[0].bRes = 1;
                return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &ret, sizeof(ret));
            }
            default:
                return false;
            }
        default:
            return false;
        }
    }

    if (entityID == 0x04) {
        switch (ctrlSel) {
        case AUDIO20_CS_CTRL_SAM_FREQ:
            switch (req->bRequest) {
            case AUDIO20_CS_REQ_CUR:
                return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &sampFreq, sizeof(sampFreq));
            case AUDIO20_CS_REQ_RANGE:
                return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &sampleFreqRng, sizeof(sampleFreqRng));
            default:
                return false;
            }
        case AUDIO20_CS_CTRL_CLK_VALID:
            return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &clkValid, sizeof(clkValid));
        default:
            return false;
        }
    }

    return false;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const* req) {
    (void)rhport;
    uint8_t itf = (uint8_t)(req->wIndex & 0xFFU);
    uint8_t alt = (uint8_t)(req->wValue & 0xFFU);

    if (itf == AUDIO_AS_IN_ITF) {
        if (alt == 1U) {
            usbAudioReset();
            usbAudioSetStreaming(1);
            captureStreaming = 1;
            usbAudioTask();
        } else {
            captureStreaming = 0;
            usbAudioSetStreaming(0);
        }
    } else if (itf == AUDIO_AS_OUT_ITF) {
        if (alt == 1U) {
            usbAudioPlaybackReset();
            usbAudioSetPlaybackStreaming(1);
            playbackStreaming = 1;
        } else {
            playbackStreaming = 0;
            usbAudioSetPlaybackStreaming(0);
        }
    }

    return true;
}

void usbAudioTask(void) {
    if (captureStreaming) {
        tu_fifo_t* ff = tud_audio_n_get_ep_in_ff(0);
        while (ff != NULL && tu_fifo_remaining(ff) >= AUDIO_TX_BYTES) {
            uint8_t pkt[AUDIO_TX_BYTES];
            usbAudioPullPacked(pkt, AUDIO_FRAMES_PER_MS);
            if (tud_audio_n_write(0, pkt, AUDIO_TX_BYTES) != AUDIO_TX_BYTES) {
                break;
            }
        }
    }

    if (playbackStreaming) {
        uint8_t rx[AUDIO_TX_BYTES];
        uint16_t avail = tud_audio_n_available(0);
        while (avail >= AUDIO_RX_FRAME_BYTES) {
            uint16_t n = (avail > sizeof(rx)) ? (uint16_t)sizeof(rx) : avail;
            n = (uint16_t)(n - (n % AUDIO_RX_FRAME_BYTES));
            if (n == 0U) {
                break;
            }
            uint16_t got = tud_audio_n_read(0, rx, n);
            if (got < AUDIO_RX_FRAME_BYTES) {
                break;
            }
            usbAudioPlaybackPush(rx, got / AUDIO_RX_FRAME_BYTES);
            avail = tud_audio_n_available(0);
        }
    }
}
