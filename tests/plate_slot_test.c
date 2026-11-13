/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (UMBRA LIVE) the plate bank (firmware/src/umb_slot.c) and its editor commands (editor_plate.c, cmds 96..100) on a
 * simulated NOR flash (erase sets 0xFF, a write only clears bits).
 *   plate_slot_test               the checks (exit 0: all passed)
 *   plate_slot_test image FILE    a valid plate file for this host (its header's layout and pointers as the bank checks
 *                                 them here), linked for slot 0, its entry points listed to move (tools/build.py's
 *                                 format), written to FILE, for the page's test (tests/plate_page_test.mjs)
 *   plate_slot_test bank FILE N   a bank image of N slots (tools/build.py plate_bank's): each slot checked as the bank does
 *   plate_slot_test serve         SysEx frames in, one hex frame a line on stdin; the replies out, the same way
 * cc -Ifirmware/src -Ifirmware/hal -o plate_slot_test tests/plate_slot_test.c */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
static uint8_t ups_gen;                                 /* (ui_umbra.c's) */
static int bt_hw_seg(void) { return 1; }
#define UPS_POOL                                        /* (the slots' states: plain memory here, the pool on the FM-1) */
#include "umb_slot.c"

/* the editor's side, as editor.c has it */
static uint8_t ed_out[600];
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
/* a plate image as the bank checks it on this host, linked for slot sl: the header (this host's layout), its entry
 * points inside it */
static uint32_t image_at(uint8_t *b, uint32_t len, uint32_t magic, uint32_t api, uint32_t entry_off, uint32_t sl, const char *nm)
{
    umb_plate_t h;
    uintptr_t base = (uintptr_t)FM1_XIP(UPS_OFF(sl));
    uint32_t i;
    memset(&h, 0, sizeof h);
    h.magic = magic;
    h.api = (uint16_t)api;
    h.state = 2264;
    memcpy(h.name, nm, strlen(nm) + 1u < 10u ? strlen(nm) + 1u : 10u);
    h.kind = UMB_KIND_OVERLAY;
    h.modes = UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD);
    h.nctl = 3;
    memcpy(h.ctl[0].name, "SWING", 6);
    memcpy(h.ctl[1].name, "RING", 5);
    memcpy(h.ctl[2].name, "SPREAD", 7);
    h.init = (void (*)(void *, const umb_lctx_t *, const umb_api_t *))(base + entry_off);
    h.note = (void (*)(void *, const umb_lctx_t *, const umb_api_t *, const umb_note_t *))(base + entry_off + 4u);
    h.frame = (void (*)(void *, const umb_lctx_t *, const umb_api_t *))(base + entry_off + 8u);
    h.draw = (void (*)(void *, const umb_lctx_t *, const umb_api_t *, const umb_strip_t *))(base + entry_off + 12u);
    for (i = 0; i < len; i++)
        b[i] = (uint8_t)(i * 37u + 11u);
    memcpy(b, &h, sizeof h);
    return st_crc32_add(0xFFFFFFFFu, b, len) ^ 0xFFFFFFFFu;
}
static uint32_t image(uint8_t *b, uint32_t len, uint32_t magic, uint32_t api, uint32_t entry_off)
{
    return image_at(b, len, magic, api, entry_off, 0, "FIL EMBER");
}
static uint32_t load_at(uint32_t sl, const uint8_t *b, uint32_t len, uint32_t crc, uint32_t piece)   /* BEGIN, WRITEs, END */
{
    uint32_t o, rc = ups_begin(sl, len);
    for (o = 0; !rc && o < len; o += piece)
        rc = ups_write(sl, o, b + o, len - o < piece ? len - o : piece);
    return rc ? 100u + rc : ups_end(sl, crc);
}
static uint32_t load(const uint8_t *b, uint32_t len, uint32_t crc, uint32_t piece) { return load_at(0, b, len, crc, piece); }
static void name_of(uint32_t sl, char *n)              /* slot sl's plate's name, as its header has it */
{
    umb_plate_t h;
    st_read(UPS_OFF(sl), &h, sizeof h);
    memcpy(n, h.name, 10);
    n[10] = 0;
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
    uint32_t len = 2188, crc, g;               /* (a plate image is whole words: plate.ld) */
    char nm[11];
    memset(nor, 0xFF, sizeof nor);
    boot_clean = 1;
    if (argc == 3 && !strcmp(argv[1], "image")) {      /* the plate file: the image, the words to move, their count,
                                                         * "URLC", the image's length (build.py plate_files) */
        FILE *f = fopen(argv[2], "wb");
        umb_plate_t h;
        uint32_t t[5 + 3], k, a = (uint32_t)(uintptr_t)FM1_XIP(UPS_OFF(0)) + 1000u;
        image(b, len, UMB_PLATE_MAGIC, UMB_API, 200);
        memcpy(b + 1002, &a, 4);                        /* (an address at a halfword, as a 48-bit load holds one) */
        for (k = 0; k < 4u; k++)                        /* (the four entry points' words: their low words on this host) */
            t[k] = (uint32_t)((k == 0 ? (uintptr_t)&h.init : k == 1 ? (uintptr_t)&h.note : k == 2 ? (uintptr_t)&h.frame
                                                                                                    : (uintptr_t)&h.draw) - (uintptr_t)&h);
        t[4] = 1002;
        t[5] = 5;
        t[6] = 0x434C5255u;
        t[7] = len;
        if (!f || fwrite(b, 1, len, f) != len || fwrite(t, 4, 8, f) != 8u)
            return 1;
        fclose(f);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "bank")) {        /* (tools/build.py plate_bank) a bank image FILE of N slots, as an
                                                         * install writes it at the bank's start: every slot a plate the
                                                         * bank runs, the next one empty */
        FILE *f = fopen(argv[2], "rb");
        uint32_t n = (uint32_t)atoi(argv[3]), k, okn = 0;
        memset(nor, 0xFF, sizeof nor);
        if (!f || fread(nor + UPS_BASE, 1, n * UPS_SIZE, f) != n * UPS_SIZE)
            return 2;
        fclose(f);
        boot_clean = 1;
        for (k = 0; k < n; k++)
            okn += ups_check(k) && ups_plate(k) == (const umb_plate_t *)FM1_XIP(UPS_OFF(k));
        printf("plate bank image: %u of %u slots hold a plate the bank runs; the next one %s\n", okn, n, ups_check(n) ? "NOT EMPTY" : "empty");
        return okn == n && !ups_check(n) ? 0 : 1;
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

    check(!ups_check(0) && ups_st[0] == 2, "an empty (erased) slot: no plate");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API, 200);
    g = ups_gen;
    check(load(b, len, crc, 256) == 0 && ups_st[0] == 1 && (name_of(0, nm), !strcmp(nm, "FIL EMBER")) && ups_gen != g,
          "a plate sent whole into slot 0, its CRC right: there, its name read, the bank changed");
    ups_st[0] = 0;
    check(ups_check(0) == len && ups_st[0] == 1, "read again (as at power-on): still there, its length");
    check(load(b, len, crc ^ 1u, 256) == 2 && ups_st[0] == 2, "its CRC wrong: refused (2), the slot left empty");
    check(!ups_check(0) && nor[UPS_OFF(0)] == 0xFF, ".. and empty when read again (erased, no record)");
    crc = image(b, len, 0x12345678u, UMB_API, 200);
    check(load(b, len, crc, 256) == 2, "not a plate (its magic): refused");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API + 1u, 200);
    check(load(b, len, crc, 256) == 2, "a plate for a newer API: refused");
    crc = image(b, len, UMB_PLATE_MAGIC, 1u, 200);
    check(load(b, len, crc, 256) == 0, "a plate for API 1 (phase 1's FIL EMBER): taken");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API, len);
    check(load(b, len, crc, 256) == 2, "an entry point past its end: refused");
    crc = image_at(b, len, UMB_PLATE_MAGIC, UMB_API, 200, 0, "TOOLONGNAME");
    check(load(b, len, crc, 256) == 2, "a name not ended within its 10 bytes: refused");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API, 200);
    check(load_at(3, b, len, crc, 256) == 2 && ups_st[3] == 2, "linked for slot 0, sent to slot 3 (not moved): refused");
    crc = image_at(b, len, UMB_PLATE_MAGIC, UMB_API, 200, 3, "TARN");
    check(load_at(3, b, len, crc, 256) == 0 && ups_st[3] == 1 && (name_of(3, nm), !strcmp(nm, "TARN")),
          "linked for slot 3, into slot 3: there");
    crc = image_at(b, len, UMB_PLATE_MAGIC, UMB_API, 200, UPS_NSLOT - 1u, "CINDER");
    check(load_at(UPS_NSLOT - 1u, b, len, crc, 256) == 0 && ups_st[UPS_NSLOT - 1u] == 1 &&
          UPS_OFF(UPS_NSLOT - 1u) + UPS_SIZE <= 0xD8000u && nor[0xD8000u] == 0xFF,
          "the last slot (15): there, under the song index (0xD8000 untouched)");
    check(ups_st[3] == 1 && !ups_check(5), "the others kept; slot 5 empty");
    crc = image(b, len, UMB_PLATE_MAGIC, UMB_API, 200);
    check(ups_begin(0, UPS_REC + 1u) == 1 && ups_begin(0, 8) == 1 && ups_begin(UPS_NSLOT, len) == 1,
          "too long for a slot, shorter than a header, or no such slot: refused (1)");
    check(ups_begin(0, len) == 0 && ups_write(0, 256, b + 256, 256) == 1, "a write out of order: refused (1)");
    check(ups_write(1, 0, b, 256) == 1, "a write into another slot than BEGIN's: refused (1)");
    check(ups_write(0, 0, b, 256) == 0 && ups_end(0, crc) == 1, "END before the whole image: refused (1)");
    check(!ups_check(0), "a transfer cut short (no END): the slot reads empty");
    check(ups_begin(0, len) == 0 && (fail_prog = 1, ups_write(0, 0, b, 256)) == 4 && ups_write(0, 256, b + 256, 256) == 1,
          "a flash error (4): the transfer is over");
    check(load(b, len, crc, 100) == 0 && ups_st[0] == 1, "pieces of any size (100 B): there");
    g = ups_gen;
    check(ups_erase(0) == 0 && !ups_check(0) && ups_gen != g && ups_st[3] == 1, "ERASE slot 0: empty, the bank changed, slot 3 kept");
    check(ups_erase(UPS_NSLOT) == 1, "ERASE past the last slot: refused (1)");
    crc = image_at(b, 6000, UMB_PLATE_MAGIC, UMB_API, 5000, 4, "BELT");
    check(load_at(4, b, 6000, crc, 256) == 0 && ups_check(4) == 6000u && nor[UPS_OFF(4) + 5999] == b[5999],
          "a plate of 6000 B (both sectors of its slot, an entry point in the second): there");
    check(ups_erase(4) == 0 && !ups_check(4) && nor[UPS_OFF(4)] == 0xFF && nor[UPS_OFF(4) + 0x1000] == 0xFF && ups_st[3] == 1,
          "ERASE it: both sectors erased, slot 3 kept");

    {                                                   /* the editor's commands, frames as the page sends them */
        static const uint8_t list[] = {0xF0, 0x7D, 0x46, 0x4C, 96, 0xF7};
        static const uint8_t list8[] = {0xF0, 0x7D, 0x46, 0x4C, 96, 8, 0xF7};
        uint8_t f[600];
        uint32_t n, o, k, j, r, sl = 2;
        r = frame(list, sizeof list);
        check(r == 5u + 3u + 7u * 7u + 11u + 1u && ed_out[5] == UMB_API && ed_out[6] == UPS_NSLOT && ed_out[7] == 0u && ed_out[8] == 0u &&
              ed_out[8 + 3 * 7] == 1u && !memcmp(ed_out + 8 + 3 * 7 + 1, "TARN", 5),
              "LIST: API, 16 slots, from 0: slot 0 empty, slot 3 TARN");
        r = frame(list8, sizeof list8);
        check(ed_out[7] == 8u && ed_out[8 + 7 * 7] == 1u && !memcmp(ed_out + 8 + 7 * 7 + 1, "CINDER", 7),
              "LIST from 8: slot 15 CINDER");
        crc = image_at(b, len, UMB_PLATE_MAGIC, UMB_API, 200, sl, "GRANULE");
        n = 0;                                          /* BEGIN slot 2, the length */
        f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x46; f[n++] = 0x4C; f[n++] = 97; f[n++] = (uint8_t)sl;
        for (j = 0; j < 5u; j++)
            f[n++] = (uint8_t)((len >> (7u * j)) & 127u);
        f[n++] = 0xF7;
        r = frame(f, n);
        check(r == 8u && ed_out[5] == sl && ed_out[6] == 0, "BEGIN: slot 2, rc 0");
        for (o = 0; o < len; o += 256) {                /* WRITEs: slot, offset, the bytes 7-bit packed */
            uint32_t m = len - o < 256u ? len - o : 256u, q;
            n = 0;
            f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x46; f[n++] = 0x4C; f[n++] = 98; f[n++] = (uint8_t)sl;
            for (j = 0; j < 5u; j++)
                f[n++] = (uint8_t)((o >> (7u * j)) & 127u);
            for (q = 0; q < m; q += 7) {
                uint32_t msb = 0;
                k = m - q < 7u ? m - q : 7u;
                for (j = 0; j < k; j++)
                    msb |= (uint32_t)(b[o + q + j] >> 7) << j;
                f[n++] = (uint8_t)msb;
                for (j = 0; j < k; j++)
                    f[n++] = b[o + q + j] & 127u;
            }
            f[n++] = 0xF7;
            r = frame(f, n);
            if (ed_out[6]) break;
        }
        check(o >= len, "WRITEs of 256 B, 7-bit packed: each rc 0");
        n = 0;
        f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x46; f[n++] = 0x4C; f[n++] = 99; f[n++] = (uint8_t)sl;
        for (j = 0; j < 5u; j++)
            f[n++] = (uint8_t)((crc >> (7u * j)) & 127u);
        f[n++] = 0xF7;
        r = frame(f, n);
        check(r == 8u && ed_out[6] == 0 && ups_st[sl] == 1, "END with the CRC: rc 0, the plate there");
        r = frame(list, sizeof list);
        k = 8 + 2 * 7;                                  /* (slot 2's entry: state, "GRANULE\0", the length) */
        check(ed_out[k] == 1u && !memcmp(ed_out + k + 1, "GRANULE", 8) &&
              (ed_out[k + 9] | ed_out[k + 10] << 7 | ed_out[k + 11] << 14 | ed_out[k + 12] << 21 | (uint32_t)ed_out[k + 13] << 28) == len,
              "LIST: slot 2 state 1, its name, its length");
        boot_clean = 0;
        r = frame(list, sizeof list);
        check(ed_out[k] == 2u, "LIST after a crash or hang: state 2 (there, not run)");
        boot_clean = 1;
        ed_stop_fails = 1;
        f[4] = 100; f[5] = (uint8_t)sl; f[6] = 0xF7;
        r = frame(f, 7);
        check(ed_out[6] == 3u && ups_st[sl] == 1, "ERASE while playback does not stop: rc 3, nothing touched");
        ed_stop_fails = 0;
        r = frame(f, 7);
        check(ed_out[6] == 0 && ups_st[sl] == 2, "ERASE: rc 0, empty");
        f[5] = UPS_NSLOT;
        r = frame(f, 7);
        check(ed_out[6] == 1u, "slot 16 (there are 16): rc 1");
    }
    printf("plate slot: %s (%d failed)\n", fails ? "FAILED" : "all passed", fails);
    return fails != 0;
}
