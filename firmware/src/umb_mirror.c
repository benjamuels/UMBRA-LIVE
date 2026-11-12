/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (UMBRA LIVE) The screen on a computer: Patrick F. Kellogg's screen mirror (screen_mirror/, MIT) and its viewers,
 * FM-1 Mirror (Mac, Windows, Linux) and FM-1 Mirror + Spyglass (Android). The viewer says hello over USB MIDI about
 * once a second (F0 7D 'F' 'S' 00 F7: usb.c sysex_byte, sm_sysex); while it does, every rectangle that goes to the
 * panel goes to it too (lcd.c lcd_fill / lcd_blit / lcd_blit_stride: umr_fill, umr_blit), run-length coded, at most
 * 70 bytes a SysEx message; 3 s after the last hello nothing more. With nobody asking it costs a test a rectangle.
 * - The SysEx queue (usb.c sx_out_q) goes out before the notes, so the mirror steps aside while notes wait (mo_w)
 *   and leaves some of the queue to the editor: a note out waits a few ms at most, never for the screen.
 * - The queue is topped up while the screen is drawn (umr_rows) and while main.c waits between frames (umr_tick).
 * - Nothing waits for USB: the keys are read between frames (ui_input), so a frame held up would hold up the notes
 *   played on them. A row that finds the buffer full is lost to the viewer and marked (umr_lost); the rows after it in
 *   the same rectangle too.
 * - Marked rows are mended in repaint rounds (umr_draw): a frame drawn whole (ui.force) of which only some marked rows
 *   go to the viewer, as many as the buffer has room for (each row's bytes counted as the last whole frame drew it,
 *   umr_rc): first those not drawn lately (a word that stays: no ordinary frame will send it again), then the others,
 *   from where the last round stopped on. A row whose every piece went is mended.
 * - A page change that lost rows is caught up at once: rounds UMR_FAST apart, the frames between held back (their rows
 *   marked), till nothing is marked or UMR_CATCH. After that a round comes only for a marked row not drawn lately,
 *   once nothing has been lost for UMR_QUIET (every UMR_LONG while a plate or the meters keep the buffer full), with a
 *   third of the buffer free: what moves (a meter, a plate) is the ordinary frames' to send. Rounds are UMR_GAP apart
 *   at least, further while they find nothing of theirs drawn (a NOTICE over the page, the screen off).
 * - A round is a frame like any other on the panel: the rolling digits, the roll's glide and the panels' granules go
 *   on as they were (ui.c UI_SNAP, ui_graph.c graph_drift), so the rounds don't change what the panel shows.
 * - A round skips the other rows, so what changed in them in that frame doesn't reach the viewer: every frame drawn
 *   whole hashes each row as it draws it (umr_rh; an ordinary frame too, a row it draws whole across: the header),
 *   and a skipped row whose hash moved, with nothing drawn in a part of it since, is marked.
 * - In ordinary frames a picture redrawn every frame (a moving plate) reaches the viewer as a rolling refresh: after a
 *   loss the next frames begin where it stopped (umr_from; the rows above it skipped, and marked), not at the top again.
 * - A viewer joining (sm_take_full_request) has every row marked, and the rounds catch up.
 * The sound's name (F0 7D 'F' 'S' 02 ..: "T1 DRUM-X 909 CLUB") goes too, for the viewer's spyglass. */
/* (its own queue: felucca.c SM_BUFFER, 3 KB in the pool) */
#ifndef UMR_SXQ_FREE
#define UMR_SXQ_FREE 32u                                /* packets of usb.c's SysEx queue the mirror leaves to the editor */
#endif
#define UMR_QUIET 300u                                  /* ms without a row lost before the rounds begin */
#define UMR_GAP 500u                                    /* ms between rounds at least (each is a frame drawn whole: the
                                                         * panel's ~77 ms at 12 MHz, the engines' code out of the cache) */
#define UMR_FAST 60u                                    /* .. catching up after a page change */
#define UMR_CATCH 1500u                                 /* catching up: the ordinary frames held this long at most */
#define UMR_LONG 2000u                                  /* rows lost all the time (a plate moving, meters): a round for
                                                         * the still ones this often all the same */
#define UMR_SET(b, y) ((b)[(y) >> 5] |= 1u << ((y) & 31u))
#define UMR_CLR(b, y) ((b)[(y) >> 5] &= ~(1u << ((y) & 31u)))
#define UMR_HAS(b, y) ((b)[(y) >> 5] >> ((y) & 31u) & 1u)
static uint32_t umr_lost[8];                            /* rows the viewer may show wrong (bit y): mended in rounds */
static uint32_t umr_sel[8], umr_hit[8], umr_bad[8];     /* a round's rows; of them, a piece sent / a piece lost */
static uint32_t umr_drew[8];                            /* rows drawn in part in ordinary frames since the last whole one */
static uint32_t umr_touch[8], umr_tprev[8], umr_tlate[8];   /* rows drawn in ordinary frames this second, the last, either */
static uint8_t umr_rh[240] __attribute__((section(".pool")));   /* each row's hash as the last whole frame drew it, */
static uint8_t umr_rn[240] __attribute__((section(".pool")));   /* and as this one does; */
static uint8_t umr_rc[240] __attribute__((section(".pool")));   /* its bytes on the wire then (8s; 0: not drawn) */
static uint8_t umr_round_on;                            /* this frame a round */
static uint8_t umr_whole;                               /* this frame drawn whole (ui.force as it began): its rows hashed */
static uint8_t umr_next;                                /* the row the next round begins at */
static uint8_t umr_idle;                                /* rounds in a row that found nothing of theirs drawn */
static uint8_t umr_full;                                /* the buffer full since umr_from was set: later losses don't move it */
static uint8_t umr_hold;                                /* the ordinary frame's rows held back (marked, not sent) */
static uint8_t umr_catch;                               /* catching up after a page change */
static uint8_t umr_lostf;                               /* a row lost (or held) in this frame */
static uint32_t umr_from;                               /* ordinary frames: rows above it skipped (0: none) */
static uint32_t umr_lost_ms, umr_round_ms, umr_from_ms, umr_name_ms, umr_catch_ms, umr_touch_ms;
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
/* row r of a rectangle: its hash, and in a whole frame its bytes on the wire (runs of 4 bytes, 15 a message of 11 more) */
static uint32_t umr_hash(uint32_t x, uint32_t w, const uint16_t *p, uint16_t c, uint32_t *bytes)
{
    uint32_t hs = 2166136261u ^ x ^ w << 8, i, runs = 1;
    if (p) {
        for (i = 0; i < w; i++) {
            hs = (hs ^ p[i]) * 16777619u;
            runs += i && p[i] != p[i - 1];
        }
    } else {
        hs = (hs ^ c) * 16777619u;
    }
    runs += w / 127u;
    *bytes = runs * 4u + (runs + 14u) / 15u * 11u;
    hs ^= hs >> 16;
    return (hs ^ hs >> 8) & 255u;
}
/* rows y .. y + h - 1, x .. x + w - 1 as they go to the panel: px (byte-swapped, stride px a row), or c (RGB565) */
static void umr_rows(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint16_t c)
{
    uint32_t r, i, yy, made = 0, lost = 0, b = 0, ph;
    if (!sm_on() || !w || !h || x >= 240u || y >= 240u)
        return;
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    if (!umr_round_on && umr_from && fm1_ms - umr_from_ms > 200u)
        umr_from = 0;                                   /* (nothing below drawn since: the top's turn again) */
    for (r = 0; r < h; r++) {
        yy = y + r;
        ph = umr_whole || w == 240u ? umr_hash(x, w, px ? px + r * stride : 0, c, &b) : 0u;
        if (umr_whole) {                                /* (each row's hash, and its bytes) */
            umr_rn[yy] = (uint8_t)(umr_rn[yy] * 31u + ph);
            b = umr_rc[yy] + (b + 7u) / 8u;
            umr_rc[yy] = (uint8_t)(b < 255u ? b : 255u);
        } else if (w < 240u) {
            UMR_SET(umr_drew, yy);                      /* (a part of it: what the row is now, not known) */
            UMR_SET(umr_touch, yy);
        } else {
            umr_rh[yy] = (uint8_t)ph;                   /* (as a whole frame drawing it in one piece would hash it) */
            UMR_CLR(umr_drew, yy);
            UMR_SET(umr_touch, yy);
        }
        if (umr_round_on && !UMR_HAS(umr_sel, yy))
            continue;                                   /* (a round: its rows only) */
        if (umr_hold && !umr_round_on) {                /* (held back: these rows, the rounds' business) */
            UMR_SET(umr_lost, yy);
            umr_lostf = 1;
            continue;
        }
        if (!umr_round_on && umr_from && yy >= umr_from)
            umr_from_ms = fm1_ms;                       /* (drawn below it: its turn goes on, sent or not) */
        if (lost || (!umr_round_on && yy < umr_from)) {   /* (after a loss, or the rolling refresh's turn below) */
            if (umr_round_on) {
                UMR_SET(umr_bad, yy);
            } else {
                UMR_SET(umr_lost, yy);
                umr_lostf = 1;
            }
            continue;
        }
        if (px) {
            for (i = 0; i < w; i++) {
                uint32_t v = px[r * stride + i];
                umr_row[i] = (uint16_t)(v >> 8 | v << 8);
            }
        } else if (!made) {                             /* (a fill: its row made once) */
            for (i = 0; i < w; i++)
                umr_row[i] = c;
            made = 1;
        }
        if (!sm_rows(umr_row, x, yy, w, 1u)) {
            sm_poll(fm1_ms, umr_send);                  /* (room made, as far as the queue takes it) */
            if (!sm_rows(umr_row, x, yy, w, 1u)) {
                lost = 1;
                if (umr_round_on) {
                    UMR_SET(umr_bad, yy);
                    continue;
                }
                UMR_SET(umr_lost, yy);
                umr_lostf = 1;
                umr_lost_ms = fm1_ms;
                if (!umr_full) {                        /* (the first row that didn't fit: the next frames begin here) */
                    umr_full = 1;
                    umr_from = yy;
                    umr_from_ms = fm1_ms;
                }
                continue;
            }
        }
        if (umr_round_on) {
            UMR_SET(umr_hit, yy);                       /* (a round's row: a piece of it sent) */
            continue;
        }
        umr_full = 0;
        if (w == 240u)
            UMR_CLR(umr_lost, yy);                      /* (drawn whole across, and sent: the viewer has it) */
        if (yy >= 239u)
            umr_from = 0;                               /* (the bottom reached: from the top again) */
    }
    sm_poll(fm1_ms, umr_send);
}
static void umr_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { umr_rows(x, y, w, h, 0, 0, c); }
static void umr_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride)
{
    umr_rows(x, y, w, h, px, stride, 0);
}
static int umr_round(void) { return umr_round_on; }   /* (ui.c UI_SNAP, ui_graph.c graph_drift) */
/* main.c's ui_draw: a repaint round when one is due (the frame drawn whole, its rows only to the viewer), and its rows
 * taken stock of after */
static void umr_draw(void)
{
    uint32_t now = fm1_ms, y = 240u, i, seen = 0, bad = 240u, sum = 0, room;
    uint32_t gap = umr_catch ? UMR_FAST : UMR_GAP << (umr_idle < 4u ? umr_idle : 4u), still = 0;
    if (!sm_on()) {                                     /* (nobody asking: the frame alone) */
        umr_catch = umr_hold = 0;
        ui_draw();
        return;
    }
    if (now - umr_touch_ms >= 1000u) {                  /* (drawn lately: in this second or the last) */
        umr_touch_ms = now;
        for (i = 0; i < 8u; i++) {
            umr_tprev[i] = umr_touch[i];
            umr_touch[i] = 0;
        }
    }
    for (i = 0; i < 8u; i++) {
        umr_tlate[i] = umr_touch[i] | umr_tprev[i];
        still |= umr_lost[i] & ~umr_tlate[i];           /* (marked, and not drawn lately: no frame will mend it) */
    }
    for (i = 0; i < 240u && y == 240u; i++)             /* (the first row marked from umr_next on) */
        if (UMR_HAS(umr_lost, (umr_next + i) % 240u))
            y = (umr_next + i) % 240u;
    if (y >= 240u || now - umr_catch_ms >= UMR_CATCH)
        umr_catch = 0;                                  /* (caught up, or long enough: the ordinary frames again) */
    umr_hold = umr_catch;                               /* (catching up: the ordinary frames held back) */
    room = SM_BUFFER - (sm_length - sm_sent);
    room = room > 128u ? room - 128u : 0u;
    if (y < 240u && !ui.force && now - umr_round_ms >= gap &&
        (umr_catch || (still && (now - umr_lost_ms >= UMR_QUIET || now - umr_round_ms >= UMR_LONG))) &&
        (umr_catch ? sm_length == sm_sent : room >= SM_BUFFER / 3u)) {   /* (catching up: the buffer empty first; else
                                                         * the room the ordinary frames leave, a third of it at least) */
        {
            uint32_t pass, k, last = 239u;
            for (pass = 0; pass < 2u; pass++)           /* (the marked rows from there, as many as the room takes: first
                                                         * those not drawn since the last round (what stays: a header's
                                                         * words), then the others (what moves, drawn again anyway)) */
                for (k = 0; k < 240u; k++) {
                    uint32_t r = (y + k) % 240u, b = umr_rc[r] ? umr_rc[r] * 8u : 512u;
                    if (!UMR_HAS(umr_lost, r) || UMR_HAS(umr_sel, r) || UMR_HAS(umr_tlate, r) != pass)
                        continue;
                    if (sum && sum + b > room) {
                        pass = 2u;
                        break;
                    }
                    sum += b;
                    UMR_SET(umr_sel, r);
                    last = r;
                }
            umr_next = (uint8_t)(last < 239u ? last + 1u : 0u);
            umr_round_on = 1;
            umr_round_ms = now;
            ui.force = 1;                               /* (no other cause this frame: !ui.force above) */
        }
    }
    umr_whole = (uint8_t)ui.force;
    if (umr_whole)
        for (y = 0; y < 240u; y++)
            umr_rn[y] = umr_rc[y] = 0;
    umr_lostf = 0;
    ui_draw();
    if (umr_whole && !umr_round_on && umr_lostf) {      /* (a page drawn whole, and rows of it lost: catch up) */
        umr_catch = 1;
        umr_catch_ms = now;
    }
    if (umr_whole) {                                    /* (a row skipped that changed, with nothing drawn in a part of it
                                                         * since the last whole frame: changed in this one, not sent) */
        for (y = 0; y < 240u; y++) {
            if (umr_round_on && !UMR_HAS(umr_sel, y) && umr_rn[y] != umr_rh[y] && !UMR_HAS(umr_drew, y))
                UMR_SET(umr_lost, y);
            umr_rh[y] = umr_rn[y];
        }
        for (i = 0; i < 8u; i++)
            umr_drew[i] = 0;
        umr_whole = 0;
    }
    if (!umr_round_on)
        return;
    for (y = 0; y < 240u; y++) {
        if (!UMR_HAS(umr_sel, y))
            continue;
        if (UMR_HAS(umr_bad, y)) {
            seen = 1;
            if (bad == 240u)
                bad = y;                                /* (the next round begins here) */
        } else if (UMR_HAS(umr_hit, y)) {
            UMR_CLR(umr_lost, y);                       /* (every piece of it went: mended) */
            seen = 1;
        }
    }
    for (i = 0; i < 8u; i++)
        umr_sel[i] = umr_hit[i] = umr_bad[i] = 0;
    if (bad < 240u)
        umr_next = (uint8_t)bad;
    umr_idle = seen ? 0u : (uint8_t)(umr_idle + 1u);
    umr_round_on = 0;
}
/* main.c's wait between frames, as often as it likes: the queue topped up as USB empties it (once a millisecond: a pass
 * a frame alone left the 672 bytes the queue takes for 15 ms, ~45 kB/s however fast the host reads) */
static void umr_tick(void)
{
    static uint32_t ms;
    if (ms != fm1_ms && sm_on()) {
        ms = fm1_ms;
        sm_poll(fm1_ms, umr_send);
    }
}
/* every main-loop pass: what is queued out; a viewer joining; the sound's name (every 100 ms) */
static void umr_poll(void)
{
    uint32_t i;
    sm_poll(fm1_ms, umr_send);
    if (sm_take_full_request()) {                       /* (a new viewer: every row) */
        for (i = 0; i < 8u; i++)
            umr_lost[i] = i < 7u ? ~0u : 0xFFFFu;       /* (rows 0 .. 239) */
        umr_catch = 1;
        umr_catch_ms = fm1_ms;
        umr_idle = 0;
        umr_from = 0;
        umr_full = 0;
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
