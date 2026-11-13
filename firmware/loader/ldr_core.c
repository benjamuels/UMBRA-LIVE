/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader, the portable part (step 2 of the M-UPGRADE update
 * protocol).
 *
 * The host serves the package ("logical image") with cmd 0x30 reads (ota.c).
 * The loader:
 *   1. reads the UFW header and finds flash.bin;
 *   2. checks that the package's app area decrypts with THIS chip's key
 *      (taken from isd_config.ini in the device's own flash head), so a
 *      package for another key is refused before anything is erased;
 *   2b. reads the whole flash.bin once and checks it against the CRC16 of its
 *      UFW entry (the vendor tools' check), keeping each app sector's CRC16:
 *      a damaged or truncated package is refused before anything is erased;
 *   3. writes ONLY the app area [0x4000, 0x93000), sector by sector, skipping
 *      sectors that are already equal, and verifies each one; a sector that
 *      comes in different from the checked pass is read again, never written;
 *      the flash head (SPL, isd_config) is never written;
 *   3b. (V10.12) with a "bt.bin" entry (type 200, tools/fm1pkg_make.py): the Bluetooth segment, checked in 2b
 *      as a whole (its entry's CRC16, its header) and by sector like the app area, written the same way at
 *      [0xEA000, 0xEA000 + its length) (plain: the XIP window from 0x93000 up is unencrypted), the last sector's
 *      tail 0xFF; a package without one leaves the region as it is (the app then finds no segment of its build);
 *   3c. (UMBRA LIVE) with a "plates.bin" entry (type 201): the plate bank's first slots as the build made them
 *      (tools/build.py plate_bank: whole 8 KiB slots from slot 0, each a plate linked for its slot with its record),
 *      checked and written as the Bluetooth segment is, at [0xB8000, 0xB8000 + its length), inside the bank
 *      [0xB8000, 0xD8000) (src/umb_slot.c: JIANT's projects 5..8, retired for it). The slots past its length are
 *      left as they are; a package without one leaves the bank alone;
 *   4. asks 0xF0000000 ("success"), then invalidates the update record (RAM
 *      and flash: 4K boundary - 256 between 0x93000 and 0xFC000, outside
 *      Felucca's store, where only user data lives) so the SPL boots the new
 *      app, and resets.
 * Power loss during 3: the record is still there, the SPL runs the loader
 * again on the next power-on and the host can resume.
 *
 * Hooks from the platform (loader.c, ldr_test.c):
 *   ldr_fread(off, p, n)  ldr_erase(off)  ldr_prog(off, p, n)   flash, 0 = ok
 *   ldr_record_clear()    forget the RAM update record
 *   ldr_progress(done, total)
 * plus the ota.c hooks (frames, time, idle). */
#define LDR_APP_LO 0x4000u
#define LDR_APP_HI 0x93000u
#define LDR_REC_LO 0x93000u                     /* update records live above the app ... */
#define LDR_REC_HI 0xFC000u                     /* ... and below Felucca's globals (Felucca's records: 0xE4F00, */
#define LDR_DATA_LO 0x97000u                    /* the stock firmware's: 0xE8F00), never in Felucca's store, whose */
#define LDR_DATA_HI 0xE0000u                    /* bytes (the store) the host writes */
#define LDR_NSEC ((LDR_APP_HI - LDR_APP_LO) >> 12)
#define LDR_FL_MAX 0x100000u                    /* flash.bin: at most the 1 MiB part */
#define LDR_BT_LO 0xEA000u                      /* (V10.12) the Bluetooth segment (hal/fm1_flash.h FL_EXT_*) */
#define LDR_BT_HI 0xFC000u
#define LDR_BT_TYPE 200u                        /* its UFW entry, "bt.bin" */
#define LDR_BT_MAGIC 0x3154424Au                /* "JBT1": its header's first word (firmware/bt/bt_seg.c bt_head) */
#define LDR_BT_NSEC ((LDR_BT_HI - LDR_BT_LO) >> 12)
#define LDR_PL_LO 0xB8000u                      /* (UMBRA LIVE) the plate bank (src/umb_slot.c UPS_BASE, 16 slots of 8 KiB) */
#define LDR_PL_HI 0xD8000u
#define LDR_PL_TYPE 201u                        /* its UFW entry, "plates.bin" */
#define LDR_PL_MAGIC 0x544C5055u                /* "UPLT": a plate's header's first word (src/umb_plate.h) */
#define LDR_PL_NSEC ((LDR_PL_HI - LDR_PL_LO) >> 12)

static int ldr_fread(uint32_t off, void *p, uint32_t n);
static int ldr_erase(uint32_t off);
static int ldr_prog(uint32_t off, const void *p, uint32_t n);
static void ldr_record_clear(void);
static void ldr_progress(uint32_t done, uint32_t total);

/* SFC cipher: per 32-byte block, key ^ (block offset >> 2) */
static void ldr_sfc(uint8_t *p, uint32_t n, uint32_t base_off, uint32_t key)
{
    uint32_t i, j;
    for (i = 0; i < n; i += 32u) {
        uint32_t k = key ^ ((base_off + i) >> 2);
        for (j = 0; j < 32u && i + j < n; j++) {
            p[i + j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
    }
}

/* chip key from the device's flash head: JLFS top entries (ENC 0xFFFF) -> isd_config.ini blob */
static int ldr_chip_key(uint32_t *key)
{
    uint8_t e[32], blob[34];
    uint32_t off, i, sum, k;
    for (off = 32; off < 0x400u; off += 32u) {
        if (ldr_fread(off, e, 32))
            return -1;
        ota_jl_enc(e, 32);
        if (ota_crc16(e + 2, 30, 0) != ota_rd16(e))
            return -2;
        if (!ota_memeq(e + 16, (const uint8_t *)"isd_config.ini", 15)) {
            if (ota_rd16(e + 14))
                return -3;                               /* last entry, not found */
            continue;
        }
        if (ldr_fread(ota_rd32(e + 4), blob, 34) || ota_crc16(blob, 32, 0) != ota_rd16(blob + 32))
            return -4;
        for (i = 0, sum = 0; i < 16u; i++)
            sum += blob[i];
        sum &= 0xFFu;
        sum = sum >= 0xE0u ? 0xAAu : sum <= 0x10u ? 0x55u : sum;
        for (i = 0, k = 0; i < 16u; i++)
            if ((uint32_t)(blob[16 + i] ^ blob[15 - i]) < sum)
                k |= 1u << i;
        *key = k;
        return 0;
    }
    return -3;
}

/* 2b. flash.bin [0, fl_size) from the host, before any erase: its CRC16 must be the UFW entry's (want); sc gets
 * the CRC16 of each app sector as it came in */
static int ldr_check(uint32_t fl_off, uint32_t fl_size, uint32_t want, uint16_t *sc, uint8_t *b)
{
    uint32_t i, n, crc = 0;
    for (i = 0; i < fl_size; i += n) {
        n = fl_size - i > 512u ? 512u : fl_size - i;
        if (ota_read(fl_off + i, b, n))
            return -7;
        crc = ota_crc16(b, n, crc);
        if (i >= LDR_APP_LO && i < LDR_APP_HI) {     /* (512-byte reads from 0: never across a sector) */
            uint32_t k = (i - LDR_APP_LO) >> 12;
            sc[k] = (uint16_t)ota_crc16(b, n, (i & 0xFFFu) ? sc[k] : 0u);
        }
    }
    return crc == want ? 0 : -12;
}

/* (V10.12) 2b for the Bluetooth segment: [off, off + len) of the package, its CRC16 the entry's (want), its header's
 * magic; sc gets each sector's CRC16 with the last one's tail 0xFF, as it is written */
/* ((UMBRA LIVE) the plate bank's slots the same way: magic its first word, err the code for a wrong one, err - 1
 * for a wrong CRC16) */
static int ldr_check_bt(uint32_t off, uint32_t len, uint32_t want, uint16_t *sc, uint8_t *b, uint32_t magic, int err)
{
    uint32_t i, n, crc = 0;
    for (i = 0; i < len; i += n) {
        n = len - i > 512u ? 512u : len - i;
        if (ota_read(off + i, b, n))
            return -7;
        if (!i && ota_rd32(b) != magic)
            return err;                                  /* not a segment (-15), not a plate (-18) */
        crc = ota_crc16(b, n, crc);
        sc[i >> 12] = (uint16_t)ota_crc16(b, n, (i & 0xFFFu) ? sc[i >> 12] : 0u);
    }
    for (i = 0; i < 512u; i++)
        b[i] = 0xFFu;
    for (i = len; i & 0xFFFu; i += n) {                  /* the last sector's tail as written: 0xFF */
        n = 512u - (i & 511u);
        sc[i >> 12] = (uint16_t)ota_crc16(b, n, sc[i >> 12]);
    }
    return crc == want ? 0 : err - 1;
}

/* 3. one sector: [src, src + n) of the package (n <= 4 KiB, the rest 0xFF) as checked in 2b (want), else read again;
 * written at dst unless it is there already, and verified */
static int ldr_sector(uint32_t dst, uint32_t src, uint32_t n, uint32_t want, uint8_t *sec, uint8_t *cur)
{
    uint32_t again, k;
    for (again = 0;; again++) {                          /* as checked in 2b, else read it again */
        for (k = 0; k < 0x1000u; k += 512u) {
            uint32_t m = k >= n ? 0u : n - k > 512u ? 512u : n - k, j;
            if (m && ota_read(src + k, sec + k, m))
                return -7;
            for (j = m; j < 512u; j++)
                sec[k + j] = 0xFFu;
        }
        if (ota_crc16(sec, 0x1000u, 0) == want)
            break;
        if (again == 2u)
            return -13;
    }
    if (ldr_fread(dst, cur, 0x1000u))
        return -8;
    if (!ota_memeq(sec, cur, 0x1000u)) {
        uint32_t tries;
        for (tries = 0; tries < 2u; tries++) {
            if (ldr_erase(dst))
                return -9;
            for (k = 0; k < 0x1000u; k += 256u)
                if (ldr_prog(dst + k, sec + k, 256))
                    return -10;
            if (ldr_fread(dst, cur, 0x1000u))
                return -8;
            if (ota_memeq(sec, cur, 0x1000u))
                break;
        }
        if (tries == 2u)
            return -11;                                  /* verify failed twice */
    }
    return 0;
}

static int ldr_session(void)
{
    static uint8_t hdr[0x400], sec[0x1000], cur[0x1000];
    static uint16_t sc[LDR_NSEC], bsc[LDR_BT_NSEC], psc[LDR_PL_NSEC];
    uint32_t i, k, fl_off, fl_size, ota_off, ota_len, key, s, want = ~0u, done = 0;   /* (no CRC16: no flash.bin entry) */
    uint32_t bt_off = 0, bt_len = 0, bt_want = 0, pl_off = 0, pl_len = 0, pl_want = 0;
    int rc;
    /* 1. UFW header + entry list (ota.c; it leaves the entries decoded in hdr) */
    if ((rc = ota_ufw(hdr, &fl_off, &fl_size, &ota_off, &ota_len)) != 0)
        return rc;
    if (!fl_off || fl_size < LDR_APP_HI || fl_size > LDR_FL_MAX)
        return -3;
    for (i = 0; i < ota_rd16(hdr + 8); i++) {
        const uint8_t *e = hdr + 0x40 + i * 0x50u;
        if (ota_rd16(e) == 0)
            want = ota_rd16(e + 4u);                     /* flash.bin's data CRC16 */
        if (ota_rd16(e) == LDR_BT_TYPE && ota_memeq(e + 0x40, (const uint8_t *)"bt.bin", 7)) {
            bt_off = ota_rd32(e + 8);                    /* (V10.12) the Bluetooth segment */
            bt_len = ota_rd32(e + 12);
            bt_want = ota_rd16(e + 4u);
        }
        if (ota_rd16(e) == LDR_PL_TYPE && ota_memeq(e + 0x40, (const uint8_t *)"plates.bin", 11)) {
            pl_off = ota_rd32(e + 8);                    /* (UMBRA LIVE) the plate bank's first slots */
            pl_len = ota_rd32(e + 12);
            pl_want = ota_rd16(e + 4u);
        }
    }
    if (pl_off && (!pl_len || (pl_len & 0x1FFFu) || pl_len > LDR_PL_HI - LDR_PL_LO))
        return -17;                                      /* (whole slots, inside the bank) */
    if (bt_off && (bt_len <= 32u || bt_len > LDR_BT_HI - LDR_BT_LO))
        return -14;
    /* 2. the package's app area must decrypt with this chip's key */
    if ((rc = ldr_chip_key(&key)) != 0)
        return -40 + rc;
    if (ota_read(fl_off + LDR_APP_LO, sec, 32))
        return -5;
    ldr_sfc(sec, 32, 0, key);
    if (ota_crc16(sec + 2, 30, 0) != ota_rd16(sec))
        return -6;                                       /* package for another chip key */
    /* 2b. the whole package first: nothing is erased for a damaged one */
    if ((rc = ldr_check(fl_off, fl_size, want, sc, sec)) != 0)
        return rc;
    if (bt_off && (rc = ldr_check_bt(bt_off, bt_len, bt_want, bsc, sec, LDR_BT_MAGIC, -15)) != 0)
        return rc;
    if (pl_off && (rc = ldr_check_bt(pl_off, pl_len, pl_want, psc, sec, LDR_PL_MAGIC, -18)) != 0)
        return rc;
    /* 3. app area, sector by sector */
    for (s = LDR_APP_LO; s < LDR_APP_HI; s += 0x1000u) {
        if ((rc = ldr_sector(s, fl_off + s, 0x1000u, sc[(s - LDR_APP_LO) >> 12], sec, cur)) != 0)
            return rc;
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO + ((bt_len + 0xFFFu) & ~0xFFFu) + pl_len);
    }
    /* 3b. (V10.12) the Bluetooth segment, after the app (a segment that does not match the app is never called) */
    for (k = 0; bt_off && k < bt_len; k += 0x1000u) {
        if ((rc = ldr_sector(LDR_BT_LO + k, bt_off + k, bt_len - k > 0x1000u ? 0x1000u : bt_len - k, bsc[k >> 12], sec,
                             cur)) != 0)
            return rc;
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO + ((bt_len + 0xFFFu) & ~0xFFFu) + pl_len);
    }
    /* 3c. (UMBRA LIVE) the plate bank's first slots */
    for (k = 0; pl_off && k < pl_len; k += 0x1000u) {
        if ((rc = ldr_sector(LDR_PL_LO + k, pl_off + k, 0x1000u, psc[k >> 12], sec, cur)) != 0)
            return rc;
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO + ((bt_len + 0xFFFu) & ~0xFFFu) + pl_len);
    }
    /* 4. finish: the host confirms, the record goes, the SPL boots the app */
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xF0000000u, sec, 8))
            break;
    ldr_record_clear();
    for (s = LDR_REC_HI; s > LDR_REC_LO; s -= 0x1000u) {   /* flash records: 4K boundary - 256 */
        uint8_t r[80];
        if (s - 0x1000u >= LDR_DATA_LO && s - 0x1000u < LDR_DATA_HI)
            continue;                                    /* Felucca's store: user data that may look like one */
        if (s - 0x1000u >= 0xEA000u && s - 0x1000u < 0xFC000u)
            continue;                                    /* (V10.11) the Bluetooth segment: code, never a record */
        if (ldr_fread(s - 0x100u, r, 80))
            break;
        if (ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0)) {
            if (s - 0x1000u >= 0xE7000u && s - 0x1000u < 0xEA000u) {   /* (V10.10) the stock VM: the radio's RF trims
                                                         * and BT address, the record at its tail (0xE8F00): the sector
                                                         * kept but those 256 bytes (was: erased whole, the trims lost) */
                if (ldr_fread(s - 0x1000u, cur, 0xF00u))
                    continue;
                if (!ldr_erase(s - 0x1000u))
                    for (k = 0; k < 0xF00u; k += 256u)  /* (a page at a time) */
                        (void)ldr_prog(s - 0x1000u + k, cur + k, 256);
            } else
                ldr_erase(s - 0x1000u);
        }
    }
    return 0;
}
