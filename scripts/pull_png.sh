#!/usr/bin/env bash
#
# pull_png.sh - grab the 240x320 TFT framebuffer over stlink and save a PNG.
#
# Usage: scripts/pull_png.sh <file_name>
#        writes images/<file_name>.png (a .png suffix is optional).
#
# ---------------------------------------------------------------------------
# How to extract the tftMemory address by hand:
#
#   arm-none-eabi-nm -S build/firmware/preenfm3_firmware.elf | grep tftMemory
#   -> 2402d068 00025800 B tftMemory
#
#   arm-none-eabi-nm -S build/bootloader/preenfm3_bootloader.elf | grep tftMemory
#   -> 24000000 00025800 B tftMemory
#
# `nm -S` prints: <address> <size> <type> <name>. The size 0x25800 is
# 240 * 320 * 2 bytes (RGB565). The address moves whenever the firmware is
# rebuilt, so this script does not hard-code it: it reads VTOR (0xE000ED08)
# to find out which image is running, then asks nm for tftMemory in the
# matching ELF:
#   VTOR 0x08000000 -> build/bootloader/preenfm3_bootloader.elf
#   VTOR 0x08020000 -> build/firmware/preenfm3_firmware.elf
#
# The pixels are stored byte-swapped (the byte order sent to the ILI9341 over
# SPI); the conversion below matches firmware/src/app/storage/PPMImage.cpp.
# ---------------------------------------------------------------------------

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [ "$#" -ne 1 ]; then
    cat <<EOF
Usage: $0 <file_name>

Reads the 240x320 TFT framebuffer from the STM32H7 over stlink and writes
images/<file_name>.png (a .png suffix on <file_name> is optional).

The running image is detected from VTOR:
  0x08000000 -> bootloader framebuffer
  0x08020000 -> firmware framebuffer
EOF
    exit 1
fi

NAME="${1%.png}"
OUT="$ROOT/images/${NAME}.png"

ST_FLASH="${ST_FLASH:-st-flash}"
NM="${NM:-arm-none-eabi-nm}"

for tool in "$ST_FLASH" "$NM" python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: '$tool' not found" >&2
        exit 1
    fi
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Which image is running?
if ! "$ST_FLASH" read "$TMP/vtor.bin" 0xE000ED08 4 >/dev/null 2>&1; then
    echo "error: failed to read VTOR (is the board connected?)" >&2
    exit 1
fi
VTOR="$(od -An -tx4 -N4 "$TMP/vtor.bin" | tr -d ' \n')"

case "$VTOR" in
    08000000) ELF="$ROOT/build/bootloader/preenfm3_bootloader.elf" ;;
    08020000) ELF="$ROOT/build/firmware/preenfm3_firmware.elf" ;;
    *)
        echo "error: unexpected VTOR 0x$VTOR" >&2
        exit 1
        ;;
esac

if [ ! -f "$ELF" ]; then
    echo "error: $ELF not found (build the project first)" >&2
    exit 1
fi

# tftMemory address and size from the ELF of the running image.
read -r ADDR SIZE < <("$NM" -S "$ELF" | awk '$4 == "tftMemory" { print $1, $2 }') || true
if [ -z "${ADDR:-}" ] || [ -z "${SIZE:-}" ]; then
    echo "error: tftMemory symbol not found in $ELF" >&2
    exit 1
fi
SIZE_DEC=$((16#$SIZE))

mkdir -p "$(dirname "$OUT")"

if ! "$ST_FLASH" read "$TMP/tft.bin" "0x$ADDR" "$SIZE_DEC" >/dev/null 2>&1; then
    echo "error: failed to read tftMemory at 0x$ADDR" >&2
    exit 1
fi

python3 - "$TMP/tft.bin" "$OUT" <<'PY'
import struct
import sys
import zlib

src, dst = sys.argv[1], sys.argv[2]
raw = open(src, "rb").read()

W, H = 240, 320
need = W * H * 2
if len(raw) < need:
    raise SystemExit(f"error: framebuffer dump is {len(raw)} bytes, expected {need}")

rows = bytearray()
for j in range(H):
    rows.append(0)  # PNG filter type 0 (None)
    base = j * W * 2
    for i in range(W):
        p = raw[base + i * 2] | (raw[base + i * 2 + 1] << 8)
        v = ((p >> 8) | (p << 8)) & 0xFFFF  # byte-swapped in memory
        r = ((v & 0xF800) >> 11) << 3
        g = ((v & 0x07E0) >> 5) << 2
        b = (v & 0x001F) << 3
        rows += bytes((r, g, b))


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
png += chunk(b"IEND", b"")
open(dst, "wb").write(png)
PY

echo "wrote $OUT"
