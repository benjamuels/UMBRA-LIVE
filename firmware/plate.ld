/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (UMBRA OBSCURA) a plate file (tools/build.py plate_files; src/umb_plate.h): a plate built alone, linked for the plate
 * bank's slot 0, flash 0xB8000 (src/umb_slot.c UPS_BASE; build.py links it again for slot 15 to find the words the page
 * moves for another slot), read where it lies through the plain XIP window (hal/fm1_xip.h FM1_XIP:
 * 0x02000000 + off - 0x4000). Its header (umb_plate_t, section .umb_head) first; code and constants only: its state is
 * the block the firmware hands it, so .data and .bss must come out empty (build.py checks). */
MEMORY
{
    SLOT (rx) : ORIGIN = 0x020B4000, LENGTH = 0x1FE0    /* FM1_XIP(0xB8000); the slot (8 KiB) less its 32-byte record (UPS_REC) */
}
SECTIONS
{
    .plate : { KEEP(*(.umb_head)) *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) . = ALIGN(4); } > SLOT
    .data : { *(.data .data.* .sdata .sdata.*) } > SLOT
    .bss : { *(.bss .bss.* .sbss .sbss.* COMMON) } > SLOT
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
