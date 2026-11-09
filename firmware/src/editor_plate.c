/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* Editor protocol: UMBRA OBSCURA's plate bank (EDITOR_PROTOCOL.md "Plates", cmds 96..100; umb_slot.c). A plate file
 * travels whole into a slot: BEGIN with its length (erases the slot), WRITEs of its bytes in order (7-bit packed, as the
 * backup's PUT), END with its CRC (checked as written, the header too; only then does the slot hold it). The image is
 * sent as linked for its slot (the page adds the slot's distance to the plate file's absolute words). Every write stops
 * the transport first (a flash erase silences the audio a moment). rc: 0 ok, 1 arguments, 2 the CRC or the header refused
 * (the slot left empty), 3 stop playback first (it did not stop in 100 ms), 4 flash. */
enum { ED_PLATE_LIST = 96, ED_PLATE_BEGIN, ED_PLATE_WRITE, ED_PLATE_END, ED_PLATE_ERASE };
#define ED_PLATE_SLOTS UPS_NSLOT

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
    case ED_PLATE_LIST:                                 /* [first slot] -> API, slots, the first slot listed, then up to 8
                                                         * slots from it: state (0 empty, 1 a plate, 2 a plate not run: no
                                                         * clean start since), name, length (u32) */
        if (n > 1u || (n && a[0] >= ED_PLATE_SLOTS))
            return 0;
        k = n ? a[0] : 0u;
        ed_b(UMB_API); ed_b(ED_PLATE_SLOTS); ed_b(k);
        for (rc = k + 8u; k < rc && k < ED_PLATE_SLOTS; k++) {
            uint32_t len = ups_check(k);
            umb_plate_t h;
            if (len && st_read(UPS_OFF(k), &h, sizeof h))
                len = 0;
            ed_b(!len ? 0u : boot_clean ? 1u : 2u);
            ed_str(len ? h.name : "", 10);
            ed_bk_u32(len);
        }
        return 1;
    case ED_PLATE_BEGIN:                                /* slot, length (u32) -> slot, rc */
        rc = n != 6u || a[0] >= ED_PLATE_SLOTS || !ed_pl_u32ok(a + 1) ? 1u : ed_flash_stop() ? 3u : ups_begin(a[0], ed_pl_u32(a + 1));
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    case ED_PLATE_WRITE:                                /* slot, offset (u32), the bytes 7-bit packed (up to 256) -> slot, rc */
        k = n > 6u ? ed_unpack7(a + 6, n - 6u, buf, sizeof buf) : 0u;
        rc = n < 8u || a[0] >= ED_PLATE_SLOTS || !ed_pl_u32ok(a + 1) || !k ? 1u : ed_flash_stop() ? 3u :
             ups_write(a[0], ed_pl_u32(a + 1), buf, k);
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    case ED_PLATE_END:                                  /* slot, CRC-32 (u32) -> slot, rc */
        rc = n != 6u || a[0] >= ED_PLATE_SLOTS || !ed_pl_u32ok(a + 1) ? 1u : ed_flash_stop() ? 3u : ups_end(a[0], ed_pl_u32(a + 1));
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    case ED_PLATE_ERASE:                                /* slot -> slot, rc */
        rc = n != 1u || a[0] >= ED_PLATE_SLOTS ? 1u : ed_flash_stop() ? 3u : ups_erase(a[0]);
        ed_b(n ? a[0] : 127u); ed_b(rc);
        return 1;
    }
    return 0;
}
