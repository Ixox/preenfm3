#!/usr/bin/env bash
#
# read_state.sh - read and decode SynthState from the STM32H7 over stlink.
#
# Usage: scripts/read_state.sh [--json] [--raw]
#
# Reads the live `synthState` global from RAM and prints the interesting
# fullState_ fields (mode, current timbre, selected page, mixer column, edited
# name) plus each timbre's engine. This is much cheaper than taking and
# analysing a screenshot when you only need to know where the UI is.
#
# Field offsets are generated from the source with a tiny compile-time
# offsetof probe, so they never drift. The probe reuses the exact compile
# command of app/preenfm3.cpp from build/compile_commands.json.
#
# Environment:
#   ST_FLASH   stlink flasher (default: st-flash)
#   NM         nm tool (default: arm-none-eabi-nm)
#   OBJCOPY    objcopy tool (default: arm-none-eabi-objcopy)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

ST_FLASH="${ST_FLASH:-st-flash}"
NM="${NM:-arm-none-eabi-nm}"
OBJCOPY="${OBJCOPY:-arm-none-eabi-objcopy}"

JSON=0
RAW=0

usage() {
    cat <<EOF
Usage: $0 [--json] [--raw]

Reads and decodes SynthState from the running firmware over stlink.

Options:
  --json   machine-readable output
  --raw    also print the decoded offsets and the raw bytes

Reads the running image address from VTOR, resolves synthState with nm, and
derives FullState / MixerState field offsets by compiling a temporary
offsetof probe with the app/preenfm3.cpp flags.
EOF
}

for arg in "$@"; do
    case "$arg" in
        --json) JSON=1 ;;
        --raw) RAW=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown argument '$arg'" >&2; usage >&2; exit 2 ;;
    esac
done

for tool in "$ST_FLASH" "$NM" "$OBJCOPY" python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: '$tool' not found" >&2
        exit 1
    fi
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ---- which image is running (same logic as pull_png.sh) -------------------
if ! "$ST_FLASH" read "$TMP/vtor.bin" 0xE000ED08 4 >/dev/null 2>&1; then
    echo "error: failed to read VTOR (is the board connected?)" >&2
    exit 1
fi
VTOR="$("${OD:-od}" -An -tx4 -N4 "$TMP/vtor.bin" | tr -d ' \n')"

case "$VTOR" in
    08000000)
        echo "error: the bootloader is running; start the firmware first" >&2
        exit 1
        ;;
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

read -r ADDR SIZE < <("$NM" -S "$ELF" | awk '$4 == "synthState" { print $1, $2 }') || true
if [ -z "${ADDR:-}" ] || [ -z "${SIZE:-}" ]; then
    echo "error: synthState symbol not found in $ELF" >&2
    exit 1
fi
SIZE_DEC=$((16#$SIZE))

# ---- field offsets: cached, regenerated when the sources change -----------
CACHE="$ROOT/build/state_offsets.txt"
CC_JSON="$ROOT/build/compile_commands.json"
DEPS=(
    firmware/src/app/synth/SynthState.h
    firmware/src/app/synth/MixerState.h
    firmware/src/app/synth/Common.h
    firmware/src/app/ui/Menu.h
)

need_regen=0
if [ ! -f "$CACHE" ]; then
    need_regen=1
else
    for dep in "${DEPS[@]}"; do
        if [ "$ROOT/$dep" -nt "$CACHE" ]; then
            need_regen=1
        fi
    done
    if [ "$CC_JSON" -nt "$CACHE" ]; then
        need_regen=1
    fi
    if [ "$ROOT/scripts/read_state.sh" -nt "$CACHE" ]; then
        need_regen=1
    fi
fi

if [ "$need_regen" -eq 1 ]; then
    if [ ! -f "$CC_JSON" ]; then
        echo "error: $CC_JSON missing; configure the build with CMAKE_EXPORT_COMPILE_COMMANDS" >&2
        exit 1
    fi

    python3 - "$ROOT" "$CC_JSON" "$TMP" "$CACHE" <<'PY'
import json
import os
import shlex
import struct
import subprocess
import sys

root, cc_json, tmp, cache = sys.argv[1:5]

entries = json.load(open(cc_json))
entry = None
for e in entries:
    if e["file"].endswith("/app/preenfm3.cpp"):
        entry = e
        break
if entry is None:
    sys.exit("error: app/preenfm3.cpp not found in compile_commands.json")

args = shlex.split(entry["command"])
out = []
i = 0
while i < len(args):
    if args[i] in ("-o", "-c"):
        i += 2
        continue
    out.append(args[i])
    i += 1
src = entry["file"]
out = [a for a in out if a != src]

probe = os.path.join(tmp, "state_probe.cpp")
obj = os.path.join(tmp, "state_probe.o")
bin_ = os.path.join(tmp, "state_offsets.bin")

exprs = [
    ("mixerState_", "offsetof(SynthState, mixerState_)"),
    ("fullState_", "offsetof(SynthState, fullState_)"),
    ("currentTimbre_", "offsetof(SynthState, currentTimbre_)"),
    ("fullStateSize", "sizeof(FullState)"),
    ("synthMode", "offsetof(FullState, synthMode)"),
    ("synthModeBeforeMenu", "offsetof(FullState, synthModeBeforeMenu)"),
    ("menuSelect", "offsetof(FullState, menuSelect)"),
    ("currentMenuItem", "offsetof(FullState, currentMenuItem)"),
    ("editName", "offsetof(FullState, name)"),
    ("mixerCurrentEdit", "offsetof(FullState, mixerCurrentEdit)"),
    ("mainPage", "offsetof(FullState, mainPage)"),
    ("buttonState", "offsetof(FullState, buttonState)"),
    ("instrumentState_", "offsetof(MixerState, instrumentState_)"),
    ("mixerInstrumentSize", "sizeof(MixerInstrumentState)"),
    ("numberOfVoices", "offsetof(MixerInstrumentState, numberOfVoices)"),
    ("engine", "offsetof(MixerInstrumentState, engine)"),
    ("cachedEditor_", "offsetof(ScreenEditor, cachedEditor_)"),
    ("editPage_", "offsetof(EditorEngine, editPage_)"),
    ("editPageSelected_", "offsetof(EditorEngine, editPageSelected_)"),
    ("enginePageState_", "offsetof(EditorEngine, enginePageState_)"),
    ("sizeofEditor", "sizeof(EditorEngine)"),
]

with open(probe, "w") as f:
    f.write("#define private public\n")
    f.write("#define protected public\n")
    f.write('#include "SynthState.h"\n')
    f.write('#include "EditorEngine.h"\n')
    f.write('#include "ScreenEditor.h"\n')
    f.write("#undef protected\n")
    f.write("#undef private\n")
    f.write("#include <stddef.h>\n")
    f.write('extern "C" const unsigned int __p3_state_offsets[] '
            '__attribute__((section(".p3offsets"), used)) = {\n')
    for _, e in exprs:
        f.write("    %s,\n" % e)
    f.write("};\n")

cmd = out + ["-w", "-g0", "-o", obj, "-c", probe]
r = subprocess.run(cmd, cwd=entry["directory"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr[-4000:])
    sys.exit("error: failed to compile the offsets probe")

objcopy = os.environ.get("OBJCOPY", "arm-none-eabi-objcopy")
r = subprocess.run([objcopy, "-O", "binary", "-j", ".p3offsets", obj, bin_],
                   capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
    sys.exit("error: failed to extract .p3offsets")

raw = open(bin_, "rb").read()
values = struct.unpack("<%dI" % (len(raw) // 4), raw)
if len(values) != len(exprs):
    sys.exit("error: offsets probe returned %d values, expected %d"
             % (len(values), len(exprs)))

with open(cache + ".tmp", "w") as f:
    for (name, _), value in zip(exprs, values):
        f.write("%s %d\n" % (name, value))
os.replace(cache + ".tmp", cache)
PY

    if [ ! -f "$CACHE" ]; then
        echo "error: failed to generate $CACHE" >&2
        exit 1
    fi
fi

# ---- read SynthState -----------------------------------------------------
if ! "$ST_FLASH" read "$TMP/state.bin" "0x$ADDR" "$SIZE_DEC" >/dev/null 2>&1; then
    echo "error: failed to read synthState at 0x$ADDR" >&2
    exit 1
fi

# ---- read the active editor's page state (best effort) -------------------
# Engine page states live per editor (EditorEngine::enginePageState_), not in
# SynthState. Reach the active editor through displayEditor.cachedEditor_.
ED_BIN="$TMP/editor.bin"
HAVE_EDITOR=0
read -r DE_ADDR DE_SIZE < <("$NM" -S "$ELF" | awk '$4 == "displayEditor" { print $1, $2 }') || true
if [ -n "${DE_ADDR:-}" ] && "$ST_FLASH" read "$TMP/displayeditor.bin" "0x$DE_ADDR" $((16#$DE_SIZE)) >/dev/null 2>&1; then
    CACHED_OFF=$(awk '$1 == "cachedEditor_" { print $2 }' "$CACHE")
    EDITOR_SIZE=$(awk '$1 == "sizeofEditor" { print $2 }' "$CACHE")
    EDITOR_PTR=$(python3 -c 'import struct,sys; b=open(sys.argv[1],"rb").read(); print(struct.unpack_from("<I", b, int(sys.argv[2]))[0])' "$TMP/displayeditor.bin" "$CACHED_OFF")
    if [ "$EDITOR_PTR" != "0" ] && \
            "$ST_FLASH" read "$ED_BIN" "$(printf '0x%08x' "$EDITOR_PTR")" "$EDITOR_SIZE" >/dev/null 2>&1; then
        HAVE_EDITOR=1
    fi
fi

# ---- decode --------------------------------------------------------------
python3 - "$CACHE" "$TMP/state.bin" "$ADDR" "$JSON" "$RAW" \
    "$ROOT/firmware/src/app/synth/Common.h" "$ED_BIN" "$HAVE_EDITOR" <<'PY'
import json
import struct
import sys

cache, state, addr, do_json, do_raw, common_h, editor_path, have_editor = sys.argv[1:9]
do_json = do_json == "1"
do_raw = do_raw == "1"
have_editor = have_editor == "1"


def parse_button_names(path):
    text = open(path).read()
    start = text.index("BUTTONID_ONLY_ONE_STATE")
    end = text.index("NUMBER_OF_BUTTONIDS", start)
    region = "\n".join(l.split("//")[0] for l in text[start:end].splitlines())
    names = {}
    value = 0
    for token in region.split(","):
        token = token.strip()
        if not token:
            continue
        if "=" in token:
            name, raw = token.split("=", 1)
            name = name.strip()
            try:
                value = int(raw.strip(), 0)
            except ValueError:
                pass
        else:
            name = token
        names[value] = name
        value += 1
    return names


button_names = parse_button_names(common_h)
button_count = max(button_names) + 1 if button_names else 0

off = {}
for line in open(cache):
    name, value = line.split()
    off[name] = int(value)

data = open(state, "rb").read()

def u8(o):
    return data[o]

def i8(o):
    return struct.unpack_from("<b", data, o)[0]

def u16(o):
    return struct.unpack_from("<H", data, o)[0]

def u32(o):
    return struct.unpack_from("<I", data, o)[0]

FS = off["fullState_"]
MIX = off["mixerState_"]

mode_names = {1: "Mixer", 2: "Editor", 3: "Menu", 4: "Sequencer", 5: "Reinit", 0: "None"}
engine_names = {0: "PFM3", 1: "DX7", 2: "BYOS", 3: "909"}
column_names = {1: "Mix", 2: "Out", 3: "Inst", 4: "Midi", 5: "Scala", 6: "Global"}

mode = u8(FS + off["synthMode"])
mode_before = u8(FS + off["synthModeBeforeMenu"])
current_timbre = struct.unpack_from("<i", data, off["currentTimbre_"])[0]
name = data[FS + off["editName"]:FS + off["editName"] + 13].split(b"\0", 1)[0].decode(
    "latin-1", "replace")
mixer_column = u8(FS + off["mixerCurrentEdit"])

editor = None
if have_editor:
    ed = open(editor_path, "rb").read()
    if len(ed) >= off["enginePageState_"] + 8:
        edit_page = ed[off["editPage_"]]
        page_states = list(ed[off["enginePageState_"]:off["enginePageState_"] + 8])
        page_selected = list(ed[off["editPageSelected_"]:off["enginePageState_"]])
        editor = {
            "editPage": edit_page,
            "pageState": page_states[edit_page] if edit_page < len(page_states) else None,
            "pageStates": page_states,
            "pageSelected": page_selected,
        }

state = {
    "synthStateAddress": "0x" + addr,
    "mode": mode,
    "modeName": mode_names.get(mode, "?%d" % mode),
    "modeBeforeMenu": mode_before,
    "modeBeforeMenuName": mode_names.get(mode_before, "?%d" % mode_before),
    "currentTimbre": current_timbre,
    "currentTimbreNumber": current_timbre + 1,
    "editName": name,
    "mainPage": i8(FS + off["mainPage"]),
    "mixerCurrentEdit": mixer_column,
    "mixerColumnName": column_names.get(mixer_column, "?"),
    "menuSelect": u16(FS + off["menuSelect"]),
    "currentMenuItem": "0x%08x" % u32(FS + off["currentMenuItem"]),
    "buttonState": {
        button_names.get(b, "ID_%d" % b):
            data[FS + off["buttonState"] + b] for b in range(button_count)
        if data[FS + off["buttonState"] + b] != 0
    },
    "editor": editor,
    "timbres": [],
}

base = MIX + off["instrumentState_"]
step = off["mixerInstrumentSize"]
for t in range(6):
    engine = data[base + t * step + off["engine"]]
    voices = data[base + t * step + off["numberOfVoices"]]
    state["timbres"].append({
        "index": t,
        "number": t + 1,
        "engine": engine,
        "engineName": engine_names.get(engine, "?%d" % engine),
        "voices": voices,
    })

if do_json:
    print(json.dumps(state, indent=2))
    sys.exit(0)

print("synthState @ %s  mode=%s  timbre=%d  editName=%r"
      % (state["synthStateAddress"], state["modeName"],
         state["currentTimbreNumber"], state["editName"]))
print("  mainPage=%d  mixerColumn=%s  menuSelect=%d  currentMenuItem=%s"
      % (state["mainPage"], state["mixerColumnName"],
         state["menuSelect"], state["currentMenuItem"]))
if state["buttonState"]:
    print("  buttonState(nonzero)=%s" % state["buttonState"])

if state["editor"] and state["mode"] == 2:
    ed = state["editor"]
    print("  engineEditor: editPage=%d pageState=%s pageStates=%s"
          % (ed["editPage"], ed["pageState"], ed["pageStates"]))

print("  engines: " + "  ".join(
    "t%d=%s/%dv" % (t["number"], t["engineName"], t["voices"])
    for t in state["timbres"]))

if do_raw:
    print("  offsets: " + " ".join("%s=%d" % (k, v) for k, v in off.items()))
    print("  bytes: " + data.hex())
PY
