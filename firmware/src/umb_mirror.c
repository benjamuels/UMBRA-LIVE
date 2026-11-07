/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (UMBRA OBSCURA) The screen on a computer: Patrick F. Kellogg's screen mirror (screen_mirror/, MIT) and its viewers,
 * FM-1 Mirror (Mac, Windows, Linux) and FM-1 Mirror + Spyglass (Android). The viewer says hello over USB MIDI about
 * once a second (F0 7D 'F' 'S' 00 F7: usb.c sysex_byte, sm_sysex); while it does, every rectangle that goes to the
 * panel goes to it too (lcd.c lcd_fill / lcd_blit / lcd_blit_stride: umr_fill, umr_blit), run-length coded, at most
 * 70 bytes a SysEx message; 3 s after the last hello nothing more. With nobody asking it costs a test a rectangle.
 * - The SysEx queue (usb.c sx_out_q) goes out before the notes, so the mirror steps aside while notes wait (mo_w)
 *   and leaves some of the queue to the editor: a note out waits a few ms at most, never for the screen.
 * - The queue is topped up while the screen is drawn (umr_rows), not only once a main-loop pass: a KILN frame takes
 *   tens of ms, the queue a few to empty.
 * - Rows that found the mirror's buffer full are lost to the viewer. The next ones begin where they stopped (umr_from:
 *   the rows above it had their turn, skipped until the rest have had theirs), so a picture redrawn every frame
 *   reaches the viewer as a rolling refresh, top to bottom, not its top again and again. Once nothing has been lost
 *   for half a second, a full repaint (ui.force, a second apart at least) mends what was; a viewer joining asks for
 *   one too (sm_take_full_request).
 * - What it can show: pages, menus and the melody plates (mostly still, mostly dark) as they change; KILN, every
 *   pixel new every frame at ~4 bytes a pixel on the wire, well under a frame a second.
 * The sound's name (F0 7D 'F' 'S' 02 ..: "T1 DRUM-X 909 CLUB") goes too, for the viewer's spyglass. */
/* (its own queue: felucca.c SM_BUFFER, 3 KB in the pool) */
#ifndef UMR_SXQ_FREE
#define UMR_SXQ_FREE 32u                                /* packets of usb.c's SysEx queue the mirror leaves to the editor */
#endif
static uint8_t umr_stale;                               /* rows lost (the buffer full): a full repaint due */
static uint32_t umr_full_ms, umr_name_ms, umr_lost_ms, umr_from_ms;
static uint32_t umr_from;                               /* rows above it had their turn (0: none skipped) */
static uint8_t umr_full;                                /* the buffer full since umr_from was set: later losses don't move it */
static uint8_t umr_round;                               /* a round of the rolling refresh ended with rows lost: repaint */
static uint16_t umr_row[240];                           /* a row as the viewer takes it: RGB565, not the panel's byte order */

#if FELUCCA_WEB
static int web_mirror_put(const uint8_t *m, unsigned n);   /* (web/emu/felucca_web.c: what the browser's viewer reads) */
#endif
/* one whole SysEx message (F0 .. F7) out, or -2: not now (the queue full, notes waiting, another sender busy) */
static int umr_send(const uint8_t *m, unsigned n)
{
#if FELUCCA_WEB
    return web_mirror_put(m, n);
#elif FELUCCA_OTA
    uint32_t i = 0, k, cin, pkt;
    if (!*(volatile uint8_t *)&usb.config)
        return -1;                                      /* (no host: the mirror stops until the next hello) */
    if (sx_busy || mo_w != mo_r || so_w - so_r + (n + 2u) / 3u > SXQ - UMR_SXQ_FREE)
        return -2;
    sx_busy = 1;                                        /* (no note between its packets: ep1_tx) */
    while (i < n) {
        k = n - i >= 3u ? 3u : n - i;
        cin = k == 3u && i + 3u < n ? 4u : k == 3u ? 7u : 4u + k;   /* (usb.c ota_wire_send's packets) */
        pkt = cin | (uint32_t)m[i] << 8 | (k > 1u ? (uint32_t)m[i + 1] << 16 : 0u) | (k > 2u ? (uint32_t)m[i + 2] << 24 : 0u);
        sx_out_q[so_w % SXQ] = pkt;
        RING_PUBLISH();
        so_w++;
        i += k;
    }
    sx_busy = 0;
    return 0;
#else
    (void)m;
    (void)n;
    return -1;
#endif
}
/* rows y .. y + h - 1, x .. x + w - 1 as they go to the panel: px (byte-swapped, stride px a row), or c (RGB565) */
static void umr_rows(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint16_t c)
{
    uint32_t r, i;
    if (!sm_on() || !w || !h || x >= 240u || y >= 240u)
        return;
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    if (umr_from && fm1_ms - umr_from_ms > 200u)
        umr_from = 0;                                   /* (nothing below drawn since: the top's turn again) */
    for (r = 0; r < h; r++) {
        if (y + r < umr_from)
            continue;
        if (px)
            for (i = 0; i < w; i++) {
                uint32_t v = px[r * stride + i];
                umr_row[i] = (uint16_t)(v >> 8 | v << 8);
            }
        else if (r == 0u || y + r - 1u < umr_from)       /* (a fill: its row made once, or after skipped ones) */
            for (i = 0; i < w; i++)
                umr_row[i] = c;
        if (!sm_rows(umr_row, x, y + r, w, 1u)) {
            sm_poll(fm1_ms, umr_send);                  /* (room made, as far as the queue takes it) */
            if (!sm_rows(umr_row, x, y + r, w, 1u)) {
                umr_stale = 1;
                umr_lost_ms = fm1_ms;
                if (!umr_full) {                        /* (the first row that didn't fit: the next round begins here) */
                    umr_full = 1;
                    umr_from = y + r;
                    umr_from_ms = fm1_ms;
                }
                return;
            }
        }
        umr_full = 0;
        if (y + r >= 239u) {
            if (umr_from || umr_stale)
                umr_round = 1;                          /* (a round with losses: what isn't redrawn by itself, again) */
            umr_from = 0;                               /* (the bottom reached: round again from the top) */
        }
    }
    sm_poll(fm1_ms, umr_send);
}
static void umr_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { umr_rows(x, y, w, h, 0, 0, c); }
static void umr_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride)
{
    umr_rows(x, y, w, h, px, stride, 0);
}
/* every main-loop pass: what is queued out; a full repaint asked for, or due; the sound's name (every 100 ms) */
static void umr_poll(void)
{
    sm_poll(fm1_ms, umr_send);
    if (sm_take_full_request() ||
        (umr_stale && sm_on() && fm1_ms - umr_lost_ms >= 500u && fm1_ms - umr_full_ms >= 1000u) ||
        (umr_round && sm_on() && fm1_ms - umr_full_ms >= 2000u)) {
        ui.force = 1;                                   /* (caught up, or a round of the rolling refresh over: what was
                                                         * lost, the header and the words too, painted again) */
        umr_stale = umr_round = 0;
        umr_from = 0;
        umr_full_ms = fm1_ms;
    }
    if (fm1_ms - umr_name_ms >= 100u) {
        const track_t *t = &trk[song.sel % NTRK];
        char b[48], s[32];
        umr_name_ms = fm1_ms;
        b[0] = 'T';
        b[1] = (char)('1' + song.sel % NTRK);
        b[2] = ' ';
        str_cpy(b + 3, trk_engp(t)->name, 16);
        sound_name(t, s);
        if (s[0]) {
            str_cpy(b + str_len(b), " ", 2);
            str_cpy(b + str_len(b), s, sizeof b - str_len(b));
        }
        sm_name(b, fm1_ms, umr_send);
    }
}
