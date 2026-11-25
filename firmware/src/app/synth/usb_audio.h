#ifndef USB_AUDIO_H
#define USB_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Capture (device -> host): the sum of the three DAC buffers, 24-bit
   left-justified int32 at 47916 Hz, resampled to 48 kHz. */
void usbAudioPush(const int32_t* w1, const int32_t* w2, const int32_t* w3, int frames);
/* Pop 48 kHz frames and pack them as 24-bit little-endian (3 bytes/sample). */
void usbAudioPullPacked(uint8_t* dst, int frames);
/* Clear the ring and the resampler (call before starting a stream). */
void usbAudioReset(void);
/* When off, usbAudioPush() returns immediately (no host stream). */
void usbAudioSetStreaming(int on);

/* Playback (host -> device): 48 kHz 24-bit little-endian frames pushed from
   the USB task, resampled to 47916 Hz and pulled by the SAI callback. */
void usbAudioPlaybackReset(void);
void usbAudioSetPlaybackStreaming(int on);
void usbAudioPlaybackPush(const uint8_t* src, int frames);
/* Produce 47916 Hz left-justified int32 stereo frames (silence on underrun). */
void usbAudioPlaybackPull(int32_t* dst, int frames);

#ifdef __cplusplus
}
#endif

#endif
