#include "main.h"
#include "tusb.h"

/* ------------------------------------------------------------------ */
/* Bootloader USB-MSC device. Same identity as the old ST stack.       */
/* ------------------------------------------------------------------ */

#define BOOT_USBD_VID 0x0483
#define BOOT_USBD_PID 0x572A

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL,
};

#define EPNUM_MSC_OUT 0x01
#define EPNUM_MSC_IN  0x81

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

/* ------------------------------------------------------------------ */
/* Device descriptor                                                  */
/* ------------------------------------------------------------------ */

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = BOOT_USBD_VID,
    .idProduct = BOOT_USBD_PID,
    .bcdDevice = 0x0200,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)&desc_device;
}

/* ------------------------------------------------------------------ */
/* String descriptors                                                 */
/* ------------------------------------------------------------------ */

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

static char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04}, /* English (0x0409) */
    "STMicroelectronics",
    "STM32 Mass Storage",
};

static char serial_str[25];
static uint16_t desc_str[32 + 1];

static void buildSerial(void) {
    static const char hex[] = "0123456789ABCDEF";
    uint32_t uid[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 8; j++) {
            serial_str[i * 8 + j] = hex[(uid[i] >> ((7 - j) * 4)) & 0x0FU];
        }
    }
    serial_str[24] = '\0';
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    switch (index) {
    case STRID_LANGID:
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
        break;
    case STRID_SERIAL:
        if (serial_str[0] == '\0') {
            buildSerial();
        }
        chr_count = strlen(serial_str);
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint16_t)serial_str[i];
        }
        break;
    default: {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 32) {
            chr_count = 32;
        }
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint16_t)str[i];
        }
        break;
    }
    }

    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str;
}