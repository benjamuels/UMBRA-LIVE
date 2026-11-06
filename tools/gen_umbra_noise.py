#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 UMBRA OBSCURA contributors
"""KILN's tables (firmware/src/ui_umbra.c), made here once so they live in flash (const), not in RAM or the pool:
the noise, 64 x 64 bytes of value noise that tiles, two octaves (lattices of 8 and 16; ui_umbra.c's own integer maths),
and the light's 1 / |normal| (Q8) for a normal's x and y (Q8) squared and summed: below 2^18 in steps of 1024
(umb_rsa), below 2^24 in steps of 65536 (umb_rsb).
   python3 tools/gen_umbra_noise.py > firmware/src/ui_umbra_noise.h"""
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
print("/* KILN's tables: the noise (64 x 64, tiling) and the light's 1 / |normal| (tools/gen_umbra_noise.py: do not edit) */")
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
