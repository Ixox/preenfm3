#ifndef USB_GLUE_H
#define USB_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Cache SD card capacity before the host can query it. */
void usbMscInit(void);

/* Start (connect) the TinyUSB device stack. */
void usbInit(void);

/* Pump the TinyUSB device task. */
void usbTask(void);

/* Disconnect and release the USB peripheral. */
void usbDeinit(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_GLUE_H */