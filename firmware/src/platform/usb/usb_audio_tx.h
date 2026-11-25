#ifndef USB_AUDIO_TX_H
#define USB_AUDIO_TX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the UAC2 control state (sample rate, clock validity). */
void usbAudioInit(void);

/* Feed the UAC2 capture endpoint from the usb_audio ring. */
void usbAudioTask(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_AUDIO_TX_H */