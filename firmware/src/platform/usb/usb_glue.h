#ifndef USB_GLUE_H
#define USB_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Board bring-up (PA11/PA12, OTG FS clock, IRQ) + tusb_init().
   withAudio selects the duplex or MIDI-only descriptor (applied at boot). */
void usbInit(int withAudio);
/* Call from the main loop: runs tud_task() and the class glue. */
void usbTask(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_GLUE_H */