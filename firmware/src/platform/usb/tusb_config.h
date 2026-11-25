#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_MCU           OPT_MCU_STM32H7
#define CFG_TUSB_OS            OPT_OS_NONE
#define CFG_TUSB_RHPORT0_MODE  OPT_MODE_DEVICE
#define CFG_TUD_MAX_SPEED      OPT_MODE_FULL_SPEED
#define CFG_TUD_ENABLED        1
#define CFG_TUD_ENDPOINT0_SIZE 64

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

/* MIDI */
#define CFG_TUD_MIDI            1
#define CFG_TUD_MIDI_RX_BUFSIZE 64
#define CFG_TUD_MIDI_TX_BUFSIZE 128

/* UAC2 duplex: capture (device -> host) + playback (host -> device), 24-bit */
#define CFG_TUD_AUDIO                              1
#define CFG_TUD_AUDIO_ENABLE_EP_IN                 1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                1
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP           0
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE           48000
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX 3
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX         2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX 3
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX         2
#define CFG_TUD_AUDIO_EP_SZ_IN                     288 /* 48 frames * 2 ch * 3 B */
#define CFG_TUD_AUDIO_EP_SZ_OUT                    288
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX          288
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ       1152
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX         288
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ      1152

#define CFG_TUD_MSC    0
#define CFG_TUD_CDC    0
#define CFG_TUD_HID    0
#define CFG_TUD_VENDOR 0

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H */