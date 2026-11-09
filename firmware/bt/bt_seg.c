/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.12) The Bluetooth segment: Optimist's BLE stack (Henri David, w0ts/optimist; the hardware-tested copy in
 * charlesvestal/fm1-base bt/ble, GPL-3.0-only OR MPL-2.0, used here under GPL-3.0-only) with the WL82 radio drivers
 * (hal/fm1_ble.h, hal/fm1_ble_rf.h: Melodee's versions, Kerem Kilic, GPL-3.0-only; hal/fm1_rf_cal.h and
 * hal/ble_rf_tables_v15.h from felucca-bsp, Apache-2.0), as a translation unit of its own that the linker puts in
 * flash at 0xEA000 (firmware/app.ld BTSEG, XIP 0x020E6000), outside the app slot, run in place through the plain XIP
 * window (hal/fm1_flash.h fl_plain_window_init, set by src/bt_hw.c before the first call). docs/BLE_FEASIBILITY.md
 * route C2.
 *
 * Peripheral only: advertising as "JIANT FM-1" with the BLE-MIDI service (connectable), no scanner, no central; (V10.32)
 * Just Works pairing with bonding when a central asks (FELUCCA_BLE_BOND), the link encrypted then (before: Pairing Not Supported). The radio is calibrated by
 * its own measurement at the first start (fm1_rf_cal.h: records 106 / 107 / 108 the SDK board's constants, 187
 * measured, ~160 ms), as Melodee and fm1-base do on units whose stock calibration is gone.
 *
 * What the app (firmware/src/bt.c) calls, only after it checked the segment installed is this build's (bt_head below:
 * its length and CRC-32, which tools/build.py also writes into the app's bt_want):
 *   bt_seg_start(addr)   the first ON: the vectors (IRQ 45 / 29 at priority 2, below the audio and TIMER5), the radio's
 *                        start-up and calibration, the stack, advertising. The caller has the interrupts off, the
 *                        audio buffer silent, the second core parked
 *   bt_seg_enable(on)    advertising on again / off (and any link closed); (V10.35) 2: AIR, scanning only; the radio stays set up until power-off
 *   bt_seg_status()      bit 0 a central connected, bit 1 its MIDI notifications on, (V10.35) bit 2 AIR scanning, [15:8] connections since start
 *   bt_seg_rssi(w, rd)   (V10.14) the RSSI words of the good packets received since the last call (PROX)
 * (V10.13) MIDI both ways (bt_midi.h): what a central writes, decoded by the stack (ble_midi.c), goes into the app's MIDI
 * input ring as USB's does (bt_midi_accept, then the app's bt_midi_rx: src/bt.c); MIDI OUT's copy (the app's bt_oq, filled
 * by the audio interrupt while a central listens) goes out in the notifications the stack packs each connection event
 * (ble_att.c att_midi_out), stamped with this segment's millisecond count. Received timestamps are not used (the
 * messages play as they come). The app's functions and data it reaches (bt_midi_rx, bt_oq) are its own or in RAM.
 *
 * Everything here is in the segment (code, constants) or in the pool (its zeroed RAM, app.ld); .ram_text (the radio
 * ramp's burst, fm1_ble_rf.h) goes with the app's RAM code. No initialised data (tools/build.py refuses it). */
#include <stdint.h>
#define FM1_IRQ_EXTERN 1                       /* (hal/fm1_irq.h: the crash record is the app's) */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define BLE_BARRIER() RING_PUBLISH()
#define BLE_DEVICE_NAME "JIANT FM-1"
#define BLE_NAME_HEX 0                         /* the name as it is (no " XXXX") */
#define BLE_LL_MAX_OCTETS 27u                  /* stock's PDU size, as Melodee (radio-tested) */
#define BLE_DIAG 0                             /* (no counters, no console: as Melodee) */
#define FELUCCA_BLE_BOND 1                     /* (V10.32) Just Works pairing with bonding, LL encryption: a central that asks
                                                * to pair (Windows, some Macs and Androids) pairs, no PIN */
#define FELUCCA_BLE_CENTRAL 3                  /* (V10.35) AIR: the scanner alone (no connecting out): every advertisement's
                                                * RSSI to PROX (bs_rssi), nothing listed */
#define BLE_SCAN_RING 1u                       /* (the list unused: AIR reads bs_rssi) */
#define BLE_SCAN_ACTIVE 0                      /* (listens only: nothing sent) */
#define BLE_LTK_DERIVE 1                       /* .. the bond's key made again from EDIV / Rand (ble_smp.c ble_smp_ltk_of):
                                                * nothing kept, every bonded central back after a power-off */
#include "fm1_cc.h"
#include "fm1_time.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
/* (V10.14) PROX: the RSSI word of every good packet on the link (ble_hw_wl82.c hw_rx_service, the RX interrupt: one a
 * connection event, 89..133 a second), kept in a ring the app reads (bt_seg_rssi; src/bt.c filters it) */
static volatile uint16_t bs_rq[8][4];        /* (V10.35: 8, was 16: the pool for AIR's scanner) */
static volatile uint32_t bs_rw;
/* (V10.18) all four RSSI words of the packet (RSSI2 / RSSI3 by the buffer first, as V10.14 read it, then the other, then
 * RSSI0, RSSI1): which one follows the distance is found by the app (src/bt.c bt_prox_tick) */
static void bs_rssi(const volatile uint16_t *r, uint32_t b)
{
    volatile uint16_t *q = bs_rq[bs_rw & 7u];
    q[0] = r[2u + (b & 1u)];
    q[1] = r[3u - (b & 1u)];
    q[2] = r[0];
    q[3] = r[1];
    RING_PUBLISH();
    bs_rw++;
}
#define BLE_HW_RX_RSSI(r, b) bs_rssi(r, b)
/* (SWARM) every advertisement heard while AIR scans, to the app's INPUT engine (src/eng_input.c bt_air_tap, the RX
 * interrupt: a copy into a table, no more): its PDU, its RSSI word as PROX reads it, its channel */
void bt_air_tap(const uint8_t *p, uint32_t n, uint32_t w, uint32_t ch);
#define BLE_HW_ADV_TAP(p, n, r, b, ch) bt_air_tap(p, n, (r)[2u + ((b) & 1u)], ch)
#include "ble/ble_stack.c"

/* the segment's header, first in it (app.ld .bt_head): magic, payload length, CRC-32 of the payload (zlib), filled by
 * tools/build.py; the payload follows at 0xEA020 */
#define BT_SEG_MAGIC 0x3154424Au               /* "JBT1" */
__attribute__((section(".bt_head"), used)) const uint32_t bt_head[8] = {BT_SEG_MAGIC, 0, 0, 0, 0, 0, 0, 0};

static uint8_t bs_conns, bs_linked;

#include "bt_midi.h"
extern struct bt_oq bt_oq;                     /* (src/usb.c: MIDI OUT's copy) */
void bt_midi_rx(uint32_t pkt);                 /* (src/bt.c: into the MIDI input ring) */

static uint32_t bs_t, bs_ms, bs_frac;          /* milliseconds from TIMER4's ticks (24 a microsecond) */
static uint32_t bs_now(void)
{
    uint32_t t = fm1_ticks(), d = t - bs_t + bs_frac;
    bs_t = t;
    bs_ms += d / 24000u;
    bs_frac = d % 24000u;
    return bs_ms;
}
BLE_API int ble_app_midi_peek(uint32_t *pkt, uint32_t *t_ms)
{
    uint32_t due, now;
    if (!bt_oq_peek(&bt_oq, pkt, &due))
        return 0;
    now = bs_now();
    if (due) {                                 /* (V10.33) a clock pulse: not before its time, stamped with it (the
                                                * receiver puts the pulses back evenly from the stamps) */
        int32_t d = (int32_t)(fm1_ticks() - due);
        if (d < 0)
            return 0;
        now -= (uint32_t)d / 24000u;
    }
    *t_ms = now;
    return 1;
}
BLE_API void ble_app_midi_pop(void) { bt_oq_pop(&bt_oq); }
/* (V10.32) bonding: the key is made again from EDIV / Rand (BLE_LTK_DERIVE), the central's identity is not needed (no
 * central role): nothing to keep */
BLE_API void ble_app_bond(const uint8_t rand[8], uint16_t ediv, const uint8_t ltk[16]) { (void)rand, (void)ediv, (void)ltk; }
BLE_API void ble_app_peer_id(const uint8_t irk[16], const uint8_t addr[6], uint8_t addr_rand) { (void)irk, (void)addr, (void)addr_rand; }
BLE_API void ble_app_midi_in(uint32_t pkt, uint16_t ts, uint16_t last_ts)
{
    (void)ts, (void)last_ts;                   /* (played as it comes) */
    if ((pkt = bt_midi_accept(pkt)) != 0u)
        bt_midi_rx(pkt);
}
BLE_API void ble_app_state(void)
{
    uint8_t c = (uint8_t)ble_connected(), on = (uint8_t)ble_midi_ready();
    if (c && !bs_linked && bs_conns < 255u)
        bs_conns++;
    bs_linked = c;
    if (on && !bt_oq.on)
        bt_oq.r = bt_oq.w;                     /* (a new listener: nothing stale) */
    bt_oq.on = on;
}

/* the SDK board's values for the three constant records (crystal load, PA bias, Wi-Fi per-rate gains: zero keeps the
 * defaults); 187 is measured (fm1_rf_cal_on), as fm1-base's bt_own.c and Melodee's midi_ble.c */
static const struct ble_rf_trims BS_TRIMS = {{0x0B, 0x0B}, {1, 7, 4, 7, 11, 1, 7}, {0}, {0}};

int bt_seg_start(const uint8_t addr[6])
{
    fm1_ble_crumb_boot();                      /* (Melodee's .noinit breadcrumb: the last start's, then from zero) */
    fm1_ble_step(FM1_BLE_STEP_STACK_INIT);
    ble_init(addr, 1);
    fm1_guard_unlock_top();                    /* (the vectors are in the protected top of RAM) */
    fm1_ble_irq_attach(BLE_HW_IRQ_PRIO);       /* the two vectors, masked until the radio runs */
    fm1_ble_irqs_hold(1);
    fm1_guard_lock_top();
    fm1_rf_cal_on = 1;                         /* (our own measurement, no stored 187) */
    ble_hw_wl82_start(&BS_TRIMS);              /* the radio's start-up, its calibration (the crystal ramp from RAM) */
    if (fm1_ble_rf_stat.section != FM1_RF_SECT_DONE)
        return 1;                              /* (the start-up did not run to its end: no advertising, held) */
    fm1_ble_step(FM1_BLE_STEP_ENABLE);
    ble_enable(1);
    fm1_ble_step(FM1_BLE_STEP_RUNNING);
    fm1_ble_irqs_hold(0);                      /* (taken once the caller turns the interrupts back on) */
    return 0;
}

void bt_seg_enable(int on)
{
    fm1_ble_irqs_hold(1);
    fm1_ble_step(on ? FM1_BLE_STEP_ENABLE : FM1_BLE_STEP_SET_OFF);
    if (on != 1)
        ble_enable(0);                         /* (V10.35) off, or AIR: the advertising / the scan stops first */
    ble_ll_scan(on == 2);                      /* AIR (2): the scanner, nothing advertised */
    ble_enable(on != 0);
    if (on != 1)
        bs_linked = bt_oq.on = 0;
    fm1_ble_irqs_hold(0);
}

uint32_t bt_seg_status(void)
{
    return (uint32_t)ble_connected() | (uint32_t)ble_midi_ready() << 1 | (uint32_t)ble_ll_scanning() << 2 |
           (uint32_t)bs_conns << 8;
}

/* (V10.19) PROX's diagnostics: [15:0] the packets the RSSI hook saw, [31:16] the good packets the link received */
uint32_t bt_seg_rxdiag(void) { return (bs_rw & 0xFFFFu) | (uint32_t)(ble_hw_stat.rx & 0xFFFFu) << 16; }
/* (V10.14) the RSSI words since *rd (the newest 16 packets at most: a main loop held up by a flash write loses the
 * older); (V10.18) four words a packet, w[4 * i .. 4 * i + 3] */
uint32_t bt_seg_rssi(uint16_t *w, uint32_t *rd)
{
    uint32_t e = bs_rw, n = e - *rd, i, j;
    RING_PUBLISH();
    if (n > 8u)
        n = 8u;
    for (i = 0; i < n; i++)
        for (j = 0; j < 4u; j++)
            w[4u * i + j] = bs_rq[(e - n + i) & 7u][j];
    *rd = e;
    return n;
}
