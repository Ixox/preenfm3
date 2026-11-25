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

/* Mass storage: SD card exposed as a single SCSI LUN. */
#define CFG_TUD_MSC            1
#define CFG_TUD_MSC_EP_BUFSIZE 512

#define CFG_TUD_AUDIO  0
#define CFG_TUD_MIDI   0
#define CFG_TUD_CDC    0
#define CFG_TUD_HID    0
#define CFG_TUD_VENDOR 0

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H */