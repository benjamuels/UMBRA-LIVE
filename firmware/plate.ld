/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (UMBRA OBSCURA) a plate file (tools/build.py plate_files; src/umb_plate.h): a plate built alone, linked for the plate
 * slot, flash 0xDA000 (src/umb_slot.c UPS_OFF), read where it lies through the plain XIP window (hal/fm1_xip.h FM1_XIP:
 * 0x02000000 + off - 0x4000). Its header (umb_plate_t, section .umb_head) first; code and constants only: its state is
 * the block the firmware hands it, so .data and .bss must come out empty (build.py checks). */
MEMORY
{
    SLOT (rx) : ORIGIN = 0x020D6000, LENGTH = 0xFE0     /* FM1_XIP(0xDA000); the slot less its 32-byte record (UPS_REC) */
}
SECTIONS
{
    .plate : { KEEP(*(.umb_head)) *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) . = ALIGN(4); } > SLOT
    .data : { *(.data .data.* .sdata .sdata.*) } > SLOT
    .bss : { *(.bss .bss.* .sbss .sbss.* COMMON) } > SLOT
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
