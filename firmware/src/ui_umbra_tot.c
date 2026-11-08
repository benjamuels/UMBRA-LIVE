/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, after the plates) TOTALITY: the plate alone over the whole screen, a mode of its own.
 * In: HOME tapped on HOME (its button lit there; on GEN too, which closes), or PLAY on SAVE > SONG > TIMELINE or SAVE >
 * PROJECT > ARRANGE (the song starts, and TOTALITY with it). Out: HOME again (back to the page it came from), or a button
 * that goes to a page (a section's, SAVE, SEQ, HOME held for the menu, a dialog, GEN: umt_check). In it:
 *   SELECT the plates, round, a detent a plate;
 *   KNOB 1..4 what HOME > ECO > ROUTING sends them to (UMT_*: the plate's own two, LIGHT, SPEED, REACT, MOVEMENT,
 *   TRAVEL);
 *   FX tapped: KNOB 1..4 the visual effects instead (MIRROR MOSAIC GLITCH SOLAR), FX lit; tapped again: the plate's;
 *   the keys, PLAY, REC, OCT, ALGORITHM, PRESETS and a layer held as on HOME (the layer's map over it while it shows);
 *   PLAY plays the song again when TOTALITY came from TIMELINE or ARRANGE;
 *   every LED dark but the keys of the notes that sound (ui_input.c ui_leds), FX while its knobs are the effects, REC
 *   while armed.
 * A plate chosen, a knob turned, FX tapped: a word at the foot for 1.5 s, the only thing ever over the plate.
 * Under it the page it came from stays (ui.page); HOME's state stands for it meanwhile (ui.home), so the keys play and
 * OCT shifts as on HOME. Built for size, as the main loop is (tools/size_fns.py) */
static int name_on(void);                               /* (ui_name.c) */
static int32_t accel(uint32_t role, int32_t s, int32_t range);   /* (ui_input.c) */
static struct {
    uint8_t on, fx, song, home;                         /* TOTALITY; KNOB 1..4 the effects; it came from TIMELINE or
                                                         * ARRANGE (PLAY: the song); the page under it was HOME's */
    uint32_t sig;                                       /* where it was entered (umt_where): elsewhere, it is over */
    uint32_t say_ms;                                    /* the word at the foot: since when (0: none) */
    char say[32];
    uint32_t qr, gms;                                   /* GLITCH: the notes read, its clock */
    int32_t ge;                                         /* .. its energy (Q8) */
} umt;
static const char *const UMT_FXN[4] = {"MIRROR", "MOSAIC", "GLITCH", "SOLAR"};
static const char *const UMT_MIR[4] = {"OFF", "L-R", "T-B", "QUAD"};
static const char *umt_tname(uint32_t pl, uint32_t t)   /* routed target t's name on plate pl */
{
    static const char *const N[5] = {"LIGHT", "SPEED", "REACT", "MOVEMENT", "TRAVEL"};
    return t < 2u ? UMV_N[pl % UMP_NPL][t] : N[(t - 2u) % 5u];
}
static uint32_t umt_where(void)
{
    return (uint32_t)ui.home | (uint32_t)ui.page << 1 | (uint32_t)(ui.hlv & 1u) << 9 | (uint32_t)(ui.hset[0] & 7u) << 10 |
           (uint32_t)(ui.hset[1] & 7u) << 13 | (uint32_t)(ui.confirm != 0) << 16 | (uint32_t)(ui.menu != 0) << 17;
}
/* TOTALITY drawn now (not under the menu, a dialog, NAME, a layer's map, the MACRO layer's cards) */
static int umb_bare(void) { return umt.on; }
static int umt_shown(void) { return umt.on && !ui.menu && !ui.confirm && !ui.layer && !mac_latch && !name_on(); }
static void umt_say(const char *a, const char *b)
{
    str_cpy(umt.say, a, sizeof umt.say);
    if (b) {
        str_cpy(umt.say + str_len(umt.say), " ", sizeof umt.say - str_len(umt.say));
        str_cpy(umt.say + str_len(umt.say), b, sizeof umt.say - str_len(umt.say));
    }
    umt.say_ms = fm1_ms | 1u;
}
static void umt_say_knobs(void)                         /* what KNOB 1..4 turn now */
{
    char b[32];
    uint32_t k, pl = umb_pl();
    b[0] = 0;
    for (k = 0; k < 4u; k++) {
        if (k)
            str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
        str_cpy(b + str_len(b), umt.fx ? UMT_FXN[k] : umt_tname(pl, umt_get(k)), sizeof b - str_len(b));
    }
    umt_say(b, 0);
}
static void umt_enter(uint32_t song)
{
    if (gen_on())
        nav_go(SEC_HOME, 0, 0);                         /* (GEN closes: ECO under it) */
    umt.home = ui.home;
    umt.song = (uint8_t)song;
    if (!ui.home)
        go_home();
    umt.on = 1;
    umt.fx = 0;
    umt.qr = umb_q.w;                                   /* (GLITCH: from the next note, not the ones before) */
    umt.ge = 0;
    umt.sig = umt_where();
    umt_say(UMV_PL[umb_pl()], 0);
    ui.force = 1;
}
static void umt_leave(void)                             /* HOME: back to the page under it */
{
    if (!umt.home)
        ui.home = 0;
    umt.on = 0;
    ui.force = 1;
}
/* each input pass, first: a page chosen since it was entered (a button, the menu, a dialog): over, that page shown */
static void umt_check(void)
{
    if (umt.on && umt_where() != umt.sig) {
        umt.on = 0;
        ui.force = 1;
    }
}
/* HOME tapped: TOTALITY left; on HOME, entered. 0: HOME as ever (from elsewhere: HOME's page last shown) */
static int umt_home(void)
{
    if (umt.on)
        umt_leave();
    else if (nav_sec() == SEC_HOME)
        umt_enter(0);
    else
        return 0;
    return 1;
}
static void umt_select(int32_t s)                       /* SELECT: the next plate, round */
{
    uint32_t pl = (umb_pl() + (s > 0 ? 1u : UMP_NPL - 1u)) % UMP_NPL;
    umb_plate = (uint8_t)(0x80u | pl);
    umv_dirty = 1;
    umv_ms = fm1_ms;
    umt_say(UMV_PL[pl], 0);
}
static void umt_fx_tap(void)                            /* FX: KNOB 1..4 the effects, or the plate's again */
{
    umt.fx ^= 1u;
    umt_say_knobs();
}
/* KNOB k turned d (accelerated; MIRROR's a detent a step): its effect, or the target ROUTING gives it */
static void umt_knob(uint32_t k, int32_t d)
{
    uint32_t pl = umb_pl(), t, v;
    char b[4];
    const char *nm, *vs = b;
    if (umt.fx) {
        umt_put(4u + k, (int32_t)umt_get(4u + k) + d);
        v = umt_get(4u + k);
        nm = UMT_FXN[k];
        if (!k)
            vs = UMT_MIR[v & 3u];
        else if (!v)
            vs = "OFF";
    } else {
        t = umt_get(k);
        nm = umt_tname(pl, t);
        if (t <= UMT_LIGHT) {
            umv_put(pl, t, (uint32_t)umb_clamp((int32_t)umv(pl, t) + d, 0, 99));
            v = umv(pl, t);
        } else {                                        /* (SPEED [8], REACT [9], MOVEMENT [10]: a detent a step,
                                                         * TRAVEL [11]) */
            umt_put(t + 5u, (int32_t)umt_get(t + 5u) + (t == UMT_MOVE ? (d > 0) - (d < 0) : d));
            v = umt_get(t + 5u);
            if (t == UMT_MOVE)
                vs = UMN_MV[v & 3u];
        }
    }
    fmt_int(b, (int32_t)v);
    umt_say(nm, vs);
}

/* ------------------------------------------------------------------------------------------ the effects --- */
static uint32_t umt_rnd(uint32_t *s)                    /* (mulberry32, its own seed: the two strips agree) */
{
    uint32_t t = *s += 0x6D2B79F5u;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
/* once a frame: GLITCH's energy, from the notes since (REACT's), falling off in ~0.2 s */
static void umt_glitch_tick(void)
{
    uint32_t g = umt_get(6u), dt = fm1_ms - umt.gms;
    umt.gms = fm1_ms;
    umt.ge -= umt.ge * (int32_t)(dt > 200u ? 200u : dt) / 200;
    if (umb_q.w - umt.qr > UMB_NQ)
        umt.qr = umb_q.w - UMB_NQ;
    for (; umt.qr != umb_q.w; umt.qr++) {
        int32_t v = (int32_t)umb_vel(umb_q.vel[umt.qr % UMB_NQ]) * 2 * (int32_t)g / 99;
        if (v > umt.ge)
            umt.ge = v;
    }
}
/* the canvas (the strip at screen row y), the plate drawn in it: MIRROR (L-R the left half again, turned; T-B: the top
 * drawn here again, upside down), SOLAR (the pixels brighter than its edge, negative), MOSAIC (blocks of 2 .. 16 px,
 * each its middle's colour), GLITCH (bands of rows torn sideways, a new tear each frame while its energy lasts) */
static void umt_fx(uint32_t y, uint32_t mir)
{
    uint16_t *px = cv_px, t[240];
    uint32_t x, r, mo = umt_get(5u), so = umt_get(7u), n = cv_w * cv_h;
    if (cv_w != 240u)
        return;
    if ((mir & 2u) && y)
        for (r = 0; r < cv_h / 2u; r++) {
            memcpy(t, px + r * 240u, sizeof t);
            memcpy(px + r * 240u, px + (cv_h - 1u - r) * 240u, sizeof t);
            memcpy(px + (cv_h - 1u - r) * 240u, t, sizeof t);
        }
    if (mir & 1u)
        for (r = 0; r < cv_h; r++)
            for (x = 0; x < 120u; x++)
                px[r * 240u + 239u - x] = px[r * 240u + x];
    if (so) {                                           /* (the light: R 5 + G 5 + B 2 of 565, 0..532) */
        uint32_t th = (99u - so) * 532u / 99u, i;
        for (i = 0; i < n; i++) {
            uint32_t q = swap16(px[i]);
            if ((q >> 11) * 5u + ((q >> 5) & 63u) * 5u + (q & 31u) * 2u > th)
                px[i] = (uint16_t)~px[i];
        }
    }
    if (mo) {                                           /* (the blocks fixed to the screen) */
        int32_t b = 2 + ((int32_t)mo - 1) * 14 / 98, r0, k;
        for (r0 = (int32_t)y / b * b - (int32_t)y; r0 < (int32_t)cv_h; r0 += b) {
            const uint16_t *src = px + umb_clamp(r0 + b / 2, 0, (int32_t)cv_h - 1) * 240;
            for (x = 0; x < 240u; x++) {
                uint32_t sx = x / (uint32_t)b * (uint32_t)b + (uint32_t)b / 2u;
                t[x] = src[sx < 240u ? sx : 239u];
            }
            for (k = r0 < 0 ? 0 : r0; k < r0 + b && k < (int32_t)cv_h; k++)
                memcpy(px + k * 240, t, sizeof t);
        }
    }
    if (umt.ge > 12) {
        uint32_t sd = ui.frame * 2654435761u, nb = 1u + (uint32_t)umt.ge / 40u, i, k;
        int32_t S = 2 + umt.ge * 40 / 255;
        for (i = 0; i < nb; i++) {
            uint32_t yy = umt_rnd(&sd) % 240u, hh = 2u + umt_rnd(&sd) % (2u + (uint32_t)umt.ge / 10u);
            int32_t sh = (int32_t)(umt_rnd(&sd) % (uint32_t)(2 * S + 1)) - S + 240;
            for (k = yy > y ? yy : y; k < yy + hh && k < y + cv_h; k++) {
                uint16_t *row = px + (k - y) * 240u;
                for (x = 0; x < 240u; x++)
                    t[x] = row[(x + (uint32_t)sh) % 240u];
                memcpy(row, t, sizeof t);
            }
        }
    }
}
/* TOTALITY drawn: the plate over the whole screen (no header, tabs, knobs or footer: ui_draw.c ui_draw_body draws nothing
 * else) in two strips, the effects on them, sent where they changed (gfx.c cv_blit_cells); under LOAD LITE / ECO every
 * 50 / 70 ms, as HUD's HOME */
static void umb_full(void)
{
    static uint32_t ms;
    uint32_t y, mir = umt_get(4u);
    if (sys_load && !ui.force && fm1_ms - ms < (sys_load == LOAD_ECO ? 70u : 50u))
        return;
    ms = fm1_ms;
    umt_glitch_tick();
    if (umt.say_ms && fm1_ms - umt.say_ms > 1500u)
        umt.say_ms = 0;
    dc_frame(ui.force);
    umb.bare = 1;
    for (y = 0; y < 240u; y += 120u) {
        cv_begin(240, 120, T_BG);
        umb_backdrop(0, (mir & 2u) ? 0 : (int32_t)y);
        umt_fx(y, mir);
        if (y && umt.say_ms)                            /* the word at the foot */
            cv_text(8, 102, &AF_S, umt.say, T_MID);
        cv_blit_cells(0, y);
    }
    umb.bare = 0;
}
