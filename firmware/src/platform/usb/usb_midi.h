#ifndef USB_MIDI_H
#define USB_MIDI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Queue bytes (USB-MIDI event packets) for the host. */
void usbMidiQueue(const uint8_t* buf, uint32_t len);
/* Pump TX and RX; call from the main loop (via usbTask). */
void usbMidiTask(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_MIDI_H */