/**
 ******************************************************************************
 * @file    usbd_midi.c
 * @author  MCD Application Team
 * @brief   This file provides the Audio core functions.
 *
 */


#include "../Inc/usbd_midi.h"



uint8_t usbMidiBuffAll[128];
uint8_t *usbMidiBuffWrt = usbMidiBuffAll;
uint8_t *usbMidiBuffRead = usbMidiBuffAll;


static uint8_t  USBD_MIDI_Init (USBD_HandleTypeDef *pdev,
		uint8_t cfgidx);

static uint8_t  USBD_MIDI_DeInit (USBD_HandleTypeDef *pdev,
		uint8_t cfgidx);

static uint8_t  USBD_MIDI_Setup (USBD_HandleTypeDef *pdev,
		USBD_SetupReqTypedef *req);

static uint8_t  *USBD_MIDI_GetCfgDesc (uint16_t *length);

static uint8_t  *USBD_MIDI_GetDeviceQualifierDesc (uint16_t *length);

static uint8_t  USBD_MIDI_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t  USBD_MIDI_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t  USBD_MIDI_EP0_RxReady (USBD_HandleTypeDef *pdev);

static uint8_t  USBD_MIDI_EP0_TxReady (USBD_HandleTypeDef *pdev);

static uint8_t  USBD_MIDI_SOF (USBD_HandleTypeDef *pdev);

static uint8_t  USBD_MIDI_IsoINIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t  USBD_MIDI_IsoOutIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum);


USBD_ClassTypeDef  USBD_MIDI =
{
		USBD_MIDI_Init,
		USBD_MIDI_DeInit,
		USBD_MIDI_Setup,
		USBD_MIDI_EP0_TxReady,
		USBD_MIDI_EP0_RxReady,
		USBD_MIDI_DataIn,
		USBD_MIDI_DataOut,
		USBD_MIDI_SOF,
		USBD_MIDI_IsoINIncomplete,
		USBD_MIDI_IsoOutIncomplete,
		USBD_MIDI_GetCfgDesc,
		USBD_MIDI_GetCfgDesc,
		USBD_MIDI_GetCfgDesc,
		USBD_MIDI_GetDeviceQualifierDesc,
};


#define CLASS_SPECIFIC_DESC_SIZE                7+6+9+9+5+9+5
#define MIDI_CONFIG_DESC_SIZE 					9+9+9+9+CLASS_SPECIFIC_DESC_SIZE
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_DEVICE_CLASS_AUDIO                  0x01
#define USB_DEVICE_SUBCLASS_MIDISTREAMING       0x03


/* USB MIDI device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_MIDI_CfgDesc[USB_MIDI_CONFIG_DESC_SIZ] __ALIGN_END =
{
		/* Configuration 1 */
		0x09,                                 /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE,    /* bDescriptorType */
		LOBYTE(MIDI_CONFIG_DESC_SIZE),        /* wTotalLength  */
		HIBYTE(MIDI_CONFIG_DESC_SIZE),
		0x02,                                 /* bNumInterfaces */
		0x01,                                 /* bConfigurationValue */
		0x00,                                 /* iConfiguration */
		0xC0,                                 /* bmAttributes  = b6 + b7 : self powered */
		0x32,                                 /* bMaxPower = 150 mA*/
		/* 09 byte*/

		// B.3.1 Standard AC Interface Descriptor
		/* Audio Control standard */
		0x9,         /* sizeof(usbDescrInterface): length of descriptor in bytes */
		0x04,         /* interface descriptor type */
		0x00,         /* index of this interface */
		0x00,         /* alternate setting for this interface */
		0x00,         /* endpoints excl 0: number of endpoint descriptors to follow */
		0x01,         /* AUDIO */
		0x01,         /* AUDIO_Control*/
		0x00,         /* */
		0x00,         /* string index for interface */

		// B.3.2 Class-specific AC Interface Descriptor
		/* AudioControl   Class-Specific descriptor */
		0x09,         /* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
		0x24,         /* descriptor type */
		0x01,         /* header functional descriptor */
		0x0, 0x01,      /* bcdADC */
		0x09, 0x00,         /* wTotalLength */
		0x01,         /* */
		0x01,         /* */


		// B.4 MIDIStreaming Interface Descriptors

		// B.4.1 Standard MS Interface Descriptor
		/* PreenFM Standard interface descriptor */
		0x09,            /* bLength */
		0x04,        /* bDescriptorType */
		0x01,                                 /* bInterfaceNumber */
		0x00,                                 /* bAlternateSetting */
		0x02,                                 /* bNumEndpoints */
		USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
		USB_DEVICE_SUBCLASS_MIDISTREAMING,	/* bInterfaceSubClass : MIDIStreaming*/
		0x00,             					/* InterfaceProtocol: NOT USED */
		0x00,                                 /* iInterface : NOT USED*/
		/* 09 byte*/

		// B.4.2 Class-specific MS Interface Descriptor
		/* MS Class-Specific descriptor */
		0x07,         /* length of descriptor in bytes */
		0x24,         /* descriptor type */
		0x01,         /* header functional descriptor */
		0x0, 0x01,      /* bcdADC */
		CLASS_SPECIFIC_DESC_SIZE, 0,         /* wTotalLength : Why 0x41 ?? XH : don't know... */

		// B.4.3 MIDI IN Jack Descriptor
		// Midi in Jack Descriptor (Embedded)
		0x06,         /* bLength */
		0x24,         /* descriptor type */
		0x02,         /* MIDI_IN_JACK desc subtype */
		0x01,         /* EMBEDDED bJackType */
		0x01,         /* bJackID */
		0x00,         /* UNUSEED */


		// Table B4.4
		// Midi Out Jack Descriptor (Embedded)
		0x09,         /* length of descriptor in bytes */
		0x24,         /* descriptor type */
		0x03,         /* MIDI_OUT_JACK descriptor */
		0x01,         /* EMBEDDED bJackType */
		0x02,         /* bJackID */
		0x01,         /* No of input pins */
		0x01,         /* ID of the Entity to which this Pin is connected. */
		0x01,         /* Output Pin number of the Entity to which this Input Pin is connected. */
		0X00,         /* iJack : UNUSED */


		// ===== B.5 Bulk OUT Endpoint Descriptors
		//B.5.1 Standard Bulk OUT Endpoint Descriptor
		0x09,      /* bLenght */
		0x05,   	/* bDescriptorType = endpoint */
		0x01,      /* bEndpointAddress OUT endpoint number 1 */
		0x02,         /* bmAttributes: 2:Bulk, 3:Interrupt endpoint */
		0x40, 0X00,         /* wMaxPacketSize 64 bytes per packet. */
		0x00,         /* bIntervall in ms : ignored for bulk*/
		0x00,         /* bRefresh Unused */
		0x00,         /* bSyncAddress Unused */

		// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
		0x05,         /* bLength of descriptor in bytes */
		0x25,         /* bDescriptorType */
		0x01,         /* bDescriptorSubtype : GENERAL */
		0x01,         /* bNumEmbMIDIJack  */
		0x01,         /* baAssocJackID (0) ID of the Embedded MIDI IN Jack. */


		//B.6 Bulk IN Endpoint Descriptors
		//B.6.1 Standard Bulk IN Endpoint Descriptor
		0x09,         /* bLenght */
		0x05,   		/* bDescriptorType = endpoint */
		0x81,         /* bEndpointAddress IN endpoint number 1 */
		0X02,         /* bmAttributes: 2: Bulk, 3: Interrupt endpoint */
		0x40, 0X00,         /* wMaxPacketSize */
		0X00,         /* bIntervall in ms */
		0X00,         /* bRefresh */
		0X00,         /* bSyncAddress */

		// B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
		0X05,         /* bLength of descriptor in bytes */
		0X25,         /* bDescriptorType */
		0x01,         /* bDescriptorSubtype */
		0X01,         /* bNumEmbMIDIJack (0) */
		0X02,         /* baAssocJackID (0) ID of the Embedded MIDI OUT Jack	*/
} ;

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_MIDI_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END=
{
		USB_LEN_DEV_QUALIFIER_DESC,
		USB_DESC_TYPE_DEVICE_QUALIFIER,
		0x00,
		0x02,
		0x00,
		0x00,
		0x00,
		0x40,
		0x01,
		0x00,
};

/**
 * @}
 */

/** @defgroup USBD_MIDI_Private_Functions
 * @{
 */

/**
 * @brief  USBD_MIDI_Init
 *         Initialize the MIDI interface
 * @param  pdev: device instance
 * @param  cfgidx: Configuration index
 * @retval status
 */
static uint8_t  USBD_MIDI_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
	/* Open EP IN */
	USBD_LL_OpenEP(pdev, MIDI_IN_EP, USBD_EP_TYPE_BULK, MIDI_OUT_PACKET);
	pdev->ep_in[MIDI_IN_EP & 0xFU].is_used = 1U;

	/* Open EP OUT */
	USBD_LL_OpenEP(pdev, MIDI_OUT_EP, USBD_EP_TYPE_BULK, MIDI_OUT_PACKET);
	pdev->ep_out[MIDI_OUT_EP & 0xFU].is_used = 1U;

	USBD_LL_PrepareReceive(pdev, MIDI_OUT_EP, usbMidiBuffWrt,  MIDI_OUT_PACKET);

	return USBD_OK;
}

/**
 * @brief  USBD_MIDI_Init
 *         DeInitialize the MIDI layer
 * @param  pdev: device instance
 * @param  cfgidx: Configuration index
 * @retval status
 */
static uint8_t  USBD_MIDI_DeInit (USBD_HandleTypeDef *pdev,
		uint8_t cfgidx)
{

	/* Close EP OUT */
	USBD_LL_CloseEP(pdev, MIDI_OUT_EP);
	pdev->ep_out[MIDI_OUT_EP & 0xFU].is_used = 0U;

	/* Close EP IN */
	USBD_LL_CloseEP(pdev, MIDI_IN_EP);
	pdev->ep_in[MIDI_OUT_EP & 0xFU].is_used = 0U;


	return USBD_OK;
}

/**
 * @brief  USBD_MIDI_Setup
 *         Handle the MIDI specific requests
 * @param  pdev: instance
 * @param  req: usb requests
 * @retval status
 */
static uint8_t  USBD_MIDI_Setup (USBD_HandleTypeDef *pdev,
		USBD_SetupReqTypedef *req)
{
	return USBD_OK;
}


/**
 * @brief  USBD_MIDI_GetCfgDesc
 *         return configuration descriptor
 * @param  speed : current device speed
 * @param  length : pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t  *USBD_MIDI_GetCfgDesc (uint16_t *length)
{
	*length = sizeof (USBD_MIDI_CfgDesc);
	return USBD_MIDI_CfgDesc;
}

/**
 * @brief  USBD_MIDI_DataIn
 *         handle data IN Stage
 * @param  pdev: device instance
 * @param  epnum: endpoint index
 * @retval status
 */
static uint8_t  USBD_MIDI_DataIn (USBD_HandleTypeDef *pdev,
		uint8_t epnum)
{

	/* Only OUT data are processed */
	return USBD_OK;
}

/**
 * @brief  USBD_MIDI_EP0_RxReady
 *         handle EP0 Rx Ready event
 * @param  pdev: device instance
 * @retval status
 */
static uint8_t  USBD_MIDI_EP0_RxReady (USBD_HandleTypeDef *pdev)
{
	return USBD_OK;
}
/**
 * @brief  USBD_MIDI_EP0_TxReady
 *         handle EP0 TRx Ready event
 * @param  pdev: device instance
 * @retval status
 */
static uint8_t  USBD_MIDI_EP0_TxReady (USBD_HandleTypeDef *pdev)
{
	/* Only OUT control data are processed */
	return USBD_OK;
}
/**
 * @brief  USBD_MIDI_SOF
 *         handle SOF event
 * @param  pdev: device instance
 * @retval status
 */
static uint8_t  USBD_MIDI_SOF (USBD_HandleTypeDef *pdev)
{
	return USBD_OK;
}


/**
 * @brief  USBD_MIDI_IsoINIncomplete
 *         handle data ISO IN Incomplete event
 * @param  pdev: device instance
 * @param  epnum: endpoint index
 * @retval status
 */
static uint8_t  USBD_MIDI_IsoINIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum)
{

	return USBD_OK;
}
/**
 * @brief  USBD_MIDI_IsoOutIncomplete
 *         handle data ISO OUT Incomplete event
 * @param  pdev: device instance
 * @param  epnum: endpoint index
 * @retval status
 */
static uint8_t  USBD_MIDI_IsoOutIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum)
{

	return USBD_OK;
}
/**
 * @brief  USBD_MIDI_DataOut
 *         handle data OUT Stage
 * @param  pdev: device instance
 * @param  epnum: endpoint index
 * @retval status
 */
static uint8_t  USBD_MIDI_DataOut (USBD_HandleTypeDef *pdev,
		uint8_t epnum)
{
	usbMidiBuffWrt += 64;
	if (usbMidiBuffWrt >= (usbMidiBuffAll + 128)) {
		usbMidiBuffWrt = usbMidiBuffAll;
	}

	USBD_LL_PrepareReceive(pdev, MIDI_OUT_EP, usbMidiBuffWrt,  MIDI_OUT_PACKET);

	((USBD_MIDI_ItfTypeDef *)pdev->pUserData)->dataReceived(usbMidiBuffRead);

	 usbMidiBuffRead = usbMidiBuffWrt;

	return USBD_OK;
}



/**
 * @brief  DeviceQualifierDescriptor
 *         return Device Qualifier descriptor
 * @param  length : pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t  *USBD_MIDI_GetDeviceQualifierDesc (uint16_t *length)
{
	*length = sizeof (USBD_MIDI_DeviceQualifierDesc);
	return USBD_MIDI_DeviceQualifierDesc;
}

/**
 * @brief  USBD_MIDI_RegisterInterface
 * @param  fops: Audio interface callback
 * @retval status
 */
uint8_t  USBD_MIDI_RegisterInterface  (USBD_HandleTypeDef   *pdev,
		USBD_MIDI_ItfTypeDef *fops)
{
	if(fops != NULL)
	{
		pdev->pUserData= fops;
	}
	return USBD_OK;
}
/**
 * @}
 */


/**
 * @}
 */


/**
 * @}
 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
