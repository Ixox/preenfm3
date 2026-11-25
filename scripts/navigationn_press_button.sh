#!/usr/bin/env bash
#
# navigationn_press_button.sh - press a front-panel button on the preenfm3.
#
# Usage: scripts/navigationn_press_button.sh <1-12>
#
# Sends NRPN MSB=120 (UI simulation), LSB = button id - 1, value 0.
# Requires firmware with the MSB=120 UI simulation block and "receives NRPN".
#
#   1  Switch 1          7  Menu
#   2  Switch 2          8  Previous instrument
#   3  Switch 3          9  Next instrument
#   4  Switch 4         10  Mixer
#   5  Switch 5         11  Edit
#   6  Switch 6         12  Sequencer
#
# Environment:
#   PORT=hw:X,0,0   MIDI port (default: auto-detect "preenfm3 MIDI 1")
#   CHANNEL=1..16   MIDI channel (default 1)
#
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 <1-12>

Press a front-panel button on the preenfm3 via NRPN MSB=120.

   1  Switch 1          7  Menu
   2  Switch 2          8  Previous instrument
   3  Switch 3          9  Next instrument
   4  Switch 4         10  Mixer
   5  Switch 5         11  Edit
   6  Switch 6         12  Sequencer

Requires the firmware with the MSB=120 UI simulation block and
"receives NRPN" enabled. Sends on MIDI channel 1 by default.

Environment:
  PORT=hw:X,0,0   MIDI port (default: auto-detect "preenfm3 MIDI 1")
  CHANNEL=1..16   MIDI channel (default 1)
EOF
}

if [ "$#" -ne 1 ]; then
    usage
    exit 0
fi

case "$1" in
    1) LSB=0;  NAME="Switch 1" ;;
    2) LSB=1;  NAME="Switch 2" ;;
    3) LSB=2;  NAME="Switch 3" ;;
    4) LSB=3;  NAME="Switch 4" ;;
    5) LSB=4;  NAME="Switch 5" ;;
    6) LSB=5;  NAME="Switch 6" ;;
    7) LSB=6;  NAME="Menu" ;;
    8) LSB=7;  NAME="Previous instrument" ;;
    9) LSB=8;  NAME="Next instrument" ;;
    10) LSB=9; NAME="Mixer" ;;
    11) LSB=10; NAME="Edit" ;;
    12) LSB=11; NAME="Sequencer" ;;
    *)
        echo "error: button must be 1..12" >&2
        usage
        exit 1
        ;;
esac

if ! command -v amidi >/dev/null 2>&1; then
    echo "error: amidi not found (install alsa-utils)" >&2
    exit 1
fi

if [ -z "${PORT:-}" ]; then
    PORT="$(amidi -l 2>/dev/null | awk '/preenfm3 MIDI/ {print $2; exit}')"
fi
if [ -z "${PORT:-}" ]; then
    echo "error: preenfm3 MIDI port not found (set PORT=hw:X,0,0)" >&2
    exit 1
fi

CHANNEL="${CHANNEL:-1}"
if ! [[ "$CHANNEL" =~ ^[0-9]+$ ]] || [ "$CHANNEL" -lt 1 ] || [ "$CHANNEL" -gt 16 ]; then
    echo "error: CHANNEL must be 1..16" >&2
    exit 1
fi

STATUS=$(printf '%02X' $((0xB0 | (CHANNEL - 1))))
LSB_HEX=$(printf '%02X' "$LSB")

amidi -p "$PORT" -S "$STATUS 63 78 $STATUS 62 $LSB_HEX $STATUS 06 00 $STATUS 26 00"

echo "pressed $NAME (NRPN [120, $LSB]) on $PORT, channel $CHANNEL"
