/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (included by ui_umbra.c, after TOTALITY) HOME > UMBRA's LIVE AUDIO CTRLS and LIVE VIZ CTRLS (the user, 2026-11-09:
 * custom controls and effects for the sound and the picture; 2026-11-10: no long lists, what the macros do in sight, taps
 * that trigger, a setup wizard; 2026-11-12: every plate its own elements, each with as many controls as it takes).
 * TOTALITY's KNOB 1..4 are JIANT's macros M1..M4 (mod.c macro_v: the project's; LFO held turns them anywhere), so one
 * turn moves what both pages send a macro to.
 *   LIVE AUDIO CTRLS  one track at a time: its four slots of JIANT's modulation matrix (LFO > MOD's), a line each, and
 *                     under them what each macro M1..M4 moves, on every track. ALGORITHM the track, KNOB 1 the SLOT,
 *                     2 SOURCE, 3 DEST, 4 AMOUNT. Nothing new in the sound: the matrix as JIANT runs it, kept with the
 *                     project.
 *   LIVE VIZ CTRLS    (since 2026-11-15 two pages, the user: PLATES - GROUND and PLATES - VEILS) a page at a time
 *                     (ALGORITHM): every ground there is (GROUND) or every veil and BODIES, UMBRA's figures (VEILS), then
 *                     on both SCREEN (LIGHT SPEED REACT MOVE TRAVEL DISSLV), EFFECTS and BUTTONS (umx_here). On the left the page's name and under it its
 *                     elements (KNOB 1), each with a bar: its value now, moving as its controls move it. On the right
 *                     the chosen row's controls (KNOB 2), as many as it takes:
 *                       the name's row   its KIND: GROUND (a plate for MAIN) or VEIL (a plate or the figures for OVER;
 *                                        LAYER 3 none since 2026-11-15); NO ROOM under it: OVER's plate waiting, its
 *                                        state not fitting beside the ground's. (No WHERE since 2026-11-13: a page lies where its kind says)
 *                       SET              the element's own value (KNOB 4)
 *                       NOTES            (an element the notes drive) the tracks whose notes drive it: KNOB 3 the track,
 *                                        KNOB 4 on or off; none on: AUTO, the plate's own choice
 *                       a link           what else moves it: KNOB 3 its source (KNOB 1..4 = M1..M4, a track's HIT, LFO
 *                                        or STEP, LEVEL, BEAT, ENV TAP, LFO TAP, EDIT TAP: in TOTALITY a tap of that
 *                                        button, falling off over 0.4 s; OFF: the link gone), KNOB 4 how far (-99 .. 99
 *                                        of the element's range with the source at full)
 *                       + ADD            KNOB 3: a new link
 *                     ALGORITHM's plate, lying nowhere, is laid where its kind goes (a ground: MAIN; a veil, BODIES too:
 *                     OVER): the picture behind the page is the page's.
 *                     PRESETS (KNOB 3 on the name's row): the page's four presets, BASIC and three more (a plate's own,
 *                     from its file: umb_pre_t); what is set goes into the preset chosen.
 *                     Kept in LIVE VIZ CTRLS' own store (ui_umbra.c umc), one set for every song.
 *   PLATES            every plate a box in a row, the chosen one's playing in the middle, its name, its line and its
 *                     preset under it (umx_list): ALGORITHM the plate (the boxes slide), PRESETS its preset, KNOB 1..4
 *                     the macros M1..M4, as in TOTALITY. No knob row there.
 *   SAVE > SONG > TIMELINE   two picture tracks, MAIN and OVER, a cell a row; a cell opened by OCT+ is LIVE VIZ CTRLS
 *                     for its plate, OCT+ again its transition (how it comes in and goes), OCT+ again keeps it all;
 *                     OCT- a step back, nothing kept (umv_*, at the end).
 *   OCT+ / OCT- on either (the user, 2026-11-13: no SETUP)  OCT+ a new control: LIVE AUDIO CTRLS the track's first free
 *                     slot (from KNOB 1, its DEST and AMOUNT to set), LIVE VIZ CTRLS a new link on the chosen element (from
 *                     KNOB 1, at half); OCT- the chosen one gone (a slot, a link).
 * Built for size, as the main loop is (tools/size_fns.py) */
static void text_column(uint32_t c, const char *label, const char *value, uint16_t color);   /* (ui_draw.c) */


/* ------------------------------------------------------------------------------------ LIVE VIZ CTRLS --- */
static struct {
    uint16_t hit[NTRK], tap[5];                         /* each track's hits, the five taps, falling off (Q8) */
    uint32_t qr, ms;                                    /* the notes read; the last frame's time */
} umq __attribute__((section(".pool")));
static uint8_t umx_pg = 0xFFu;                          /* the page chosen (a plate's number, UMX_*; 0xFF: not yet) */
static uint8_t umx_el;                                  /* .. its row: 0 its name (where it lies), then its elements */
static uint8_t umx_ct;                                  /* .. the row's control: SET, NOTES, its links, + ADD */
static uint8_t umx_tr;                                  /* .. NOTES: the track under KNOB 3 */
/* once a frame (umb_clock): the sources now (umq_s), then SCREEN's and EFFECTS' elements as their links move them (umq_o) */
static void umq_tick(void)
{
    uint32_t dt = fm1_ms - umq.ms, c, pg, j;
    umq.ms = fm1_ms;
    for (c = 0; c < NTRK; c++)                          /* (a hit falls off over 0.3 s) */
        umq.hit[c] = (uint16_t)(umq.hit[c] > dt * 256u / 300u ? umq.hit[c] - dt * 256u / 300u : 0u);
    for (c = 0; c < 5u; c++)                            /* (a tap over 0.4 s) */
        umq.tap[c] = (uint16_t)(umq.tap[c] > dt * 256u / 400u ? umq.tap[c] - dt * 256u / 400u : 0u);
    if (umb_q.w - umq.qr > UMB_NQ)
        umq.qr = umb_q.w - UMB_NQ;
    for (; umq.qr != umb_q.w; umq.qr++) {
        uint32_t i = umq.qr % UMB_NQ, h = umb_q.vel[i] * 2u + 2u;
        if (umb_q.vel[i] && h > umq.hit[umb_q.c[i] % NTRK])
            umq.hit[umb_q.c[i] % NTRK] = (uint16_t)h;
    }
    for (c = 0; c < 4u; c++)
        umq_s[UMQ_M1 + c] = (int16_t)(macro_v[c] * 256 / 127);
    for (c = 0; c < NTRK; c++) {
        const track_t *t = &trk[c];                     /* (the step's level now, as mod.c ms_tick's, unslewed) */
        uint32_t per = div_samples((uint32_t)t->p[P_MSDIV]), len = t->p[P_MSLEN] > 0 ? (uint32_t)t->p[P_MSLEN] : 1u;
        umq_s[UMQ_HIT + c] = (int16_t)umq.hit[c];
        umq_s[UMQ_LFO + c] = (int16_t)clamp((t->lfo_val + 32768) >> 8, 0, 256);
        umq_s[UMQ_STEP + c] = (int16_t)(t->p[P_MS0 + (per ? ms_clock / per % len : 0u) % 16u] * 2);
    }
    umq_s[UMQ_LEVEL] = (int16_t)bg_energy();
    c = beat_samples();                                 /* BEAT: full on the beat, falling to the next (stopped: 0) */
    umq_s[UMQ_BEAT] = (int16_t)(song.playing && c ? 256 - (int32_t)(ms_clock % c * 256u / c) : 0);
    for (c = 0; c < 5u; c++)
        umq_s[UMQ_TAP + c] = (int16_t)umq.tap[c];
    for (pg = 0; pg < 2u; pg++) {
        const umc_rec_t *r = umx_rec(UMX_SCREEN + pg);
        for (j = 0; j < UMX_NC[1u + pg]; j++)
            umq_o[pg * 8u + j] = (uint8_t)umc_ev(r, j, UMX_C[1u + pg][j].def, umx_max(UMX_SCREEN + pg, j));
    }
}
/* TOTALITY: ENV (0), LFO (1), EDIT (2), SEL (3) or ARP (4) tapped, set to TAP. 1: a link takes that tap (it fires) */
static int umq_tap(uint32_t i)
{
    uint32_t r, k;
    umc_load();
    for (r = 0; r < UMC_NREC; r++)
        for (k = 0; umc.rec[r].key && k < UMC_NL; k++)
            if ((umc.rec[r].ls[k] & 31u) == UMQ_TAP + i) {
                umq.tap[i] = 256;
                return 1;
            }
    return 0;
}
/* BUTTONS' button i (SEL ENV ARP LFO EDIT): what it does, UMX_BF's; its tap's source (UMQ_TAP + ..) */
static const uint8_t UMT_BB[5] = {B_SCL, B_ENV, B_ARP, B_LFO, B_EDIT}, UMT_BQ[5] = {3, 0, 4, 1, 2};
static uint32_t umt_bf(uint32_t i) { return umx_raw(UMX_BTN, i); }
/* OVER's veil (ENV's): the one laid there before SEL's BODIES (umx_vk), or what lies there (a store from before: no
 * umx_vk yet) unless BODIES. 0: none */
static uint32_t umt_veil(void)
{
    umc_load();
    return umx_vk ? umx_vk : umc.lay[0] != umx_key(UMX_PN[0]) ? umc.lay[0] : 0u;
}
static int umt_lay_on(uint32_t f)                       /* f (UMX_BF: MAIN, OVER, BODIES) shows something, hidden or not */
{
    umc_load();
    return !f ? umb_pick() != UMB_NONE : f == 1u ? umc.lay[0] && umc.lay[0] == umt_veil() : umc.lay[0] == umx_key(UMX_PN[0]);
}
/* each input pass in TOTALITY (none over it): the five buttons. MAIN's or OVER's: tapped, that layer hides or shows;
 * OVER's with BODIES there (SEL laid them): its veil back, shown. BODIES' (the user, 2026-11-15: no third layer, BODIES
 * a veil on OVER, so never at once with OVER's veil): tapped, BODIES on OVER in its veil's place, shown, or hidden if
 * they were showing. A tap takes nothing over (the rows' cells go on; a row's own OVER cell lays its veil again). Held
 * with ALGORITHM turned: the next plate there (that one takes over), BODIES' the next track's figure (umt_algo; let go:
 * no tap; held half a second or more: none either). TAP's: tapped, its links fire (held: its layer, ui_layer.c). ENV
 * and ARP are no page's here (ui_input.c lets them be) */
static void umt_btns(void)
{
    uint32_t i, f, v;
    for (i = 0; i < 5u; i++) {
        f = umt_bf(i);
        if ((fm1_in.buttons >> panel.btn[UMT_BB[i]]) & 1u) {
            if (!umt.bt[i])
                umt.bt[i] = fm1_ms | 1u;
        } else if (umt.bt[i]) {
            if (!(umt.bh >> i & 1u) && fm1_ms - umt.bt[i] < 500u) {   /* (a tap: let go within half a second) */
                if (f == 3u)
                    (void)umq_tap(UMT_BQ[i]);
                else {
                    v = umt_veil();
                    if (f && !umt_lay_on(f) && (f == 2u || v)) {   /* (OVER: BODIES there, its veil back; BODIES: laid
                                                         * there, the veil kept for OVER's button. No veil: OVER's button
                                                         * shows or hides what lies there) */
                        umx_place_set(f == 1u ? umx_find(v) : UMX_BODY, 2u);
                        umx_vk = (uint16_t)v;
                        umt.lh |= 2u;                   /* (shown below, fading in from none) */
                        umt.ba[1] = 0;
                    }
                    f = f ? 1u : 0u;
                    umt.lh ^= (uint8_t)(1u << f);
                    umt_say(UMX_BF[umt_bf(i)], umt.lh >> f & 1u ? "OFF" : "ON");   /* (no takeover, the user 2026-11-15:
                                                         * shown or hidden as the song plays, the rows' cells go on: a
                                                         * veil set ON ENV is ENV's to bring in) */
                }
            }
            umt.bt[i] = 0;
            umt.bh &= (uint8_t)~(1u << i);
        }
    }
}
/* (ui_input.c) ALGORITHM turned s in TOTALITY: with a layer's button held (the user, 2026-11-13), the next plate on that
 * layer (MAIN: a ground; OVER: a veil), shown if it was hidden; BODIES' held: the next track (its figure the one BODIES
 * show), BODIES shown on OVER; that button's let go is no tap. 1: taken */
static int umt_algo(int32_t s)
{
    uint32_t i, f;
    for (i = 0; i < 5u; i++)
        if (umt.bt[i] && (f = umt_bf(i)) < 3u) {
            if (f < 2u)
                umt_select(s, f | 4u);
            else {
                uint32_t v = umt_veil();
                char t[3] = {'T', 0, 0};
                track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
                umx_place_set(UMX_BODY, 2u);
                umx_vk = (uint16_t)v;
                umt.lh &= (uint8_t)~2u;
                umt.tms = fm1_ms;
                t[1] = (char)('1' + song.sel % NTRK);
                umt_say(UMX_PN[0], t);
            }
            umt.bh |= (uint8_t)(1u << i);
            return 1;
        }
    return 0;
}
/* (ui_input.c ui_leds) button i lit: what it shows is showing; TAP: its tap lasting */
static int umt_blit(uint32_t i)
{
    uint32_t f = umt_bf(i);
    return f == 3u ? umq.tap[UMT_BQ[i]] != 0u : !(umt.lh >> (f ? 1u : 0u) & 1u) && umt_lay_on(f);
}
/* SAVE > SONG > TIMELINE's picture tracks (the user, 2026-11-13): past T6, ALGORITHM reaches MAIN and OVER, a cell a row
 * each: the plate that is the ground, and the one on LAYER 1, from that row on (umv_apply). KNOB 4 there turns the cell's
 * plate; OCT+ opens the cell: LIVE VIZ CTRLS for its plate and its preset, over the picture (ALGORITHM the plate, PRESETS
 * the preset, the knobs as on that page). OCT+ there: its transition (umv_ed 2), KNOB 1 how the plate comes IN, 2 how
 * long, 3 how it goes OUT, 4 how long; OCT+ again: all of it kept. OCT- a step back with nothing kept (the user,
 * 2026-11-13): from the transition its four as they were, from the cell the store as it was when the cell was opened
 * (read again from flash: it was saved as the cell opened, and nothing is saved while one is open, umv_poll) */
static uint16_t umv_trd __attribute__((section(".pool")));   /* the transition being set (umv_trv) */
static uint8_t umv_bp;                                  /* the ground's plate as the cell was opened */
static const char *const UMV_TY[5] = {"DISSOLVE", "FADE", "BLOCKS", "CUT", "ON ENV"};   /* (ON ENV: IN on OVER alone) */
#define UMV_NLS 6u                                      /* (the user, 2026-11-14: a fade up to 16 s; 4 s until then) */
static const char *const UMV_LS[UMV_NLS] = {"0.5 S", "1 S", "2 S", "4 S", "8 S", "16 S"};   /* (500 ms << the place) */
/* a transition (umv_trv: a cell's tr, its lengths' high bits 8 IN's, 9 OUT's, 10 IN ON ENV)'s field k as its place in
 * the lists: 0 IN's kind (UMV_TY), 1 IN's length (UMV_LS), 2 OUT's kind, 3 OUT's length; umv_ts the transition with
 * field k at place i (0: IN a DISSOLVE of 1 s, OUT a CUT; a plate chosen in a cell, umv_turn: UMV_CUT, IN a CUT too; a
 * store from before the high bits: all 0, every one as it was). ON ENV (the user, 2026-11-15): an OVER cell's veil laid
 * hidden at its row's start, ENV in TOTALITY shows it (ui_umbra_plate.c umv_poll); its two bits a CUT's (umv_tick) */
static uint32_t umv_tf(uint32_t v, uint32_t k)
{
    uint32_t f = v >> 2u * k & 3u;
    return k == 2u ? (f + 3u) & 3u : (k & 1u) ? (f ^ 1u) | (v >> (8u + (k >> 1)) & 1u) << 2 : (v >> 10 & 1u) ? 4u : f;
}
static uint32_t umv_ts(uint32_t v, uint32_t k, uint32_t i)
{
    uint32_t h = k ? 8u + (k >> 1) : 10u;
    v = (v & ~(3u << 2u * k)) | (k == 2u ? (i + 1u) & 3u : (k & 1u) ? (i ^ 1u) & 3u : i > 3u ? 3u : i) << 2u * k;
    return k == 2u ? v : (v & ~(1u << h)) | (i >> 2 & 1u) << h;
}
static uint32_t umv_trv(const umc_cell_t *c) { return c->tr | (uint32_t)(c->pre >> 6) << 8 | (uint32_t)(c->pre >> 5 & 1u) << 10; }
static void umv_trput(umc_cell_t *c, uint32_t v)
{
    c->tr = (uint8_t)v;
    c->pre = (uint8_t)((c->pre & 31u) | (v >> 8 & 3u) << 6 | (v >> 10 & 1u) << 5);
}
/* once a frame (umb_clock): the transitions now. A plate come in with its row (umv_st.t0, umv_poll) comes IN over its
 * length; one the song's next row replaces (a cell there) goes OUT over the end of this row. FADE: the track's light;
 * DISSOLVE: TOTALITY's gate (going: the track dark behind it); BLOCKS: MOSAIC, from or to its coarsest */
BTAPP_FN static void umv_tick(void)                     /* (in the Bluetooth segment, as PLATES; no segment: every plate cut) */
{
    uint32_t tk, left = ~0u, b = beat_samples(), mb = 60000u / (uint32_t)clamp(song.g[G_BPM], 20, 400);
    umv_mo = 0;
    umv_gt = 256u;
    uint32_t r = umv_st.row;                            /* (the row whose plates show) */
    if (chain.running && !chain.hold && umv_own != 1u && b > mb && r + 1u < chain.config.count)   /* ms to the row's end; past
                                                         * it, the next (taken over in TOTALITY: nothing goes) */
        left = chain.row != r ? 0u : ((chain.remaining ? chain.remaining : 1u) * chain_bar() - chain.bar_pos) / (b / mb);   /* row's
                                                         * plates not in yet: gone still */
    for (tk = 0; tk < 2u; tk++) {
        uint32_t v = umv_st.tr[tk] | (umv_sthi >> 2u * tk & 3u) << 8, ty = umv_tf(v, 2u), ln = 500u << umv_tf(v, 3u), p = left,
                 out = 1, lt = 256u;
        if (left >= ln || !umv_cell(r + 1u, tk)->key) { /* (not going: coming in?) */
            out = 0;
            ty = umv_tf(v, 0u);
            ln = 500u << umv_tf(v, 1u);
            p = umv_st.t0[tk] ? fm1_ms - (umv_st.t0[tk] & ~1u) : ln;   /* (t0 is fm1_ms | 1: in the same ms, not -1 and
                                                         * so long over; 2026-11-15) */
            if (p >= ln)
                umv_st.t0[tk] = 0;
        }
        if (p < ln && ty != 3u) {
            p = p * 256u / ln;                          /* (how much of it shows, Q8) */
            if (ty == 1u)
                lt = p;
            else if (ty == 2u)
                umv_mo = (uint8_t)((256u - p) * 99u >> 8);
            else {
                umv_gt = (uint16_t)(out ? 256u - p : p);
                lt = out ? 0u : 256u;
            }
        }
        umv_lt[tk] = (uint16_t)lt;
    }
}
static int umv_on(void) { return umv_ed && umv_tk && !ui.home && !ui.menu && !ui.confirm && SONG_TRK(cur_page()); }
static umc_cell_t *umv_cc(void) { return umv_cell(ui.song_row, umv_tk - 1u); }
/* the cursor's row shown (not while the song plays its own), the open cell's page LIVE VIZ CTRLS' */
static void umv_sync(void)
{
    uint32_t pl;
    if (!chain.running || (chain.hold && chain.row == ui.song_row && umv_own != 1u))   /* (LOOP ROW on it: at once, the
                                                         * loop's next round lays nothing anew: umv_poll) */
        umv_apply(ui.song_row);
    if (umv_ed && umv_tk && (pl = umx_find(umv_cc()->key)) != umx_pg) {   /* (umv_ed: 1 the cell, 2 its transition) */
        umx_pg = (uint8_t)pl;
        umx_el = umx_ct = 0;
    }
    ui.force = 1;
}
#define umk_au() 0u                                     /* (LIVE AUDIO CTRLS: gone, the user 2026-11-16) */
static int umk_on(void), umx_veil(uint32_t pl);
/* page pl is one the page shown offers (the user, 2026-11-15: LIVE VIZ CTRLS split in two): PLATES - GROUND the grounds,
 * PLATES - VEILS the veils and BODIES, both SCREEN, EFFECTS and BUTTONS; PLATES, a TIMELINE cell, TOTALITY: any */
static int umx_here(uint32_t pl)
{
    uint32_t h = ui.hset[0];
    if (!umv_on() && umk_on() && h == HOME_COLOR)       /* (COLOR: the plates, each with its parts) */
        return pl < UMB_NPLT;
    return umv_on() || !umk_on() || (h != 2u && h != 3u) || pl > UMX_BODY || (h == 3u) == (umx_veil(pl) != 0);
}
static uint32_t umx_cur(void)                           /* the page chosen (at first the ground's plate, on PLATES - VEILS
                                                         * OVER's, BODIES; none: SCREEN) */
{
    umx_desc_t d;
    if (!umx_desc(umx_pg, &d) || !umx_here(umx_pg)) {
        umx_layers();
        umx_pg = (uint8_t)(ui.hset[0] == 3u && umk_on() && !umv_on() ? umx_lp[0] != UMB_NONE ? umx_lp[0] : UMX_BODY :
                           umb_pl() == UMB_NONE ? UMX_SCREEN : umb_pl());
        umx_el = umx_ct = 0;
    }
    return umx_pg;
}
/* the chosen element j's control row ct: 0 SET, 1 NOTES (notes: it has one), 2 a link (*li: which), 3 + ADD (*li: a free
 * link), 4: past its last */
#define UMX_NT(d, j) ((d).el && (j) < UMB_NCTL ? (int)((d).el[j] & (UMB_EL_NOTES | 0x80u)) : 0)   /* (umx_row's notes) */
static uint32_t umx_row(const umc_rec_t *r, uint32_t j, int notes, uint32_t ct, uint32_t *li)
{
    uint32_t i;
    if (!ct)
        return 0;
    if (notes > 1)                                      /* (BUTTONS: SET alone) */
        return 4;
    if (notes && !--ct)
        return 1;
    for (i = 0; i < UMC_NL; i++)
        if ((r->ls[i] & 31u) && umc_lj(r, i) == j && !--ct) {
            *li = i;
            return 2;
        }
    for (i = 0; ct == 1u && i < UMC_NL; i++)
        if (!(r->ls[i] & 31u)) {
            *li = i;
            return 3;
        }
    return 4;
}
static void umx_src(char *b, uint32_t s)                /* a source's name (12 bytes) */
{
    static const char *const SL[7] = {"LEVEL", "BEAT", "ENV TAP", "LFO TAP", "EDIT TAP", "SEL TAP", "ARP TAP"};
    static const char *const SN[3] = {"HIT", "LFO", "STEP"};
    if (!s)
        str_cpy(b, "OFF", 12);
    else if (s < UMQ_HIT) {
        b[0] = 'M', b[1] = (char)('0' + s), b[2] = 0;   /* (the macros: a BODY's knob set to one turns it, KNOBS) */
    } else if (s >= UMQ_LEVEL)
        str_cpy(b, SL[s - UMQ_LEVEL], 12);
    else {
        b[0] = 'T', b[1] = (char)('1' + (s - UMQ_HIT) % NTRK), b[2] = ' ';
        str_cpy(b + 3, SN[(s - UMQ_HIT) / NTRK], 9);
    }
}
static void umx_fmt(char *b, uint32_t pl, uint32_t j, uint32_t v)   /* page pl's element j's value v, as words */
{
    if (umx_max(pl, j) == 1u)                           /* (an other element's toggle) */
        str_cpy(b, v ? "ON" : "OFF", 12);
    else if (umx_max(pl, j) == 3u)
        str_cpy(b, (pl == UMX_SCREEN ? UMN_MV : pl == UMX_BTN ? UMX_BF : UMT_MIRN)[v & 3u], 12);
    else if (umx_max(pl, j) == NTRK) {
        str_cpy(b, "SEL", 12);
        if (v)
            b[0] = 'T', b[1] = (char)('0' + v), b[2] = 0;
    } else
        fmt_int(b, (int32_t)v);
}
/* page pl is a VEIL (the user, 2026-11-13): a plate light enough to lie over another (its kind UMB_KIND_OVERLAY), or
 * BODIES; the other plates are grounds */
static int umx_veil(uint32_t pl)
{
    const umb_plate_t *p = pl < UMB_NPLT ? umb_mod(pl) : 0;
    return pl == UMX_BODY || (p && p->kind == UMB_KIND_OVERLAY);
}
static const char *umx_kind(uint32_t pl) { return pl > UMX_BODY ? "ALL" : umx_veil(pl) ? "VEIL" : "GROUND"; }
/* page pl can lie at w (0 nowhere, 1 MAIN, 2 OVER): nowhere, any (SCREEN, EFFECTS and BUTTONS never: they are every
 * plate's); MAIN a ground, OVER a veil. LAYER 2 and LAYER 3 are no more (the user, 2026-11-13: LAYER 2 too hard to
 * find; 2026-11-15: BODIES a veil on OVER; what lay there lies nowhere: umc_load) */
static int umx_can(uint32_t pl, uint32_t w)
{
    return pl <= UMX_BODY && w < 3u && (!w || (w == 1u) != umx_veil(pl));
}
/* ALGORITHM: the next page there is of the first lim, round. A plate lying nowhere is laid where its kind goes (a
 * ground MAIN, a veil OVER, BODIES too): the picture behind the page is the one the page is about (the user,
 * 2026-11-13) */
static void umx_algo(int32_t s, uint32_t lim)
{
    umx_desc_t d;
    uint32_t k, pl = umx_cur() % lim;
    for (k = 0; k < lim; k++)
        if (umx_desc(pl = (pl + (s > 0 ? 1u : lim - 1u)) % lim, &d) && umx_here(pl))
            break;
    umx_pg = (uint8_t)pl;
    umx_el = umx_ct = 0;
    if (!umx_place(pl) && pl <= UMX_BODY)
        umx_place_set(pl, umx_veil(pl) ? 2u : 1u);
}
/* KNOB k turned s: 1 the row, 2 its control, 3 the track or the source, 4 the value */
BTAPP_FN static void umx_knob(uint32_t k, int32_t s)   /* (in the Bluetooth segment, 2026-11-14: the app slot is full;
                                                         * no segment: nothing to turn) */
{
    umx_desc_t d;
    uint32_t pl = umx_cur(), j, li = 0, row, w;
    umc_rec_t *r;
    int notes;
    if (!umx_desc(pl, &d))
        return;
    if (k == 0u) {
        umx_el = (uint8_t)clamp((int32_t)umx_el + s, 0, (int32_t)(d.nctl + d.nx));
        umx_ct = 0;
        return;
    }
    if (!umx_el) {                                      /* the name's row: DICE (KNOB 2), its preset */
        if (k == 2u)
            umx_pre_set(pl, (uint32_t)clamp((int32_t)umx_pre(pl) + s, 0, UMC_NPRE - 1));
        else if (k == 1u && d.nx) {                     /* (the user, 2026-11-14: the other elements randomized: each turn
                                                         * a new throw, in the preset chosen; a toggle on or off) */
            r = umx_recf(pl, 1);
            for (j = UMB_NCTL; j < UMB_NCTL + UMC_NX; j++)
                if (umx_c(&d, j))
                    *umc_vp(r, j) = (uint8_t)(1u + umb_rnd() % (umx_max(pl, j) + 1u));
            umc_touch();
        }
        return;
    }
    j = umx_jat(&d, umx_el);
    r = umx_recf(pl, k >= 2u);                          /* (a preset not changed yet: its own record now) */
    notes = UMX_NT(d, j);
    row = umx_row(r, j, notes, umx_ct, &li);
    if (k == 1u) {
        if (s < 0 ? umx_ct : umx_row(r, j, notes, umx_ct + 1u, &li) < 4u)
            umx_ct = (uint8_t)(umx_ct + (s < 0 ? -1 : 1));
        return;
    }
    if (row == 0u) {
        if (k == 3u)
            umx_put(pl, j, (int32_t)umx_raw(pl, j) + accel(EN_K1 + k, s, (int32_t)umx_max(pl, j) == 99 ? 99 : 0));
        return;
    }
    if (row == 1u) {
        if (k == 2u)
            umx_tr = (uint8_t)clamp((int32_t)umx_tr + s, 0, NTRK - 1);
        else
            r->t[j] = (uint8_t)(s > 0 ? r->t[j] | 1u << umx_tr : r->t[j] & ~(1u << umx_tr));
    } else if (k == 2u) {                               /* a link's source (+ ADD: a new one, half way at first) */
        w = (uint32_t)clamp((int32_t)(r->ls[li] & 31u) + s, 0, UMQ_NS - 1);
        if (row == 3u)
            r->ld[li] = 50;
        umc_lset(r, li, w, j);
        for (umx_ct = (uint8_t)(1u + (uint32_t)notes), row = 0; w && umx_ct < 2u + UMC_NL; umx_ct++)   /* (its row now; gone: */
            if (umx_row(r, j, notes, umx_ct, &row) == 2u && row == li)                          /* the row after SET, NOTES) */
                break;
    } else if (row == 2u)
        r->ld[li] = (int8_t)clamp(r->ld[li] + accel(EN_K1 + k, s, 198), -99, 99);
    umc_touch();
}

/* ------------------------------------------------------------------------------------------- the pages --- */
static int umk_on(void) { return UMB_ON && !umt.on && home_bare() && ui.hset[0]; }   /* a LIVE CTRLS page or PLATES shown */
/* LIVE VIZ CTRLS' chosen row and control as words: the row (the page's name, an element), the control, the track or the
 * source, the value. 1: KNOB 3 has something to turn there */
BTAPP_FN static int umx_names(char (*b)[12])           /* (the segment's: umk_cards calls it) */
{
    umx_desc_t d;
    uint32_t pl = umx_cur(), j, li = 0, row;
    const umc_rec_t *r;
    b[1][0] = b[2][0] = b[3][0] = 0;
    if (!umx_desc(pl, &d)) {
        b[0][0] = 0;
        return 0;
    }
    j = umx_jat(&d, umx_el);
    str_cpy(b[0], umx_el ? umx_en(&d, j) : d.name, 12);
    if (!umx_el) {
        if (d.nx)
            str_cpy(b[1], "TURN", 12);
        umx_pre_name(pl, umx_pre(pl), b[2]);
        if (b[2][2])                                    /* (the column: its name alone) */
            str_cpy(b[2], b[2] + 3, 9);
        str_cpy(b[3], umx_kind(pl), 12);
        return 1;
    }
    r = umx_rec(pl);
    row = umx_row(r, j, UMX_NT(d, j), umx_ct, &li);
    if (row == 0u) {
        str_cpy(b[1], "SET", 12);
        umx_fmt(b[3], pl, j, umx_raw(pl, j));
    } else if (row == 1u) {
        str_cpy(b[1], "NOTES", 12);
        b[2][0] = 'T', b[2][1] = (char)('1' + umx_tr), b[2][2] = 0;
        str_cpy(b[3], !r->t[j] ? "AUTO" : r->t[j] >> umx_tr & 1u ? "ON" : "OFF", 12);
    } else if (row == 2u) {
        str_cpy(b[1], "LINK", 12);
        umx_src(b[2], r->ls[li] & 31u);
        b[3][0] = r->ld[li] > 0 ? '+' : 0;
        fmt_int(b[3] + (r->ld[li] > 0), r->ld[li]);
    } else {
        str_cpy(b[1], "+ ADD", 12);
        str_cpy(b[2], "TURN", 12);
    }
    return row != 0u;
}
/* ------------------------------------------------------------------------------------------ COLOR, KNOBS --- */
#define UMK_RH 17                                       /* a row's height (AF_S) */
static void umk_text(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text(x, y, &AF_S, s, c); }
static int umk_preset(int32_t s);
/* HOME > UMBRA > COLOR (the user, 2026-11-16): ALGORITHM the plate, as the other pages; KNOB 1 the part of its picture
 * (its colours' parts, umb_plate_t.col: "FISH", "PATCH 1" ..), KNOB 2 that part's colour: OWN (as the plate was drawn),
 * 1 .. 6 the palette's, TRACKS (a part that follows the notes: track n the palette's colour n); KNOB 3 or PRESETS the
 * palette in use, of the four kept (the plates page makes and sends them: editor_plate.c) */
static uint8_t umcl_r __attribute__((section(".pool")));   /* the part chosen */
static uint8_t uma_slot __attribute__((used));          /* (LIVE AUDIO CTRLS', PLATES' boxes': read by nothing, kept so .bss lies */
static uint8_t umx_sd __attribute__((used));            /* as it did: a few bytes moved there cost the image hundreds) */
static uint32_t umx_sms __attribute__((used));
static const umb_plate_t *umcl_p(void)                 /* the page's plate, if it has parts */
{
    const umb_plate_t *p = umx_cur() < UMB_NPLT ? umb_mod(umx_cur()) : 0;
    return p && p->api >= 7u && p->ncol ? p : 0;
}
static void umcl_word(char *b, uint32_t v)             /* a choice as a word */
{
    str_cpy(b, v == UMB_COL_OWN ? "OWN" : v == UMB_COL_TRK ? "TRACKS" : "1", 8);
    if (b[0] == '1')
        b[0] = (char)('0' + v);
}
static void umcl_knob(uint32_t k, int32_t s)
{
    const umb_plate_t *p = umcl_p();
    if (k == 2u)
        umb_pal_set((uint32_t)clamp((int32_t)umc.palcur + s, 0, UMP_NPAL - 1));
    if (!p || k >= 2u)
        return;
    umcl_r = (uint8_t)clamp((int32_t)umcl_r + (k ? 0 : s), 0, (int32_t)p->ncol - 1);
    if (k)
        umb_colc_set(p, umcl_r, (uint32_t)clamp((int32_t)umb_colc(p, umcl_r) + s, 0,
                                                p->col[umcl_r].def & UMB_COLF_TRK ? (int32_t)UMB_COL_TRK : (int32_t)UMB_NCOL));
}
/* part j's colour as c has it, a box at x, y (a part on the tracks: the six tracks' side by side) */
BTAPP_FN static void umcl_box(const umb_lctx_t *c, uint32_t j, int32_t x, int32_t y)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        const uint8_t *q = c->pm[j] == UMB_PM_NOTES ? UMB_EVC[t * 3u % 8u] : c->pm[j] ? c->tc[t] : c->pc[j];
        cv_rect(x + (int32_t)t * 2, y, 2, 10, RGB(q[0], q[1], q[2]));
    }
}
BTAPP_FN static void umcl_draw(int32_t x0, int32_t y, int32_t bot)
{
    const umb_plate_t *p = umcl_p();
    umb_lctx_t c;
    char b[12];
    uint32_t j;
    int32_t yy = y;
    umk_text(6 - x0, yy, umb_pname(umx_cur()), T_TEXT);
    cv_rect(6 - x0, yy + 14, 108, 1, T_LINE);
    if (!p)
        cv_text(6 - x0, yy + 20, &AF_X, "NO PARTS (AN OLDER FILE)", T_DIM);
    else {
        umb_cols(&c, p, 1u);                            /* (as a veil: the ground's palette let be) */
        if (umcl_r >= p->ncol)
            umcl_r = 0;
        for (j = 0, yy += UMK_RH; j < p->ncol && j < UMB_NCOL && yy + 12 <= bot; j++, yy += UMK_RH) {
            uint16_t col = j == umcl_r ? T_THEME : T_TEXT;
            umcl_box(&c, j, 6 - x0, yy + 2);
            umk_text(22 - x0, yy, p->col[j].name, col);
            umcl_word(b, umb_colc(p, j));
            umk_text(114 - text_w(&AF_S, b) - x0, yy, b, col);
        }
    }
    str_cpy(b, "PALETTE 1", sizeof b);                  /* the palette in use: its six colours, numbered */
    b[8] = (char)('1' + umc.palcur % UMP_NPAL);
    umk_text(126 - x0, y, b, T_THEME);
    cv_rect(126 - x0, y + 14, 108, 1, T_LINE);
    for (j = 0; j < UMB_NCOL; j++) {
        const uint8_t *q = umc.pal[umc.palcur % UMP_NPAL][j];
        int32_t x = 126 + (int32_t)j * 18 - x0;
        cv_rect(x, y + UMK_RH + 2, 16, 16, RGB(q[0], q[1], q[2]));
        b[0] = (char)('1' + j), b[1] = 0;
        cv_text(x + 5, y + UMK_RH + 21, &AF_X, b, p && umb_colc(p, umcl_r) == j + 1u ? T_THEME : T_MID);
    }
}
/* HOME > BEING > KNOBS (ui_lzhome.c draws it): KNOB k turned s, what it turns for the selected track: its engine's own
 * (AUTO), then the sound's parameters a player reaches for, then the macros M1..M4 */
static const uint8_t UMK_KL[] = {0, P_E0 + 1, P_E1 + 1, P_E2 + 1, P_E3 + 1, P_E4 + 1, P_E5 + 1, P_E6 + 1, P_E7 + 1, P_FCUT + 1, P_FRES + 1,
                                 P_ATK + 1, P_DEC + 1, P_SUS + 1, P_REL + 1, P_LEVEL + 1, P_PAN + 1, P_DIST + 1, P_CHOR + 1, P_DLY + 1,
                                 P_REV + 1, P_GLIDE + 1, P_DETUNE + 1, P_LRATE + 1, 0x80, 0x81, 0x82, 0x83};
static void umk_kturn(uint32_t k, int32_t s)
{
    uint8_t *v = &umc.kn[song.sel % NTRK][k & 3u];
    int32_t i;
    umc_load();
    for (i = (int32_t)sizeof UMK_KL; --i > 0 && UMK_KL[i] != *v;) {}
    *v = UMK_KL[clamp(i + (s > 0 ? 1 : -1), 0, (int32_t)sizeof UMK_KL - 1)];
    umc_touch();
    ui.force = 1;
}
static void umk_algo(int32_t s)                         /* ALGORITHM: the page (PLATES - GROUND / - VEILS: their kind's,
                                                         * umx_here; PLATES: any plate and BODIES; COLOR: any plate) */
{
    umx_algo(s, ui.hset[0] == HOME_PLATES ? UMX_BODY + 1u : ui.hset[0] == HOME_COLOR ? UMB_NPLT : UMX_N);
    umcl_r = 0;
}
static void umk_knob(uint32_t k, int32_t s)
{
    if (ui.hset[0] == HOME_PLATES) {                    /* PLATES: KNOB 1 the plate, as ALGORITHM */
        if (!k)
            umk_algo(s);
    } else if (ui.hset[0] == HOME_COLOR)
        umcl_knob(k, s);
    else if (bt_hw_seg())
        umx_knob(k, s);
}
/* each input pass, after the octave keys' taps (the user, 2026-11-13: no SETUP): OCT+ a new control, OCT- the chosen one
 * gone. LIVE AUDIO CTRLS: OCT+ the selected track's first free slot, from KNOB 1 (its DEST and AMOUNT still to set),
 * chosen; OCT- the chosen slot emptied. LIVE VIZ CTRLS: OCT+ a new link on the chosen element (the name's row: the first
 * element), from KNOB 1 at half, chosen; OCT- the chosen link gone */
static void umk_oct(uint32_t oct)
{
    umx_desc_t d;
    umc_rec_t *r;
    uint32_t j, li = 0, n, k;
    const char *m;
    if (!oct || !umk_on() || ui.hset[0] == HOME_PLATES || ui.hset[0] == HOME_COLOR)
        return;
    {
        if (!umx_desc(umx_cur(), &d) || !d.nctl || UMX_NT(d, 0) > 1)   /* (BUTTONS: SET alone) */
            return;
        if (!umx_el)
            umx_el = 1;
        j = umx_jat(&d, umx_el);
        n = (uint32_t)UMX_NT(d, j);
        r = umx_recf(umx_pg, 1);
        if (oct & 2u) {
            umx_ct = (uint8_t)(1u + n);                 /* (the + ADD row: a free link) */
            while (umx_row(r, j, (int)n, umx_ct, &li) == 2u)
                umx_ct++;
            if (umx_row(r, j, (int)n, umx_ct, &li) != 3u)
                m = "NO FREE LINK";
            else {
                umc_lset(r, li, UMQ_M1, j);
                r->ld[li] = 50;
                for (umx_ct = (uint8_t)(1u + n); !(umx_row(r, j, (int)n, umx_ct, &k) == 2u && k == li); umx_ct++) {}   /* (chosen) */
                m = "LINK ADDED";
            }
        } else if (umx_row(r, j, (int)n, umx_ct, &li) != 2u)
            m = "CHOOSE A LINK";
        else {
            r->ls[li] = 0;
            if (umx_row(r, j, (int)n, umx_ct, &li) == 4u)
                umx_ct--;
            m = "LINK REMOVED";
        }
        umc_touch();
    }
    ui_message(m);
    ui.force = 1;
}
/* a cell's transition shown (in the Bluetooth segment, as PLATES: umv_input opens it only with the segment there). cards:
 * KNOB 1..4's columns, IN, OUT, each its length; else over the picture: the cell, then IN and OUT, large */
BTAPP_FN static void umv_trshow(int cards)
{
    char b[12];
    uint32_t k;
    for (k = 0; cards && k < 4u; k++)
        text_column(k, (k & 1u) ? "LENGTH" : k ? "OUT" : "IN", ((k & 1u) ? UMV_LS : UMV_TY)[umv_tf(umv_trd, k)],
                    (k & 1u) && umv_tf(umv_trd, k - 1u) >= 3u ? T_DIM : VAL(k));
    if (cards)
        return;
    umv_name(umv_cc(), b);
    cv_text(8, 4, &AF_S, b, T_THEME);
    cv_text(232 - text_w(&AF_S, "TRANSITION"), 4, &AF_S, "TRANSITION", T_MID);
    for (k = 0; k < 4u; k += 2u) {
        int32_t yy = 30 + (int32_t)k * 17;
        const char *ln = UMV_LS[umv_tf(umv_trd, k + 1u)];
        cv_text(8, yy + 4, &AF_S, k ? "OUT" : "IN", T_MID);
        cv_text(50, yy, &AF_M, UMV_TY[umv_tf(umv_trd, k)], T_THEME);
        if (umv_tf(umv_trd, k) < 3u)
            cv_text(232 - text_w(&AF_M, ln), yy, &AF_M, ln, T_TEXT);
    }
}
BTAPP_FN static void umk_cards(void)                             /* KNOB 1..4: the chosen row's columns */
{
    static const char *const LB[3][4] = {{"PART", "COLOR", "PALETTE", ""}, {"ELEMENT", "CONTROL", "SOURCE", "VALUE"},
                                         {"PAGE", "DICE", "PRESET", "KIND"}};
    char b[4][12];
    uint32_t k, au = umk_au(), nm;
    int on;
    if (umv_on() && umv_ed == 2u) {                     /* (a cell's transition) */
        umv_trshow(1);
        return;
    }
    if (!umv_on() && ui.hset[0] == HOME_COLOR) {        /* COLOR: the part, its colour, the palette */
        const umb_plate_t *p = umcl_p();
        b[0][0] = b[1][0] = b[3][0] = 0;
        if (p) {
            str_cpy(b[0], p->col[umcl_r % UMB_NCOL].name, 8);
            umcl_word(b[1], umb_colc(p, umcl_r % UMB_NCOL));
        }
        b[2][0] = (char)('1' + umc.palcur % UMP_NPAL), b[2][1] = 0;
        for (k = 0; k < 3u; k++)
            text_column(k, LB[0][k], b[k][0] ? b[k] : "-", b[k][0] ? VAL(k) : T_DIM);
        text_column(3u, "", "", T_DIM);
        return;
    }
    nm = !au && !umx_el;
    if (umv_on() && umx_pg == UMB_NONE) {               /* (a cell with no plate: its word, nothing to turn) */
        umv_name(umv_cc(), b[0]);
        b[1][0] = b[2][0] = b[3][0] = 0;
        on = 0;
    } else
        on = umx_names(b);
    for (k = 0; k < 4u; k++)
        text_column(k, !au && k == 2u && b[1][0] == 'N' ? "TRACK" : LB[nm ? 2u : !au][k], b[k][0] ? b[k] : "-",
                    (au ? on || k < 2u : k != 2u || on) && b[k][0] ? VAL(k) : T_DIM);
}
/* LIVE VIZ CTRLS over the plate from canvas row y to bot. On the left the page's name and its elements (the chosen row in
 * the theme's colour), a bar beside each: its value now of its range, a tick where it is set. On the right the chosen
 * row's controls: where the page lies, or the element's SET, NOTES (the tracks on: their numbers), links and + ADD */
BTAPP_FN static void umx_draw(int32_t x0, int32_t y, int32_t bot)   /* (the segment's; none: the plate alone) */
{
    umx_desc_t d;
    char b[12], t[8];
    uint32_t pl = umx_cur(), rows = (uint32_t)clamp((bot - y - 12) / UMK_RH, 1, 9), i, k, li, row;
    const umc_rec_t *r;
    int32_t yy;
    if (!umx_desc(pl, &d))
        return;
    r = umx_rec(pl);
    for (i = umx_el >= rows ? umx_el - rows + 1u : 0u, k = i + rows, yy = y; i <= d.nctl + d.nx && i < k; i++, yy += UMK_RH) {
        uint16_t col = i == umx_el ? T_THEME : T_TEXT;
        uint32_t e = umx_jat(&d, i);
        if (!i) {                                       /* (its name, ruled off from its elements) */
            umk_text(6 - x0, yy, d.name, col);
            cv_rect(6 - x0, yy + 14, 102, 1, T_LINE);
            continue;
        }
        if (e == UMB_NCTL || (e > UMB_NCTL && umx_tog(&d, e)))   /* (the other elements: a rule over the first, each
                                                                  * toggle's name in full, its parameters set in) */
            cv_rect(6 - x0, yy, 102, 1, T_LINE);
        umk_text((e >= UMB_NCTL && !umx_tog(&d, e) ? 14 : 6) - x0, yy, umx_en(&d, e), col);
        {                                               /* its bar: 40 px its range */
            uint32_t m = umx_max(pl, e), v = umc_ev(r, e, umx_c(&d, e)->def, m) * 40u / m,
                     sv = umc_raw(r, e, umx_c(&d, e)->def, m) * 39u / m;
            cv_rect(68 - x0, yy + 7, 40, 1, T_LINE);
            cv_rect(68 - x0, yy + 6, (int32_t)v, 3, col);
            cv_rect(68 + (int32_t)sv - x0, yy + 4, 1, 7, T_TEXT);
            for (m = 0, v = 0; m < UMC_NL && v < 3u; m++)   /* a dot a link (three at most), a longer one its tracks chosen */
                if ((r->ls[m] & 31u) && umc_lj(r, m) == e)
                    cv_rect(111 + (int32_t)v++ * 3 - x0, yy + 6, 2, 2, T_MID);
            if (e < UMB_NCTL && r->t[e])
                cv_rect(111 - x0, yy + 10, 8, 1, T_MID);
        }
    }
    if (!umx_el) {                                      /* its kind (a TIMELINE cell: its row and track), its preset */
        str_cpy(t, "ROW ", sizeof t);
        fmt_int(t + 4, (int32_t)ui.song_row + 1);
        umk_text(124 - x0, y, umv_on() ? t : "KIND", T_MID);
        str_cpy(b, umv_on() ? UMV_TN[umv_tk - 1u] : umx_kind(pl), sizeof b);
        umk_text(234 - text_w(&AF_S, b) - x0, y, b, pl > UMX_BODY ? T_MID : T_THEME);
        umk_text(124 - x0, y + UMK_RH, "PRESET", T_MID);
        umx_pre_name(pl, umx_pre(pl), b);
        umk_text(234 - text_w(&AF_S, b) - x0, y + UMK_RH, b, T_THEME);
        i = umx_place(pl);                              /* (a plate on a layer with no room beside the ground's: it waits) */
        if (i >= 2u && pl < UMB_NPLT && !umb_lay[i - 1u].pp)
            umk_text(124 - x0, y + 2 * UMK_RH, "NO ROOM", T_DIM);
    } else
        for (i = umx_ct >= rows ? umx_ct - rows + 1u : 0u, k = i + rows, yy = y; i < k; i++, yy += UMK_RH) {
            uint16_t col = i == umx_ct ? T_THEME : T_TEXT;
            uint32_t j = umx_jat(&d, umx_el);
            row = umx_row(r, j, UMX_NT(d, j), i, &li);
            if (row == 4u)
                break;
            b[0] = 0;
            if (row == 0u) {
                str_cpy(t, "SET", sizeof t);
                umx_fmt(b, pl, j, umx_raw(pl, j));
            } else if (row == 1u) {
                str_cpy(t, "NOTES", sizeof t);
                for (li = 0; li < NTRK; li++)
                    b[li] = (char)(r->t[j] >> li & 1u ? '1' + li : '.');
                b[NTRK] = 0;
                if (!r->t[j])
                    str_cpy(b, "AUTO", sizeof b);
            } else if (row == 2u) {
                umx_src(b, r->ls[li] & 31u);
                umk_text(124 - x0, yy, b, col);
                b[0] = r->ld[li] > 0 ? '+' : 0;
                fmt_int(b + (r->ld[li] > 0), r->ld[li]);
                t[0] = 0;
            } else {
                str_cpy(t, "+ ADD", sizeof t);
                col = i == umx_ct ? T_THEME : T_MID;
            }
            umk_text(124 - x0, yy, t, col);
            umk_text(234 - text_w(&AF_S, b) - x0, yy, b, col);
        }
}
/* HOME > UMBRA > PLATES (the user, 2026-11-16: no more boxes sliding with still pictures; "a page that makes it clear and
 * easy to see what ground and veil plates are installed"): two columns over the picture, GROUND and VEILS, every plate
 * there is by name (CORONA, the bank's; BODIES with the veils), a mark by the two lying now, the chosen one in the theme's
 * colour; at the foot the chosen one's own line and the bank's slots taken. ALGORITHM (or KNOB 1) the next plate, laid
 * where its kind goes */
BTAPP_FN static void umx_list(int32_t x0, int32_t y, int32_t bot)   /* (in the Bluetooth segment: the app slot is full; no
                                                                     * segment: the plate alone) */
{
    char b[44];
    uint32_t pl, n[2] = {0, 0}, at[2] = {0, 0}, cur = umx_cur(), rows = (uint32_t)clamp((bot - y - 36) / 14, 1, 10), used = 0, v;
    const umb_plate_t *p;
    int32_t yy;
    if (cur > UMX_BODY)
        cur = umx_pg = (uint8_t)(umb_pl() != UMB_NONE ? umb_pl() : 4u);
    for (pl = 0; pl <= UMX_BODY; pl++)                  /* (where the chosen one stands in its column: the column scrolls) */
        if (pl == UMX_BODY || umb_has(pl)) {
            v = (uint32_t)umx_veil(pl);
            if (pl == cur)
                at[v] = n[v];
            n[v]++;
            used += pl >= UMP_NPL && pl < UMB_NPLT;
        }
    for (pl = 0; pl < UMP_NPL; pl++)                    /* (a plate known by name lies in a slot too, CORONA apart) */
        used += pl != 4u && umb_has(pl);
    for (v = 0; v < 2u; v++) {
        int32_t x = (v ? 126 : 8) - x0;
        uint32_t i = 0, first = at[v] >= rows ? at[v] - rows + 1u : 0u;
        cv_text(x, y, &AF_X, v ? "VEILS" : "GROUND", T_MID);
        cv_rect(x, y + 10, 106, 1, T_LINE);
        for (pl = 0, yy = y + 13; pl <= UMX_BODY; pl++) {
            if (!(pl == UMX_BODY || umb_has(pl)) || (uint32_t)umx_veil(pl) != v)
                continue;
            if (i >= first && i < first + rows) {
                if (umx_place(pl))                      /* (lying now) */
                    cv_rect(x, yy + 5, 5, 5, T_THEME);
                umk_text(x + 10, yy, umb_pname(pl)[0] ? umb_pname(pl) : UMX_PN[0], pl == cur ? T_THEME : T_TEXT);
                yy += 14;
            }
            i++;
        }
    }
    b[0] = 0;
    p = cur < UMB_NPLT ? umb_mod(cur) : 0;
    str_cpy(b, !p ? "The sounds' own figures" : p->api >= 4u ? p->desc : "", 41);
    cv_text(8 - x0, bot - 21, &AF_X, b, T_TEXT);
    str_cpy(b, "BANK ", 8);
    fmt_frac(b + 5, (int32_t)used, (int32_t)UMB_NSLOT);
    cv_text(8 - x0, bot - 10, &AF_X, b, T_MID);
    str_cpy(b, "PALETTE 1", 12);
    b[8] = (char)('1' + umc.palcur % UMP_NPAL);
    cv_text(232 - text_w(&AF_X, b) - x0, bot - 10, &AF_X, b, T_MID);
}
/* the page over the plate from canvas row y (h rows). LIVE AUDIO CTRLS: the selected track's four slots, then what each
 * macro moves (its routes on every track: "T2 CUT", as many as fit); LIVE VIZ CTRLS: umx_draw. At the foot what OCT- and
 * OCT+ do (umk_oct) */
static void umk_draw(int32_t x0, int32_t y, int32_t h)
{
    uint8_t ha = cv_halo;
    int32_t bot = y + h;
    cv_halo = 1;
    if (!bt_hw_seg()) {}
    else if (ui.hset[0] == HOME_PLATES)
        umx_list(x0, y, bot);
    else if (ui.hset[0] == HOME_COLOR)
        umcl_draw(x0, y, bot);
    else {
        umx_draw(x0, y, bot);
        cv_text(8 - x0, bot - 10, &AF_X, "Oct -  remove", T_MID);
        cv_text(232 - text_w(&AF_X, "add  Oct +") - x0, bot - 10, &AF_X, "add  Oct +", T_MID);
    }
    cv_halo = ha;
}

/* --------------------------------------------------------------------- SAVE > SONG > TIMELINE's picture tracks --- */
/* ALGORITHM on TIMELINE: past the last sound track MAIN, then OVER; back the same way. In an open cell: its plate.
 * 1: taken */
static int umv_algo(int32_t s)
{
    if (ui.home || !SONG_TRK(cur_page()))
        return 0;
    if (umv_on() && umv_ed == 2u)                       /* (its transition: no plate turned) */
        return 1;
    if (umv_on())
        umv_turn(umv_cc(), umv_tk - 1u, s);
    else if (umv_tk)
        umv_tk = (uint8_t)clamp((int32_t)umv_tk + (s > 0 ? 1 : -1), 0, 3);   /* (3: COLOR, the row's palette) */
    else if (s > 0 && (uint32_t)song.sel + 1u >= trk_live)
        umv_tk = 1;
    else
        return 0;
    umv_sync();
    return 1;
}
/* a knob there. KNOB 4 on a picture track: its cell's plate. In an open cell LIVE VIZ CTRLS' knobs for its plate (on its
 * name's row KNOB 3 the cell's preset; a cell with no plate: none). 1: taken */
static int umv_knob(uint32_t k, int32_t s)
{
    umc_cell_t *c;
    if (!umv_tk || !(umv_on() || k == 3u))
        return 0;
    if (umv_tk == 3u) {                                 /* COLOR: the row's palette (--: as the row before it) */
        uint8_t *p = &umc.cp[song_cur % NSONG_USE][ui.song_row % CHAIN_ROWS];
        *p = (uint8_t)clamp((int32_t)*p + s, 0, UMP_NPAL);
        umc_touch();
        umv_sync();
        return 1;
    }
    c = umv_cc();
    if (umv_on() && umv_ed == 2u)                       /* its transition: KNOB k its field, a detent a place */
        umv_trd = (uint16_t)umv_ts(umv_trd, k, (uint32_t)clamp((int32_t)umv_tf(umv_trd, k) + s, 0, (k & 1u) ? (int32_t)UMV_NLS - 1 :
                                                                       !k && umv_tk == 2u ? 4 : 3));   /* (ON ENV: OVER's) */
    else if (!umv_on())
        umv_turn(c, umv_tk - 1u, s);
    else if (umx_pg != UMB_NONE && (umx_el || !k)) {
        if (bt_hw_seg())
            umx_knob(k, s);
    }
    else if (k == 2u)
        return umk_preset(s);
    umv_sync();
    return 1;
}
/* PRESETS: an open cell's preset; LIVE VIZ CTRLS' or PLATES' page's. 1: taken */
static int umk_preset(int32_t s)
{
    if (umv_on()) {
        if (umx_pg != UMB_NONE && umv_ed == 1u) {
            umc_cell_t *c = umv_cc();
            c->pre = (uint8_t)((c->pre & ~3u) | (uint32_t)clamp((int32_t)UMC_CPRE(c) + s, 0, UMC_NPRE - 1));
            umc_touch();
            umv_sync();
        }
        return 1;
    }
    if (!umk_on())
        return 0;
    if (ui.hset[0] == HOME_COLOR) {                     /* (COLOR: the palette) */
        umcl_knob(2u, s);
        ui.force = 1;
        return 1;
    }
    umx_pre_set(umx_cur(), (uint32_t)clamp((int32_t)umx_pre(umx_cur()) + s, 0, UMC_NPRE - 1));
    ui.force = 1;
    return 1;
}
/* each input pass, the octave keys' taps there: OCT+ on a picture track opens its cell (the store saved first, so OCT-
 * can bring it back; not while the sound plays: a save stops it), then its transition, then keeps it all; OCT- a step
 * back, nothing kept; so does leaving the page or the song starting. What is left of the taps for the page (OCT+ ADD ROW,
 * OCT- DELETE) */
/* (the user, 2026-11-13) GENE's picture: a song made brings its own. Each row (a part: its tracks muted or not) reads as
 * lively as the song's ENERGY en (0..100) by the tracks playing in it, half that with no drums; its MAIN plate the one
 * there whose own liveliness (umb_plate_t.energy) is nearest, leaning a way of the song's own (sd: its dice); a lively
 * row an OVER plate too, another one; a cell only where the plate changes, its preset the song's and the section's, its
 * transition slow fades for a quiet row, blocks for a lively one. The drums' hits (track dr; none: the beat) push every
 * plate's SPEED, and GLITCH and MOSAIC the livelier the song: in SCREEN's and EFFECTS' last links, GENE's own. No rows
 * (a loop): one part, shown now */
#define UMV_GL (UMC_NL - 3u)                            /* GENE's links: a record's last three */
BTAPP_FN static void umv_gene(uint32_t en, uint32_t dr, uint32_t sd)
{
    uint32_t r, tk, k, n = chain_config.count, last[2] = {0, 0}, src = dr < NTRK ? UMQ_HIT + dr : UMQ_BEAT;
    umc_rec_t *rc;
    umc_cell_t *c = umv_cell(0, 0);
    umv_req = 0;                                        /* (the new song's "its cells none", still waiting: these are they) */
    memset(c, 0, sizeof umc.cell[0]);
    for (r = 0; r < (n ? n : 1u); r++) {
        const scene_t *sc = &song_idx.scene[song_cur][r];
        uint32_t m = n ? (sc->mute & 15u) | (sc->on & 6u) << 3 : 0u, a = 1, e;
        for (k = 0; k < trk_live; k++)
            a += !((m >> k) & 1u);
        e = en * a / (trk_live + 1u) >> (dr < NTRK && ((m >> dr) & 1u));
        for (tk = 0; tk < 2u; tk++, c++) {
            uint32_t key = UMC_OFF, bd = ~0u, pl;
            int32_t want = (int32_t)e + (int32_t)((sd >> (8u * tk)) % 31u) - 15;
            for (pl = 0; pl < UMB_NPLT && (!tk || e >= 50u); pl++) {
                umx_desc_t ds;
                int32_t d;
                if (!umx_desc(pl, &ds) || !umx_can(pl, 1u + tk) || (tk && umx_key(ds.name) == last[0]))   /* (OVER: not MAIN's) */
                    continue;
                d = want - (umb_mod(pl)->api >= 5u ? (int32_t)umb_mod(pl)->energy : 50);
                if (d < 0)
                    d = -d;
                if ((uint32_t)d < bd) {
                    bd = (uint32_t)d;
                    key = umx_key(ds.name);
                }
            }
            if (key != last[tk]) {
                c->key = (uint16_t)(last[tk] = key);
                c->pre = (uint8_t)(((sd >> 4) ^ chain_config.row[r].slot) & 3u);
                c->tr = (uint8_t)(e < 34u ? 233u : e > 60u ? 6u : 0u);   /* (umv_ts: FADE 4 S in, FADE 2 S out; BLOCKS 0.5 S in) */
            }
        }
    }
    rc = umc_rec(UMX_PN[1], 2u, 6u);                    /* (SCREEN's and EFFECTS' own records: BASIC's) */
    rc->ls[UMV_GL] = (uint8_t)(src | 1u << 5);          /* SPEED */
    rc->ld[UMV_GL] = (int8_t)(en / 3u);
    rc = umc_rec(UMX_PN[2], 3u, 5u);
    rc->ls[UMV_GL] = (uint8_t)(src | 2u << 5);          /* GLITCH */
    rc->ld[UMV_GL] = (int8_t)(en > 45u ? (en - 35u) / 2u : 0u);
    rc->ls[UMV_GL + 1u] = (uint8_t)(src | 1u << 5);     /* MOSAIC */
    rc->ld[UMV_GL + 1u] = (int8_t)(en > 75u ? (en - 60u) / 3u : 0u);
    umv_apply(0);
    umc_gen++;
    umc_touch();
}
/* SAVE > PROJECT > TOOLS' CLEAR PLATES: the song's picture gone: its rows' cells, the OVER plate, GENE's links */
BTAPP_FN static void umv_clear(void)
{
    uint32_t i;
    umc_load();
    memset(umc.cell[song_cur % NSONG_USE], 0, sizeof umc.cell[0]);
    for (i = 0; i < UMC_NREC; i++)
        memset(umc.rec[i].ls + UMV_GL, 0, 3);
    umc.lay[0] = umx_vk = 0;
    umc_gen++;
    umc_touch();
    ui_message("PLATES CLEARED");
}
static void umv_undo(void)                              /* the cell closed, the store as it was when it was opened */
{
    umc.magic = 0;
    umc_load();
    umc_dirty = 0;
    umb_plate = umv_bp;
    umv_ed = 0;
}
static uint32_t umv_input(uint32_t oct)
{
    if (ui.home || !SONG_TRK(cur_page()) || !umv_tk || umv_tk == 3u || chain_busy() || !chain_config.count) {
        if (umv_ed)
            umv_undo();
        return oct;
    }
    if (!oct || ui.menu || ui.confirm)
        return oct;
    if (!(oct & 2u)) {
        if (!umv_ed)
            return oct;
        if (umv_ed == 2u)
            umv_ed = 1;
        else
            umv_undo();
    } else if (!umv_ed) {
        if (umc_dirty && (umc_dirty = (uint8_t)(umc_flash(&umc, sizeof umc, 1) != 0)) != 0) {
            ui_message("STOP THE SOUND FIRST");
            return 0;
        }
        umv_bp = umb_plate;
        umv_ed = 1;
    } else if (umv_ed == 1u && bt_hw_seg()) {           /* (no segment: no transitions, kept at once) */
        umv_trd = (uint16_t)umv_trv(umv_cc());
        umv_ed = 2;
    } else {
        if (umv_ed == 2u)
            umv_trput(umv_cc(), umv_trd);
        umc_touch();
        umv_ed = 0;
        ui_message("CELL KEPT");
    }
    umv_sync();
    return 0;
}
/* TIMELINE's right half: two previews, the ground and LAYER 1 as the cursor's row leaves them, under each its track, its
 * cell's plate and preset (while the song plays: the picture as it plays, and the plates' names); the track ALGORITHM
 * chose framed */
static void umv_tl(void)
{
    char b[12];
    uint32_t tk;
    if (!chain.running)
        umv_apply(ui.song_row);
    for (tk = 0; tk < 2u; tk++) {
        int32_t x = 124 + (int32_t)tk * 58;
        const umc_cell_t *c = umv_cell(ui.song_row, tk);
        if (!umb_thumb(tk, x, 38)) {
            cv_rect(x, 38, UMB_THS, UMB_THS, T_BG);
            if (tk && umx_lp[0] == UMX_BODY)            /* (the figures: their name) */
                cv_text(x + 4, 60, &AF_X, UMX_PN[umx_lp[0] - UMX_BODY], T_MID);
        }
        cv_frame(x - 1, 37, UMB_THS + 2, UMB_THS + 2, umv_tk == tk + 1u ? T_THEME : T_LINE);
        cv_text(x, 96, &AF_X, UMV_TN[tk], umv_tk == tk + 1u ? T_THEME : T_MID);
        umv_name(c, b);
        if (chain.running) {                            /* (the song plays: what it shows now, whatever the cursor's row says) */
            uint32_t pl = tk ? umx_lp[0] : umb_pl();
            umx_desc_t d;
            cv_text(x, 106, &AF_X, umx_desc(pl, &d) ? d.name : "--", T_TEXT);
        } else
            cv_text(x, 106, &AF_X, b, c->key ? T_TEXT : T_DIM);
        if (!chain.running && c->key && c->key != UMC_OFF) {
            umx_pre_name(umx_find(c->key), UMC_CPRE(c), b);
            cv_text(x, 116, &AF_X, b[2] ? b + 3 : b, T_MID);
        }
    }
    cv_text(124, 128, &AF_X, "COLOR", umv_tk == 3u ? T_THEME : T_MID);   /* the palette now: its six colours; the row's own
                                                         * word in the knob's column (--: as before it) */
    for (tk = 0; tk < UMB_NCOL; tk++) {
        const uint8_t *q = umc.pal[umc.palcur % UMP_NPAL][tk];
        cv_rect(164 + (int32_t)tk * 9, 128, 8, 8, RGB(q[0], q[1], q[2]));
    }
    b[0] = (char)('1' + umc.palcur % UMP_NPAL), b[1] = 0;
    cv_text(224, 128, &AF_X, b, umv_tk == 3u ? T_THEME : T_TEXT);
}
/* an open cell over TIMELINE's panel: the picture, LIVE VIZ CTRLS for the cell's plate over it. 0: none open */
static int umv_draw(void)
{
    uint8_t ha = cv_halo;
    char b[12];
    if (!umv_on())
        return 0;
    umb_backdrop(0, (int32_t)graph_y());
    cv_halo = 1;
    if (umv_ed == 2u)
        umv_trshow(0);
    else if (umx_pg == UMB_NONE) {
        umv_name(umv_cc(), b);
        cv_text(120 - text_w(&AF_M, b) / 2, 40, &AF_M, b, T_THEME);
        cv_text(120 - text_w(&AF_X, "ALGORITHM: A PLATE") / 2, 66, &AF_X, "ALGORITHM: A PLATE", T_MID);
    } else if (bt_hw_seg())
        umx_draw(0, 4, (int32_t)graph_h() - 6);
    cv_text(8, (int32_t)graph_h() - 10, &AF_X, "Oct -  back", T_MID);
    cv_text(232 - text_w(&AF_X, "keep  Oct +"), (int32_t)graph_h() - 10, &AF_X, umv_ed == 2u ? "keep  Oct +" : "next  Oct +", T_TEXT);
    cv_halo = ha;
    return 1;
}
