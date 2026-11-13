/* SPDX-License-Identifier: GPL-3.0-only */
/* (UMBRA LIVE) Where the update loader may erase / program: ONE definition, used by the device loader
 * (loader.c, as fm1_flash.h's FL_RANGE_OK) and by the host test (tests/ldr_test.c), so the test cannot allow a
 * write the device refuses. (Umbra Live 1.0 shipped with the plate bank allowed only in the test's own copy: the
 * device refused 0xB8000's first erase and the install stopped there, the FM-1 left in update mode.)
 *   [0x4000, 0x97000)   the app area and the update records above it (never the head)
 *   [0xB8000, 0xD8000)  the plate bank (src/umb_slot.c: JIANT's projects 5..8, retired for it), step 3c
 *   [0xE0000, 0xFC000)  the update records, the stock VM sector, the Bluetooth segment (step 3b)
 * Never the rest of Felucca's store [0x97000, 0xE0000): user data. */
#ifndef LDR_WINDOW_H
#define LDR_WINDOW_H
#ifndef FL_IN
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))
#endif
#define LDR_WINDOW_OK(off, n) (FL_IN(off, n, 0x4000u, 0x97000u) || FL_IN(off, n, 0xB8000u, 0xD8000u) || \
                               FL_IN(off, n, 0xE0000u, 0xFC000u))
#endif
