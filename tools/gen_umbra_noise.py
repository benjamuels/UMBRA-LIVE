#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 UMBRA OBSCURA contributors
"""KILN's tables (firmware/src/ui_umbra.c), made here once so they live in flash (const), not in RAM or the pool:
the noise, 32 x 32 bytes of value noise that tiles, two octaves (lattices of 4 and 8, features 8 and 4 texels apart;
ui_umbra.c's own integer maths; 64 x 64 with lattices of 8 and 16 until the melody plates needed 3 KB of the FM-1's slot),
the light's 1 / |normal| (Q8) for a normal's x and y (Q8) squared and summed: below 2^18 in steps of 1024
(umb_rsa), below 2^24 in steps of 65536 (umb_rsb); the lava's colour at each value of the field (umb_tl: the tone, a dark
base then a steep rise from UMB_T0 over UMB_TW and a brighter crest, through the cold palette), the waves' profile
(umb_E: 255 exp(-i / 32)), the flashes' dome (umb_D: 255 sqrt(1 - i / 64)) and the dither (umb_bn: 16 x 16 blue
noise, void and cluster).
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
print("/* KILN's tables: the noise (%d x %d, tiling), the light's 1 / |normal|, the lava's colour, the waves' and the flashes'" % (N, N))
print(" * profiles (tools/gen_umbra_noise.py: do not edit) */")
print("static const uint8_t umb_tn[%d * %d] = {" % (N, N))
for r in range(0, N * N, 32):
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


V0, B0, B1 = 56, 26, 46                               # the base: from v V0 (palette B0) up to the edge (B1), gently: the
tl = []                                               # noise's own shapes in the dark lava
for v in range(256):
    t, u = min(256, max(0, (v - T0) * 256 // TW)), min(256, max(0, (v - 186) * 5))
    b = B0 + (B1 - B0) * min(T0 - V0, max(0, v - V0)) // (T0 - V0)
    tl += pal(b + ((smooth(t) * (170 - B1)) >> 8) + ((smooth(u) * 44) >> 8))
E, e = [], 255 << 16
for k in range(256):
    E.append(e >> 16)
    e = ((e >> 8) * 63519) >> 8
D = [math.isqrt(65025 * (64 - i) // 64) for i in range(65)]


def blue(n, sigma=1.5):                               # void and cluster (Ulichney): an n x n threshold map, ranks 0..n^2-1
    def k(dx, dy):
        dx, dy = min(dx, n - dx), min(dy, n - dy)
        return math.exp(-(dx * dx + dy * dy) / (2 * sigma * sigma))
    K = [[k(dx, dy) for dx in range(n)] for dy in range(n)]

    def en(m, i):
        y, x = divmod(i, n)
        return sum(K[(y - j // n) % n][(x - j % n) % n] for j in range(n * n) if m[j])
    seed, m = 12345, [False] * (n * n)
    for _ in range(n * n // 10):                      # (a sparse start, placed by a fixed LCG)
        while True:
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            if not m[seed % (n * n)]:
                m[seed % (n * n)] = True
                break
    while True:                                       # spread it: the tightest cluster's point into the largest void
        c = max((i for i in range(n * n) if m[i]), key=lambda i: en(m, i))
        m[c] = False
        v = min((i for i in range(n * n) if not m[i]), key=lambda i: en(m, i))
        m[v] = True
        if c == v:
            break
    rank, ones = [0] * (n * n), sum(m)
    p = m[:]
    for r in range(ones - 1, -1, -1):
        c = max((i for i in range(n * n) if p[i]), key=lambda i: en(p, i))
        p[c], rank[c] = False, r
    p = m[:]
    for r in range(ones, n * n):
        v = min((i for i in range(n * n) if not p[i]), key=lambda i: en(p, i))
        p[v], rank[v] = True, r
    return rank


BN = blue(16)                                         # the dither: blue noise, 16 x 16 (a grain, not a cross-hatch)
assert sorted(BN) == list(range(256))
print("#define UMB_T0 %d" % T0)
print("#define UMB_TW %d" % TW)
for name, tab in (("umb_tl[256 * 3]", tl), ("umb_E[256]", E), ("umb_D[65]", D), ("umb_bn[256]", BN)):
    print("static const uint8_t %s = {" % name)
    for r in range(0, len(tab), 32):
        print("    " + ", ".join(str(v) for v in tab[r:r + 32]) + ",")
    print("};")
