/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z5) The RAM code's and the initialised data's load images, LZ4 blocks in the app image.
 *
 * Until V10.6z4 the image carried them as they run (app.ld: .ram_text, .c1_text, .hot_text, .data at their load
 * addresses after .text, ~33 KB) and main.c fm1_cstart copied them word by word. Now app.ld links their load images
 * outside the flash (region PACKED) and tools/build.py stores each one as an LZ4 block
 * (https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md, no frame, no length), the four one after the other
 * from _etext on (the end of .text), in the order .ram_text, .c1_text, .hot_text, .data; fm1_cstart unpacks them with
 * boot_lz4 below into their sections. What is in RAM after the start is byte for byte what the copy put there
 * (tools/build.py unpacks every block with the same algorithm in Python and compares before it writes the image;
 * tests/boot_lz4_test.c runs this C code on the build's own sections). ~5 KB of flash.
 *
 * A block ends where its output is full (its section's size, from the linker's symbols): after a literal run that
 * reaches d_end nothing more is read. Runs before .data and .bss are set up (fm1_cstart): only locals, no tables, no
 * calls. Out-of-range input (a match before the section's start, a run past its end) stops at the section's end:
 * nothing is written outside [d, d_end). Returns where the next block starts. */
#pragma once
#include <stdint.h>

static const uint8_t *boot_lz4(const uint8_t *s, uint8_t *d, uint8_t *d_end)
{
    uint8_t *const d0 = d;
    while (d < d_end) {
        uint32_t t = *s++, n = t >> 4, b, off;
        const uint8_t *r;
        if (n == 15u)
            do { b = *s++; n += b; } while (b == 255u);
        while (n-- && d < d_end)
            *d++ = *s++;
        if (d >= d_end)
            break;
        off = (uint32_t)s[0] | (uint32_t)s[1] << 8;
        s += 2;
        n = t & 15u;
        if (n == 15u)
            do { b = *s++; n += b; } while (b == 255u);
        n += 4u;
        if (!off || off > (uint32_t)(d - d0))
            return s;                       /* a broken block: nothing outside the section (build.py never makes one) */
        r = d - off;
        while (n-- && d < d_end)            /* (byte by byte: a match may overlap what it writes) */
            *d++ = *r++;
    }
    return s;
}
