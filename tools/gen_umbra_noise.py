#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 UMBRA OBSCURA contributors
"""The plates' tables (firmware/src/ui_umbra.c, ui_umbra_field.c), made here once so they live in flash (const), not in RAM
or the pool: the angle (umb_at: atan(i / 32), 128 an eighth of a turn) that CORONA's map uses. (KILN's light, lava and
dither tables went with it; the cold palette, umb_pal, is worked out from its stops once, ui_umbra_field.c ump_lut_make,
since 768 bytes of the slot were wanted for the warm tints; and the noise, umb_tn, 32 x 32 bytes, the fades' profile, umb_E, and
the log, umb_ln, at power-on in RAM, ui_umbra.c umb_tabs, the same values this script wrote until 2026-11-08, since the
welcome wanted 1.6 KB of the slot.)
   python3 tools/gen_umbra_noise.py > firmware/src/ui_umbra_noise.h"""
import math

print("/* SPDX-License-Identifier: GPL-3.0-only")
print(" * Copyright (C) 2026 UMBRA OBSCURA contributors */")
print("/* The plates' tables: the angle (the noise, the fades' profile, the log: ui_umbra.c umb_tabs)")
print(" * (tools/gen_umbra_noise.py: do not edit) */")
AT = [round(128 * math.atan(i / 32) / (math.pi / 4)) for i in range(33)]
for name, tab in (("uint8_t umb_at[33]", AT),):
    print("static const %s = {" % name)
    for r in range(0, len(tab), 32):
        print("    " + ", ".join(str(v) for v in tab[r:r + 32]) + ",")
    print("};")
