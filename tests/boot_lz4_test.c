/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z5) Host test of the boot's LZ4 unpacker (firmware/src/boot_lz4.h, main.c fm1_cstart): the RAM code and the
 * initialised data are LZ4 blocks in the app image (tools/build.py, tools/lz4blk.py compress_small).
 *   - the build's own blocks (build/boot_lz4.bin; when there is a build): unpacked one after the other into the
 *     sizes of build/{ramtext,c1text,hottext,data}.bin they are byte for byte those sections, each block ending where
 *     the next starts and the last at the stream's end;
 *   - blocks made here (a greedy compressor of this file's own, matches as long as they go, overlapping ones, literal
 *     and match lengths past 15 + 255): random, runs, text-like, empty and tiny inputs, 400 of them;
 *   - broken input (a match before the start, an offset of 0) writes nothing outside its section.
 *   cc -O1 -Ifirmware/src tests/boot_lz4_test.c -o build/boot_lz4_test && build/boot_lz4_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/boot_lz4.h"

static int fails;
static void check(const char *what, int ok)
{
    printf("boot_lz4: %-92s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static uint8_t *put_len(uint8_t *o, uint32_t n)
{
    while (n >= 255u) { *o++ = 255; n -= 255u; }
    *o++ = (uint8_t)n;
    return o;
}
/* greedy, a 4-byte hash of the last position; the format's end rules (the last 5 bytes literals, no match from the
 * last 12) */
static uint32_t pack(const uint8_t *s, uint32_t n, uint8_t *o0)
{
    static int32_t last[65536];
    uint8_t *o = o0;
    uint32_t i = 0, anchor = 0;
    memset(last, -1, sizeof last);
    while (n >= 13u && i < n - 12u) {
        uint32_t h = (uint32_t)(s[i] | s[i + 1] << 8 | s[i + 2] << 16 | (uint32_t)s[i + 3] << 24) * 2654435761u >> 16;
        int32_t c = last[h];
        last[h] = (int32_t)i;
        if (c >= 0 && i - (uint32_t)c <= 65535u && !memcmp(s + c, s + i, 4)) {
            uint32_t m = 4, lit = i - anchor, off = i - (uint32_t)c;
            while (i + m < n - 5u && s[c + m] == s[i + m]) m++;
            *o++ = (uint8_t)((lit < 15u ? lit : 15u) << 4 | (m - 4u < 15u ? m - 4u : 15u));
            if (lit >= 15u) o = put_len(o, lit - 15u);
            memcpy(o, s + anchor, lit); o += lit;
            *o++ = (uint8_t)off; *o++ = (uint8_t)(off >> 8);
            if (m - 4u >= 15u) o = put_len(o, m - 4u - 15u);
            i += m;
            anchor = i;
        } else {
            i++;
        }
    }
    {
        uint32_t lit = n - anchor;
        *o++ = (uint8_t)((lit < 15u ? lit : 15u) << 4);
        if (lit >= 15u) o = put_len(o, lit - 15u);
        memcpy(o, s + anchor, lit); o += lit;
    }
    return (uint32_t)(o - o0);
}

static uint8_t *slurp(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)*n + 1u);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
    fclose(f);
    return b;
}

int main(void)
{
    static uint8_t src[70000], blk[80000], out[70000 + 64];
    uint32_t r = 12345u, t, i;
    {   /* the build's own */
        static const char *const SECT[4] = {"build/ramtext.bin", "build/c1text.bin", "build/hottext.bin", "build/data.bin"};
        long sn, n[4];
        uint8_t *stream = slurp("build/boot_lz4.bin", &sn), *sec[4];
        if (!stream) {
            printf("boot_lz4: (no build/boot_lz4.bin: the build's blocks not checked)\n");
        } else {
            const uint8_t *z = stream;
            int same = 1;
            uint32_t total = 0;
            for (i = 0; i < 4u; i++) {
                sec[i] = slurp(SECT[i], &n[i]);
                if (!sec[i]) n[i] = 0;
                if (n[i]) {
                    uint8_t *d = malloc((size_t)n[i] + 16u);
                    memset(d, 0xA5, (size_t)n[i] + 16u);
                    z = boot_lz4(z, d, d + n[i]);
                    same &= !memcmp(d, sec[i], (size_t)n[i]);
                    for (t = 0; t < 16u; t++) same &= d[n[i] + t] == 0xA5;   /* nothing past the section */
                    free(d);
                }
                total += (uint32_t)n[i];
            }
            check("the build's blocks: the RAM code and .data back byte for byte, nothing past a section", same);
            check("the build's blocks: each ends where the next starts, the last at the stream's end", z == stream + sn);
            printf("boot_lz4: (the build: %u B of sections in %ld B)\n", total, sn);
            for (i = 0; i < 4u; i++)
                free(sec[i]);
            free(stream);
        }
    }
    {
        int ok = 1;
        for (t = 0; t < 400u && ok; t++) {
            uint32_t n = t < 20u ? t : (t * 7919u) % 65000u + 1u, kind = t % 4u, nb;
            for (i = 0; i < n; i++) {
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                src[i] = kind == 0 ? (uint8_t)r                                   /* noise */
                       : kind == 1 ? (uint8_t)(i / (1u + r % 3u) % 7u)            /* runs */
                       : kind == 2 ? (uint8_t)"the keys of the FM-1 "[(i * 3u + (r & 1u)) % 21u]
                                   : (uint8_t)((r & 7u) ? 0u : r >> 8);           /* mostly zeros: long matches */
            }
            nb = n ? pack(src, n, blk) : 0u;               /* (an empty section: no block, build.py) */
            memset(out, 0x5A, sizeof out);
            if (boot_lz4(blk, out, out + n) != blk + nb || memcmp(out, src, n) || out[n] != 0x5A)
                ok = 0;
        }
        check("400 blocks made here (noise, runs, text, long zero runs; 0 .. 65000 B) back exactly", ok);
    }
    {
        /* a literal run of 300 (15 + 255 + 30), then a match of 1000 back 1 (an overlapping run), 5 literals */
        uint8_t b[400], *o = b;
        uint32_t n = 300u + 1000u + 5u;
        *o++ = 0xFF; *o++ = 255; *o++ = 30;           /* (the token: literal 15+, match 15+) */
        for (i = 0; i < 300u; i++) *o++ = (uint8_t)i;
        *o++ = 1; *o++ = 0; *o++ = 255; *o++ = 255; *o++ = 255; *o++ = (uint8_t)(1000u - 4u - 15u - 765u);
        *o++ = 0x50; for (i = 0; i < 5u; i++) *o++ = (uint8_t)(200u + i);
        memset(out, 0, sizeof out);
        {
            int ok = boot_lz4(b, out, out + n) == o;
            for (i = 0; i < 300u; i++) ok &= out[i] == (uint8_t)i;
            for (i = 300u; i < 1300u; i++) ok &= out[i] == (uint8_t)299u;
            for (i = 0; i < 5u; i++) ok &= out[1300u + i] == (uint8_t)(200u + i);
            check("lengths past 15 + 255, an overlapping match (a run), the last literals", ok);
        }
    }
    {
        static const uint8_t bad1[] = {0x14, 'a', 9, 0, 0x10, 'z'};   /* a match 9 back after 1 byte */
        static const uint8_t bad2[] = {0x14, 'a', 0, 0, 0x10, 'z'};   /* offset 0 */
        memset(out, 0x77, 64);
        boot_lz4(bad1, out + 8, out + 16);
        boot_lz4(bad2, out + 24, out + 32);
        check("broken blocks: nothing written outside their section",
              out[7] == 0x77 && out[16] == 0x77 && out[23] == 0x77 && out[32] == 0x77 && out[8] == 'a' && out[24] == 'a');
    }
    printf("boot_lz4: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
