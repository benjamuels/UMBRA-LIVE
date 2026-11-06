#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 UMBRA OBSCURA contributors
"""KILN's tables (firmware/src/ui_umbra.c), made here once so they live in flash (const), not in RAM or the pool:
the noise, 64 x 64 bytes of value noise that tiles, two octaves (lattices of 8 and 16; ui_umbra.c's own integer maths),
the light's 1 / |normal| (Q8) for a normal's x and y (Q8) squared and summed: below 2^18 in steps of 1024
(umb_rsa), below 2^24 in steps of 65536 (umb_rsb); the lava's colour at each value of the field (umb_tl: the tone, a dark
base then a steep rise from UMB_T0 over UMB_TW and a brighter crest, through the cold palette), the waves' profile
(umb_E: 255 exp(-i / 32)) and the flashes' dome (umb_D: 255 sqrt(1 - i / 64)).
   python3 tools/gen_umbra_noise.py > firmware/src/ui_umbra_noise.h"""
import math

M = 0xFFFFFFFF
NS = 437                                              # the table's seed


def h32(x, y, s):
    h = (x * 374761393 + y * 668265263 + s * 1442695041) & M
    h = ((h ^ (h >> 13)) * 1274126177) & M
    return h ^ (h >> 16)


def octave(x, y, per, s):
    gx, gy = x * per, y * per
    xi, yi, xf, yf = gx >> 6, gy >> 6, (gx & 63) << 2, (gy & 63) << 2
    u, v = (xf * xf * (768 - 2 * xf)) >> 16, (yf * yf * (768 - 2 * yf)) >> 16
    a, b = h32(xi % per, yi % per, s) >> 24, h32((xi + 1) % per, yi % per, s) >> 24
    c, d = h32(xi % per, (yi + 1) % per, s) >> 24, h32((xi + 1) % per, (yi + 1) % per, s) >> 24
    return a + (((b - a) * u) >> 8) + (((c - a) * v) >> 8) + (((a - b - c + d) * u * v) >> 16)


t = [(octave(x, y, 8, NS) * 159 + octave(x, y, 16, NS + 1) * 97) >> 8 for y in range(64) for x in range(64)]
assert all(0 <= v <= 255 for v in t)
print("/* SPDX-License-Identifier: GPL-3.0-only")
print(" * Copyright (C) 2026 UMBRA OBSCURA contributors */")
print("/* KILN's tables: the noise (64 x 64, tiling), the light's 1 / |normal|, the lava's colour, the waves' and the flashes'")
print(" * profiles (tools/gen_umbra_noise.py: do not edit) */")
print("static const uint8_t umb_tn[64 * 64] = {")
for r in range(0, 4096, 32):
    print("    " + ", ".join(str(v) for v in t[r:r + 32]) + ",")
print("};")
rsa = [min(255, round(256 / (1 + (i * 1024 + 512) / 65536) ** 0.5)) for i in range(256)]
rsb = [min(255, round(256 / (1 + (i * 65536 + 32768) / 65536) ** 0.5)) for i in range(256)]
for name, tab in (("umb_rsa", rsa), ("umb_rsb", rsb)):
    print("static const uint8_t %s[256] = {" % name)
    for r in range(0, 256, 32):
        print("    " + ", ".join(str(v) for v in tab[r:r + 32]) + ",")
    print("};")
T0, TW = 102, 34                                      # the lava's edge: where its tone starts to rise, and over how much


def pal(v):                                           # ui_umbra.c's cold palette (its integer maths)
    P = [(0, 5, 5, 8), (97, 24, 44, 74), (199, 143, 217, 201), (255, 236, 250, 246)]
    INV = (2702, 2570, 4681)
    k = 2 if v > 199 else 1 if v > 97 else 0
    f = ((v - P[k][0]) * INV[k]) >> 10
    return [P[k][c] + (((P[k + 1][c] - P[k][c]) * f) >> 8) for c in (1, 2, 3)]


def smooth(t):
    return (t * t * (768 - 2 * t)) >> 16


tl = []
for v in range(256):
    t, u = min(256, max(0, (v - T0) * 256 // TW)), min(256, max(0, (v - 186) * 5))
    tl += pal(36 + ((smooth(t) * 134) >> 8) + ((smooth(u) * 44) >> 8))
assert tl[:3] * T0 == tl[:3 * T0]                     # (below T0 one colour: ui_umbra.c draws the base a pair at once)
E, e = [], 255 << 16
for k in range(256):
    E.append(e >> 16)
    e = ((e >> 8) * 63519) >> 8
D = [math.isqrt(65025 * (64 - i) // 64) for i in range(65)]
print("#define UMB_T0 %d" % T0)
print("#define UMB_TW %d" % TW)
for name, tab in (("umb_tl[256 * 3]", tl), ("umb_E[256]", E), ("umb_D[65]", D)):
    print("static const uint8_t %s = {" % name)
    for r in range(0, len(tab), 32):
        print("    " + ", ".join(str(v) for v in tab[r:r + 32]) + ",")
    print("};")
