/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (UMBRA OBSCURA) the plate slot (firmware/src/umb_slot.c) and its editor commands (editor_plate.c, cmds 96..100) on a
 * simulated NOR flash (erase sets 0xFF, a write only clears bits).
 *   plate_slot_test               the checks (exit 0: all passed)
 *   plate_slot_test image FILE    a valid plate image for this host (its header's layout and pointers as the slot
 *                                 checks them here) written to FILE, for the page's test (tests/plate_page_test.mjs)
 *   plate_slot_test serve         SysEx frames in, one hex frame a line on stdin; the replies out, the same way
 * cc -Ifirmware/src -Ifirmware/hal -o plate_slot_test tests/plate_slot_test.c */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FELUCCA_WEB 1                                   /* (ups_plate never runs the slot's code here) */
#include "fm1_xip.h"
#include "umb_plate.h"

static uint8_t nor[0x100000];
static int fail_prog;                                   /* the next st_prog fails (a flash error) */
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + (off & ~0xFFFu), 0xFF, 0x1000u); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_prog) {
        fail_prog = 0;
        return -1;
    }
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t st_crc32_add(uint32_t c, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        c ^= b[i];
        for (k = 0; k < 8u; k++)
            c = c & 1u ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    return c;
}
#include "umb_slot.c"

/* the editor's side, as editor.c has it */
static uint8_t ed_out[512];
static uint32_t ed_n;
static void ed_b(uint32_t v) { if (ed_n < sizeof ed_out - 1u) ed_out[ed_n++] = (uint8_t)(v & 0x7Fu); }
static void ed_str(const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i] & 0x7Fu);
    ed_b(0);
}
static void ed_bk_u32(uint32_t n) { for (uint32_t i = 0; i < 5u; i++) ed_b((n >> (i * 7u)) & 127u); }
static int ed_stop_fails;
static int ed_flash_stop(void) { return ed_stop_fails; }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)   /* (editor.c's, as it is) */
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
#include "editor_plate.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("plate slot: %-70s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
/* a plate image as the slot checks it on this host: the header (this host's layout), its entry points inside it */
static uint32_t image(uint8_t *b, uint32_t len, uint32_t magic, uint32_t api, uint32_t entry_off)
{
    umb_plate_t h;
    uintptr_t base = (uintptr_t)FM1_XIP(UPS_OFF);
    uint32_t i;
    memset(&h, 0, sizeof h);
    h.magic = magic;
    h.api = (uint16_t)api;
    h.state = 2264;
    memcpy(h.name, "FIL EMBER", 10);
    h.kind = UMB_KIND_OVERLAY;
    h.modes = UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD);
    h.init = (void (*)(void *, const umb_lctx_t *, const umb_api_t *))(base + entry_off);
    h.note = (void (*)(void *, const umb_lctx_t *, const umb_api_t *, const umb_note_t *))(base + entry_off + 4u);
    h.frame = (void (*)(void *, const umb_lctx_t *, const umb_api_t *))(base + entry_off + 8u);
    h.draw = (void (*)(void *, const umb_lctx_t *, const umb_api_t *, const umb_strip_t *))(base + entry_off + 12u);
    for (i = 0; i < len; i++)
        b[i] = (uint8_t)(i * 37u + 11u);
    memcpy(b, &h, sizeof h);
    return st_crc32_add(0xFFFFFFFFu, b, len) ^ 0xFFFFFFFFu;
}
static uint32_t load(const uint8_t *b, uint32_t len, uint32_t crc, uint32_t piece)   /* BEGIN, WRITEs, END: END's rc */
{
    uint32_t o, rc = ups_begin(len);
    for (o = 0; !rc && o < len; o += piece)
        rc = ups_write(o, b + o, len - o < piece ? len - o : piece);
    return rc ? 100u + rc : ups_end(crc);
}
static uint32_t frame(const uint8_t *f, uint32_t n)    /* a request frame through the handler: the reply's length */
{
    ed_n = 0;
    if (n < 6u || f[0] != 0xF0 || f[1] != 0x7D || f[2] != 0x46 || f[3] != 0x4C || f[n - 1] != 0xF7)
        return 0;
    ed_out[ed_n++] = 0xF0; ed_out[ed_n++] = 0x7D; ed_out[ed_n++] = 0x46; ed_out[ed_n++] = 0x4C; ed_out[ed_n++] = f[4];
    if (!ed_plate_handle(f[4], f + 5, n - 6u))
        return 0;
    ed_out[ed_n++] = 0xF7;
    return ed_n;
}

int main(int argc, char **argv)
{
    static uint8_t b[UPS_SIZE];
    uint32_t len = 2186, crc;
    memset(nor, 0xFF, sizeof nor);
    boot_clean = 1;
    if (argc == 3 && !strcmp(argv[1], "image")) {
        FILE *f = fopen(argv[2], "wb");
        image(b, len, UMB_PLATE_MAGIC, UMB_API, 200);
        if (!f || fwrite(b, 1, len, f) != len)
            return 1;
        fclose(f);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "serve")) {       /* hex frames a line in, hex replies a line out */
        char ln[8192];
        while (fgets(ln, sizeof ln, stdin)) {
            uint8_t fr[4096];
            uint32_t n = 0, i, r;
            unsigned v;
            char *p = ln;
            while (n < sizeof fr && sscanf(p, "%2x", &v) == 1) {
                fr[n++] = (uint8_t)v;
                p += 2;
            }
            r = frame(fr, n);
            for (i = 0; i < r; i++)
                printf("%02x", ed_out[i]);
            printf("\n");
            fflush(stdout);
        }
        return 0;
    }

    ups_st = 0;
    ups_check();
    check(ups_st == 2, "an empty (erased) slot: no plate");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API, 200);
    check(load(b, len, crc, 256) == 0 && ups_st == 1 && !strcmp(ups_name, "FIL EMBER") && ups_len == len,
          "a plate sent whole, its CRC right: there, its name read");
    ups_st = 0;
    ups_check();
    check(ups_st == 1, "read again (as at power-on): still there");
    check(load(b, len, crc ^ 1u, 256) == 2 && ups_st == 2, "its CRC wrong: refused (2), the slot left empty");
    ups_st = 0;
    ups_check();
    check(ups_st == 2 && nor[UPS_OFF] == 0xFF, ".. and empty when read again (erased, no record)");
    crc = image(b, len, 0x12345678u, UMB_API, 200);
    check(load(b, len, crc, 256) == 2, "not a plate (its magic): refused");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API + 1u, 200);
    check(load(b, len, crc, 256) == 2, "a plate for a newer API: refused");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API, len);
    check(load(b, len, crc, 256) == 2, "an entry point past its end (not linked for the slot): refused");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API, 200);
    check(ups_begin(UPS_REC + 1u) == 1 && ups_begin(8) == 1, "too long for the slot, or shorter than a header: refused (1)");
    check(ups_begin(len) == 0 && ups_write(256, b + 256, 256) == 1, "a write out of order: refused (1)");
    check(ups_write(0, b, 256) == 0 && ups_end(crc) == 1, "END before the whole image: refused (1)");
    ups_st = 0;
    ups_check();
    check(ups_st == 2, "a transfer cut short (no END): the slot reads empty");
    check(ups_begin(len) == 0 && (fail_prog = 1, ups_write(0, b, 256)) == 4 && ups_write(256, b + 256, 256) == 1,
          "a flash error (4): the transfer is over");
    check(load(b, len, crc, 100) == 0 && ups_st == 1, "pieces of any size (100 B): there");
    check(ups_erase() == 0 && (ups_st = 0, ups_check(), ups_st == 2), "ERASE: empty");

    {                                                   /* the editor's commands, frames as the page sends them */
        static const uint8_t list[] = {0xF0, 0x7D, 0x46, 0x4C, 96, 0xF7};
        uint8_t f[600];
        uint32_t n, o, k, j, r;
        r = frame(list, sizeof list);
        check(r == 5u + 2u + 1u + 1u + 5u + 1u && ed_out[5] == UMB_API && ed_out[6] == 1u && ed_out[7] == 0u,
              "LIST, empty: API, 1 slot, state 0, no name, length 0");
        n = 0;                                          /* BEGIN slot 0, the length */
        f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x46; f[n++] = 0x4C; f[n++] = 97; f[n++] = 0;
        for (j = 0; j < 5u; j++)
            f[n++] = (uint8_t)((len >> (7u * j)) & 127u);
        f[n++] = 0xF7;
        r = frame(f, n);
        check(r == 8u && ed_out[5] == 0 && ed_out[6] == 0, "BEGIN: slot 0, rc 0");
        for (o = 0; o < len; o += 256) {                /* WRITEs: slot, offset, the bytes 7-bit packed */
            uint32_t m = len - o < 256u ? len - o : 256u, g;
            n = 0;
            f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x46; f[n++] = 0x4C; f[n++] = 98; f[n++] = 0;
            for (j = 0; j < 5u; j++)
                f[n++] = (uint8_t)((o >> (7u * j)) & 127u);
            for (g = 0; g < m; g += 7) {
                uint32_t msb = 0;
                k = m - g < 7u ? m - g : 7u;
                for (j = 0; j < k; j++)
                    msb |= (uint32_t)(b[o + g + j] >> 7) << j;
                f[n++] = (uint8_t)msb;
                for (j = 0; j < k; j++)
                    f[n++] = b[o + g + j] & 127u;
            }
            f[n++] = 0xF7;
            r = frame(f, n);
            if (ed_out[6]) break;
        }
        check(o >= len, "WRITEs of 256 B, 7-bit packed: each rc 0");
        n = 0;
        f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x46; f[n++] = 0x4C; f[n++] = 99; f[n++] = 0;
        for (j = 0; j < 5u; j++)
            f[n++] = (uint8_t)((crc >> (7u * j)) & 127u);
        f[n++] = 0xF7;
        r = frame(f, n);
        check(r == 8u && ed_out[6] == 0 && ups_st == 1, "END with the CRC: rc 0, the plate there");
        r = frame(list, sizeof list);
        check(ed_out[7] == 1u && !memcmp(ed_out + 8, "FIL EMBER", 10) &&
              (ed_out[18] | ed_out[19] << 7 | ed_out[20] << 14 | ed_out[21] << 21 | (uint32_t)ed_out[22] << 28) == len,
              "LIST: state 1, its name, its length");
        boot_clean = 0;
        r = frame(list, sizeof list);
        check(ed_out[7] == 2u, "LIST after a crash or hang: state 2 (there, not run)");
        boot_clean = 1;
        ed_stop_fails = 1;
        f[4] = 100; f[5] = 0; f[6] = 0xF7;
        r = frame(f, 7);
        check(ed_out[6] == 3u && ups_st == 1, "ERASE while playback does not stop: rc 3, nothing touched");
        ed_stop_fails = 0;
        r = frame(f, 7);
        check(ed_out[6] == 0 && ups_st == 2, "ERASE: rc 0, empty");
        f[5] = 1;
        r = frame(f, 7);
        check(ed_out[6] == 1u, "slot 1 (there is one slot): rc 1");
    }
    printf("plate slot: %s (%d failed)\n", fails ? "FAILED" : "all passed", fails);
    return fails != 0;
}
