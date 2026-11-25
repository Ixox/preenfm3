#include "usb_midi.h"
#include "tusb.h"

#define MIDI_TX_RING 256U

static uint8_t midiTxRing[MIDI_TX_RING];
static volatile uint16_t midiTxHead = 0;
static volatile uint16_t midiTxTail = 0;

void preenfm3_usbDataReceive(uint8_t* buffer);

void usbMidiQueue(const uint8_t* buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint16_t next = (uint16_t)((midiTxHead + 1U) % MIDI_TX_RING);
        if (next == midiTxTail) {
            break;
        }
        midiTxRing[midiTxHead] = buf[i];
        midiTxHead = next;
    }
}

static void midiTxFlush(void) {
    while (midiTxTail != midiTxHead) {
        uint16_t n = (uint16_t)((midiTxHead - midiTxTail) % MIDI_TX_RING);
        if (n > 64U) {
            n = 64U;
        }
        uint32_t written = tud_midi_stream_write(0, &midiTxRing[midiTxTail], n);
        if (written == 0U) {
            break;
        }
        midiTxTail = (uint16_t)((midiTxTail + written) % MIDI_TX_RING);
    }
}

void usbMidiTask(void) {
    midiTxFlush();

    if (!tud_midi_mounted()) {
        return;
    }

    uint8_t packet[4];
    while (tud_midi_packet_read(packet)) {
        uint8_t buf[5];
        buf[0] = packet[0];
        buf[1] = packet[1];
        buf[2] = packet[2];
        buf[3] = packet[3];
        buf[4] = 0U;
        preenfm3_usbDataReceive(buf);
    }
}