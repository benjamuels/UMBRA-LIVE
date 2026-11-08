#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 UMBRA OBSCURA contributors
"""The plates' tables (firmware/src/ui_umbra.c, ui_umbra_field.c), made here once so they live in flash (const), not in RAM
or the pool: the noise, 32 x 32 bytes of value noise that tiles, two octaves (lattices of 4 and 8, features 8 and 4
texels apart; 64 x 64 with lattices of 8 and 16 until the melody plates needed 3 KB of the FM-1's slot), the fades' profile
(umb_E: 255 exp(-i / 32)), the angle (umb_at: atan(i / 32),
128 an eighth of a turn) and the log (umb_ln: 512 ln(r / 16), r 16 .. 175 px) that CORONA's map and TARN's reflection use.
(KILN's light, lava and dither tables went with it; the cold palette, umb_pal, is worked out from its stops once,
ui_umbra_field.c ump_lut_make, since 768 bytes of the slot were wanted for the warm tints.)
   python3 tools/gen_umbra_noise.py > firmware/src/ui_umbra_noise.h"""
import math

M = 0xFFFFFFFF
N = 32                                                # the noise table's side (ui_umbra.c UMB_T)
NS = 719                                              # its seed (the one whose spread, range and slopes are closest to
                                                      # the 64 x 64 table's of seed 437: KILN's chemistry and relief kept)


def h32(x, y, s):
    h = (x * 374761393 + y * 668265263 + s * 1442695041) & M
    h = ((h ^ (h >> 13)) * 1274126177) & M
    return h ^ (h >> 16)


def octave(x, y, per, s):
    gx, gy = x * per, y * per
    xi, yi, xf, yf = gx // N, gy // N, (gx % N) * 256 // N, (gy % N) * 256 // N
    u, v = (xf * xf * (768 - 2 * xf)) >> 16, (yf * yf * (768 - 2 * yf)) >> 16
    a, b = h32(xi % per, yi % per, s) >> 24, h32((xi + 1) % per, yi % per, s) >> 24
    c, d = h32(xi % per, (yi + 1) % per, s) >> 24, h32((xi + 1) % per, (yi + 1) % per, s) >> 24
    return a + (((b - a) * u) >> 8) + (((c - a) * v) >> 8) + (((a - b - c + d) * u * v) >> 16)


t = [(octave(x, y, N // 8, NS) * 159 + octave(x, y, N // 4, NS + 1) * 97) >> 8 for y in range(N) for x in range(N)]
assert all(0 <= v <= 255 for v in t)
print("/* SPDX-License-Identifier: GPL-3.0-only")
print(" * Copyright (C) 2026 UMBRA OBSCURA contributors */")
print("/* The plates' tables: the noise (%d x %d, tiling), the fades' profile, the angle and the log" % (N, N))
print(" * (tools/gen_umbra_noise.py: do not edit) */")
print("static const uint8_t umb_tn[%d * %d] = {" % (N, N))
for r in range(0, N * N, 32):
    print("    " + ", ".join(str(v) for v in t[r:r + 32]) + ",")
print("};")
E, e = [], 255 << 16
for k in range(256):
    E.append(e >> 16)
    e = ((e >> 8) * 63519) >> 8
AT = [round(128 * math.atan(i / 32) / (math.pi / 4)) for i in range(33)]
LN = [round(512 * math.log(r / 16)) for r in range(16, 176)]
for name, tab in (("uint8_t umb_E[256]", E), ("uint8_t umb_at[33]", AT),
                  ("uint16_t umb_ln[160]", LN)):
    print("static const %s = {" % name)
    for r in range(0, len(tab), 32):
        print("    " + ", ".join(str(v) for v in tab[r:r + 32]) + ",")
    print("};")
