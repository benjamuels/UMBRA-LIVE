/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* UMBRA OBSCURA: the plate slot (plans/umbra-plate-bank.md, phase 1): one plate file in flash, run where it lies.
 * Flash 0xDA000 .. 0xDAFFF (storage.c's map: free), one 4 KiB sector. From its start, the plate's image as the plate
 * file holds it (its header, umb_plate_t, first; linked for this address: tools/build.py plate_files); in its last 32
 * bytes the slot's record, written last, so a transfer cut short leaves none and the slot reads empty. The editor fills
 * it (editor_plate.c, cmds 96..100): BEGIN erases the sector, WRITE programs the image in order, END checks its CRC and
 * its header and writes the record, ERASE empties it.
 * A plate there takes the resident FILAMENT's place on every layer (ui_umbra.c umb_lay_draw). It runs only after a clean
 * power-on (main.c boot_clean: a plate that crashed or hung the FM-1 is not run again until a start without one; the
 * editor can still reach the FM-1 and erase it). Included by felucca.c after storage.c (st_read, st_erase, st_prog,
 * st_crc32_add); the host test (tests/plate_slot_test.c) brings its own on a simulated NOR. */
#define UPS_OFF 0xDA000u                                /* the slot's sector (flash offset) */
#define UPS_SIZE 0x1000u
#define UPS_REC (UPS_SIZE - 32u)                        /* the record's place, and the image's room */
#define UPS_MAGIC 0x4C535055u                           /* "UPSL" */
typedef struct {
    uint32_t magic;                                     /* UPS_MAGIC */
    uint32_t len;                                       /* the image's bytes */
    uint32_t crc;                                       /* its CRC-32 (zlib's) */
    uint32_t rsv[5];
} ups_rec_t;
_Static_assert(sizeof(ups_rec_t) == 32u, "the slot's record");
static uint8_t ups_st;                                  /* 0 not checked, 1 a plate there, 2 none (or a transfer going) */
static uint32_t ups_len, ups_got = 0xFFFFFFFFu;         /* a transfer: the image's length, the bytes written (none: ~0) */
static char ups_name[11];                               /* the plate's name, as its header has it */
static uint8_t boot_clean;                              /* (main.c's: no crash, hang or failed boot before this one) */

static int ups_in(uintptr_t fn, uint32_t len)          /* an entry point inside the image (linked for the slot) */
{
    uintptr_t o = fn - (uintptr_t)FM1_XIP(UPS_OFF);
    return fn >= (uintptr_t)FM1_XIP(UPS_OFF) && o < len;
}
/* the slot read and checked: its record, the image's CRC, its header (a plate, an API this firmware has, every entry
 * point inside it) */
static void ups_check(void)
{
    ups_rec_t r;
    umb_plate_t h;
    uint8_t b[64];
    uint32_t i, k, c = 0xFFFFFFFFu;
    ups_st = 2;
    ups_name[0] = 0;
    if (st_read(UPS_OFF + UPS_REC, &r, sizeof r) || r.magic != UPS_MAGIC || r.len < sizeof h || r.len > UPS_REC)
        return;
    for (i = 0; i < r.len; i += k) {
        k = r.len - i < sizeof b ? r.len - i : sizeof b;
        if (st_read(UPS_OFF + i, b, k))
            return;
        c = st_crc32_add(c, b, k);
    }
    if (~c != r.crc || st_read(UPS_OFF, &h, sizeof h) || h.magic != UMB_PLATE_MAGIC || !h.api || h.api > UMB_API ||
        !ups_in((uintptr_t)h.init, r.len) || !ups_in((uintptr_t)h.note, r.len) || !ups_in((uintptr_t)h.frame, r.len) ||
        !ups_in((uintptr_t)h.draw, r.len))
        return;
    for (i = 0; i < 10u && h.name[i]; i++)
        ups_name[i] = h.name[i] >= 32 && h.name[i] < 127 ? h.name[i] : '?';
    ups_name[i] = 0;
    ups_len = r.len;
    ups_st = 1;
}
/* the plate in the slot, to run (0: none, a transfer going, not after a clean start, or not this FM-1's code) */
static const umb_plate_t *ups_plate(void)
{
    if (!ups_st)
        ups_check();
#if FELUCCA_WEB
    return 0;                                           /* (the emulator cannot run the FM-1's code) */
#else
    if (ups_st != 1 || !boot_clean)
        return 0;
    (void)bt_hw_seg();                                  /* (the plain window: flash from 0x93000 read as it is; set at
                                                         * power-on, main.c; this only makes sure) */
    return (const umb_plate_t *)FM1_XIP(UPS_OFF);
#endif
}
/* the editor's transfer (editor_plate.c): rc 0 ok, 1 arguments, 4 flash */
static uint32_t ups_begin(uint32_t len)
{
    if (len < sizeof(umb_plate_t) || len > UPS_REC)
        return 1;
    ups_st = 2;                                         /* (nothing runs from the slot while it changes) */
    ups_got = 0xFFFFFFFFu;
    if (st_erase(UPS_OFF))
        return 4;
    ups_len = len;
    ups_got = 0;
    return 0;
}
static uint32_t ups_write(uint32_t off, const uint8_t *p, uint32_t n)   /* the image's bytes off .. off + n, in order */
{
    if (ups_got == 0xFFFFFFFFu || off != ups_got || !n || n > ups_len - off)
        return 1;
    if (st_prog(UPS_OFF + off, p, n)) {
        ups_got = 0xFFFFFFFFu;
        return 4;
    }
    ups_got += n;
    return 0;
}
/* the whole image written, its CRC crc: checked as written (2: the CRC or the header refused, the slot left empty) */
static uint32_t ups_end(uint32_t crc)
{
    ups_rec_t r;
    uint32_t i;
    if (ups_got == 0xFFFFFFFFu || ups_got != ups_len)
        return 1;
    ups_got = 0xFFFFFFFFu;
    r.magic = UPS_MAGIC;
    r.len = ups_len;
    r.crc = crc;
    for (i = 0; i < 5u; i++)
        r.rsv[i] = 0xFFFFFFFFu;
    if (st_prog(UPS_OFF + UPS_REC, &r, sizeof r))
        return 4;
    ups_check();                                        /* (read back: the CRC over what the flash holds, the header) */
    if (ups_st == 1)
        return 0;
    (void)st_erase(UPS_OFF);                            /* (refused: never left half there) */
    ups_st = 2;
    return 2;
}
static uint32_t ups_erase(void)
{
    ups_st = 2;
    ups_got = 0xFFFFFFFFu;
    return st_erase(UPS_OFF) ? 4u : 0u;
}
