#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Generated data for the 909 drum engine (firmware/src/dsp/drum909.c, fxbus.c).

Writes two headers (both flash-resident `static const` data):

  x0x_drum_samples.h  the hi-hat, ride and crash as 8-bit mu-law (G.711) with
                      its 256-entry int16 decode table (assets/909/*.wav, from
                      ER-99 via 9W9, GPL-3.0): 110 KB instead of 221 KB of int16,
                      37 dB below the signal (the 909's own cymbals are 6-bit).
                      hh.wav is 24-bit: it is rounded to 16 bits first.
  x0x_drum_tables.h   the tanh lookup used by every saturator, and 9W9's EXP pot
                      curves (er99_pots.h: min * (max/min)^(pot/127)) evaluated
                      here, so the device needs no powf and gets the same values
                      9W9 computes with libm.

  tools/gen_drum_samples.py [build/gen/x0x_drum_samples.h] [--ref-wavs DIR]
--ref-wavs also writes the samples as the device plays them (mu-law decoded,
16-bit) to DIR/{hh,ride,crash}.wav, for the 9W9 reference in tests/host.
The tables header is written next to the samples header.
"""
import math
import struct
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
ASSETS = SRC / "assets" / "909"

# (C name, file). The closed hat plays the open hat's buffer (as in 9W9).
SAMPLES = [("x0x_smp_hh", "hh.wav"), ("x0x_smp_ride", "ride.wav"), ("x0x_smp_crash", "crash.wav")]

# 9W9's EXP pot ranges (er99_pots.h). One table per distinct (min, max).
EXP_RANGES = [
    ("BD_DECAY", 100.0, 4000.0), ("BD_PITCH", 0.43, 4.7), ("DRIVE", 0.85, 12.0),
    ("SD_TUNE", 130.0, 320.0), ("SD_TONE", 300.0, 4000.0),
    ("LT_TUNE", 50.0, 90.0), ("LT_DECAY", 80.0, 2600.0),
    ("MT_TUNE", 80.0, 125.0), ("MT_DECAY", 70.0, 2200.0),
    ("HT_TUNE", 110.0, 170.0), ("HT_DECAY", 60.0, 2000.0),
    ("RS_TUNE", 150.0, 300.0), ("CP_TUNE", 650.0, 1400.0), ("CP_TAIL", 120.0, 1000.0),
    ("OH_DECAY", 20.0, 1200.0), ("CH_DECAY", 15.0, 300.0), ("CY_DECAY", 100.0, 3000.0),
    ("SMP_PITCH", 0.25, 4.0), ("FX_HPF", 30.0, 800.0),
]

TANH_N = 1024          # points over [0, TANH_MAX]; linear interpolation
TANH_MAX = 8.0


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def cfloat(x):
    s = repr(f32(x))
    if "e" not in s and "." not in s:
        s += ".0"
    return s + "f"


def read_wav(path):
    """first channel as a list of int16 (16-bit kept, 24-bit rounded)"""
    d = path.read_bytes()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise SystemExit(f"{path}: not a WAV")
    i, ch, bits, data = 12, 1, 16, None
    while i + 8 <= len(d):
        cid, sz = d[i:i + 4], struct.unpack("<I", d[i + 4:i + 8])[0]
        if cid == b"fmt ":
            ch, bits = struct.unpack("<H", d[i + 10:i + 12])[0], struct.unpack("<H", d[i + 22:i + 24])[0]
        elif cid == b"data":
            data = d[i + 8:i + 8 + sz]
            break                                   # what 9W9's loader does: stop at data
        i += 8 + sz + (sz & 1)
    if data is None:
        raise SystemExit(f"{path}: no data chunk")
    bp = bits // 8 * ch
    out = []
    for k in range(len(data) // bp):
        p = data[k * bp:k * bp + bits // 8]
        if bits == 16:
            v = struct.unpack("<h", p)[0]
        elif bits == 24:
            v24 = int.from_bytes(p, "little", signed=True)
            v = max(-32768, min(32767, int(math.floor(v24 / 256.0 + 0.5))))
        else:
            raise SystemExit(f"{path}: {bits}-bit not handled")
        out.append(v)
    return out, bits


def arr(ctype, name, vals, fmt=str, per=12, shared=False):
    # shared: a table several units include, defined once (in the unit that defines
    # X0X_DRUM_TABLES_DEFINE, drum909.c) and declared extern in the rest. Static gave each unit its own
    # copy: two of x0x_tanh_tab and x0x_pot_exp, 13.8 KB of the app's flash.
    if shared:
        return "\n".join([f"#ifdef X0X_DRUM_TABLES_DEFINE",
                          f"const {ctype} {name}[{len(vals)}] = {{"] +
                         ["    " + ", ".join(fmt(v) for v in vals[i:i + per]) + "," for i in range(0, len(vals), per)] +
                         ["};", "#else", f"extern const {ctype} {name}[{len(vals)}];", "#endif"])
    lines = [f"static const {ctype} {name}[{len(vals)}] = {{"]
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join(fmt(v) for v in vals[i:i + per]) + ",")
    lines.append("};")
    return "\n".join(lines)


MU_BIAS, MU_CLIP = 0x84, 32635


def mulaw_enc(x):
    """G.711 mu-law of an int16"""
    sign = 0x80 if x < 0 else 0
    x = min(-x if x < 0 else x, MU_CLIP) + MU_BIAS
    e, m = 7, 0x4000
    while e > 0 and not x & m:
        e -= 1
        m >>= 1
    return ~(sign | e << 4 | (x >> (e + 3)) & 0x0F) & 0xFF


def mulaw_dec(u):
    u = ~u & 0xFF
    x = ((((u & 0x0F) << 3) + MU_BIAS) << ((u >> 4) & 7)) - MU_BIAS
    return -x if u & 0x80 else x


def pot_exp(lo, hi, pot):
    """er99_pot_to_value for an EXP pot, in float as 9W9 evaluates it"""
    ratio = f32(f32(hi) / f32(lo))
    t = f32(pot / 127.0)
    return f32(f32(lo) * f32(ratio ** t))


def main():
    args = sys.argv[1:]
    ref_wavs = None
    if "--ref-wavs" in args:
        i = args.index("--ref-wavs")
        ref_wavs = Path(args[i + 1])
        del args[i:i + 2]
    out_s = Path(args[0]) if args else SRC / "build" / "gen" / "x0x_drum_samples.h"
    out_t = out_s.parent / "x0x_drum_tables.h"
    out_s.parent.mkdir(parents=True, exist_ok=True)

    hdr = ["/* Generated by tools/gen_drum_samples.py -- do not edit.",
           " * Hi-hat, ride and crash from ER-99 (Matthew Cieplak) via 9W9, GPL-3.0.",
           " * Mono, 44.1 kHz, 8-bit mu-law (G.711); x0x_mulaw decodes to int16 (full scale 32768). */",
           "#pragma once", "#include <stdint.h>", "",
           arr("int16_t", "x0x_mulaw", [mulaw_dec(u) for u in range(256)], per=16), ""]
    total = 0
    for name, fn in SAMPLES:
        pcm, bits = read_wav(ASSETS / fn)
        total += len(pcm)
        hdr.append(f"/* {fn}: {len(pcm)} frames, source {bits}-bit */")
        hdr.append(f"#define {name.upper()}_LEN {len(pcm)}u")
        mu = [mulaw_enc(v) for v in pcm]
        hdr.append(arr("uint8_t", name, mu, per=24))
        if ref_wavs:
            ref_wavs.mkdir(parents=True, exist_ok=True)
            body = b"".join(struct.pack("<h", mulaw_dec(u)) for u in mu)
            (ref_wavs / fn).write_bytes(b"RIFF" + struct.pack("<I", 36 + len(body)) + b"WAVEfmt " +
                                        struct.pack("<IHHIIHH", 16, 1, 1, 44100, 88200, 2, 16) +
                                        b"data" + struct.pack("<I", len(body)) + body)
        hdr.append("")
    out_s.write_text("\n".join(hdr))

    tab = [f32(math.tanh(TANH_MAX * i / TANH_N)) for i in range(TANH_N + 2)]
    t = ["/* Generated by tools/gen_drum_samples.py -- do not edit. */",
         "#pragma once", "",
         f"/* tanh(u) at u = i * {TANH_MAX} / {TANH_N}, i = 0..{TANH_N + 1} (one guard point) */",
         f"#define X0X_TANH_N {TANH_N}",
         f"#define X0X_TANH_SCALE {cfloat(TANH_N / TANH_MAX)}",
         arr("float", "x0x_tanh_tab", tab, cfloat, per=6, shared=True), "",
         "/* 9W9 EXP pots (er99_pots.h): value = min * (max/min)^(pot/127), pot 0..127 */"]
    for i, (n, lo, hi) in enumerate(EXP_RANGES):
        t.append(f"#define X0X_EXP_{n} {i}   /* {lo:g} .. {hi:g} */")
    t.append(f"#define X0X_EXP_COUNT {len(EXP_RANGES)}")
    t.append("#ifdef X0X_DRUM_TABLES_DEFINE")                   # shared: see arr()
    t.append(f"const float x0x_pot_exp[{len(EXP_RANGES)}][128] = {{")
    for n, lo, hi in EXP_RANGES:
        vals = [pot_exp(lo, hi, p) for p in range(128)]
        t.append("    { /* " + n + " */")
        for i in range(0, 128, 6):
            t.append("        " + ", ".join(cfloat(v) for v in vals[i:i + 6]) + ",")
        t.append("    },")
    t.append("};")
    t.append("#else")
    t.append(f"extern const float x0x_pot_exp[{len(EXP_RANGES)}][128];")
    t.append("#endif")
    t.append("")
    out_t.write_text("\n".join(t))
    print(f"gen_drum_samples: {total} frames ({total} bytes, mu-law) -> {out_s.name}; "
          f"tanh {TANH_N + 2} + {len(EXP_RANGES)} pot curves -> {out_t.name}")


if __name__ == "__main__":
    main()
