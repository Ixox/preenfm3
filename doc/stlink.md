# Flashing with ST-LINK — preenFM3

The Arch `stlink` package provides `st-flash` and `st-info` (tested with 1.9.0).

## Prerequisites

All paths below are relative to the repository root. If you are elsewhere inside the repository, move there first:

```bash
cd "$(git rev-parse --show-toplevel)"
```

Otherwise `st-flash` reports `open(build/firmware/... ) == -1 / map_file() == -1`, which means it cannot find the file from the current directory.

## Flash the firmware

The firmware starts at `0x08020000`, after the 128 KB bootloader:

```bash
st-flash --reset write build/firmware/p3_1_04.bin 0x08020000
```

## Check the probe / troubleshooting

```bash
st-info --probe
st-flash --connect-under-reset --reset write build/firmware/p3_1_04.bin 0x08020000
```

## Flash the bootloader

Needed on a never-flashed board, or when the bootloader itself must be replaced. It lives at `0x08000000`:

```bash
st-flash --reset write build/bootloader/p3_boot_1_10.bin 0x08000000
```

## HEX alternative

The `.hex` file already contains the address, so no `addr` argument is needed:

```bash
st-flash --format ihex write build/firmware/preenfm3_firmware.hex
```

## Notes

- `st-flash write` verifies the flash content after programming.
- If you get `Permission denied`, unplug/replug the ST-LINK or run with `sudo`. The `49-stlinkv*.rules` udev rules are installed by the `stlink` package.
- `st-info --probe` prints the chip ID/flash size and confirms the connection.
- Firmware artifacts are named after the version in `firmware/include/version.h` (`p3_<version>.bin`, e.g. `p3_1_04.bin`). Optimization experiment builds can opt into a short `p3_<variant>.bin` name with `-DPRE_ENFM3_FW_VARIANT`; see `doc/compilation.md` for what each variant means.