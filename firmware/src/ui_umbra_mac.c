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
 *   LIVE VIZ CTRLS    a page at a time (ALGORITHM): every plate there is, then BODIES (UMBRA's figures), SCREEN (LIGHT
 *                     SPEED REACT MOVE TRAVEL DISSLV), EFFECTS and BUTTONS. On the left the page's name and under it its
 *                     elements (KNOB 1), each with a bar: its value now, moving as its controls move it. On the right
 *                     the chosen row's controls (KNOB 2), as many as it takes:
 *                       the name's row   its KIND: GROUND (a plate for MAIN) or VEIL (a plate or the figures for a layer:
 *                                        OVER, LAYER 3); NO ROOM under it: a layer's plate waiting, its state not fitting
 *                                        beside the ground's. (No WHERE since 2026-11-13: a page lies where its kind says)
 *                       SET              the element's own value (KNOB 4)
 *                       NOTES            (an element the notes drive) the tracks whose notes drive it: KNOB 3 the track,
 *                                        KNOB 4 on or off; none on: AUTO, the plate's own choice
 *                       a link           what else moves it: KNOB 3 its source (KNOB 1..4 = M1..M4, a track's HIT, LFO
 *                                        or STEP, LEVEL, BEAT, ENV TAP, LFO TAP, EDIT TAP: in TOTALITY a tap of that
 *                                        button, falling off over 0.4 s; OFF: the link gone), KNOB 4 how far (-99 .. 99
 *                                        of the element's range with the source at full)
 *                       + ADD            KNOB 3: a new link
 *                     ALGORITHM's plate, lying nowhere, is laid where its kind goes (a ground: MAIN; a veil: OVER; BODIES:
 *                     LAYER 3): the picture behind the page is the page's.
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

/* ------------------------------------------------------------------------------------- MACROS - AUDIO --- */
static const uint8_t UMA_SR[MS_N] = {MS_OFF, MS_M1, MS_M1 + 1, MS_M1 + 2, MS_M1 + 3, MS_LFO, MS_STEP, MS_ENV, MS_ENV2,
                                     MS_VEL, MS_KEY, MS_RAND, MS_NOISE, MS_MODW, MS_AT, MS_EXPR, MS_FOLLOW, MS_PROX};
static uint8_t uma_slot;                                /* LIVE AUDIO CTRLS: the selected track's slot chosen, 0..3 */
static int16_t *uma_p(uint32_t s) { return &trk[s / 4u % NTRK].p[P_M1SRC + 3u * (s % 4u)]; }   /* SRC DST AMT */
static int umk_preset(int32_t s);
static uint32_t uma_cur(void) { return (uint32_t)song.sel % NTRK * 4u + uma_slot % 4u; }
static uint32_t uma_rank(uint32_t s)                    /* slot s's source's place in UMA_SR */
{
    uint32_t r, v = (uint32_t)uma_p(s)[0];
    for (r = 0; r < MS_N && UMA_SR[r] != v; r++) {}
    return r;
}
static void uma_knob(uint32_t k, int32_t s)             /* KNOB k turned s: the SLOT, its SOURCE, DEST, AMOUNT */
{
    int16_t *p = uma_p(uma_cur());
    if (k == 0u)
        uma_slot = (uint8_t)clamp((int32_t)uma_slot + s, 0, 3);
    else if (k == 1u)                                   /* SOURCE, a detent a source (OFF first) */
        p[0] = UMA_SR[clamp((int32_t)uma_rank(uma_cur()) + s, 0, MS_N - 1)];
    else {                                              /* DEST, AMOUNT: as LFO > MOD turns them */
        const param_desc_t *pd = &TP[P_M1SRC + k - 1u];
        p[k - 1u] = (int16_t)param_turn(pd, p[k - 1u], accel(EN_K1 + k, s, pd->fmt == F_ENUM ? 0 : pd->max - pd->min));
    }
}

/* ------------------------------------------------------------------------------------ LIVE VIZ CTRLS --- */
static struct {
    uint16_t hit[NTRK], tap[5];                         /* each track's hits, the five taps, falling off (Q8) */
    uint32_t qr, ms;                                    /* the notes read; the last frame's time */
} umq __attribute__((section(".pool")));
static uint8_t umx_pg = 0xFFu;                          /* the page chosen (a plate's number, UMX_*; 0xFF: not yet) */
static uint8_t umx_el;                                  /* .. its row: 0 its name (where it lies), then its elements */
static uint8_t umx_ct;                                  /* .. the row's control: SET, NOTES, its links, + ADD */
static uint8_t umx_tr;                                  /* .. NOTES: the track under KNOB 3 */
static uint8_t umx_sd;                                  /* PLATES: the boxes sliding (1 leftwards: the next plate's from the */
static uint32_t umx_sms;                                /* right; 2 the other way), since when */
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
static int umt_lay_on(uint32_t f)                       /* layer f (UMX_BF: MAIN, OVER, LAYER 3) has something */
{
    umc_load();
    return f ? umc.lay[f * 2u - 2u] != 0u : umb_pick() != UMB_NONE;
}
/* each input pass in TOTALITY (none over it): the five buttons. A layer's: tapped, it hides or shows (LAYER 3 shown with
 * nothing there: BODIES laid on it); held with ALGORITHM turned, the next plate there (umt_algo; let go: no tap; held
 * half a second or more: none either). TAP's:
 * tapped, its links fire (held: its layer, ui_layer.c). ENV and ARP are no page's here (ui_input.c lets them be) */
static void umt_btns(void)
{
    uint32_t i, f;
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
                    if (f == 2u && !umt_lay_on(2u)) {
                        umx_place_set(UMX_BODY, 4u);
                        umt.lh |= 4u;                   /* (shown below: it fades in from where it was) */
                    }
                    umt.lh ^= (uint8_t)(1u << f);
                    umt_say(UMX_BF[f], umt.lh >> f & 1u ? "OFF" : "ON");
                }
            }
            umt.bt[i] = 0;
            umt.bh &= (uint8_t)~(1u << i);
        }
    }
}
/* (ui_input.c) ALGORITHM turned s in TOTALITY: with a layer's button held (the user, 2026-11-13), the next plate on that
 * layer (MAIN: a ground; OVER, LAYER 3: a veil lying nowhere else), shown if it was hidden; that button's let go is no
 * tap. 1: taken */
static int umt_algo(int32_t s)
{
    uint32_t i, f;
    for (i = 0; i < 5u; i++)
        if (umt.bt[i] && (f = umt_bf(i)) < 3u) {
            umt_select(s, f | 4u);
            umt.bh |= (uint8_t)(1u << i);
            return 1;
        }
    return 0;
}
/* (ui_input.c ui_leds) button i lit: its layer shows something; TAP: its tap lasting */
static int umt_blit(uint32_t i)
{
    uint32_t f = umt_bf(i);
    return f == 3u ? umq.tap[UMT_BQ[i]] != 0u : !(umt.lh >> f & 1u) && umt_lay_on(f);
}
/* SAVE > SONG > TIMELINE's picture tracks (the user, 2026-11-13): past T6, ALGORITHM reaches MAIN and OVER, a cell a row
 * each: the plate that is the ground, and the one on LAYER 1, from that row on (umv_apply). KNOB 4 there turns the cell's
 * plate; OCT+ opens the cell: LIVE VIZ CTRLS for its plate and its preset, over the picture (ALGORITHM the plate, PRESETS
 * the preset, the knobs as on that page). OCT+ there: its transition (umv_ed 2), KNOB 1 how the plate comes IN, 2 how
 * long, 3 how it goes OUT, 4 how long; OCT+ again: all of it kept. OCT- a step back with nothing kept (the user,
 * 2026-11-13): from the transition its four as they were, from the cell the store as it was when the cell was opened
 * (read again from flash: it was saved as the cell opened, and nothing is saved while one is open, umv_poll) */
static uint8_t umv_trd, umv_bp;                         /* the transition being set; the ground's plate as the cell was opened */
static const char *const UMV_TY[4] = {"DISSOLVE", "FADE", "BLOCKS", "CUT"};
static const char *const UMV_LS[4] = {"0.5 S", "1 S", "2 S", "4 S"};
/* a transition byte's field k as its place in the lists: 0 IN's kind (UMV_TY), 1 IN's length (UMV_LS), 2 OUT's kind,
 * 3 OUT's length; and the field's two bits for a place (a new cell's 0: IN a DISSOLVE of 1 s, OUT a CUT) */
static uint32_t umv_tf(uint32_t v, uint32_t k)
{
    v = v >> 2u * k & 3u;
    return k == 2u ? (v + 3u) & 3u : (k & 1u) ? v ^ 1u : v;
}
static uint32_t umv_tb(uint32_t i, uint32_t k) { return k == 2u ? (i + 1u) & 3u : (k & 1u) ? i ^ 1u : i; }
/* once a frame (umb_clock): the transitions now. A plate come in with its row (umv_st.t0, umv_poll) comes IN over its
 * length; one the song's next row replaces (a cell there) goes OUT over the end of this row. FADE: the track's light;
 * DISSOLVE: TOTALITY's gate (going: the track dark behind it); BLOCKS: MOSAIC, from or to its coarsest */
BTAPP_FN static void umv_tick(void)                     /* (in the Bluetooth segment, as PLATES; no segment: every plate cut) */
{
    uint32_t tk, left = ~0u, b = beat_samples(), mb = 60000u / (uint32_t)clamp(song.g[G_BPM], 20, 400);
    umv_mo = 0;
    umv_gt = 256u;
    uint32_t r = umv_st.row;                            /* (the row whose plates show) */
    if (chain.running && !chain.hold && b > mb && r + 1u < chain.config.count)   /* ms to the row's end; past it, the next */
        left = chain.row != r ? 0u : ((chain.remaining ? chain.remaining : 1u) * chain_bar() - chain.bar_pos) / (b / mb);   /* row's
                                                         * plates not in yet: gone still */
    for (tk = 0; tk < 2u; tk++) {
        uint32_t v = umv_st.tr[tk], ty = umv_tf(v, 2u), ln = 500u << umv_tf(v, 3u), p = left, out = 1, lt = 256u;
        if (left >= ln || !umv_cell(r + 1u, tk)->key) { /* (not going: coming in?) */
            out = 0;
            ty = umv_tf(v, 0u);
            ln = 500u << umv_tf(v, 1u);
            p = umv_st.t0[tk] ? fm1_ms - umv_st.t0[tk] : ln;
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
    if (!chain.running)
        umv_apply(ui.song_row);
    if (umv_ed && umv_tk && (pl = umx_find(umv_cc()->key)) != umx_pg) {   /* (umv_ed: 1 the cell, 2 its transition) */
        umx_pg = (uint8_t)pl;
        umx_el = umx_ct = 0;
    }
    ui.force = 1;
}
static uint32_t umk_au(void) { return !umv_on() && ui.hset[0] == 1u; }   /* LIVE AUDIO CTRLS shown */
static uint32_t umx_cur(void)                           /* the page chosen (at first the ground's plate; none: SCREEN) */
{
    umx_desc_t d;
    if (!umx_desc(umx_pg, &d))
        umx_pg = (uint8_t)(umb_pl() == UMB_NONE ? UMX_SCREEN : umb_pl());
    return umx_pg;
}
/* the chosen element j's control row ct: 0 SET, 1 NOTES (notes: it has one), 2 a link (*li: which), 3 + ADD (*li: a free
 * link), 4: past its last */
#define UMX_NT(d, j) ((d).el ? (int)((d).el[j] & (UMB_EL_NOTES | 0x80u)) : 0)   /* (umx_row's notes) */
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
        if ((r->ls[i] & 31u) && (uint32_t)(r->ls[i] >> 5) == j && !--ct) {
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
        str_cpy(b, "KNOB 1", 12);
        b[5] = (char)('0' + s);
    } else if (s >= UMQ_LEVEL)
        str_cpy(b, SL[s - UMQ_LEVEL], 12);
    else {
        b[0] = 'T', b[1] = (char)('1' + (s - UMQ_HIT) % NTRK), b[2] = ' ';
        str_cpy(b + 3, SN[(s - UMQ_HIT) / NTRK], 9);
    }
}
static void umx_fmt(char *b, uint32_t pl, uint32_t j, uint32_t v)   /* page pl's element j's value v, as words */
{
    if (umx_max(pl, j) == 3u)
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
/* page pl can lie at w (0 nowhere, 1 MAIN, 2 .. 4 LAYER 1..3): nowhere, any (SCREEN, EFFECTS and BUTTONS never: they are
 * every plate's); MAIN a ground, a layer a veil. LAYER 2 is not offered (the user, 2026-11-13: too hard to find, dropped
 * for now; what lay there lies nowhere: umc_load) */
static int umx_can(uint32_t pl, uint32_t w)
{
    return pl <= UMX_BODY && w != 3u && (!w || (w == 1u) != umx_veil(pl));
}
/* ALGORITHM: the next page there is of the first lim, round. A plate lying nowhere is laid where its kind goes (a
 * ground MAIN, a veil OVER, BODIES LAYER 3): the picture behind the page is the one the page is about (the user,
 * 2026-11-13) */
static void umx_algo(int32_t s, uint32_t lim)
{
    umx_desc_t d;
    uint32_t k, pl = umx_cur() % lim;
    for (k = 0; k < lim; k++)
        if (umx_desc(pl = (pl + (s > 0 ? 1u : lim - 1u)) % lim, &d))
            break;
    umx_pg = (uint8_t)pl;
    umx_el = umx_ct = 0;
    if (!umx_place(pl) && pl <= UMX_BODY)
        umx_place_set(pl, pl == UMX_BODY ? 4u : umx_veil(pl) ? 2u : 1u);
}
/* KNOB k turned s: 1 the row, 2 its control, 3 the track or the source, 4 the value */
static void umx_knob(uint32_t k, int32_t s)
{
    umx_desc_t d;
    uint32_t pl = umx_cur(), j, li = 0, row, w;
    umc_rec_t *r;
    int notes;
    if (!umx_desc(pl, &d))
        return;
    if (k == 0u) {
        umx_el = (uint8_t)clamp((int32_t)umx_el + s, 0, (int32_t)d.nctl);
        umx_ct = 0;
        return;
    }
    if (!umx_el) {                                      /* the name's row: its preset */
        if (k == 2u)
            umx_pre_set(pl, (uint32_t)clamp((int32_t)umx_pre(pl) + s, 0, UMC_NPRE - 1));
        return;
    }
    j = umx_el - 1u;
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
        r->ls[li] = (uint8_t)(w ? w | j << 5 : 0u);
        for (umx_ct = (uint8_t)(1u + (uint32_t)notes), row = 0; w && umx_ct < 2u + UMC_NL; umx_ct++)   /* (its row now; gone: */
            if (umx_row(r, j, notes, umx_ct, &row) == 2u && row == li)                          /* the row after SET, NOTES) */
                break;
    } else if (row == 2u)
        r->ld[li] = (int8_t)clamp(r->ld[li] + accel(EN_K1 + k, s, 198), -99, 99);
    umc_touch();
}

/* ------------------------------------------------------------------------------------------- the pages --- */
static int umk_on(void) { return UMB_ON && !umt.on && home_bare() && ui.hset[0]; }   /* a LIVE CTRLS page or PLATES shown */
/* LIVE AUDIO CTRLS' row i's words, a column each (slot i of the selected track): its number, the SOURCE, the DEST, the
 * AMOUNT; 1: it acts (else drawn dim) */
static int umk_names(uint32_t i, char (*b)[12])
{
    const int16_t *p = uma_p((uint32_t)song.sel % NTRK * 4u + i);
    const char *u;
    fmt_int(b[0], (int32_t)i + 1);
    str_cpy(b[1], N_MSRC[clamp(p[0], 0, MS_N - 1)], 12);
    str_cpy(b[2], mod_dst_name(&trk[song.sel % NTRK], p[1]), 12);
    param_format(&TP[P_M1AMT], p[2], b[3], &u);        /* (+50: LFO > MOD's) */
    return p[0] && p[1] && p[2];
}
/* LIVE VIZ CTRLS' chosen row and control as words: the row (the page's name, an element), the control, the track or the
 * source, the value. 1: KNOB 3 has something to turn there */
static int umx_names(char (*b)[12])
{
    umx_desc_t d;
    uint32_t pl = umx_cur(), j = umx_el - 1u, li = 0, row;
    const umc_rec_t *r;
    b[1][0] = b[2][0] = b[3][0] = 0;
    if (!umx_desc(pl, &d)) {
        b[0][0] = 0;
        return 0;
    }
    str_cpy(b[0], umx_el ? d.ctl[j].name : d.name, 12);
    if (!umx_el) {
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
static void umk_algo(int32_t s)                         /* ALGORITHM: the track (LIVE AUDIO CTRLS), the page (LIVE VIZ CTRLS) */
{
    if (ui.hset[0] != 1u) {
        umx_algo(s, ui.hset[0] == 3u ? UMB_NPLT : UMX_N);
        umx_sd = (uint8_t)(s > 0 ? 1u : 2u);            /* (PLATES: the boxes slide) */
        umx_sms = fm1_ms;
        return;
    }
    track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
}
static void umk_knob(uint32_t k, int32_t s)
{
    if (ui.hset[0] == 3u) {                             /* PLATES: the macros M1..M4, as TOTALITY's */
        macro_v[k & 3u] = (uint8_t)clamp(macro_v[k & 3u] + accel(EN_K1 + k, s, 127), 0, 127);
        return;
    }
    if (ui.hset[0] == 1u)
        uma_knob(k, s);
    else
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
    int16_t *p;
    const char *m;
    if (!oct || !umk_on() || ui.hset[0] == 3u)
        return;
    if (ui.hset[0] == 1u) {
        if (oct & 2u)
            for (uma_slot = 0; uma_slot < 3u && uma_p(uma_cur())[0]; uma_slot++) {}
        p = uma_p(uma_cur());
        if (oct & 2u ? p[0] != 0 : !p[0])
            m = oct & 2u ? "NO FREE SLOT" : "NOTHING TO REMOVE";
        else {
            for (j = 0; j < 3u; j++)                    /* (OCT-: SOURCE DEST AMOUNT as a new project's) */
                p[j] = (int16_t)TP[P_M1SRC + j].def;
            if (oct & 2u)
                p[0] = MS_M1;
            m = oct & 2u ? "SLOT ADDED" : "SLOT REMOVED";
        }
    } else {
        if (!umx_desc(umx_cur(), &d) || !d.nctl || UMX_NT(d, 0) > 1)   /* (BUTTONS: SET alone) */
            return;
        if (!umx_el)
            umx_el = 1;
        j = umx_el - 1u;
        n = (uint32_t)UMX_NT(d, j);
        r = umx_recf(umx_pg, 1);
        if (oct & 2u) {
            umx_ct = (uint8_t)(1u + n);                 /* (the + ADD row: a free link) */
            while (umx_row(r, j, (int)n, umx_ct, &li) == 2u)
                umx_ct++;
            if (umx_row(r, j, (int)n, umx_ct, &li) != 3u)
                m = "NO FREE LINK";
            else {
                r->ls[li] = (uint8_t)(UMQ_M1 | j << 5);
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
                    (k & 1u) && umv_tf(umv_trd, k - 1u) == 3u ? T_DIM : VAL(k));
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
        if (umv_tf(umv_trd, k) != 3u)
            cv_text(232 - text_w(&AF_M, ln), yy, &AF_M, ln, T_TEXT);
    }
}
BTAPP_FN static void umk_cards(void)                             /* KNOB 1..4: the chosen row's columns */
{
    static const char *const LB[3][4] = {{"SLOT", "SOURCE", "DEST", "AMOUNT"}, {"ELEMENT", "CONTROL", "SOURCE", "VALUE"},
                                         {"PAGE", "", "PRESET", "KIND"}};
    char b[4][12];
    uint32_t k, au = umk_au(), nm;
    int on;
    if (umv_on() && umv_ed == 2u) {                     /* (a cell's transition) */
        umv_trshow(1);
        return;
    }
    nm = !au && !umx_el;
    if (umv_on() && umx_pg == UMB_NONE) {               /* (a cell with no plate: its word, nothing to turn) */
        umv_name(umv_cc(), b[0]);
        b[1][0] = b[2][0] = b[3][0] = 0;
        on = 0;
    } else
        on = au ? umk_names(uma_slot, b) : umx_names(b);
    for (k = 0; k < 4u; k++)
        text_column(k, !au && k == 2u && b[1][0] == 'N' ? "TRACK" : LB[nm ? 2u : !au][k], b[k][0] ? b[k] : "-",
                    (au ? on || k < 2u : k != 2u || on) && b[k][0] ? VAL(k) : T_DIM);
}
#define UMK_RH 17                                       /* a row's height (AF_S) */
static void umk_text(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text(x, y, &AF_S, s, c); }
/* LIVE VIZ CTRLS over the plate from canvas row y to bot. On the left the page's name and its elements (the chosen row in
 * the theme's colour), a bar beside each: its value now of its range, a tick where it is set. On the right the chosen
 * row's controls: where the page lies, or the element's SET, NOTES (the tracks on: their numbers), links and + ADD */
static void umx_draw(int32_t x0, int32_t y, int32_t bot)
{
    umx_desc_t d;
    char b[12], t[8];
    uint32_t pl = umx_cur(), rows = (uint32_t)clamp((bot - y - 12) / UMK_RH, 1, 9), i, k, li, row;
    const umc_rec_t *r;
    int32_t yy;
    if (!umx_desc(pl, &d))
        return;
    r = umx_rec(pl);
    for (i = umx_el >= rows ? umx_el - rows + 1u : 0u, k = i + rows, yy = y; i <= d.nctl && i < k; i++, yy += UMK_RH) {
        uint16_t col = i == umx_el ? T_THEME : T_TEXT;
        if (!i) {                                       /* (its name, ruled off from its elements) */
            umk_text(6 - x0, yy, d.name, col);
            cv_rect(6 - x0, yy + 14, 102, 1, T_LINE);
            continue;
        }
        umk_text(6 - x0, yy, d.ctl[i - 1u].name, col);
        {                                               /* its bar: 40 px its range */
            uint32_t m = umx_max(pl, i - 1u), v = umc_ev(r, i - 1u, d.ctl[i - 1u].def, m) * 40u / m,
                     sv = umc_raw(r, i - 1u, d.ctl[i - 1u].def, m) * 39u / m;
            cv_rect(68 - x0, yy + 7, 40, 1, T_LINE);
            cv_rect(68 - x0, yy + 6, (int32_t)v, 3, col);
            cv_rect(68 + (int32_t)sv - x0, yy + 4, 1, 7, T_TEXT);
            for (m = 0, v = 0; m < UMC_NL && v < 3u; m++)   /* a dot a link (three at most), a longer one its tracks chosen */
                if ((r->ls[m] & 31u) && (uint32_t)(r->ls[m] >> 5) == i - 1u)
                    cv_rect(111 + (int32_t)v++ * 3 - x0, yy + 6, 2, 2, T_MID);
            if (r->t[i - 1u])
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
            uint32_t j = umx_el - 1u;
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
/* HOME > UMBRA > PLATES (the user, 2026-11-13: a plate browser with descriptions; then: no knob row, the plates boxes in
 * a row, only the chosen one's playing, the boxes sliding as ALGORITHM turns): the chosen plate plays in the middle box
 * (the picture behind the page, the rest of it blacked out), its neighbours' boxes at the screen's edges with their names;
 * under the middle one its name, its own line (its file's, umb_plate_t.desc; an older file: none) and its preset;
 * at the foot the four macros (KNOB 1..4) and its place among the plates. ALGORITHM: the boxes slide a place over 0.2 s,
 * the new plate's box and its words coming in from the side. It is LIVE VIZ CTRLS' page too */
/* a plate's still picture (umb_plate_t.splash, API 5) at x, y on the canvas; a plate with none (CORONA, an older file):
 * a ring of light round a black disc */
#define UMX_SB (int32_t)UMB_SPL                          /* PLATES' side boxes: a picture's side */
BTAPP_FN static void umx_splash(const umb_plate_t *p, int32_t x, int32_t y)
{
    const uint8_t *sp = p->api >= 5u ? p->splash : 0;
    int32_t i, j;
    y += cv_oy;
    for (j = 0; j < UMX_SB; j++)
        for (i = 0; i < UMX_SB; i++) {
            uint32_t c;
            if ((uint32_t)(x + i) >= cv_w || (uint32_t)(y + j) >= cv_h)
                continue;
            if (sp) {
                c = (sp[32 + j * (UMB_SPL / 2u) + (i >> 1)] >> (i & 1 ? 0 : 4) & 15u) * 2u;
                c = sp[c] | (uint32_t)sp[c + 1u] << 8;
            } else {
                c = (uint32_t)((i - 32) * (i - 32) + (j - 32) * (j - 32));
                c = c < 144u ? 0u : 642816u / (c * c);  /* (31 at the limb, falling off fast) */
                c = swap16((uint16_t)(c << 11 | c << 6 | c));
            }
            cv_px[(uint32_t)(y + j) * cv_w + (uint32_t)(x + i)] = (uint16_t)c;
        }
}
#define UMX_BW 120                                      /* a box's width, and from one to the next */
#define UMX_BP (UMX_BW + 8)
BTAPP_FN static void umx_list(int32_t x0, int32_t y, int32_t bot)   /* (in the Bluetooth segment, as WELCOME's pages: the
                                                                     * app slot is full; no segment: the plate alone) */
{
    uint8_t ls[UMB_NPLT];
    char b[48];
    uint32_t pl, n = 0, at = 0, k, cur = umx_cur(), el = fm1_ms - umx_sms;
    int32_t bh = bot - y - 56, bx = 120 - UMX_BW / 2 - x0, i, x, m;
    const umb_plate_t *p;
    if (cur >= UMB_NPLT)
        cur = umx_pg = (uint8_t)(umb_pl() != UMB_NONE ? umb_pl() : 4u);
    for (pl = 0; pl < UMB_NPLT; pl++)
        if (umb_has(pl)) {
            if (pl == cur)
                at = n;
            ls[n++] = (uint8_t)pl;
        }
    if (umx_sd && el < 200u) {                          /* sliding: the way still to go */
        bx += (int32_t)(200u - el) * UMX_BP / 200 * (umx_sd == 1u ? 1 : -1);
        ui.force = 1;
    } else
        umx_sd = 0;
    cv_halo = 0;
    cv_rect(-x0, y - 4, 240, 4, T_BG);                  /* all but the middle box: black */
    cv_rect(-x0, y + bh, 240, bot - y - bh + 6, T_BG);
    cv_rect(-x0, y, bx + x0 > 0 ? bx + x0 : 0, bh, T_BG);
    cv_rect(bx + UMX_BW, y, 240, bh, T_BG);
    for (i = -2; i <= 2; i++) {
        x = bx + i * UMX_BP;
        p = umb_mod(ls[(at + n + (uint32_t)(i + 2) - 2u) % n]);
        if (!i) {
            cv_frame(x - 1, y - 1, UMX_BW + 2, bh + 2, T_THEME);
            continue;
        }
        if (i < 0)                                      /* (a neighbour: a smaller box against the middle one, its still */
            x += UMX_BW - UMX_SB;             /* picture in it, its name under it) */
        m = y + (bh - UMX_SB) / 2 - 6;
        cv_frame(x - 1, m - 1, UMX_SB + 2, UMX_SB + 2, T_LINE);
        umx_splash(p, x, m);
        cv_text(i < 0 ? x + UMX_SB - text_w(&AF_X, p->name) : x, m + UMX_SB + 4, &AF_X, p->name, T_MID);
    }
    p = umb_mod(cur);
    x = bx + UMX_BW / 2;
    cv_text(x - text_w(&AF_M, p->name) / 2, y + bh + 5, &AF_M, p->name, T_THEME);
    b[0] = 0;
    if (p->api >= 4u)                                   /* (an older file: no line) */
        str_cpy(b, p->desc, 40);
    cv_text(x - text_w(&AF_X, b) / 2, y + bh + 24, &AF_X, b, T_TEXT);
    str_cpy(b, umx_kind(cur), 8);                       /* (GROUND or VEIL, then the preset) */
    str_cpy(b + str_len(b), "  ", 4);
    umx_pre_name(cur, umx_pre(cur), b + str_len(b));
    cv_text(x - text_w(&AF_X, b) / 2, y + bh + 35, &AF_X, b, T_MID);
    for (k = 0; k < 4u; k++) {                          /* the macros: KNOB 1..4 */
        m = 8 + (int32_t)k * 46 - x0;
        cv_rect(m, bot - 4, 40, 1, T_LINE);
        cv_rect(m, bot - 5, macro_v[k] * 40 / 127, 3, T_THEME);
    }
    fmt_frac(b, (int32_t)at + 1, (int32_t)n);
    cv_text(232 - text_w(&AF_X, b) - x0, bot - 9, &AF_X, b, T_MID);
}
/* the page over the plate from canvas row y (h rows). LIVE AUDIO CTRLS: the selected track's four slots, then what each
 * macro moves (its routes on every track: "T2 CUT", as many as fit); LIVE VIZ CTRLS: umx_draw. At the foot what OCT- and
 * OCT+ do (umk_oct) */
static void umk_draw(int32_t x0, int32_t y, int32_t h)
{
    static const uint8_t CX[4] = {6, 26, 104, 186};
    uint8_t ha = cv_halo;
    char b[4][12], ln[44];
    uint32_t i, k, c, au = ui.hset[0] == 1u;
    int32_t bot = y + h;
    cv_halo = 1;
    if (ui.hset[0] == 3u) {
        if (bt_hw_seg())
            umx_list(x0, y, bot);
        cv_halo = ha;
        return;
    }
    if (!au)
        umx_draw(x0, y, bot);
    for (i = 0; au && i < 4u; i++, y += UMK_RH) {
        uint16_t col = umk_names(i, b) ? T_TEXT : T_DIM;
        if (i == uma_slot)
            col = T_THEME;
        for (c = 0; c < 4u; c++)
            if (c < 2u || b[1][0] != 'O' || b[1][1] != 'F')   /* (OFF: its number and its source alone) */
                umk_text(CX[c] - x0, y, b[c], col);
    }
    for (k = 0, y += 4; au && k < 4u && y + 20 <= bot; k++, y += 11) {   /* what each macro moves */
        uint32_t w = 0, more = 0;
        ln[0] = 0;
        for (i = 0; i < trk_live * 4u; i++) {
            const int16_t *p = uma_p(i);
            if (p[0] != (int16_t)(MS_M1 + k) || !p[1] || !p[2])
                continue;
            if (w > 26u) { more++; continue; }
            ln[w++] = 'T', ln[w++] = (char)('1' + i / 4u), ln[w++] = ' ';
            str_cpy(ln + w, mod_dst_name(&trk[i / 4u], p[1]), 9);
            w += str_len(ln + w);
            ln[w++] = ' ', ln[w++] = ' ', ln[w] = 0;
        }
        if (more) { ln[w++] = '+', ln[w++] = (char)('0' + (more > 9u ? 9u : more)), ln[w] = 0; }
        cv_text(CX[0] - x0, y, &AF_X, N_MSRC[MS_M1 + k], T_THEME);
        cv_text(CX[1] - x0, y, &AF_X, w ? ln : "-", w ? T_TEXT : T_DIM);
    }
    cv_text(8 - x0, bot - 10, &AF_X, "Oct -  remove", T_MID);
    cv_text(232 - text_w(&AF_X, "add  Oct +") - x0, bot - 10, &AF_X, "add  Oct +", T_MID);
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
        umv_tk = (uint8_t)clamp((int32_t)umv_tk + (s > 0 ? 1 : -1), 0, 2);
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
    c = umv_cc();
    if (umv_on() && umv_ed == 2u)                       /* its transition: KNOB k its field, a detent a place */
        umv_trd = (uint8_t)((umv_trd & ~(3u << 2u * k)) | umv_tb((uint32_t)clamp((int32_t)umv_tf(umv_trd, k) + s, 0, 3), k) << 2u * k);
    else if (!umv_on())
        umv_turn(c, umv_tk - 1u, s);
    else if (umx_pg != UMB_NONE && (umx_el || !k))
        umx_knob(k, s);
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
            umv_cc()->pre = (uint8_t)clamp((int32_t)umv_cc()->pre + s, 0, UMC_NPRE - 1);
            umc_touch();
            umv_sync();
        }
        return 1;
    }
    if (!umk_on() || ui.hset[0] == 1u)
        return 0;
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
                c->tr = (uint8_t)(e < 34u ? 233u : e > 60u ? 6u : 0u);   /* (umv_tb: FADE 4 S in, FADE 2 S out; BLOCKS 0.5 S in) */
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
    umc.lay[0] = 0;
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
    if (ui.home || !SONG_TRK(cur_page()) || !umv_tk || chain_busy() || !chain_config.count) {
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
        umv_trd = umv_cc()->tr;
        umv_ed = 2;
    } else {
        if (umv_ed == 2u)
            umv_cc()->tr = umv_trd;
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
            umx_pre_name(umx_find(c->key), c->pre, b);
            cv_text(x, 116, &AF_X, b[2] ? b + 3 : b, T_MID);
        }
    }
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
    } else
        umx_draw(0, 4, (int32_t)graph_h() - 6);
    cv_text(8, (int32_t)graph_h() - 10, &AF_X, "Oct -  back", T_MID);
    cv_text(232 - text_w(&AF_X, "keep  Oct +"), (int32_t)graph_h() - 10, &AF_X, umv_ed == 2u ? "keep  Oct +" : "next  Oct +", T_TEXT);
    cv_halo = ha;
    return 1;
}
