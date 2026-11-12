

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"


#define USBD_VID     1155
#define USBD_LANGID_STRING     1033
#define USBD_MANUFACTURER_STRING     "Xavier Hosxe"
#define USBD_PID_FS     22336
#define USBD_PRODUCT_STRING_FS     "preenfm3"
#define USBD_CONFIGURATION_STRING_FS     "Midi Config"
#define USBD_INTERFACE_STRING_FS     "MIDI Interface"

static void Get_SerialNum(void);
static void IntToUnicode(uint32_t value, uint8_t * pbuf, uint8_t len);

uint8_t * USBD_FS_MIDI_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_MIDI_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_MIDI_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_MIDI_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_MIDI_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_MIDI_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t * USBD_FS_MIDI_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);


USBD_DescriptorsTypeDef FS_Desc_Midi =
{
  USBD_FS_MIDI_DeviceDescriptor
, USBD_FS_MIDI_LangIDStrDescriptor
, USBD_FS_MIDI_ManufacturerStrDescriptor
, USBD_FS_MIDI_ProductStrDescriptor
, USBD_FS_MIDI_SerialStrDescriptor
, USBD_FS_MIDI_ConfigStrDescriptor
, USBD_FS_MIDI_InterfaceStrDescriptor
};


__ALIGN_BEGIN uint8_t USBD_FS_MIDI_DeviceDesc[18] __ALIGN_END =
{
	    0x12,                       /*bLength */
		USB_DESC_TYPE_DEVICE, /*bDescriptorType*/
	    0x10,                       /*bcdUSB */
	    0x01,
	    0x00,                       /*bDeviceClass*/
	    0x00,                       /*bDeviceSubClass*/
	    0x00,                       /*bDeviceProtocol*/
	    USB_OTG_MAX_EP0_SIZE,      /*bMaxPacketSize*/
	    LOBYTE(USBD_VID),           /*idVendor*/
	    HIBYTE(USBD_VID),           /*idVendor*/
	    LOBYTE(USBD_PID_FS),           /*idVendor*/
	    HIBYTE(USBD_PID_FS),           /*idVendor*/
	    0x00,                       /*bcdDevice rel. 1.00*/
	    0x01,
	    USBD_IDX_MFC_STR,           /*Index of manufacturer  string*/
	    USBD_IDX_PRODUCT_STR,       /*Index of product string*/
	    0x00,        /*Index of serial number string*/
		USBD_MAX_NUM_CONFIGURATION            /*bNumConfigurations*/
};

__ALIGN_BEGIN uint8_t USBD_MIDI_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
     USB_LEN_LANGID_STR_DESC,
     USB_DESC_TYPE_STRING,
     LOBYTE(USBD_LANGID_STRING),
     HIBYTE(USBD_LANGID_STRING)
};


/* Internal string descriptor. */
__ALIGN_BEGIN uint8_t USBD_MIDI_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;


__ALIGN_BEGIN uint8_t USBD_MIDI_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END = {
  USB_SIZ_STRING_SERIAL,
  USB_DESC_TYPE_STRING,
};

uint8_t * USBD_FS_MIDI_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(USBD_FS_MIDI_DeviceDesc);
  return USBD_FS_MIDI_DeviceDesc;
}

uint8_t * USBD_FS_MIDI_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(USBD_FS_MIDI_DeviceDesc);
  return USBD_FS_MIDI_DeviceDesc;
}

uint8_t * USBD_FS_MIDI_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  if(speed == 0)
  {
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_MIDI_StrDesc, length);
  }
  else
  {
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_MIDI_StrDesc, length);
  }
  return USBD_MIDI_StrDesc;
}

uint8_t * USBD_FS_MIDI_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_MIDI_StrDesc, length);
  return USBD_MIDI_StrDesc;
}

uint8_t * USBD_FS_MIDI_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = USB_SIZ_STRING_SERIAL;

  /* Update the serial number string descriptor with the data from the unique
   * ID */
  Get_SerialNum();

  return (uint8_t *) USBD_MIDI_StringSerial;
}

uint8_t * USBD_FS_MIDI_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  if(speed == USBD_SPEED_HIGH)
  {
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_MIDI_StrDesc, length);
  }
  else
  {
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_MIDI_StrDesc, length);
  }
  return USBD_MIDI_StrDesc;
}

uint8_t * USBD_FS_MIDI_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  if(speed == 0)
  {
    USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_FS, USBD_MIDI_StrDesc, length);
  }
  else
  {
    USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_FS, USBD_MIDI_StrDesc, length);
  }
  return USBD_MIDI_StrDesc;
}

static void Get_SerialNum(void)
{
  uint32_t deviceserial0, deviceserial1, deviceserial2;

  deviceserial0 = *(uint32_t *) DEVICE_ID1;
  deviceserial1 = *(uint32_t *) DEVICE_ID2;
  deviceserial2 = *(uint32_t *) DEVICE_ID3;

  deviceserial0 += deviceserial2;

  if (deviceserial0 != 0)
  {
    IntToUnicode(deviceserial0, &USBD_MIDI_StringSerial[2], 8);
    IntToUnicode(deviceserial1, &USBD_MIDI_StringSerial[18], 4);
  }
}

static void IntToUnicode(uint32_t value, uint8_t * pbuf, uint8_t len)
{
  uint8_t idx = 0;

  for (idx = 0; idx < len; idx++)
  {
    if (((value >> 28)) < 0xA)
    {
      pbuf[2 * idx] = (value >> 28) + '0';
    }
    else
    {
      pbuf[2 * idx] = (value >> 28) + 'A' - 10;
    }

    value = value << 4;

    pbuf[2 * idx + 1] = 0;
  }
}
