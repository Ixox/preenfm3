#include "usb_descriptors.h"
#include "main.h"
#include "tusb.h"

/* Selected before tusb_init() from the "Usb Audio" config setting. */
static uint8_t usbAudioEnabled = 1;

void usbDescriptorsSetAudioEnabled(int on) {
    usbAudioEnabled = on ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Composite descriptor: USB-MIDI (0/1) + UAC2 duplex audio (2/3/4).   */
/* ------------------------------------------------------------------ */

/* UAC2 duplex, 2 channels: capture (mic) + playback (headphones).
   One AC interface, one AS IN, one AS OUT, one IAD. No AC interrupt
   endpoint and no feedback endpoint (the H7 OTG1_FS has 9 endpoints). */
#define TUD_AUDIO20_DUPLEX_2CH_DESC_LEN                                                                                                    \
    (TUD_AUDIO20_DESC_IAD_LEN + TUD_AUDIO20_DESC_STD_AC_LEN + TUD_AUDIO20_DESC_CS_AC_LEN + TUD_AUDIO20_DESC_CLK_SRC_LEN +                  \
     TUD_AUDIO20_DESC_INPUT_TERM_LEN + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2) +                           \
     TUD_AUDIO20_DESC_INPUT_TERM_LEN + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN + TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_STD_AS_LEN +      \
     TUD_AUDIO20_DESC_CS_AS_INT_LEN + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN +                            \
     TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN + TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_CS_AS_INT_LEN +      \
     TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO20_DUPLEX_2CH_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epin, _epout, _epsize)                 \
    TUD_AUDIO20_DESC_IAD(_itfnum, 0x03, 0x00), TUD_AUDIO20_DESC_STD_AC(_itfnum, 0x00, _stridx),                                            \
        TUD_AUDIO20_DESC_CS_AC(0x0200, AUDIO20_FUNC_HEADSET,                                                                               \
                               TUD_AUDIO20_DESC_CLK_SRC_LEN + TUD_AUDIO20_DESC_INPUT_TERM_LEN + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN +         \
                                   TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2) + TUD_AUDIO20_DESC_INPUT_TERM_LEN +                                \
                                   TUD_AUDIO20_DESC_OUTPUT_TERM_LEN,                                                                       \
                               AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),                                                                  \
        TUD_AUDIO20_DESC_CLK_SRC(0x04, AUDIO20_CLOCK_SOURCE_ATT_INT_FIX_CLK, (AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),    \
                                 0x01, 0x00), /* capture: input terminal 1 -> feature unit 2 -> output terminal 3 */                       \
        TUD_AUDIO20_DESC_INPUT_TERM(0x01, AUDIO_TERM_TYPE_IN_GENERIC_MIC, 0x03, 0x04, 0x02,                                                \
                                    AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT, 0x00,                          \
                                    AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS, 0x00),                                           \
        TUD_AUDIO20_DESC_OUTPUT_TERM(0x03, AUDIO_TERM_TYPE_USB_STREAMING, 0x01, 0x02, 0x04, 0x0000, 0x00),                                 \
        TUD_AUDIO20_DESC_FEATURE_UNIT(                                                                                                     \
            0x02, 0x01, 0x00,                                                                                                              \
            AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS,               \
            AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS,               \
            AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS |                                                                        \
                AUDIO20_CTRL_RW                                                                                                            \
                    << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), /* playback: input terminal 5 (USB streaming) -> output terminal 6 */        \
        TUD_AUDIO20_DESC_INPUT_TERM(0x05, AUDIO_TERM_TYPE_USB_STREAMING, 0x06, 0x04, 0x02,                                                 \
                                    AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT, 0x00, 0x0000, 0x00),           \
        TUD_AUDIO20_DESC_OUTPUT_TERM(0x06, AUDIO_TERM_TYPE_OUT_HEADPHONES, 0x05, 0x05, 0x04, 0x0000, 0x00), /* AS IN (capture) */          \
        TUD_AUDIO20_DESC_STD_AS_INT((uint8_t)((_itfnum) + 1), 0x00, 0x00, 0x00),                                                           \
        TUD_AUDIO20_DESC_STD_AS_INT((uint8_t)((_itfnum) + 1), 0x01, 0x01, 0x00),                                                           \
        TUD_AUDIO20_DESC_CS_AS_INT(0x03, AUDIO20_CTRL_NONE, AUDIO20_FORMAT_TYPE_I, AUDIO20_DATA_FORMAT_TYPE_I_PCM, 0x02,                   \
                                   AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT, 0x00),                          \
        TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),                                                             \
        TUD_AUDIO20_DESC_STD_AS_ISO_EP(_epin, (uint8_t)(TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA),      \
                                       _epsize, 0x01),                                                                                     \
        TUD_AUDIO20_DESC_CS_AS_ISO_EP(AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO20_CTRL_NONE,                                 \
                                      AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, 0x0000), /* AS OUT (playback) */                \
        TUD_AUDIO20_DESC_STD_AS_INT((uint8_t)((_itfnum) + 2), 0x00, 0x00, 0x00),                                                           \
        TUD_AUDIO20_DESC_STD_AS_INT((uint8_t)((_itfnum) + 2), 0x01, 0x01, 0x00),                                                           \
        TUD_AUDIO20_DESC_CS_AS_INT(0x05, AUDIO20_CTRL_NONE, AUDIO20_FORMAT_TYPE_I, AUDIO20_DATA_FORMAT_TYPE_I_PCM, 0x02,                   \
                                   AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT, 0x00),                          \
        TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),                                                             \
        TUD_AUDIO20_DESC_STD_AS_ISO_EP(_epout, (uint8_t)(TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ADAPTIVE | TUSB_ISO_EP_ATT_DATA),         \
                                       _epsize, 0x01),                                                                                     \
        TUD_AUDIO20_DESC_CS_AS_ISO_EP(AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO20_CTRL_NONE,                                 \
                                      AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, 0x0000)

/* Composite: USB-MIDI (0/1) + UAC2 duplex audio (2/3/4). */
enum {
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_AUDIO_CONTROL,
    ITF_NUM_AUDIO_STREAMING_IN,
    ITF_NUM_AUDIO_STREAMING_OUT,
    ITF_NUM_TOTAL,
};

#define EPNUM_MIDI_OUT  0x01
#define EPNUM_MIDI_IN   0x81
#define EPNUM_AUDIO_IN  0x82
#define EPNUM_AUDIO_OUT 0x02

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_AUDIO20_DUPLEX_2CH_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64),
    TUD_AUDIO20_DUPLEX_2CH_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 0, 3, 24, EPNUM_AUDIO_IN, EPNUM_AUDIO_OUT, 288),
};

/* "Usb Audio = Off": MIDI only, no audio function at all. */
#define CONFIG_MIDI_ONLY_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

static uint8_t const desc_configuration_midi_only[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_MIDI_ONLY_LEN, 0x00, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64),
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return usbAudioEnabled ? desc_configuration : desc_configuration_midi_only;
}

/* ------------------------------------------------------------------ */
/* Device descriptor                                                  */
/* ------------------------------------------------------------------ */

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x0483,
    /* Custom PID: 0x5740 is ST's CDC "Virtual COM Port", which makes hosts
       label the audio interface as a serial port. */
    .idProduct = 0x5723,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

/* MIDI-only: no IAD, so the device class is per-interface. */
static tusb_desc_device_t const desc_device_midi_only = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x0483,
    .idProduct = 0x5723,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)(usbAudioEnabled ? &desc_device : &desc_device_midi_only);
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
    "Xavier Hosxe",
    "preenfm3",
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