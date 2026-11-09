/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 SARADC: PB6 = MASTER pot (ch4),
 * PB1 = battery divider (ch3). Polled, 10 bit, from the main loop.
 * (V10.6z5) Also two of the key matrix's rows, PA7 (ch0) and PA8 (ch1), for the velocity diagnostic
 * (src/vel_diag.c, hal/fm1_input.h fm1__an_*: after Velo, Charles Vestal's charlesvestal/fm1-velocity, GPL-3.0, as
 * fm1-dock's hal/fm1_adc.h has it).
 *
 *   fm1_adc_init()        both pins analog inputs, no pulls
 *   fm1_adc_read(ch)      0..1023, -1 on timeout; while the key scan owns the converter (fm1_an.on / want: the
 *                         diagnostic on) the MASTER and battery readings the scan took in their slots (-1: none yet),
 *                         no conversion here
 *   fm1_adc_start(ch, con), fm1_adc_done(), fm1_adc_take(), fm1_adc_abort()   one conversion between two scan ticks
 *                         (the scan only)
 */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"
#include "fm1_input.h"                  /* (V10.6z5: fm1_an, the scan's; it includes this file at its end) */

#ifndef FM1_ADC_CON                       /* (tests/vel_diag_test.c: as memory) */
#define FM1_ADC_CON (*(volatile uint32_t *)0x13100u)
#define FM1_ADC_RES (*(volatile uint32_t *)0x13104u)
#define FM1_WLA_CON0 (*(volatile uint32_t *)0x11900u)
#endif
enum { FM1_ADC_ROW_PA7 = 0, FM1_ADC_ROW_PA8 = 1, FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
/* CON: bits 0-2 the clock divider (the SDK's 6), bit 3 set, bit 4 enable, bit 6 start / clear, bit 7 done, bits 8-11
 * the channel, bits 12-15 the start delay (x 8 clocks; the SDK's 15). The scan's: divider 7, delay 2 (CS-1's and
 * fm1-dock's, measured: done well inside the 100 us between two ticks) */
#define FM1_ADC_CON_SCAN (7u | 8u | (2u << 12))


/* Both are only ever analog inputs: PB1 held low for 8 s would reset the
 * chip (isd_config). */
static void fm1_adc_init(void)
{
    uint32_t m = (1u << 6) | (1u << 1);
    FM1_PR(FM1_PB, FM1_DIE) &= ~m;
    FM1_PR(FM1_PB, FM1_PU) &= ~m;
    FM1_PR(FM1_PB, FM1_PD) &= ~m;
    FM1_PR(FM1_PB, FM1_DIR) |= m;
}

static int32_t fm1_adc_read(uint32_t ch)
{
    uint32_t t, v;
    if (fm1_an.want | fm1_an.on)                  /* (V10.6z5) the scan has the converter */
        return ch == FM1_ADC_MASTER ? fm1_an.master : ch == FM1_ADC_BATT ? fm1_an.batt : -1;
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = 0xF04Eu | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
    for (t = 0; t < 20000u; t++)
        if (FM1_ADC_CON & 0x80u)
            break;
    v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return t == 20000u ? -1 : (int32_t)v;
}

/* (V10.6z5) one conversion, started on a scan tick and taken on the next (fm1-dock's hal/fm1_adc.h) */
static void fm1_adc_start(uint32_t ch, uint32_t con)
{
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = (con & 0xF00Fu) | 0x40u | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
}
static inline int fm1_adc_done(void) { return (FM1_ADC_CON & 0x80u) != 0; }
static inline uint32_t fm1_adc_take(void)
{
    uint32_t v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return v;
}
static inline void fm1_adc_abort(void)
{
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
}

/* (V10.6z5) the velocity diagnostic's samples (hal/fm1_input.h, top): the drive of row bit rb (3 PA7, 4 PA8), and back */
static void fm1__row_drive(uint32_t rb, uint32_t strength)
{
    uint32_t m = 1u << (rb == 3u ? 7 : 8);
    if (strength & 1u)
        FM1_PR(FM1_PA, FM1_HD0) |= m;
    else
        FM1_PR(FM1_PA, FM1_HD0) &= ~m;
    if (strength & 2u)
        FM1_PR(FM1_PA, FM1_HD) |= m;
    else
        FM1_PR(FM1_PA, FM1_HD) &= ~m;
    FM1_PR(FM1_PA, FM1_OUT) |= m;
    FM1_PR(FM1_PA, FM1_DIR) &= ~m;
}
static void fm1__row_release(uint32_t rb)
{
    uint32_t m = 1u << (rb == 3u ? 7 : 8);
    FM1_PR(FM1_PA, FM1_DIR) |= m;                  /* an input with its pull-up again (as fm1_input_init left it) */
    FM1_PR(FM1_PA, FM1_OUT) &= ~m;
    FM1_PR(FM1_PA, FM1_HD0) &= ~m;
    FM1_PR(FM1_PA, FM1_HD) &= ~m;
}
/* the conversion started last tick: its row an input again, its result; off once the main loop let go */
static __attribute__((noinline)) void fm1__an_take(void)
{
    uint32_t rb = fm1_an.rowbit, driven = fm1_an.driven;
    if (driven) {
        fm1_an.driven = 0;
        fm1__row_release(rb);
    }
    if (fm1_an.pend) {
        fm1_an.pend = 0;
        if (fm1_adc_done()) {
            uint32_t v = fm1_adc_take();
            if (rb)
                FM1_INPUT_ANALOG(fm1_an.col, rb, v, driven);
            else if (fm1_an.col)
                fm1_an.batt = (int16_t)v;
            else
                fm1_an.master = (int16_t)v;
        } else {
            fm1_adc_abort();
            fm1_an.timeouts++;
        }
    }
    if (!fm1_an.want)
        fm1_an.on = 0;                             /* (nothing pending, the rows inputs: the main loop's again) */
}
/* column n is latched: sample its PA7 / PA8 key (or the pot, the battery) until the next tick */
static __attribute__((noinline)) void fm1__an_start(uint32_t n)
{
    uint32_t odd, ch, rb;
    if (n == 0u)
        fm1_an.phase ^= 1u;
    odd = fm1_an.phase;
    ch = odd ? FM1_ADC_ROW_PA8 : FM1_ADC_ROW_PA7;
    rb = odd ? 4u : 3u;
    if (odd && n < 2u) {                           /* (PA8's columns 0, 1: OCT- OCT+) */
        ch = n ? FM1_ADC_BATT : FM1_ADC_MASTER;
        rb = 0;
    }
    fm1_an.col = (uint8_t)n;
    fm1_an.rowbit = (uint8_t)rb;
    if (rb && fm1_an.drive && FM1_INPUT_DRIVE(n, rb)) {
        fm1__row_drive(rb, fm1_an.drive - 1u);
        fm1_an.driven = 1;
    }
    fm1_adc_start(ch, FM1_ADC_CON_SCAN);
    fm1_an.pend = 1;
}
