#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Select the duplex (audio + MIDI) or MIDI-only descriptor before tusb_init(). */
void usbDescriptorsSetAudioEnabled(int on);

#ifdef __cplusplus
}
#endif

#endif /* USB_DESCRIPTORS_H */
