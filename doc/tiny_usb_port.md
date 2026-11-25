# TinyUSB port

Status: **completed**. The ST USB Device Library (and the ST HAL PCD usage)
was replaced by **TinyUSB v0.21.0** in both the firmware and the bootloader.

- Firmware: composite **USB-MIDI + UAC2 stereo capture**.
- Bootloader: **USB mass storage** exposing the SD card.

See `doc/usb_audio.md` for the audio capture path and verification.

## Why

The ST stack enumerated and registered the UAC1 card, but every isochronous IN
packet arrived at the host as a zero-length packet. The root cause is in the
ST PCD integration on the OTG1_FS core: `DIEPTSIZ` was dropped when the
endpoint was disabled between setup and the FIFO write. A second vendor stack
(`synopsys-usb-otg`) on the same board captured correctly, so the USB
hardware/host were ruled out. TinyUSB's `dcd_dwc2` is the maintained Synopsys
driver and is used instead.

A second TinyUSB-specific bug was found and patched:
`third_party/tinyusb/src/portable/synopsys/dwc2/dwc2_stm32.h` hardcodes
`DFIFO_DEPTH_FS 1024`, but OTG1_FS has 1.25 KB = **320 words**. With 1024 the
TX FIFOs are placed outside the physical FIFO, producing the same
zero-length-packet symptom. The local value is 320.

## Architecture

```
firmware/src/platform/usb/
  tusb_config.h        MIDI + UAC2 capture configuration
  usb_glue.c/.h        PA11/PA12 AF10, OTG clock/IRQ, tusb_init(), usbTask()
  usb_descriptors.c    device/config/string descriptors
  usb_midi.c/.h        MIDI TX ring; RX packet poll -> preenfm3_usbDataReceive()
  usb_audio_tx.c/.h    UAC2 control callbacks + 1 ms capture feed
firmware/src/app/synth/usb_audio.c/.h   downmix/resample/ring (unchanged)
third_party/tinyusb/   vendored v0.21.0 (src/, LICENSE)
```

Bootloader:

```
bootloader/src/platform/usb/
  tusb_config.h        MSC only
  usb_glue.c/.h        start/stop TinyUSB around SD access mode
  usb_descriptors.c    MSC device descriptors (0483:572A, "STM32 Mass Storage")
  usb_msc.c            tud_msc_* callbacks over PFM3_SD_Read/WriteBlocks
```

`stm32h7xx_it.c` (both projects) routes `OTG_FS_IRQHandler` to
`tusb_int_handler(0, true)`. The bootloader starts USB only while
"SD CARD ACCESS" is on screen (`usbInit()`/`usbDeinit()`), preserving the old
behaviour.

## Removed

- `third_party/stm32_usb_device/` (entire vendored ST stack).
- Firmware ST USB files (`usb_device.*`, `usbd_conf.*`, `usbd_desc*`,
  `usbd_midi*`, `usbd_midi_audio.*`, `usbd_audio_if.*`).
- Bootloader ST USB files (`usb_device.*`, `usbd_desc.*`, `usbd_conf.*`,
  `usbd_storage_if.*`).
- `stm32_usb_core` / `stm32_usb_msc` CMake targets; both projects link
  `tinyusb`.
- `HAL_PCD_MODULE_ENABLED` and the `stm32h7xx_hal_pcd.h` include.

## Firmware

Device: `0483:5723`, `"Xavier Hosxe"` / `"preenfm3"`, USB 2.00,
MISC/IAD.

- MIDI interfaces 0/1 (bulk `0x01`/`0x81`, 64 B).
- UAC2 audio control interface 2, streaming interface 3 alt 0/1.
- ISO IN `0x82`, 192 B/ms (48 frames, 2 ch, 16-bit), async.
- 48 kHz, resampled from the synth's 47916 Hz.
- IAD groups interfaces 2/3.

UAC2 control requests (`tud_audio_get_req_entity_cb`) answer the clock
sample-frequency `GET_CUR`/`GET_RANGE`, clock valid, feature-unit mute/volume
and terminal connector. Without the clock range answer Linux creates no PCM.

MIDI RX uses `tud_midi_packet_read()`: `preenfm3_usbDataReceive()` expects
4-byte USB-MIDI event packets. The stream API strips the CIN and drops notes.

## Bootloader

Device: `0483:572A`, `"STM32 Mass Storage"`, one SCSI LUN
(`IXOX` / `Pfm3SD` / `1.05`), 512-byte blocks. `tud_msc_read10_cb` /
`tud_msc_write10_cb` forward to `PFM3_SD_ReadBlocks` / `PFM3_SD_WriteBlocks`.

Two non-obvious requirements:

1. **DMA-capable memory.** TinyUSB's `_mscd_epbuf` is in DTCM (`0x2000...`),
   which H7 DMA cannot access. The SD DMA path therefore goes through a
   512-byte bounce buffer placed in `.ram_d2` (`0x3000...`), then `memcpy`.
2. **Correct capacity.** `ADAFRUIT_802_SD_GetCardInfo()` is cached once at
   `usbMscInit()` instead of being called from the USB control callback. The
   CSD parser (`SD_GetCSDRegister`) used an uninitialized `CSD_Tab[16]`; only
   the low byte of each word was written, so uninitialized stack leaked into
   the 22-bit `DeviceSize` and the reported capacity was random (1.12 TB,
   292 GB, 360 GB...). It is now zero-initialized.

## Verification

Firmware (Linux):

```
arecord -l                       -> card "preenfm3", capture device
/proc/asound/cardN/stream0       -> S16_LE, 2 ch, 48000, EP 0x82 (ASYNC)
arecord -D hw:CARD=preenfm3 -f S16_LE -r 48000 -c 2 -d 2 ...
amidi -l                         -> preenfm3 MIDI 1
```

A MIDI note during capture produces non-zero samples.

Bootloader:

```
lsblk -> 15.9 GB "Pfm3SD", sdc1 vfat FAT32 label PFM3
```

Writing the current firmware to `P3_1_99.BIN` over MSC matched SHA-256,
survived a reboot, appeared in the bootloader file list, and flashed
successfully.

## Notes

- The bootloader keeps the previous VID/PID and product string, so the host
  treats it separately from the firmware.
- Kernel probe warnings (`Unlikely small volume range`, `falling back to
  MIDI 1.0`) are informational.
- No feedback endpoint: the audio IN endpoint is async and the device is the
  clock master; the resampler absorbs 47916 vs 48000 Hz drift.