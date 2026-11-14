/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (included by ui_umbra.c, after the plates) TOTALITY: the plate alone over the whole screen, a mode of its own.
 * In: HOME tapped on HOME (its button lit there; on GEN too, which closes), or PLAY on SAVE > SONG > TIMELINE or SAVE >
 * PROJECT > ARRANGE (the song starts, and TOTALITY with it). Out: HOME again (back to the page it came from), or a button
 * that goes to a page (a section's, SAVE, SEQ, HOME held for the menu, a dialog, GEN: umt_check). In it:
 *   PRESETS the MAIN plate (the ground), round, a detent a plate: the old one fades out, then the new one in (the user,
 *   2026-11-14: no more the blocks spreading from the middle; DISSLV how long, both halves: 0 a cut .. 99 four seconds);
 *   while the song plays, a plate chosen here (PRESETS, SELECT, a layer's button) takes the picture over: the rows'
 *   cells let be, GLO lit; GLO tapped hands it back, blinking until the next row (or its next round) brings the
 *   row's plates again, every layer shown (umv_own, ui_umbra_plate.c umv_poll);
 *   PRESETS a ground, never a veil (the user, 2026-11-13);
 *   SELECT the OVER plate (LAYER 1: OFF, then the veils: the light plates and BODIES), round (the user, 2026-11-13);
 *   SEL ENV ARP LFO EDIT as LIVE VIZ CTRLS' BUTTONS page sets them (the user, 2026-11-13; at first SEL LAYER 3, the
 *   figures; ENV OVER; ARP MAIN; LFO and EDIT TAP: the strobe, the blocks): a layer's button tapped hides it or shows it
 *   again (over half a second), held with ALGORITHM turned the next plate there, a detent a plate (umt_algo), lit while
 *   it shows something; a TAP button tapped fires its links, held its layer as ever, lit while its tap lasts (umt_btns);
 *   ALGORITHM the track, as ever (a layer's button held: its plate); with SEQ's words on, its number at the upper left for a second, then fading;
 *   FX + a key (JIANT's punch-in effects) on the picture too: a REPEAT or a STUTTER a strobe at its rate, each flash
 *   another colour; OCT+ / OCT- the picture pushed up / down, round the screen's edge (umb_full, umt_fx);
 *   KNOB 1..4 the macros M1..M4 (mod.c macro_v, the project's), sent on to the sound and the picture as HOME > UMBRA's
 *   LIVE AUDIO CTRLS and LIVE VIZ CTRLS say (ui_umbra_mac.c; at first the plate's first three elements and LIGHT); with
 *   SEQ's words on, what the knob's macro moves listed over its word (umt_mlist);
 *   FX tapped: KNOB 1..4 the visual effects instead (MIRROR MOSAIC GLITCH SOLAR), FX lit; tapped again: the plate's;
 *   LAYER 3 shown with nothing there: BODIES laid on it, the selected track's engine's figure in the middle
 *   (ui_engvis.c; ALGORITHM another track's);
 *   the keys, PLAY, REC, OCT, ALGORITHM, PRESETS and a layer held as on HOME (the layer's map over it while it shows);
 *   PLAY plays the song again when TOTALITY came from TIMELINE or ARRANGE;
 *   LEDs: the keys of the notes that sound (ui_input.c ui_leds), FX while its knobs are the effects, SEQ with the words,
 *   REC while armed, the five above as they say.
 * A plate chosen, a knob turned, FX tapped: a word at the foot for 1.5 s; over the picture only that.
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
    uint32_t fms;                                       /* PRESETS' fade: since when (0: none) */
    uint8_t np;                                         /* .. the ground coming once the old one is out (| 0x80; 0: none) */
    uint8_t lh;                                         /* hidden (bits): the ground, OVER, LAYER 3 (BUTTONS) */
    uint8_t bh;                                         /* SEL ENV ARP LFO EDIT (bits): held past a tap (ALGORITHM turned) */
    uint32_t bt[5];                                     /* .. down since (| 1; 0: up) */
    int8_t roll;                                        /* FX + OCT+ / OCT-: the picture pushed up / down so far (px) */
    uint32_t tms;                                       /* the track's number at the upper left: since when */
    uint16_t ba[3];                                     /* the ground's, OVER's, LAYER 3's light now, 0..256 (fading: umb_full) */
} umt;
#define UMT_FXN(k) (UMX_C[2][k].name)                   /* the effects (FX): MIRROR MOSAIC GLITCH SOLAR */
static const char *const UMT_MIRN[4] = {"OFF", "L-R", "T-B", "QUAD"};
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
static void umt_say_knobs(void) { umt_say(umt.fx ? "MIRROR MOSAIC GLITCH SOLAR" : "KNOB 1 2 3 4", 0); }   /* KNOB 1..4's */
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
static uint32_t umt_fade(void);
static void umt_take(void);
/* the next page for tk, round: 0 (PRESETS) a ground; 1 (SELECT) a veil (a light plate, BODIES) lying nowhere else, for
 * LAYER 1 (OVER), through OFF; 2 the same on LAYER 3. | 4: never OFF (a button held). What is chosen is shown */
BTAPP_FN static void umt_select(int32_t s, uint32_t tk)   /* (in the Bluetooth segment: the app slot is full) */
{
    umx_desc_t d;
    int32_t n = (int32_t)UMX_BODY + 2, pl, k;
    uint32_t t = tk & 3u, w = t == 2u ? 4u : t + 1u;
    umc_load();
    pl = (int32_t)(t ? umx_find(umc.lay[w - 2u]) : umt.np ? umt.np & 0x3Fu : umb_pl());
    if (pl > (int32_t)UMX_BODY)
        pl = -1;                                        /* (none) */
    for (k = 0; k < n; k++) {
        pl = (pl + 1 + (s > 0 ? 1 : n - 1)) % n - 1;    /* (-1: OFF, a layer's alone) */
        if (pl < 0 ? t && tk < 4u : umx_desc((uint32_t)pl, &d) && umx_can((uint32_t)pl, w) &&
                                    (t ? !umx_place((uint32_t)pl) : umx_place((uint32_t)pl) < 2u))
            break;
    }
    if (k == n)
        return;
    umt_take();
    if (!t) {                                           /* (the new ground once the old one has faded out: umb_full) */
        if (!umt.np || fm1_ms - umt.fms >= umt_fade() / 2u)
            umt.fms = fm1_ms | 1u;
        umt.np = (uint8_t)(0x80u | (uint32_t)pl);
    } else if (pl < 0) {
        umc.lay[w - 2u] = 0;
        umc_gen++;
        umc_touch();
    } else
        umx_place_set((uint32_t)pl, w);
    umt.lh &= (uint8_t)~(1u << t);
    umt_say(UMX_BF[t], pl < 0 ? "OFF" : d.name);
}
/* PRESETS' fade, both halves, ms (SCREEN's DISSLV: 0 a cut .. 99 four seconds) */
static uint32_t umt_fade(void) { return umq_o[G_FADE] * umq_o[G_FADE] * 4000u / 9801u; }
static void umt_take(void)                              /* a plate chosen in TOTALITY: the song's rows let it be (GLO lit) */
{
    if (chain.running && chain.config.count)
        umv_own = 1;
}
static void umt_seq_tap(void)                           /* SEQ: the words at the foot on, or off again (lit while on) */
{
    umt.info ^= 1u;
    umt.say_ms = 0;
    umt.tms = fm1_ms;                                   /* (the track's number with them) */
    umt_say("INFO", 0);
}
static void umt_fx_tap(void)                            /* FX: KNOB 1..4 the effects, or the plate's again */
{
    umt.fx ^= 1u;
    umt_say_knobs();
}
/* (the user, 2026-11-13) with SEQ's words on, a macro's knob turned: what it moves, a line each, over the word at the foot
 * for as long as it: in the picture now (the ground's, the layers' and SCREEN's and EFFECTS' links from it: "MOTHS FLIGHT")
 * and in the sound (LIVE AUDIO CTRLS' routes: "T2 TUNE"); none: "NOTHING" */
#define UMT_NML 8u
static struct {
    uint8_t n;
    char ln[UMT_NML][18];
} umt_ml __attribute__((section(".pool")));
static void umt_mlist(uint32_t k)
{
    umx_desc_t d;
    uint32_t i, j, pl, t;
    char *b;
    umt_ml.n = 0;
    umx_layers();
    for (i = 0; i < 6u; i++) {                          /* (the ground, LAYER 1..3, SCREEN, EFFECTS) */
        pl = i ? i < 4u ? umx_lp[i - 1u] : UMX_SCREEN + i - 4u : umb_pl();
        if (!umx_desc(pl, &d))
            continue;
        const umc_rec_t *r = umx_rec(pl);
        for (j = 0; j < UMC_NL && umt_ml.n < UMT_NML; j++)
            if ((r->ls[j] & 31u) == UMQ_M1 + k && (uint32_t)(r->ls[j] >> 5) < d.nctl) {
                b = umt_ml.ln[umt_ml.n++];
                str_cpy(b, d.name, 10);
                str_cpy(b + str_len(b), " ", 2);
                str_cpy(b + str_len(b), d.ctl[r->ls[j] >> 5].name, 7);
            }
    }
    for (t = 0; t < trk_live * 4u && umt_ml.n < UMT_NML; t++) {
        const int16_t *p = &trk[t / 4u].p[P_M1SRC + 3u * (t % 4u)];
        if (p[0] == (int16_t)(MS_M1 + k) && p[1] && p[2]) {
            b = umt_ml.ln[umt_ml.n++];
            b[0] = 'T', b[1] = (char)('1' + t / 4u), b[2] = ' ';
            str_cpy(b + 3, mod_dst_name(&trk[t / 4u], p[1]), 12);
        }
    }
    if (!umt_ml.n)
        str_cpy(umt_ml.ln[umt_ml.n++], "NOTHING", 18);
}
/* KNOB k turned d (accelerated; MIRROR's a detent a step): its effect, or its macro (as the MACRO layer turns them) */
static void umt_knob(uint32_t k, int32_t d)
{
    uint32_t v;
    char b[4], m[7] = {'K', 'N', 'O', 'B', ' ', (char)('1' + k), 0};
    const char *nm = m, *vs = b;
    if (umt.fx) {
        umg_put(G_MIR + k, (int32_t)umg_raw(G_MIR + k) + d);
        v = umg_raw(G_MIR + k);
        nm = UMT_FXN(k);
        if (!k)
            vs = UMT_MIRN[v & 3u];
        else if (!v)
            vs = "OFF";
    } else {
        macro_v[k & 3u] = (uint8_t)clamp(macro_v[k & 3u] + d, 0, 127);
        v = macro_v[k & 3u] * 100u / 127u;
        if (umt.info)
            umt_mlist(k & 3u);
    }
    fmt_int(b, (int32_t)v);
    umt_say(nm, vs);
}

/* ------------------------------------------------------------------------------------------ the effects --- */
#define umt_rnd umb_mb32                                /* (mulberry32, its own seed: the two strips agree) */
/* once a frame: GLITCH's energy, from the notes since (REACT's), falling off in ~0.2 s */
static void umt_glitch_tick(void)
{
    uint32_t g = umq_o[G_GLI], dt = fm1_ms - umt.gms;
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
 * an LFO a strobe; a hard switch, so the panel is sent the whole screen only as it turns). A TIMELINE cell's BLOCKS
 * (umv_mo) is MOSAIC's. FX + a REPEAT or STUTTER key held (perform.c, pfx.c): a strobe at its rate, the first half of each
 * repeat the whole picture washed in a colour (half its full light: the plate shows through), the next repeat the next
 * colour */
static void umt_fx(uint32_t y, uint32_t mir)
{
    uint16_t *px = cv_px, t[240];
    static const uint16_t SC[6] = {0x7800u, 0x7BE0u, 0x03E0u, 0x03EFu, 0x400Fu, 0x780Fu};
    uint32_t x, r, mo = umq_o[G_MOS] > umv_mo ? umq_o[G_MOS] : umv_mo, so = umq_o[G_SOL], n = cv_w * cv_h;
    uint32_t pf = perf_held | perf_latched, sb = beat_samples(), sc = song.playing ? ms_clock : fm1_ms * 44u;
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
    if (umq_o[G_INV] >= 50u)                            /* INVERT (50 or more) */
        for (x = 0; x < n; x++)
            px[x] = (uint16_t)~px[x];
    sb /= pf & PF_BIT(PF_R32) ? 8u : pf & (PF_BIT(PF_R16) | PF_BIT(PF_S16)) ? 4u : 2u;
    if ((pf & (PF_REPEAT | PF_BIT(PF_S8) | PF_BIT(PF_S16))) && sb && sc % sb < sb / 2u) {   /* the strobe */
        uint16_t c = swap16(SC[sc / sb % 6u]);
        for (x = 0; x < n; x++)
            px[x] |= c;
    }
}
static void being_fig(uint32_t c, int32_t cx, int32_t cy, int32_t k);   /* (ui_lzhome.c) */
/* the figures on a layer (pl: BODIES, the engine's figure; JIANT's beings no more since 2026-11-13) over the strip at
 * screen row y, a (0..256)
 * of its LIGHT: its TRACK's (0: the selected one's), in the middle, as large as its SIZE (50: an eighth larger than HOME >
 * BEING's; no labels, no floors) */
static void umt_fig(uint32_t y, uint32_t pl, uint32_t a)
{
    const umc_rec_t *r = umx_rec(pl);
    uint32_t t = umc_ev(r, 2u, 0u, NTRK);
    a = a * umc_ev(r, 0u, 99u, 99u) / 99u;
    if (!a)
        return;
    og_dim = (uint16_t)(256u - a);
    cv_oy = -(int32_t)y;
    vx_small = vx_bare = 1;
    being_fig(t ? t - 1u : song.sel % NTRK, 120, 120, 9 + (int32_t)umc_ev(r, 1u, 50u, 99u) * 18 / 99);
    vx_small = vx_bare = 0;
    cv_oy = 0;
    og_dim = 0;
}
/* TOTALITY drawn: the ground and its layers over the whole screen (no header, tabs, knobs or footer: ui_draw.c
 * ui_draw_body draws nothing else) in two strips, the effects on them, the figures over those (they stay whole), sent
 * where they changed (gfx.c cv_blit_cells); under LOAD LITE / ECO every 50 / 70 ms, as HUD's HOME */
static void umb_full(void)
{
    static uint32_t ms;
    uint32_t y, h, i, mir = umq_o[G_MIR], f = umt_fade() / 2u, el = fm1_ms - umt.fms, ba = fm1_ms - ms, g = 256u;
    uint32_t pf = perf_held | perf_latched, tg = fm1_ms - umt.tms;
    uint16_t la[4] = {0, 0, 256u, 0};
    int32_t ro = (mir & 2u) ? 0 : (int32_t)(pf >> PF_OCTU & 1u) * 64 - (int32_t)(pf >> PF_OCTD & 1u) * 64;   /* (T-B: none) */
    char tn[3] = {'T', (char)('1' + song.sel % NTRK), 0};
    if (sys_load && !ui.force && ba < (sys_load == LOAD_ECO ? 70u : 50u))
        return;
    ms = fm1_ms;
    ba = ba >= 500u ? 256u : ba * 256u / 500u;          /* each layer's fade, half a second (a turn back from where it was) */
    for (i = 0; i < 3u; i++)
        umt.ba[i] = (uint16_t)(!(umt.lh >> i & 1u) ? (umt.ba[i] + ba > 256u ? 256u : umt.ba[i] + ba) : (umt.ba[i] > ba ? umt.ba[i] - ba : 0u));
    la[0] = umt.ba[0], la[1] = umt.ba[1], la[3] = umt.ba[2];
    umb_clock();                                        /* (the routes and a cell's transition now: before the gate) */
    if (umt.np && el >= f) {                            /* PRESETS: the old ground out, the new one in */
        umb_plate = umt.np;
        umt.np = 0;
        umv_dirty = 1;
        umv_ms = fm1_ms;
    }
    if (umt.fms && el < 2u * f)                         /* (its light: down to none, then up again) */
        g = el < f ? (f - el) * 256u / f : (el - f) * 256u / f;
    else
        umt.fms = 0;
    la[0] = (uint16_t)(la[0] * g >> 8);
    if (umv_gt < dc_gate)                               /* (.. a TIMELINE cell's) */
        dc_gate = umv_gt;
    umt_glitch_tick();
    umx_layers();
    if ((umx_lp[0] >= UMX_BODY || umx_lp[1] >= UMX_BODY || umx_lp[2] >= UMX_BODY))   /* a figure: its track's level, */
        (void)eco_step();                               /* once a frame (UMB_NONE, no layer, past them: harmless) */
    if (umt.say_ms && fm1_ms - umt.say_ms > 1500u)
        umt.say_ms = 0;
    i = ro != umt.roll;                                 /* FX + OCT+ / OCT-: the picture pushed up / down 64 px, 8 a frame */
    umt.roll = (int8_t)(umt.roll + umb_clamp(ro - umt.roll, -8, 8));
    dc_frame(ui.force || i);                            /* (the strips move with it: all sent while it does) */
    umb.bare = 1;
    for (y = 0; y < 240u; y += h) {                     /* strips of 120 rows at most, each the picture from row src on: */
        uint32_t src = (y + 240u + (uint32_t)(int32_t)umt.roll) % 240u;   /* pushed, it goes round the screen's edge */
        h = 240u - src < 120u ? 240u - src : 120u;
        if (240u - y < h)
            h = 240u - y;
        cv_begin(240, h, T_BG);
        umb_scene(0, (mir & 2u) ? 0 : (int32_t)src, la);
        umt_fx(y, mir);
        for (i = 0; i < 3u; i++)
            if (umx_lp[i] == UMX_BODY)
                umt_fig(src, umx_lp[i], i ? la[1u + i] : la[1] * umv_lt[1] >> 8);
        if (umt.say_ms) {                               /* the word at the foot (a macro's: what it moves, over it) */
            cv_text(8, 222 - (int32_t)y, &AF_S, umt.say, T_MID);
            for (i = 0; !umt.fx && umt.say[0] == 'K' && umt.say[1] == 'N' && i < umt_ml.n; i++)
                cv_text(8, 210 - 10 * (int32_t)(umt_ml.n - i) - (int32_t)y, &AF_X, umt_ml.ln[i], T_TEXT);
        }
        if (umt.info && tg < 1500u)                     /* the track, a second, then fading (ALGORITHM, SEQ: again) */
            cv_text(6, 5 - (int32_t)y, &AF_S, tn, tg < 1000u ? T_TEXT : tg < 1250u ? T_MID : T_DIM);
        cv_blit_cells(0, y);
    }
    umb.bare = 0;
    dc_gate = 256u;
}
