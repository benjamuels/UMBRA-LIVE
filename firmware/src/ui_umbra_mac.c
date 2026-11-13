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
 *   LIVE VIZ CTRLS    a page at a time (ALGORITHM): every plate there is, then BODIES and BEINGS (the engines' figures,
 *                     JIANT's beings), SCREEN (LIGHT SPEED REACT MOVE TRAVEL DISSLV) and EFFECTS. On the left the page's
 *                     name and under it its elements (KNOB 1), each with a bar: its value now, moving as its controls
 *                     move it. On the right the chosen row's controls (KNOB 2), as many as it takes:
 *                       the name's row   WHERE the page lies: OFF, GROUND, LAYER 1..3 (KNOB 4); NO ROOM under it: a
 *                                        layer's plate waiting, its state not fitting beside the ground's
 *                       SET              the element's own value (KNOB 4)
 *                       NOTES            (an element the notes drive) the tracks whose notes drive it: KNOB 3 the track,
 *                                        KNOB 4 on or off; none on: AUTO, the plate's own choice
 *                       a link           what else moves it: KNOB 3 its source (KNOB 1..4 = M1..M4, a track's HIT, LFO
 *                                        or STEP, LEVEL, BEAT, ENV TAP, LFO TAP, EDIT TAP: in TOTALITY a tap of that
 *                                        button, falling off over 0.4 s; OFF: the link gone), KNOB 4 how far (-99 .. 99
 *                                        of the element's range with the source at full)
 *                       + ADD            KNOB 3: a new link
 *                     ALGORITHM's plate, lying nowhere, becomes the ground: the plate behind the page is the page's.
 *                     PRESETS (KNOB 3 on the name's row): the page's four presets, BASIC and three more (a plate's own,
 *                     from its file: umb_pre_t); what is set goes into the preset chosen.
 *                     Kept in LIVE VIZ CTRLS' own store (ui_umbra.c umc), one set for every song.
 *   PLATES            every plate, four a page, a line on each (umx_list): KNOB 1 or ALGORITHM the plate, KNOB 3 or
 *                     PRESETS its preset, KNOB 4 where it lies.
 *   SAVE > SONG > TIMELINE   two picture tracks, MAIN and OVER, a cell a row; a cell opened by OCT+ is LIVE VIZ CTRLS
 *                     for its plate (umv_*, at the end).
 *   OCT+ on either    SETUP: a control made a question at a time, large, any knob the answer, OCT+ the next, OCT- back
 *                     (from the first: out).
 * Built for size, as the main loop is (tools/size_fns.py) */
static void text_column(uint32_t c, const char *label, const char *value, uint16_t color);   /* (ui_draw.c) */

/* ------------------------------------------------------------------------------------- MACROS - AUDIO --- */
static const uint8_t UMA_SR[MS_N] = {MS_OFF, MS_M1, MS_M1 + 1, MS_M1 + 2, MS_M1 + 3, MS_LFO, MS_STEP, MS_ENV, MS_ENV2,
                                     MS_VEL, MS_KEY, MS_RAND, MS_NOISE, MS_MODW, MS_AT, MS_EXPR, MS_FOLLOW, MS_PROX};
static uint8_t uma_slot;                                /* LIVE AUDIO CTRLS: the selected track's slot chosen, 0..3 */
static uint8_t umz;                                     /* SETUP: its question + 1 (0: the page) */
static int16_t *uma_p(uint32_t s) { return &trk[s / 4u % NTRK].p[P_M1SRC + 3u * (s % 4u)]; }   /* SRC DST AMT */
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
    uint16_t hit[NTRK], tap[3];                         /* each track's hits, the three taps, falling off (Q8) */
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
    for (c = 0; c < 3u; c++)                            /* (a tap over 0.4 s) */
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
    for (c = 0; c < 3u; c++)
        umq_s[UMQ_TAP + c] = (int16_t)umq.tap[c];
    for (pg = 0; pg < 2u; pg++) {
        const umc_rec_t *r = umx_rec(UMX_SCREEN + pg);
        for (j = 0; j < UMX_NC[1u + pg]; j++)
            umq_o[pg * 8u + j] = (uint8_t)umc_ev(r, j, UMX_C[1u + pg][j].def, umx_max(UMX_SCREEN + pg, j));
    }
}
/* TOTALITY: ENV (0), LFO (1) or EDIT (2) tapped. 1: a link takes that tap (it fires: the button's own tap is left out) */
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
/* SAVE > SONG > TIMELINE's picture tracks (the user, 2026-11-13): past T6, ALGORITHM reaches MAIN and OVER, a cell a row
 * each: the plate that is the ground, and the one on LAYER 1, from that row on (umv_apply). KNOB 4 there turns the cell's
 * plate; OCT+ opens the cell: LIVE VIZ CTRLS for its plate and its preset, over the picture (ALGORITHM the plate, PRESETS
 * the preset, the knobs as on that page; OCT- back) */
static int umv_on(void) { return umv_ed && umv_tk && !ui.home && !ui.menu && !ui.confirm && SONG_TRK(cur_page()); }
static umc_cell_t *umv_cc(void) { return umv_cell(ui.song_row, umv_tk - 1u); }
/* the cursor's row shown (not while the song plays its own), the open cell's page LIVE VIZ CTRLS' */
static void umv_sync(void)
{
    uint32_t pl;
    if (!chain.running)
        umv_apply(ui.song_row);
    if (umv_ed && umv_tk && (pl = umx_find(umv_cc()->key)) != umx_pg) {
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
static uint32_t umx_row(const umc_rec_t *r, uint32_t j, int notes, uint32_t ct, uint32_t *li)
{
    uint32_t i;
    if (!ct)
        return 0;
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
    static const char *const SL[5] = {"LEVEL", "BEAT", "ENV TAP", "LFO TAP", "EDIT TAP"};
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
        str_cpy(b, (pl == UMX_SCREEN ? UMN_MV : UMT_MIRN)[v & 3u], 12);
    else if (umx_max(pl, j) == NTRK) {
        str_cpy(b, "SEL", 12);
        if (v)
            b[0] = 'T', b[1] = (char)('0' + v), b[2] = 0;
    } else
        fmt_int(b, (int32_t)v);
}
static const char *const UMX_WH[5] = {"OFF", "GROUND", "LAYER 1", "LAYER 2", "LAYER 3"};
/* page pl can lie at w: nowhere (SCREEN and EFFECTS never: they are every plate's); the ground, a plate that fills its
 * strip; a layer, a figure or a plate that adds its light */
static int umx_can(uint32_t pl, uint32_t w)
{
    uint32_t m = pl < UMB_NPLT ? umb_mod(pl)->modes : UMB_MODE(UMB_DRAW_ADD);
    return pl <= UMX_BEING && (!w || (m & UMB_MODE(w == 1u ? UMB_DRAW_REPLACE : UMB_DRAW_ADD)));
}
/* ALGORITHM: the next page there is of the first lim, round. A plate lying nowhere becomes the ground: the plate
 * behind the page is the one the page is about (the user, 2026-11-13) */
static void umx_algo(int32_t s, uint32_t lim)
{
    umx_desc_t d;
    uint32_t k, pl = umx_cur() % lim;
    for (k = 0; k < lim; k++)
        if (umx_desc(pl = (pl + (s > 0 ? 1u : lim - 1u)) % lim, &d))
            break;
    umx_pg = (uint8_t)pl;
    umx_el = umx_ct = 0;
    if (!umx_place(pl) && umx_can(pl, 1u))
        umx_place_set(pl, 1u);
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
    if (!umx_el) {                                      /* the name's row: its preset, where the page lies */
        if (k == 2u)
            umx_pre_set(pl, (uint32_t)clamp((int32_t)umx_pre(pl) + s, 0, UMC_NPRE - 1));
        for (w = umx_place(pl); k == 3u && !umv_on() && (w += (uint32_t)(s > 0 ? 1 : -1)) < 5u;)
            if (umx_can(pl, w)) {
                umx_place_set(pl, w);
                break;
            }
        return;
    }
    j = umx_el - 1u;
    r = umx_recf(pl, k >= 2u);                          /* (a preset not changed yet: its own record now) */
    notes = d.el && (d.el[j] & UMB_EL_NOTES);
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
static const char *const UMV_TN[2] = {"MAIN", "OVER"};
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
        str_cpy(b[3], umv_on() ? UMV_TN[umv_tk - 1u] : pl > UMX_BEING ? "ALWAYS" : UMX_WH[umx_place(pl)], 12);
        return 1;
    }
    r = umx_rec(pl);
    row = umx_row(r, j, d.el && (d.el[j] & UMB_EL_NOTES), umx_ct, &li);
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
        return;
    }
    track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
    if (umz == 1u)                                      /* (SETUP's first question: the track's first free slot) */
        for (uma_slot = 0; uma_slot < 3u && uma_p(uma_cur())[0]; uma_slot++) {}
}
static void umk_knob(uint32_t k, int32_t s)
{
    uint32_t au = ui.hset[0] == 1u;
    if (ui.hset[0] == 3u) {                             /* PLATES: KNOB 1 the plate, 3 its preset, 4 where it lies */
        umx_el = 0;
        if (!k)
            umx_algo(s, UMB_NPLT);
        else
            umx_knob(k, s);
        return;
    }
    if (umz == 1u) {                                    /* (SETUP: any knob the answer; the first, ALGORITHM's) */
        umk_algo(s);
        return;
    }
    if (umz)                                            /* (.. LIVE VIZ CTRLS' second: the element, KNOB 1's) */
        k = au || umz > 2u ? umz - 1u : 0u;
    if (au)
        uma_knob(k, s);
    else
        umx_knob(k, s);
}
/* each input pass, after the octave keys' taps: OCT+ starts SETUP (LIVE AUDIO CTRLS: the track's first free slot) or goes
 * to its next question (after the last: done); OCT- back, or out. LIVE VIZ CTRLS' questions: the page, its element, what
 * moves it (a new link: its + ADD row), how far */
static void umz_input(uint32_t oct)
{
    if (!umk_on() || ui.hset[0] == 3u) {
        umz = 0;
        return;
    }
    if (!oct)
        return;
    if (oct & 2u) {
        if (!umz && ui.hset[0] == 1u)
            for (uma_slot = 0; uma_slot < 3u && uma_p(uma_cur())[0]; uma_slot++) {}
        umz = (uint8_t)(umz < 4u ? umz + 1u : 0u);
        if (ui.hset[0] != 1u && umz == 2u && !umx_el)
            umx_el = 1;                                 /* (an element, not the name's row) */
        if (ui.hset[0] != 1u && umz == 3u) {            /* (its last row: + ADD, or its last link when all are taken) */
            umx_desc_t d;
            uint32_t li;
            (void)umx_desc(umx_cur(), &d);
            for (umx_ct = 0; umx_row(umx_rec(umx_pg), umx_el - 1u, d.el && (d.el[umx_el - 1u] & UMB_EL_NOTES), umx_ct + 1u, &li) < 4u; umx_ct++) {}
        }
        if (!umz)
            ui_message("CONTROL SET");
    } else if (umz)
        umz--;
    ui.force = 1;
}
static void umk_cards(void)                             /* KNOB 1..4: the chosen row's columns */
{
    static const char *const LB[3][4] = {{"SLOT", "SOURCE", "DEST", "AMOUNT"}, {"ELEMENT", "CONTROL", "SOURCE", "VALUE"},
                                         {"PAGE", "", "PRESET", "WHERE"}};
    char b[4][12];
    uint32_t k, au = umk_au(), nm;
    int on;
    if (!umv_on() && ui.hset[0] == 3u)                  /* (PLATES: the plate's own row) */
        umx_el = 0;
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
    if (!umx_el) {                                      /* where it lies (a TIMELINE cell: its row and track), its preset */
        str_cpy(t, "ROW ", sizeof t);
        fmt_int(t + 4, (int32_t)ui.song_row + 1);
        umk_text(124 - x0, y, umv_on() ? t : "WHERE", T_MID);
        str_cpy(b, umv_on() ? UMV_TN[umv_tk - 1u] : pl > UMX_BEING ? "ALWAYS" : UMX_WH[umx_place(pl)], sizeof b);
        umk_text(234 - text_w(&AF_S, b) - x0, y, b, pl > UMX_BEING ? T_MID : T_THEME);
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
            row = umx_row(r, j, d.el && (d.el[j] & UMB_EL_NOTES), i, &li);
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
/* HOME > UMBRA > PLATES (the user, 2026-11-13: a plate browser, four at a time, with descriptions): every plate there is,
 * a row each: its name, where it lies, under them its own line (its file's, umb_plate_t.desc; an older file: its
 * elements). The chosen one (KNOB 1, ALGORITHM) plays behind the page as the ground unless it lies on a layer; it is
 * LIVE VIZ CTRLS' page too */
static void umx_list(int32_t x0, int32_t y, int32_t bot)
{
    char b[48];
    uint32_t pl, n = 0, at = 0, i = 0, k, cur = umx_cur();
    int32_t rh = (bot - y - 10) / 4, yy;
    if (cur >= UMB_NPLT)
        cur = umx_pg = (uint8_t)(umb_pl() != UMB_NONE ? umb_pl() : 4u);
    for (pl = 0; pl < UMB_NPLT; pl++)
        if (umb_has(pl)) {
            if (pl == cur)
                at = n;
            n++;
        }
    for (pl = 0; pl < UMB_NPLT; pl++) {
        const umb_plate_t *p;
        if (!umb_has(pl) || i++ / 4u != at / 4u)
            continue;
        p = umb_mod(pl);
        yy = y + (int32_t)((i - 1u) % 4u) * rh;
        if (pl == cur)
            cv_rect(2 - x0, yy + 2, 2, rh - 6, T_THEME);
        umk_text(8 - x0, yy, p->name, pl == cur ? T_THEME : T_TEXT);
        if ((k = umx_place(pl)) != 0u)
            umk_text(234 - text_w(&AF_S, UMX_WH[k]) - x0, yy, UMX_WH[k], T_MID);
        b[0] = 0;
        if (p->api >= 4u)
            str_cpy(b, p->desc, 40);
        else
            for (k = 0; k < p->nctl && str_len(b) < 36u; k++) {
                str_cpy(b + str_len(b), p->ctl[k].name, 8);
                str_cpy(b + str_len(b), " ", 2);
            }
        cv_text(8 - x0, yy + 15, &AF_X, b, pl == cur ? T_TEXT : T_MID);
    }
    fmt_frac(b, (int32_t)(at / 4u) + 1, (int32_t)((n + 3u) / 4u));
    cv_text(232 - text_w(&AF_X, b) - x0, bot - 10, &AF_X, b, T_MID);
}
/* the page over the plate from canvas row y (h rows). LIVE AUDIO CTRLS: the selected track's four slots, then what each
 * macro moves (its routes on every track: "T2 CUT", as many as fit); LIVE VIZ CTRLS: umx_draw. SETUP: its question, large,
 * and the answer */
static void umk_draw(int32_t x0, int32_t y, int32_t h)
{
    static const uint8_t CX[4] = {6, 26, 104, 186};
    static const char *const QN[2][4] = {{"WHICH TRACK?", "WHICH CONTROL?", "IT MOVES WHAT?", "HOW MUCH?"},
                                         {"WHICH PLATE?", "WHICH PART?", "MOVED BY WHAT?", "HOW FAR?"}};
    uint8_t ha = cv_halo;
    char b[4][12], ln[44];
    uint32_t i, k, c, au = ui.hset[0] == 1u;
    int32_t bot = y + h;
    cv_halo = 1;
    if (ui.hset[0] == 3u) {
        umx_list(x0, y, bot);
        cv_halo = ha;
        return;
    }
    if (umz) {                                          /* SETUP */
        const char *a = b[umz - 1u];
        if (au) {
            umk_names(uma_slot, b);
            if (umz == 1u) {
                ln[0] = 'T', ln[1] = (char)('1' + song.sel % NTRK), ln[2] = ' ';
                str_cpy(ln + 3, trk_engp(&trk[song.sel % NTRK])->name, sizeof ln - 3u);
                a = ln;
            }
        } else {
            umx_desc_t d;
            umx_names(b);
            if (umz == 1u && umx_desc(umx_cur(), &d))
                a = d.name;
            if (umz == 2u)
                a = b[0];
        }
        cv_text(120 - text_w(&AF_M, QN[!au][umz - 1u]) / 2 - x0, y + h / 2 - 34, &AF_M, QN[!au][umz - 1u], 0xBF7Du);
        cv_text(120 - text_w(&AF_M, a) / 2 - x0, y + h / 2 - 6, &AF_M, a, T_THEME);
        cv_text(120 - text_w(&AF_X, "turn any knob") / 2 - x0, y + h / 2 + 22, &AF_X, "turn any knob", T_MID);
        cv_text(8 - x0, bot - 10, &AF_X, umz > 1u ? "Oct -  back" : "Oct -  out", T_MID);
        cv_text(232 - text_w(&AF_X, "next  Oct +") - x0, bot - 10, &AF_X, umz < 4u ? "next  Oct +" : "done  Oct +", T_TEXT);
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
    cv_text(232 - text_w(&AF_X, "setup  Oct +") - x0, bot - 10, &AF_X, "setup  Oct +", T_MID);
    cv_halo = ha;
}

/* --------------------------------------------------------------------- SAVE > SONG > TIMELINE's picture tracks --- */
/* ALGORITHM on TIMELINE: past the last sound track MAIN, then OVER; back the same way. In an open cell: its plate.
 * 1: taken */
static int umv_algo(int32_t s)
{
    if (ui.home || !SONG_TRK(cur_page()))
        return 0;
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
    if (!umv_on())
        umv_turn(c, umv_tk - 1u, s);
    else if (umx_pg != UMB_NONE && (umx_el || !k))
        umx_knob(k, s);
    else if (umx_pg != UMB_NONE && k == 2u) {
        c->pre = (uint8_t)clamp((int32_t)c->pre + s, 0, UMC_NPRE - 1);
        umc_touch();
    }
    umv_sync();
    return 1;
}
/* PRESETS: an open cell's preset; LIVE VIZ CTRLS' or PLATES' page's. 1: taken */
static int umk_preset(int32_t s)
{
    if (umv_on()) {
        if (umx_pg != UMB_NONE) {
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
/* each input pass, the octave keys' taps there: OCT+ on a picture track opens its cell, OCT- closes it. What is left of
 * the taps for the page (OCT+ ADD ROW, OCT- DELETE) */
static uint32_t umv_input(uint32_t oct)
{
    if (ui.home || !SONG_TRK(cur_page()) || !umv_tk || chain_busy() || !chain_config.count) {
        umv_ed = 0;
        return oct;
    }
    if (!oct || ui.menu || ui.confirm)
        return oct;
    if (oct & 2u)
        umv_ed = 1;
    else if (umv_ed)
        umv_ed = 0;
    else
        return oct;
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
            if (tk && umx_lp[0] >= UMX_BODY && umx_lp[0] <= UMX_BEING)   /* (a figure: its name) */
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
    if (umx_pg == UMB_NONE) {
        umv_name(umv_cc(), b);
        cv_text(120 - text_w(&AF_M, b) / 2, 40, &AF_M, b, T_THEME);
        cv_text(120 - text_w(&AF_X, "ALGORITHM: A PLATE") / 2, 66, &AF_X, "ALGORITHM: A PLATE", T_MID);
    } else
        umx_draw(0, 4, (int32_t)graph_h() - 6);
    cv_text(8, (int32_t)graph_h() - 10, &AF_X, "Oct -  back", T_MID);
    cv_halo = ha;
    return 1;
}
