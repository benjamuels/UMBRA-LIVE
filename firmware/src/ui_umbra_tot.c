/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, after the plates) TOTALITY: the plate alone over the whole screen, a mode of its own.
 * In: HOME tapped on HOME (its button lit there; on GEN too, which closes), or PLAY on SAVE > SONG > TIMELINE or SAVE >
 * PROJECT > ARRANGE (the song starts, and TOTALITY with it). Out: HOME again (back to the page it came from), or a button
 * that goes to a page (a section's, SAVE, SEQ, HOME held for the menu, a dialog, GEN: umt_check). In it:
 *   SELECT the plates, round, a detent a plate; the new one spreads from the middle over the old (DISSOLVE: gfx.c dc_gate,
 *   the cells not yet reached keep what they showed; 0 a cut .. 99 four seconds, as HOME > ECO > NOTES set it, gone);
 *   KNOB 1..4 the macros M1..M4 (mod.c macro_v, the project's), sent on to the sound and the picture as HOME > UMBRA's
 *   MACROS - AUDIO and MACROS - VISUALS say (ui_umbra_mac.c; at first the plate's own three and LIGHT);
 *   FX tapped: KNOB 1..4 the visual effects instead (MIRROR MOSAIC GLITCH SOLAR), FX lit; tapped again: the plate's;
 *   SCL tapped: the selected track's being over the plate, in the middle (HOME > BEING's: its engine's own creature,
 *   ui_engvis.c, the snail, the mouth, the kit ..; no floors under them; else its being; ALGORITHM another track's),
 *   fading in over half a second, SCL lit; tapped again: it fades out (as it was left when TOTALITY is entered again);
 *   the keys, PLAY, REC, OCT, ALGORITHM, PRESETS and a layer held as on HOME (the layer's map over it while it shows);
 *   PLAY plays the song again when TOTALITY came from TIMELINE or ARRANGE;
 *   every LED dark but the keys of the notes that sound (ui_input.c ui_leds), FX while its knobs are the effects, REC
 *   while armed.
 * A plate chosen, a knob turned, FX tapped: a word at the foot for 1.5 s; over the plate only that and the being.
 * Under it the page it came from stays (ui.page); HOME's state stands for it meanwhile (ui.home), so the keys play and
 * OCT shifts as on HOME. Built for size, as the main loop is (tools/size_fns.py) */
static int name_on(void);                               /* (ui_name.c) */
static int32_t accel(uint32_t role, int32_t s, int32_t range);   /* (ui_input.c) */
static struct {
    uint8_t on, fx, song, home, info;                   /* TOTALITY; the words at the foot (SEQ); KNOB 1..4 the effects; it came from TIMELINE or
                                                         * ARRANGE (PLAY: the song); the page under it was HOME's */
    uint32_t sig;                                       /* where it was entered (umt_where): elsewhere, it is over */
    uint32_t say_ms;                                    /* the word at the foot: since when (0: none) */
    char say[32];
    uint32_t qr, gms;                                   /* GLITCH: the notes read, its clock */
    int32_t ge;                                         /* .. its energy (Q8) */
    uint32_t fms;                                       /* DISSOLVE: since when (0: none) */
    uint8_t bn;                                         /* the being (SCL): on */
    uint16_t ba;                                        /* .. their strength now, 0..256 (fading: umb_full) */
} umt;
static const char *const UMT_TN[UMT_NT - 3] = {"LIGHT", "SPEED", "REACT", "MOVEMENT", "TRAVEL", "DISSOLVE", "MIRROR", "MOSAIC",
                                                "GLITCH", "SOLAR", "BEING", "INVERT"};   /* the targets past the plate's three */
#define UMT_FXN (UMT_TN + UMT_MIR - 3)                  /* the effects (FX): MIRROR MOSAIC GLITCH SOLAR */
static const char *const UMT_MIRN[4] = {"OFF", "L-R", "T-B", "QUAD"};
static const char *umt_tname(uint32_t pl, uint32_t t)   /* target t's name on plate pl */
{
    return t < 3u ? umb_cname(pl, t) : UMT_TN[(t - 3u) % (UMT_NT - 3u)];
}
static uint32_t umt_where(void)
{
    return (uint32_t)ui.home | (uint32_t)ui.page << 1 | (uint32_t)(ui.hlv & 1u) << 9 | (uint32_t)(ui.hset[0] & 7u) << 10 |
           (uint32_t)(ui.hset[1] & 7u) << 13 | (uint32_t)(ui.confirm != 0) << 16 | (uint32_t)(ui.menu != 0) << 17;
}
/* TOTALITY drawn now (not under the menu, a dialog, NAME, a layer's map, the MACRO layer's cards) */
static int umb_bare(void) { return umt.on; }
static int umt_shown(void) { return umt.on && !ui.menu && !ui.confirm && !ui.layer && !mac_latch && !name_on(); }
/* (the user, 2026-11-09) a layer held in TOTALITY whose map is not drawn: FX, SCL, LFO (MACRO, latched too), EDIT, GLO. The
 * keys, the knobs and the LEDs are the layer's as ever (ui_input.c); the screen stays the plate's (ui_draw.c) */
static int umt_ly(void)
{
    return umt.on && (mac_latch || ui.layer == LAYER_FX || ui.layer == LAYER_SCL || ui.layer == LAYER_MACRO ||
                      ui.layer == LAYER_EDIT || ui.layer == LAYER_GLO);
}
static void umt_say(const char *a, const char *b)
{
    if (!umt.info)                                      /* (the words are off until SEQ is tapped) */
        return;
    str_cpy(umt.say, a, sizeof umt.say);
    if (b) {
        str_cpy(umt.say + str_len(umt.say), " ", sizeof umt.say - str_len(umt.say));
        str_cpy(umt.say + str_len(umt.say), b, sizeof umt.say - str_len(umt.say));
    }
    umt.say_ms = fm1_ms | 1u;
}
static void umt_say_knobs(void) { umt_say(umt.fx ? "MIRROR MOSAIC GLITCH SOLAR" : "M1 M2 M3 M4", 0); }   /* KNOB 1..4's */
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
    umt.info = 0;
    umt.qr = umb_q.w;                                   /* (GLITCH: from the next note, not the ones before) */
    umt.ge = 0;
    umt.sig = umt_where();
    umt_say(umb_pname(umb_pl()), 0);
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
static void umt_select(int32_t s)                       /* SELECT: the next plate there, round (the bank's after the six) */
{
    uint32_t pl = umb_pl(), k;
    for (k = 0; k < UMB_NPLT; k++)
        if (umb_has(pl = (pl + (s > 0 ? 1u : UMB_NPLT - 1u)) % UMB_NPLT))
            break;
    umb_plate = (uint8_t)(0x80u | pl);
    umv_dirty = 1;
    umv_ms = fm1_ms;
    umt.fms = umt.on ? fm1_ms | 1u : 0u;                /* (DISSOLVE: from here) */
    umt_say(umb_pname(pl), 0);
}
static void umt_seq_tap(void)                           /* SEQ: the words at the foot on, or off again (lit while on) */
{
    umt.info ^= 1u;
    umt.say_ms = 0;
    umt_say("INFO", 0);
}
static void umt_scl_tap(void)                           /* SCL: the being over the plate, or gone again (lit while on; */
{                                                       /* they fade in or out over half a second: umb_full) */
    umt.bn ^= 1u;
    umt_say("BEING", umt.bn ? "ON" : "OFF");
}
static void umt_fx_tap(void)                            /* FX: KNOB 1..4 the effects, or the plate's again */
{
    umt.fx ^= 1u;
    umt_say_knobs();
}
/* KNOB k turned d (accelerated; MIRROR's a detent a step): its effect, or its macro (as the MACRO layer turns them) */
static void umt_knob(uint32_t k, int32_t d)
{
    uint32_t v;
    char b[4], m[3] = {'M', (char)('1' + k), 0};
    const char *nm = m, *vs = b;
    if (umt.fx) {
        umt_put(4u + k, (int32_t)umt_raw(4u + k) + d);
        v = umt_raw(4u + k);
        nm = UMT_FXN[k];
        if (!k)
            vs = UMT_MIRN[v & 3u];
        else if (!v)
            vs = "OFF";
    } else {
        macro_v[k & 3u] = (uint8_t)clamp(macro_v[k & 3u] + d, 0, 127);
        v = macro_v[k & 3u] * 100u / 127u;
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
 * each its middle's colour), GLITCH (bands of rows torn sideways, a new tear each frame while its energy lasts), INVERT
 * (routed only, LIVE VIZ CTRLS: the whole picture negative while it is 50 or more: a hit a flash as long as it is hard,
 * an LFO a strobe; a hard switch, so the panel is sent the whole screen only as it turns) */
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
    if (umq_o[UMT_INV] > 50u)                           /* INVERT (its value + 1: 50 or more) */
        for (x = 0; x < n; x++)
            px[x] = (uint16_t)~px[x];
}
static void being_fig(uint32_t c, int32_t cx, int32_t cy, int32_t k);   /* (ui_lzhome.c) */
/* the being (SCL) over the strip at screen row y, at strength a (0..256): the selected track's, as HOME > BEING draws it
 * (an eighth larger, no labels) */
static void umt_being(uint32_t y, uint32_t a)
{
    og_dim = (uint16_t)(256u - a);
    cv_oy = -(int32_t)y;
    vx_small = vx_bare = 1;
    being_fig(song.sel % NTRK, 120, 120, 18);
    vx_small = vx_bare = 0;
    cv_oy = 0;
    og_dim = 0;
}
/* TOTALITY drawn: the plate over the whole screen (no header, tabs, knobs or footer: ui_draw.c ui_draw_body draws nothing
 * else) in two strips, the effects on them, sent where they changed (gfx.c cv_blit_cells); under LOAD LITE / ECO every
 * 50 / 70 ms, as HUD's HOME */
static void umb_full(void)
{
    static uint32_t ms;
    uint32_t y, mir = umt_get(4u), f = umt_get(0u) * umt_get(0u) * 4000u / 9801u, el = fm1_ms - umt.fms, ba = fm1_ms - ms, bs;
    if (sys_load && !ui.force && ba < (sys_load == LOAD_ECO ? 70u : 50u))
        return;
    ms = fm1_ms;
    ba = ba >= 500u ? 256u : ba * 256u / 500u;          /* the being: its fade, half a second (a turn back from */
    ba = umt.ba = (uint16_t)(umt.bn ? (umt.ba + ba > 256u ? 256u : umt.ba + ba) : (umt.ba > ba ? umt.ba - ba : 0u));   /* where it was) */
    if (umt.fms && el < f)                              /* DISSOLVE: the cells let through so far (gfx.c) */
        dc_gate = (uint16_t)(el * 256u / f);
    else
        umt.fms = 0;
    umt_glitch_tick();
    bs = umq_o[UMT_BEING] ? ba * (umq_o[UMT_BEING] - 1u) / 99u : ba;   /* (MACROS - VISUALS: its strength routed) */
    if (ba)                                             /* the being: its track's level, once a frame */
        (void)eco_step();
    if (umt.say_ms && fm1_ms - umt.say_ms > 1500u)
        umt.say_ms = 0;
    dc_frame(ui.force);
    umb.bare = 1;
    for (y = 0; y < 240u; y += 120u) {
        cv_begin(240, 120, T_BG);
        umb_backdrop(0, (mir & 2u) ? 0 : (int32_t)y);
#if UMB_LAYERS > 1
        if (umb_ovl)                                    /* (the emulator's test of two at once: FILAMENT's light over it) */
            umb_lay_draw(1u, 2u, &umb_plate_filament, 0, (mir & 2u) ? 0 : (int32_t)y, UMB_DRAW_ADD);
#endif
        umt_fx(y, mir);
        if (ba && bs)                                   /* (over the effects: the being stays whole) */
            umt_being(y, bs);
        if (y && umt.say_ms)                            /* the word at the foot */
            cv_text(8, 102, &AF_S, umt.say, T_MID);
        cv_blit_cells(0, y);
    }
    umb.bare = 0;
    dc_gate = 256u;
}
