/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* UMBRA LIVE: the plate bank (plans/umbra-plate-bank.md, phase 2): plate files in flash, each run where it lies.
 * UPS_NSLOT slots of one 4 KiB sector each from flash 0xB8000: JIANT's projects 5..8 (songs 5..8, storage.c's map), retired
 * for it (song_chain.c NSONG_USE: four projects). (They were 16 of two sectors until the fluid plates and their layers
 * came: 27 plates in an install, the largest of them 3.9 KB. A plate left by the older layout fails its slot's check
 * and reads empty.) From a slot's start, the plate's image as its plate file holds it (its
 * header, umb_plate_t, first), linked for that slot (tools/build.py plate_files links every plate for slot 0 and lists its
 * absolute words; the plates page adds the slot's distance to them before sending); in its last 32 bytes the slot's
 * record, written last, so a transfer cut short leaves none and the slot reads empty. The editor fills them
 * (editor_plate.c, cmds 96..100): BEGIN erases the slot's sectors, WRITE programs the image in order, END checks its CRC
 * and its header and writes the record, ERASE empties it.
 * A plate there runs as its number (ui_umbra.c umb_mod: a known name as that plate, another as 6 + its slot). The bank
 * runs only after a clean power-on (main.c boot_clean: a plate that crashed or hung the FM-1 is not run again until a
 * start without one; the editor can still reach the FM-1 and erase it). Included by felucca.c after storage.c (st_read,
 * st_erase, st_prog, st_crc32_add); the host test (tests/plate_slot_test.c) brings its own on a simulated NOR. */
#ifndef UMB_NSLOT
#define UMB_NSLOT 32u
#endif
#define UPS_NSLOT UMB_NSLOT
#define UPS_BASE 0xB8000u                               /* slot 0's sector (flash offset): song 5's first */
#define UPS_SIZE 0x1000u                                /* (a sector: every plate so far is under 4 KB) */
#define UPS_OFF(i) (UPS_BASE + (uint32_t)(i) * UPS_SIZE)
#define UPS_REC (UPS_SIZE - 32u)                        /* the record's place, and the image's room */
#define UPS_MAGIC 0x4C535055u                           /* "UPSL" */
_Static_assert(UPS_OFF(UPS_NSLOT) <= 0xD8000u, "the bank inside projects 5..8 (the song index at 0xD8000)");
typedef struct {
    uint32_t magic;                                     /* UPS_MAGIC */
    uint32_t len;                                       /* the image's bytes */
    uint32_t crc;                                       /* its CRC-32 (zlib's) */
    uint32_t rsv[5];
} ups_rec_t;
_Static_assert(sizeof(ups_rec_t) == 32u, "the slot's record");
/* each slot: 0 not checked, 1 a plate there, 2 none (or a transfer). (In the pool, zeroed at boot: 32 slots' worth in
 * .bss moved what lies after it, and 200 functions that reach those by a short address grew, 1.4 KB in all) */
#ifndef UPS_POOL                                        /* (a host test's build has no pool: tests/plate_slot_test.c) */
#define UPS_POOL __attribute__((section(".pool")))
#endif
static uint8_t ups_st[UPS_NSLOT] UPS_POOL;
static uint32_t ups_len, ups_got = 0xFFFFFFFFu;         /* a transfer: the image's length, the bytes written (none: ~0) .. */
static uint8_t ups_ws;                                  /* .. into this slot */
static uint8_t boot_clean;                              /* (main.c's: no crash, hang or failed boot before this one) */

static int ups_in(uint32_t i, uintptr_t fn, uint32_t len)   /* an entry point inside slot i's image (linked for it) */
{
    uintptr_t b = (uintptr_t)FM1_XIP(UPS_OFF(i)), o = fn - b;
    return fn >= b && o < len;
}
static int ups_str(const char *s, uint32_t n)           /* a name: printable, 0-ended within n, not empty */
{
    uint32_t k;
    for (k = 0; k < n && s[k]; k++)
        if (s[k] < 32 || s[k] > 126)
            return 0;
    return k && k < n;
}
/* slot i read and checked: its record, the image's CRC, its header (a plate, an API this firmware has, its names, every
 * entry point inside it); its length (0: none) */
static uint32_t ups_check(uint32_t i)
{
    ups_rec_t r;
    umb_plate_t h;
    uint8_t b[64];
    uint32_t k, n, c = 0xFFFFFFFFu;
    ups_st[i] = 2;
    if (st_read(UPS_OFF(i) + UPS_REC, &r, sizeof r) || r.magic != UPS_MAGIC || r.len < sizeof h || r.len > UPS_REC)
        return 0;
    for (k = 0; k < r.len; k += n) {
        n = r.len - k < sizeof b ? r.len - k : sizeof b;
        if (st_read(UPS_OFF(i) + k, b, n))
            return 0;
        c = st_crc32_add(c, b, n);
    }
    if (~c != r.crc || st_read(UPS_OFF(i), &h, sizeof h) || h.magic != UMB_PLATE_MAGIC || !h.api || h.api > UMB_API ||
        h.nctl > UMB_NCTL || !ups_str(h.name, sizeof h.name) || !ups_in(i, (uintptr_t)h.init, r.len) ||
        !ups_in(i, (uintptr_t)h.note, r.len) || !ups_in(i, (uintptr_t)h.frame, r.len) || !ups_in(i, (uintptr_t)h.draw, r.len))
        return 0;
    for (k = 0; k < h.nctl; k++)
        if (!ups_str(h.ctl[k].name, sizeof h.ctl[k].name))
            return 0;
    ups_st[i] = 1;
    return r.len;
}
/* slot i's plate, to run (0: none, a transfer going, not after a clean start, or not this FM-1's code) */
static const umb_plate_t *ups_plate(uint32_t i)
{
    if (i >= UPS_NSLOT)
        return 0;
    if (!ups_st[i] && ups_check(i))
        (void)bt_hw_seg();                              /* (the plain window: flash from 0x93000 read as it is; set at
                                                         * power-on, main.c; this only makes sure) */
    if (ups_st[i] != 1 || !boot_clean)
        return 0;
    return (const umb_plate_t *)FM1_XIP(UPS_OFF(i));
}
/* the editor's transfer (editor_plate.c): rc 0 ok, 1 arguments, 4 flash */
static uint32_t ups_begin(uint32_t i, uint32_t len)
{
    if (i >= UPS_NSLOT || len < sizeof(umb_plate_t) || len > UPS_REC)
        return 1;
    ups_st[i] = 2;                                      /* (nothing runs from the slot while it changes) */
    ups_gen++;
    ups_got = 0xFFFFFFFFu;
    if (st_erase(UPS_OFF(i)))
        return 4;
    ups_ws = (uint8_t)i;
    ups_len = len;
    ups_got = 0;
    return 0;
}
static uint32_t ups_write(uint32_t i, uint32_t off, const uint8_t *p, uint32_t n)   /* the image's bytes off .. off + n, in order */
{
    if (ups_got == 0xFFFFFFFFu || i != ups_ws || off != ups_got || !n || n > ups_len - off)
        return 1;
    if (st_prog(UPS_OFF(i) + off, p, n)) {
        ups_got = 0xFFFFFFFFu;
        return 4;
    }
    ups_got += n;
    return 0;
}
/* the whole image written, its CRC crc: checked as written (2: the CRC or the header refused, the slot left empty) */
static uint32_t ups_end(uint32_t i, uint32_t crc)
{
    ups_rec_t r;
    uint32_t k;
    if (ups_got == 0xFFFFFFFFu || i != ups_ws || ups_got != ups_len)
        return 1;
    ups_got = 0xFFFFFFFFu;
    r.magic = UPS_MAGIC;
    r.len = ups_len;
    r.crc = crc;
    for (k = 0; k < 5u; k++)
        r.rsv[k] = 0xFFFFFFFFu;
    if (st_prog(UPS_OFF(i) + UPS_REC, &r, sizeof r))
        return 4;
    ups_gen++;
    if (ups_check(i))                                   /* (read back: the CRC over what the flash holds, the header) */
        return 0;
    (void)st_erase(UPS_OFF(i));                         /* (refused: never left half there) */
    ups_st[i] = 2;
    return 2;
}
static uint32_t ups_erase(uint32_t i)
{
    if (i >= UPS_NSLOT)
        return 1;
    ups_st[i] = 2;
    ups_gen++;
    if (i == ups_ws)
        ups_got = 0xFFFFFFFFu;
    return st_erase(UPS_OFF(i)) ? 4u : 0u;
}
