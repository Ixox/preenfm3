/**
  ******************************************************************************
  * @file    usbd_midi.h
  * @author  MCD Application Team
  * @brief   header file for the usbd_midi.c file.
  ******************************************************************************
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_MIDI_H
#define __USB_MIDI_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include  "usbd_ioreq.h"

/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */

/** @defgroup USBD_MIDI
  * @brief This file is the Header file for usbd_midi.c
  * @{
  */


/** @defgroup USBD_MIDI_Exported_Defines
  * @{
  */
#ifndef USBD_MIDI_FREQ
/* MIDI Class Config */
#define USBD_MIDI_FREQ                               48000U
#endif /* USBD_MIDI_FREQ */

#ifndef USBD_MAX_NUM_INTERFACES
#define USBD_MAX_NUM_INTERFACES                       1U
#endif /* USBD_MIDI_FREQ */

#define MIDI_OUT_EP                                  0x01U
#define MIDI_IN_EP                                   0x81U
#define USB_MIDI_CONFIG_DESC_SIZ                     0x6DU
#define MIDI_INTERFACE_DESC_SIZE                     0x09U
#define USB_MIDI_DESC_SIZ                            0x09U
#define MIDI_STANDARD_ENDPOINT_DESC_SIZE             0x09U
#define MIDI_STREAMING_ENDPOINT_DESC_SIZE            0x07U

#define MIDI_DESCRIPTOR_TYPE                         0x21U
#define USB_DEVICE_CLASS_MIDI                        0x01U
#define MIDI_SUBCLASS_MIDICONTROL                   0x01U
#define MIDI_SUBCLASS_MIDISTREAMING                 0x02U
#define MIDI_PROTOCOL_UNDEFINED                      0x00U
#define MIDI_STREAMING_GENERAL                       0x01U
#define MIDI_STREAMING_FORMAT_TYPE                   0x02U

/* Audio Descriptor Types */
#define MIDI_INTERFACE_DESCRIPTOR_TYPE               0x24U
#define MIDI_ENDPOINT_DESCRIPTOR_TYPE                0x25U

/* Audio Control Interface Descriptor Subtypes */
#define MIDI_CONTROL_HEADER                          0x01U
#define MIDI_CONTROL_INPUT_TERMINAL                  0x02U
#define MIDI_CONTROL_OUTPUT_TERMINAL                 0x03U
#define MIDI_CONTROL_FEATURE_UNIT                    0x06U

#define MIDI_INPUT_TERMINAL_DESC_SIZE                0x0CU
#define MIDI_OUTPUT_TERMINAL_DESC_SIZE               0x09U
#define MIDI_STREAMING_INTERFACE_DESC_SIZE           0x07U

#define MIDI_CONTROL_MUTE                            0x0001U

#define MIDI_FORMAT_TYPE_I                           0x01U
#define MIDI_FORMAT_TYPE_III                         0x03U

#define MIDI_ENDPOINT_GENERAL                        0x01U

#define MIDI_REQ_GET_CUR                             0x81U
#define MIDI_REQ_SET_CUR                             0x01U

#define MIDI_OUT_STREAMING_CTRL                      0x02U


#define MIDI_OUT_PACKET                              64

/* Number of sub-packets in the midi transfer buffer. You can modify this value but always make sure
  that it is an even number and higher than 3 */
#define MIDI_OUT_PACKET_NUM                          80U
/* Total size of the midi transfer buffer */
#define MIDI_TOTAL_BUF_SIZE                          ((uint16_t)(MIDI_OUT_PACKET * MIDI_OUT_PACKET_NUM))



typedef struct
{
    void  (*dataReceived)         (uint8_t *buffer);
} USBD_MIDI_ItfTypeDef;

extern USBD_ClassTypeDef  USBD_MIDI;
#define USBD_MIDI_CLASS    &USBD_MIDI

uint8_t  USBD_MIDI_RegisterInterface  (USBD_HandleTypeDef   *pdev,
                                        USBD_MIDI_ItfTypeDef *fops);


#ifdef __cplusplus
}
#endif

#endif  /* __USB_MIDI_H */
