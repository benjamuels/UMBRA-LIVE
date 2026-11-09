/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* Editor protocol: UMBRA OBSCURA's plate slot (EDITOR_PROTOCOL.md "Plates", cmds 96..100; umb_slot.c). A plate file
 * travels whole: BEGIN with its length (erases the slot), WRITEs of its bytes in order (7-bit packed, as the backup's
 * PUT), END with its CRC (checked as written, the header too; only then does the slot hold it). Every write stops the
 * transport first (a flash erase silences the audio a moment). rc: 0 ok, 1 arguments, 2 the CRC or the header refused
 * (the slot left empty), 3 stop playback first (it did not stop in 100 ms), 4 flash. */
enum { ED_PLATE_LIST = 96, ED_PLATE_BEGIN, ED_PLATE_WRITE, ED_PLATE_END, ED_PLATE_ERASE };
#define ED_PLATE_SLOTS 1u

static uint32_t ed_pl_u32(const uint8_t *a)             /* u32: 5 x 7 bits, LSB first (the last 0..15) */
{
    return (uint32_t)a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14 | (uint32_t)a[3] << 21 | (uint32_t)a[4] << 28;
}
static int ed_pl_u32ok(const uint8_t *a) { return a[4] <= 15u; }

static int ed_plate_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint8_t buf[256];
    uint32_t rc, k;
    switch (cmd) {
    case ED_PLATE_LIST:                                 /* -> API, slots, per slot: state (0 empty, 1 a plate, 2 a plate
                                                         * not run: no clean start since), name, length (u32) */
        if (n)
            return 0;
        if (!ups_st)
            ups_check();
        ed_b(UMB_API); ed_b(ED_PLATE_SLOTS);
        ed_b(ups_st != 1 ? 0u : boot_clean ? 1u : 2u);
        ed_str(ups_st == 1 ? ups_name : "", 10);
        ed_bk_u32(ups_st == 1 ? ups_len : 0u);
        return 1;
    case ED_PLATE_BEGIN:                                /* slot, length (u32) -> slot, rc */
        rc = n != 6u || a[0] >= ED_PLATE_SLOTS || !ed_pl_u32ok(a + 1) ? 1u : ed_flash_stop() ? 3u : ups_begin(ed_pl_u32(a + 1));
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    case ED_PLATE_WRITE:                                /* slot, offset (u32), the bytes 7-bit packed (up to 256) -> slot, rc */
        k = n > 6u ? ed_unpack7(a + 6, n - 6u, buf, sizeof buf) : 0u;
        rc = n < 8u || a[0] >= ED_PLATE_SLOTS || !ed_pl_u32ok(a + 1) || !k ? 1u : ed_flash_stop() ? 3u :
             ups_write(ed_pl_u32(a + 1), buf, k);
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    case ED_PLATE_END:                                  /* slot, CRC-32 (u32) -> slot, rc */
        rc = n != 6u || a[0] >= ED_PLATE_SLOTS || !ed_pl_u32ok(a + 1) ? 1u : ed_flash_stop() ? 3u : ups_end(ed_pl_u32(a + 1));
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    case ED_PLATE_ERASE:                                /* slot -> slot, rc */
        rc = n != 1u || a[0] >= ED_PLATE_SLOTS ? 1u : ed_flash_stop() ? 3u : ups_erase();
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    }
    return 0;
}
