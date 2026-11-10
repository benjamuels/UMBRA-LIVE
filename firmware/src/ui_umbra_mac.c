/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, after TOTALITY) HOME > UMBRA's LIVE AUDIO CTRLS and LIVE VIZ CTRLS (the user, 2026-11-09:
 * custom controls and effects for the sound and the picture; 2026-11-10: no long lists, ALGORITHM the track / the
 * plate, what the macros do in sight, taps that trigger, a setup wizard). TOTALITY's KNOB 1..4 are JIANT's macros
 * M1..M4 (mod.c macro_v: the project's; LFO held turns them anywhere), so one turn moves what both pages send a macro to.
 *   LIVE AUDIO CTRLS  one track at a time: its four slots of JIANT's modulation matrix (LFO > MOD's), a line each, and
 *                     under them what each macro M1..M4 moves, on every track. ALGORITHM the track, KNOB 1 the SLOT,
 *                     2 SOURCE, 3 DEST, 4 AMOUNT. Nothing new in the sound: the matrix as JIANT runs it, kept with the
 *                     project.
 *   LIVE VIZ CTRLS    UMQ_NR routes into the picture, a few lines in sight under the plate's name. ALGORITHM the plate,
 *                     KNOB 1 the ROUTE, 2 SOURCE (M1..M4; a track's hits, its LFO, its step; the level; the beat; ENV
 *                     TAP, LFO TAP, EDIT TAP: in TOTALITY a tap of that button, falling off over 0.4 s, where a route
 *                     takes it: its own tap then does nothing else there), 3 TARGET (the plate's own three, LIGHT .. the
 *                     effects, the being), 4 TO: the target's value with the source at full (at 0: FROM, the target's own
 *                     setting for a route made here). Routes on one target add their moves to the first one's FROM; a
 *                     target no route reaches keeps its setting. Kept with the settings, one set for every song. At
 *                     first: M1..M3 the plate's three, M4 LIGHT (to 0: it darkens), ENV TAP INVERT, LFO TAP GLITCH, EDIT
 *                     TAP MOSAIC.
 *   OCT+ on either    SETUP: a route made a question at a time, large (which track or plate, which control, what it
 *                     moves, how far), any knob the answer, OCT+ the next, OCT- back (from the first: out).
 * Built for size, as the main loop is (tools/size_fns.py) */
#define UMQ_SND 1                                       /* the sound's own sources (HIT LFO STEP LEVEL BEAT) */
static void text_column(uint32_t c, const char *label, const char *value, uint16_t color);   /* (ui_draw.c) */

/* ------------------------------------------------------------------------------------- MACROS - AUDIO --- */
static const uint8_t UMA_SR[MS_N] = {MS_OFF, MS_M1, MS_M1 + 1, MS_M1 + 2, MS_M1 + 3, MS_LFO, MS_STEP, MS_ENV, MS_ENV2,
                                     MS_VEL, MS_KEY, MS_RAND, MS_NOISE, MS_MODW, MS_AT, MS_EXPR, MS_FOLLOW, MS_PROX};
static uint8_t uma_slot;                                /* LIVE AUDIO CTRLS: the selected track's slot chosen, 0..3 */
static uint8_t umq_sel;                                 /* LIVE VIZ CTRLS: the route chosen */
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

/* ------------------------------------------------------------------------------------ MACROS - VISUALS --- */
/* the routes, 3 bytes each: SOURCE | TARGET's low 3 bits << 5, FROM | TARGET's 4th bit << 7, TO (FROM, TO: 0 .. 99, or
 * UMQ_SET). In retired favourites bytes nothing else uses (ui_umbra.c's map): SAMPLE's row 4 [0..5] (its stars, never
 * shown), [9..11] (the first routing's), [20..31] (the plates' routing, gone); TRIO's row 6 [0..5] (its stars), [14..16];
 * row 6 [18] says they are laid out so (else, once: the routes at first) */
#define UMQ_NR 10u
#define UMQ_SET 127u
static const uint8_t UMQ_AT[UMQ_NR] = {0, 3, 20, 23, 26, 29, 9, 32, 35, 46};   /* row 4's byte; + 32: row 6's */
#define UMQ_MK 0xA8u                                    /* (0xA7: before the taps) */
enum { UMQ_OFF, UMQ_M1, UMQ_HIT = UMQ_M1 + 4, UMQ_LFO = UMQ_HIT + NTRK, UMQ_STEP = UMQ_LFO + NTRK, UMQ_LEVEL = UMQ_STEP + NTRK,
       UMQ_BEAT, UMQ_TAP, UMQ_NS = UMQ_TAP + 3 };       /* (.. ENV TAP, LFO TAP, EDIT TAP) */
_Static_assert(UMQ_NS <= 32, "a route's source: 5 bits");
#if !UMQ_SND
#undef UMQ_NS
#define UMQ_NS UMQ_HIT
#endif
static struct {
    uint16_t hit[NTRK], tap[3];                         /* each track's hits, the three taps, falling off (Q8) */
    uint32_t qr, ms;                                    /* the notes read; the last frame's time */
} umq __attribute__((section(".pool")));
static uint8_t *umq_b(uint32_t r)
{
    uint32_t a = UMQ_AT[r % UMQ_NR], i;
    uint8_t *q;
    if (favorites.factory[6][18] != UMQ_MK) {          /* (once: the routes at first) */
        for (i = 0; i < UMQ_NR; i++) {
            q = &favorites.factory[UMQ_AT[i] & 32u ? 6 : 4][UMQ_AT[i] & 31u];
            static const uint8_t TT[3] = {UMT_INV, UMT_GLI, UMT_MOS};   /* (the taps' targets at first) */
            uint32_t tg = i < 4u ? i : i < 7u ? TT[i - 4u] : 0u;
            q[0] = (uint8_t)(i < 4u ? (UMQ_M1 + i) | tg << 5 : i < 7u ? (UMQ_TAP + i - 4u) | (tg & 7u) << 5 : 0u);
            q[1] = (uint8_t)(i < 7u ? UMQ_SET | (tg >> 3) << 7 : 0u);
            q[2] = (uint8_t)(i == 3u ? 0u : i < 7u ? 99u : 0u);
        }
        favorites.factory[6][18] = UMQ_MK;
    }
    return &favorites.factory[a & 32u ? 6 : 4][a & 31u];
}
static uint32_t umq_max(uint32_t t) { return t == UMT_MOVE || t == UMT_MIR ? 3u : 99u; }
static void umq_get(uint32_t r, uint32_t *v)            /* route r: SOURCE, TARGET, FROM, TO */
{
    const uint8_t *b = umq_b(r);
    uint32_t j, m;
    v[0] = b[0] & 31u;
    v[1] = (uint32_t)(b[0] >> 5 | (b[1] >> 7) << 3);
    v[0] = v[0] < UMQ_NS ? v[0] : 0u;
    v[1] = v[1] < UMT_NT ? v[1] : 0u;
    v[2] = b[1] & 127u;
    v[3] = b[2] & 127u;
    for (j = 2, m = umq_max(v[1]); j < 4u; j++)
        if (v[j] != UMQ_SET && v[j] > m)
            v[j] = m;
}
static void umq_put(uint32_t r, const uint32_t *v)
{
    uint8_t *b = umq_b(r);
    b[0] = (uint8_t)(v[0] | (v[1] & 7u) << 5);
    b[1] = (uint8_t)(v[2] | (v[1] >> 3) << 7);
    b[2] = (uint8_t)v[3];
    umv_dirty = 1;
    umv_ms = fm1_ms;
}
static uint32_t umq_raw(uint32_t t)                     /* target t's own setting */
{
    uint32_t i;
    if (t <= UMT_LIGHT)
        return umv_raw(umb_pl(), t);
    for (i = 0; i < UMT_NS && UMT_Q[i] != t; i++) {}
    return i < UMT_NS ? umt_raw(i) : t == UMT_INV ? 0u : 99u;   /* (the being: whole; INVERT: none) */
}
/* source s now, 0 .. 256 */
static int32_t umq_sv(uint32_t s)
{
    if (s < UMQ_HIT)
        return s ? macro_v[s - UMQ_M1] * 256 / 127 : 0;
#if UMQ_SND
    if (s < UMQ_LEVEL) {
        const track_t *t = &trk[(s - UMQ_HIT) % NTRK];
        if (s < UMQ_LFO)
            return umq.hit[s - UMQ_HIT];
        if (s < UMQ_STEP)
            return clamp((t->lfo_val + 32768) >> 8, 0, 256);
        {                                               /* the step's level now (as mod.c ms_tick's, unslewed) */
            uint32_t per = div_samples((uint32_t)t->p[P_MSDIV]), len = t->p[P_MSLEN] > 0 ? (uint32_t)t->p[P_MSLEN] : 1u;
            return t->p[P_MS0 + (per ? ms_clock / per % len : 0u) % 16u] * 2;
        }
    }
    if (s == UMQ_LEVEL)
        return bg_energy();
    if (s >= UMQ_TAP)
        return umq.tap[s - UMQ_TAP];
    {                                                   /* BEAT: full on the beat, falling to the next (stopped: 0) */
        uint32_t q = beat_samples();
        return song.playing && q ? 256 - (int32_t)(ms_clock % q * 256u / q) : 0;
    }
#endif
}
/* once a frame (umb_clock): every target's value where a route reaches it (umq_o: + 1) */
static void umq_tick(void)
{
    int32_t a[UMT_NT];
    uint32_t r, v[4];
#if UMQ_SND
    uint32_t dt = fm1_ms - umq.ms, c;
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
#endif
    memset(umq_o, 0, sizeof umq_o);
    for (r = 0; r < UMQ_NR; r++) {
        umq_get(r, v);
        if (!v[0])
            continue;
        if (v[2] == UMQ_SET)
            v[2] = umq_raw(v[1]);
        if (v[3] == UMQ_SET)
            v[3] = umq_raw(v[1]);
        if (!umq_o[v[1]]) {
            umq_o[v[1]] = 1;
            a[v[1]] = (int32_t)v[2] << 8;
        }
        a[v[1]] += ((int32_t)v[3] - (int32_t)v[2]) * umq_sv(v[0]);
    }
    for (r = 0; r < UMT_NT; r++)
        if (umq_o[r])
            umq_o[r] = (uint8_t)(1 + umb_clamp((a[r] + 128) >> 8, 0, (int32_t)umq_max(r)));
}
static void umq_val(char *b, uint32_t t, uint32_t v)    /* a FROM or TO's words */
{
    if (v == UMQ_SET)
        str_cpy(b, "SET", 12);
    else if (t == UMT_MOVE || t == UMT_MIR)
        str_cpy(b, (t == UMT_MOVE ? UMN_MV : UMT_MIRN)[v & 3u], 12);
    else
        fmt_int(b, (int32_t)v);
}
static void umq_knob(uint32_t k, int32_t s)            /* KNOB k turned s: the ROUTE, its SOURCE, TARGET, TO */
{
    uint32_t v[4], m;
    int32_t r;
    if (k == 0u) {
        umq_sel = (uint8_t)clamp((int32_t)umq_sel + s, 0, UMQ_NR - 1);
        return;
    }
    umq_get(umq_sel, v);
    if (k < 3u) {                                       /* SOURCE, TARGET, a detent each (FROM and TO kept within its range) */
        if (k == 1u && !v[0])
            v[2] = UMQ_SET;                             /* (a route made here: from the target's own setting) */
        v[k - 1u] = (uint32_t)clamp((int32_t)v[k - 1u] + s, 0, (k == 2u ? UMT_NT : UMQ_NS) - 1);
    } else {                                            /* TO: SET, then 0 .. its most */
        m = umq_max(v[1]);
        r = v[3] == UMQ_SET ? 0 : (int32_t)v[3] + 1;
        r = clamp(r + accel(EN_K1 + k, s, (int32_t)m), 0, (int32_t)m + 1);
        v[3] = r ? (uint32_t)r - 1u : UMQ_SET;
    }
    umq_put(umq_sel, v);
}
/* TOTALITY: ENV (0), LFO (1) or EDIT (2) tapped. 1: a route takes that tap (it fires: the button's own tap is left out) */
static int umq_tap(uint32_t i)
{
    uint32_t r, v[4];
    for (r = 0; r < UMQ_NR; r++) {
        umq_get(r, v);
        if (v[0] == UMQ_TAP + i) {
            umq.tap[i] = 256;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------- the pages --- */
static int umk_on(void) { return UMB_ON && !umt.on && home_bare() && ui.hset[0]; }   /* a MACROS page shown */
/* row i's words, a column each (LIVE AUDIO CTRLS: slot i of the selected track; LIVE VIZ CTRLS: route i): its number, the
 * SOURCE, the DEST / TARGET, the AMOUNT / TO; 1: it acts (else drawn dim) */
static int umk_names(uint32_t i, char (*b)[12])
{
    uint32_t v[4];
    const char *u;
    fmt_int(b[0], (int32_t)i + 1);
    if (ui.hset[0] == 1u) {
        const int16_t *p = uma_p((uint32_t)song.sel % NTRK * 4u + i);
        str_cpy(b[1], N_MSRC[clamp(p[0], 0, MS_N - 1)], 12);
        str_cpy(b[2], mod_dst_name(&trk[song.sel % NTRK], p[1]), 12);
        param_format(&TP[P_M1AMT], p[2], b[3], &u);    /* (+50: LFO > MOD's) */
        return p[0] && p[1] && p[2];
    }
    umq_get(i, v);
    if (v[0] < UMQ_HIT)                                 /* (OFF, M1 .. M4: the matrix's names) */
        str_cpy(b[1], N_MSRC[v[0] ? MS_M1 + v[0] - UMQ_M1 : MS_OFF], 12);
#if UMQ_SND
    else if (v[0] >= UMQ_LEVEL) {
        static const char *const SL[5] = {"LEVEL", "BEAT", "ENV TAP", "LFO TAP", "EDIT TAP"};
        str_cpy(b[1], SL[v[0] - UMQ_LEVEL], 12);
    } else {
        static const char *const SN[3] = {"HIT", "LFO", "STEP"};
        b[1][0] = 'T', b[1][1] = (char)('1' + (v[0] - UMQ_HIT) % NTRK), b[1][2] = ' ';
        str_cpy(b[1] + 3, SN[(v[0] - UMQ_HIT) / NTRK], 9);
    }
#endif
    str_cpy(b[2], umt_tname(umb_pl(), v[1]), 12);
    umq_val(b[3], v[1], v[3]);
    return v[0] != 0u;
}
static void umk_algo(int32_t s)                         /* ALGORITHM: the track (LIVE AUDIO CTRLS), the plate (LIVE VIZ CTRLS) */
{
    if (ui.hset[0] == 1u)
        track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
    else
        umt_select(s);
    if (umz == 1u && ui.hset[0] == 1u)                  /* (SETUP's first question: the track's first free slot) */
        for (uma_slot = 0; uma_slot < 3u && uma_p(uma_cur())[0]; uma_slot++) {}
}
static void umk_knob(uint32_t k, int32_t s)
{
    if (umz == 1u) {                                    /* (SETUP: any knob the answer; the first, ALGORITHM's) */
        umk_algo(s);
        return;
    }
    if (umz)
        k = umz - 1u;
    if (ui.hset[0] == 1u)
        uma_knob(k, s);
    else
        umq_knob(k, s);
}
/* each input pass, after the octave keys' taps: OCT+ starts SETUP (a row with no source yet: the track's first free
 * slot, the first route that is OFF) or goes to its next question (after the last: done); OCT- back, or out */
static void umz_input(uint32_t oct)
{
    uint32_t v[4];
    if (!umk_on()) {
        umz = 0;
        return;
    }
    if (!oct)
        return;
    if (oct & 2u) {
        if (!umz) {
            if (ui.hset[0] == 1u)
                for (uma_slot = 0; uma_slot < 3u && uma_p(uma_cur())[0]; uma_slot++) {}
            else
                for (umq_sel = 0; umq_sel < UMQ_NR - 1u && (umq_get(umq_sel, v), v[0]); umq_sel++) {}
        }
        umz = (uint8_t)(umz < 4u ? umz + 1u : 0u);
        if (!umz)
            ui_message("CONTROL SET");
    } else if (umz)
        umz--;
    ui.force = 1;
}
static void umk_cards(void)                             /* KNOB 1..4: the chosen row's columns */
{
    static const char *const LB[2][4] = {{"SLOT", "SOURCE", "DEST", "AMOUNT"}, {"ROUTE", "SOURCE", "TARGET", "TO"}};
    char b[4][12];
    uint32_t k, au = ui.hset[0] == 1u;
    int on = umk_names(au ? uma_slot : umq_sel, b);
    for (k = 0; k < 4u; k++)
        text_column(k, LB[!au][k], b[k], on || k < 2u ? VAL(k) : T_DIM);
}
/* the page over the plate from canvas row y (h rows). LIVE AUDIO CTRLS: the selected track's four slots, then what each
 * macro moves (its routes on every track: "T2 CUT", as many as fit); LIVE VIZ CTRLS: the plate's name, then the routes
 * about the chosen one. The chosen row in the theme's colour. SETUP: its question, large, and the answer */
#define UMK_RH 17                                       /* a row's height (AF_S) */
static void umk_draw(int32_t x0, int32_t y, int32_t h)
{
    static const uint8_t CX[4] = {6, 26, 104, 186};
    static const char *const QN[2][4] = {{"WHICH TRACK?", "WHICH CONTROL?", "IT MOVES WHAT?", "HOW MUCH?"},
                                         {"WHICH PLATE?", "WHICH CONTROL?", "IT CHANGES WHAT?", "HOW FAR?"}};
    uint8_t ha = cv_halo;
    char b[4][12], ln[44];
    uint32_t i, k, c, au = ui.hset[0] == 1u, n = au ? 4u : UMQ_NR, sel = au ? uma_slot : umq_sel, rows;
    int32_t bot = y + h;
    cv_halo = 1;
    if (umz) {                                          /* SETUP */
        const char *a = b[umz - 1u];
        umk_names(sel, b);
        if (umz == 1u) {
            if (au) {
                ln[0] = 'T', ln[1] = (char)('1' + song.sel % NTRK), ln[2] = ' ';
                str_cpy(ln + 3, trk_engp(&trk[song.sel % NTRK])->name, sizeof ln - 3u);
                a = ln;
            } else
                a = umb_pname(umb_pl());
        }
        cv_text(120 - text_w(&AF_M, QN[!au][umz - 1u]) / 2 - x0, y + h / 2 - 34, &AF_M, QN[!au][umz - 1u], 0xBF7Du);
        cv_text(120 - text_w(&AF_M, a) / 2 - x0, y + h / 2 - 6, &AF_M, a, T_THEME);
        cv_text(120 - text_w(&AF_X, "turn any knob") / 2 - x0, y + h / 2 + 22, &AF_X, "turn any knob", T_MID);
        cv_text(8 - x0, bot - 10, &AF_X, umz > 1u ? "Oct -  back" : "Oct -  out", T_MID);
        cv_text(232 - text_w(&AF_X, "next  Oct +") - x0, bot - 10, &AF_X, umz < 4u ? "next  Oct +" : "done  Oct +", T_TEXT);
        cv_halo = ha;
        return;
    }
    if (!au) {
        cv_text(CX[0] - x0, y, &AF_S, umb_pname(umb_pl()), T_THEME);
        y += UMK_RH + 3;
    }
    rows = au ? 4u : (uint32_t)clamp((bot - y - 12) / UMK_RH, 1, UMQ_NR);
    for (i = sel >= rows ? sel - rows + 1u : 0u, k = i + rows; i < n && i < k; i++, y += UMK_RH) {
        uint16_t col = umk_names(i, b) ? T_TEXT : T_DIM;
        if (i == sel)
            col = T_THEME;
        for (c = 0; c < 4u; c++)
            if (c < 2u || b[1][0] != 'O' || b[1][1] != 'F')   /* (OFF: its number and its source alone) */
                cv_text(CX[c] - x0, y, &AF_S, b[c], col);
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
