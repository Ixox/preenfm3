# USB Audio (capture / device to host)

The firmware exposes a **USB Audio Class 2 (UAC2) duplex device** plus
**USB-MIDI** on the same cable, as one composite device. It runs on
**TinyUSB v0.21.0** (vendored in `third_party/tinyusb/`), not the ST USB
Device Library.

- UAC2, PCM, **24-bit stereo**, **48000 Hz**, asynchronous IN / adaptive OUT.
- **Capture**: the sum of the three stereo DAC outputs, 24-bit.
- **Playback**: host audio mixed into **DAC1 only** (the main out), 24-bit.
  The capture is tapped before the mix, so the host never records itself.
- MIDI and audio coexist as a single composite device: `0483:5723`,
  product `"preenfm3"`.

## Menu setting

`MENU -> Config -> Usb Audio` selects `Off` or `On` (default `On`). The value
is stored as `usbaudio=1` in the config file and is read **at boot**, so a
reboot is required to apply a change:

- **On**: the duplex descriptor (USB-MIDI + UAC2 capture/playback).
- **Off**: a MIDI-only descriptor; the audio function, its interfaces and
  endpoints are absent. The host sees a MIDI device only (no PCM).

It is independent of `Usb Midi`, which only gates USB MIDI output today.

## Device layout

Device descriptor: `bcdUSB = 0x0200`, `bDeviceClass = MISC`,
`bDeviceSubClass = COMMON`, `bDeviceProtocol = IAD`.

| Interface | Alt | Class | Endpoints |
|-----------|-----|-------|-----------|
| 0 | 0 | Audio / AudioControl (MIDI) | — |
| 1 | 0 | Audio / MIDIStreaming | bulk OUT `0x01`, bulk IN `0x81`, 64 B |
| 2 | 0 | Audio / AudioControl (UAC2) | — |
| 3 | 0 | Audio / AudioStreaming IN | — |
| 3 | 1 | Audio / AudioStreaming IN | isochronous IN `0x82`, 288 B, 1 ms |
| 4 | 0 | Audio / AudioStreaming OUT | — |
| 4 | 1 | Audio / AudioStreaming OUT | isochronous OUT `0x02`, 288 B, 1 ms |

The audio function has an IAD (`bFirstInterface = 2`, 3 interfaces,
`bFunctionProtocol = 0x20`). The descriptor is a local duplex macro in
`usb_descriptors.c`, based on TinyUSB's UAC2 headset example:

- Audio EP: 288 bytes = 48 frames × 2 ch × 3 B, `bInterval = 1`.
- UAC2 clock source ID 4; capture path 1 (mic) → 2 (feature unit) → 3
  (USB streaming); playback path 5 (USB streaming) → 6 (headphones).
- No AC interrupt endpoint and no feedback endpoint (endpoint budget).
- The feature unit/terminal controls are answered but only store values;
  mute/volume do not affect the stream.

## TinyUSB configuration

`firmware/src/platform/usb/tusb_config.h`:

```c
CFG_TUD_MIDI                            1
CFG_TUD_AUDIO                           1
CFG_TUD_AUDIO_ENABLE_EP_IN              1
CFG_TUD_AUDIO_ENABLE_EP_OUT             1
CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP        0
CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE        48000
CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX 3
CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX      2
CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX 3
CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX      2
CFG_TUD_AUDIO_EP_SZ_IN                  288
CFG_TUD_AUDIO_EP_SZ_OUT                 288
CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX       288
CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ    1152
CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX      288
CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ   1152
```

Device stack files in `firmware/src/platform/usb/`:

| File | Role |
|------|------|
| `tusb_config.h` | TinyUSB configuration |
| `usb_glue.c/.h` | PA11/PA12 AF10, OTG FS clock/IRQ, `tusb_init()`, `usbTask()` |
| `usb_descriptors.c` | Device/config/string descriptors (MIDI + UAC2 duplex) |
| `usb_midi.c/.h` | MIDI TX ring + RX poll into `preenfm3_usbDataReceive()` |
| `usb_audio_tx.c/.h` | UAC2 control callbacks, capture top-up, playback drain |

`firmware/src/platform/cubemx/stm32h7xx_it.c` routes `OTG_FS_IRQHandler` to
`tusb_int_handler(0, true)`. `main.c` calls `usbInit()` and `usbTask()`.

## UAC2 control requests

Linux queries the clock at probe. TinyUSB has no default implementation, so
`usb_audio_tx.c` implements `tud_audio_get_req_entity_cb()` (and a small
`tud_audio_set_req_entity_cb()` for mute/volume):

- entity 4 (clock source): `GET_CUR` sample frequency, `GET_RANGE` (one
  sub-range, 48000..48000), `GET_CUR` clock valid.
- entity 2 (feature unit): mute/volume current value and volume range.
- entity 1 (input terminal): connector control.

Without the clock `GET_RANGE` answer, Linux reports
`parse_audio_format_rates_v2v3(): unable to retrieve number of sample rates`
and creates no PCM device.

## Audio data path

Capture (device → host):

1. In both SAI callbacks (`preenfm3.cpp`) after `buildNewSampleBlock()`,
   `usbAudioPush()` sums the three stereo DAC buffers in int64 and clamps to
   the 24-bit range.
2. `usb_audio.c` resamples 47916 → 48000 with a persistent linear phase and a
   servo on the ring fill; the ring holds **int32** 24-bit left-justified
   frames.
3. `usbAudioPullPacked()` pops 48 frames and packs them as 24-bit
   little-endian (3 bytes/sample).
4. `usbAudioTask()` tops up the TinyUSB IN FIFO whenever it has room for a
   full 288-byte packet (it no longer writes exactly one packet per
   millisecond). This is the pacing fix: UI/display stalls no longer underrun
   the 4-packet FIFO.

Playback (host → device):

1. `usbAudioTask()` drains `tud_audio_n_available()` from the OUT FIFO, in
   whole 6-byte frames, and `usbAudioPlaybackPush()` unpacks the 24-bit
   samples into a 48 kHz int32 ring.
2. The SAI callback calls `usbAudioPlaybackPull()`: it resamples
   48000 → 47916 (output-driven, linear, ring-fill servo) and returns int32
   frames.
3. `mixUsbPlayback()` in `preenfm3.cpp` adds those frames to **DAC1** and
   clamps to the 24-bit range, after the capture push (no loopback).

The synth runs at 47916 Hz, not 48000 Hz; the resampler absorbs the
difference. There is no feedback endpoint: the capture IN is asynchronous and
the playback OUT is adaptive, both resampled on the device.

### Two fixes included here

- **int32 interpolation overflow**: the linear-interpolation difference is
  computed in `int64_t` before the multiply (`(int64_t)l - prevL`), not in
  `int32_t`. The old code could wrap on large sample swings and produce
  broadband noise.
- **FIFO pacing**: `usbAudioTask()` tops up the TinyUSB IN FIFO instead of
  writing one packet per `HAL_GetTick()` millisecond. The old pacing
  underran the FIFO whenever the main loop was late.

## Future: three outputs (one per DAC)

The same 9-endpoint / 320-word FIFO budget drives the design:

- **OTG1_FS limits**: `EP_MAX_FS = 9` (EP0 + 8 data), `DFIFO_DEPTH_FS = 320`
  words. No feedback endpoints, no AC interrupt endpoints.
- **Recommended: one 6-channel capture + one dummy playback** (one device):
  capture carries the 3 DAC stereo pairs as channels 1/2, 3/4, 5/6; a
  2-channel playback stream exists only to make the device duplex (Bitwig
  lists duplex devices).
  - 6ch 16-bit capture = 576 B/ms = 144 words TX; 2ch 16-bit dummy OUT =
    192 B → RX FIFO 130 words; plus EP0 16 + MIDI 16 → **306 / 320**.
  - 6ch 24-bit capture = 864 B/ms = 216 words TX; a normal 2ch 48 kHz dummy
    OUT would need 130 words → **378 / 320, does not fit**. Keep the dummy
    playback small (1ch 16-bit at ≤32 kHz → ~66 words → **314 / 320**) if
    24-bit capture is required.
  - 16-bit is recommended for v1.
- **Alternative: three separate stereo functions** (3 IN + 3 OUT + MIDI):
  8 data endpoints + EP0 = **9 / 9** (exactly full), FIFO **306 / 320**.
  More descriptors, and the host may present one card with 3 PCM pairs rather
  than 3 independent devices. 24-bit does not fit this layout (3 × 72 words TX
  + RX 178 → 426 / 320).
- **Mixing**: input N → DAC N, capture N = DAC N. Add the playback to each DAC
  buffer after the capture push (no loopback) and clamp.
- Rough effort: 6-channel + dummy playback ~4–5 days; 3 separate functions
  ~5–8 days; capture-only ~2–3 days.

## USB-MIDI

`usb_midi.c` uses the TinyUSB **packet** API (`tud_midi_packet_read()`), because
`preenfm3_usbDataReceive()` expects 4-byte USB-MIDI event packets (it parses
the CIN nibble and cable number). `tud_midi_stream_read()` would strip the CIN
and the notes would be dropped. MIDI Out is queued with `usbMidiQueue()` and
sent with `tud_midi_stream_write()`.

## Files

| File | Change |
|------|--------|
| `firmware/src/platform/usb/*` | TinyUSB device stack (config, glue, descriptors, MIDI, audio TX) |
| `firmware/src/app/synth/usb_audio.c/.h` | Capture/playback rings, 24-bit pack/unpack, resamplers |
| `firmware/src/app/preenfm3.cpp` | `usbAudioPush()` from both SAI callbacks; playback mixed into DAC1 |
| `firmware/src/app/midi/MidiDecoder.cpp` | `usbMidiQueue()` instead of `USBD_LL_Transmit()` |
| `firmware/src/platform/cubemx/stm32h7xx_it.c` | `tusb_int_handler(0, true)` |
| `firmware/src/app/main.c` | `usbInit()` / `usbTask()` |
| `third_party/tinyusb/` | Vendored TinyUSB v0.21.0 (`src/`, `LICENSE`) |

## Verification

On Linux:

```sh
arecord -l
# card N: preenfm3 ... device 0: USB Audio
cat /proc/asound/cardN/stream0      # Capture: S24_3LE, 2 ch, 48000, EP 0x82
arecord -D hw:CARD=preenfm3 -f S24_3LE -r 48000 -c 2 -d 2 /tmp/cap.wav
aplay   -D hw:CARD=preenfm3 -f S24_3LE -r 48000 -c 2 /tmp/cap.wav
```

`arecord` exits 0 and writes 288044 bytes for 2 s (96000 frames × 3 B × 2 ch
+ 44 header). The device now also has a playback PCM, so Bitwig/PipeWire list
it as duplex. `amidi -l` shows `preenfm3 MIDI 1`.

Benign kernel messages at probe:

- `Warning! Unlikely small volume range (=180)` — informational; the feature
  unit reports -90..+90 dB in 1 dB steps.
- `Quirk or no altset; falling back to MIDI 1.0` — snd-usb-midi informational.

## Notes / landmines

- **TinyUSB DFIFO depth**: `third_party/tinyusb/src/portable/synopsys/dwc2/dwc2_stm32.h`
  is patched to `DFIFO_DEPTH_FS 320` (the H7 OTG1_FS has 1.25 KB = 320 words).
  Upstream hardcodes 1024, which places TX FIFOs outside the physical FIFO and
  yields zero-length isochronous packets.
- 24-bit is packed little-endian as 3 bytes/sample; `S24_3LE` on Linux.
- The capture is tapped before the playback mix, so the host does not record
  its own playback (no loopback).
- The audio `bInterval`/packet size is fixed at 48 frames/ms; the resampler
  must keep up, so do not increase the block size without checking CPU load.
- The ST USB Device Library was removed; nothing in `firmware/` uses `USBD_*`
  or `HAL_PCD` any more.
