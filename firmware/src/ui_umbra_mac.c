/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, after TOTALITY) HOME > UMBRA's MACROS pages (the user, 2026-11-09: custom controls and effects
 * for the sound and the picture). TOTALITY's KNOB 1..4 are JIANT's macros M1..M4 (mod.c macro_v: the project's; LFO held
 * turns them anywhere), so one turn moves what both pages send a macro to.
 *   MACROS - AUDIO    every route of JIANT's modulation matrix (each track's four slots, LFO > MOD's) on one page: the
 *                     macros' first, then the tracks' own sources (LFO, STEP ..), a line each, under + NEW. ALGORITHM the
 *                     route, KNOB 1 SOURCE, 2 TRACK (the route moved to a free slot of the next track that has one),
 *                     3 DEST, 4 AMOUNT. Nothing new in the sound: the matrix as JIANT runs it, kept with the project.
 *   MACROS - VISUALS  UMQ_NR routes into the picture, a line each. ALGORITHM the route, KNOB 1 SOURCE (M1..M4; a track's
 *                     hits, its LFO, its step; the level; the beat), 2 TARGET (the plate's own three, LIGHT .. the
 *                     effects, the being), 3 FROM, 4 TO: the target's value with the source at 0 and at full (FROM over TO
 *                     turns it round; SET: the target's own setting). Routes on one target add their moves to the first
 *                     one's FROM; a target no route reaches keeps its setting. Kept with the settings, one set for every
 *                     song (the project's 3,840 bytes are all in use). At first: M1..M3 the plate's three, M4 LIGHT, each
 *                     from SET (as set, with the macro at 0) to 99, LIGHT's to 0 (M4 darkens).
 * Built for size, as the main loop is (tools/size_fns.py) */
#define UMQ_SND 1                                       /* the sound's own sources (HIT LFO STEP LEVEL BEAT) */
static void text_column(uint32_t c, const char *label, const char *value, uint16_t color);   /* (ui_draw.c) */

/* ------------------------------------------------------------------------------------- MACROS - AUDIO --- */
static const uint8_t UMA_SR[MS_N] = {MS_OFF, MS_M1, MS_M1 + 1, MS_M1 + 2, MS_M1 + 3, MS_LFO, MS_STEP, MS_ENV, MS_ENV2,
                                     MS_VEL, MS_KEY, MS_RAND, MS_NOISE, MS_MODW, MS_AT, MS_EXPR, MS_FOLLOW, MS_PROX};
#define UMA_NEW 0xFFu
static uint8_t uma_sel = UMA_NEW;                       /* the route chosen: its track x 4 + its slot; UMA_NEW: + NEW */
static uint8_t umq_sel;                                 /* MACROS - VISUALS': the route chosen */
static int16_t *uma_p(uint32_t s) { return &trk[s / 4u % NTRK].p[P_M1SRC + 3u * (s % 4u)]; }   /* SRC DST AMT */
static uint32_t uma_rank(uint32_t s)                    /* slot s's place in the list: its source's in UMA_SR */
{
    uint32_t r, v = (uint32_t)uma_p(s)[0];
    for (r = 0; r < MS_N && UMA_SR[r] != v; r++) {}
    return r;
}
/* the list into L: + NEW, then the routes (a slot with a source; the one chosen even without) of the tracks that play, by
 * source, then track. How many */
static uint32_t uma_list(uint8_t *L)
{
    uint8_t R[NTRK * 4u];
    uint32_t r, s, n = 1;
    L[0] = UMA_NEW;
    for (s = 0; s < trk_live * 4u; s++)
        R[s] = (uint8_t)uma_rank(s);
    for (r = 0; r < MS_N; r++)
        for (s = 0; s < trk_live * 4u; s++)
            if (R[s] == r && (r || s == uma_sel))
                L[n++] = (uint8_t)s;
    return n;
}
static uint32_t uma_at(const uint8_t *L, uint32_t n)    /* the chosen one's place in L (gone: + NEW's) */
{
    uint32_t i;
    for (i = n - 1u; i && L[i] != uma_sel; i--) {}
    return i;
}
static uint32_t uma_free(uint32_t c)                    /* track c's first free slot + 1 (0: none) */
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        if (!trk[c].p[P_M1SRC + 3u * k])
            return c * 4u + k + 1u;
    return 0;
}
/* KNOB k turned s. + NEW: KNOB 1 starts a route in a free slot, the selected track's or the next one's that has one; TRACK:
 * the route to the next track (round) with a free slot */
static void uma_knob(uint32_t k, int32_t s)
{
    static const int16_t Z[3];
    uint32_t c, f = 0, i, nw = uma_sel == UMA_NEW;
    int16_t *p;
    if (nw || k == 1u) {
        if (nw && k)
            return;
        for (c = nw ? song.sel : uma_sel / 4u + (s > 0 ? 1u : trk_live - 1u), i = !nw; !f && i < trk_live;
             i++, c += s > 0 || nw ? 1u : trk_live - 1u)
            f = uma_free(c % trk_live);
        if (!f) {
            ui_message("NO FREE SLOT");
            return;
        }
        memcpy(uma_p(f - 1u), nw ? Z : uma_p(uma_sel), sizeof Z);
        if (!nw)
            memset(uma_p(uma_sel), 0, sizeof Z);
        uma_sel = (uint8_t)(f - 1u);
        if (k)
            return;
    }
    p = uma_p(uma_sel);
    if (k == 0u)                                        /* SOURCE, a detent a source in the list's order (OFF first) */
        p[0] = UMA_SR[clamp((int32_t)uma_rank(uma_sel) + s, 0, MS_N - 1)];
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
#define UMQ_MK 0xA7u
enum { UMQ_OFF, UMQ_M1, UMQ_HIT = UMQ_M1 + 4, UMQ_LFO = UMQ_HIT + NTRK, UMQ_STEP = UMQ_LFO + NTRK, UMQ_LEVEL = UMQ_STEP + NTRK,
       UMQ_BEAT, UMQ_NS };
#if !UMQ_SND
#undef UMQ_NS
#define UMQ_NS UMQ_HIT
#endif
static struct {
    uint16_t hit[NTRK];                                 /* each track's hits, falling off (Q8) */
    uint32_t qr, ms;                                    /* the notes read; the last frame's time */
} umq __attribute__((section(".pool")));
static uint8_t *umq_b(uint32_t r)
{
    uint32_t a = UMQ_AT[r % UMQ_NR], i;
    uint8_t *q;
    if (favorites.factory[6][18] != UMQ_MK) {          /* (once: the routes at first) */
        for (i = 0; i < UMQ_NR; i++) {
            q = &favorites.factory[UMQ_AT[i] & 32u ? 6 : 4][UMQ_AT[i] & 31u];
            q[0] = (uint8_t)(i < 4u ? (UMQ_M1 + i) | i << 5 : 0u);
            q[1] = (uint8_t)(i < 4u ? UMQ_SET : 0u);
            q[2] = (uint8_t)(i < 3u ? 99u : 0u);
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
    return i < UMT_NS ? umt_raw(i) : 99u;               /* (the being: whole) */
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
static void umq_knob(uint32_t k, int32_t s)
{
    uint32_t v[4], m;
    int32_t r;
    umq_get(umq_sel, v);
    if (k < 2u)                                         /* SOURCE, TARGET, a detent each (FROM and TO kept within its range) */
        v[k] = (uint32_t)clamp((int32_t)v[k] + s, 0, (k ? UMT_NT : UMQ_NS) - 1);
    else {                                              /* FROM, TO: SET, then 0 .. its most */
        m = umq_max(v[1]);
        r = v[k] == UMQ_SET ? 0 : (int32_t)v[k] + 1;
        r = clamp(r + accel(EN_K1 + k, s, (int32_t)m), 0, (int32_t)m + 1);
        v[k] = r ? (uint32_t)r - 1u : UMQ_SET;
    }
    umq_put(umq_sel, v);
}

/* ------------------------------------------------------------------------------------------- the pages --- */
static int umk_on(void) { return UMB_ON && !umt.on && home_bare() && ui.hset[0]; }   /* a MACROS page shown */
/* route i's words, a column each (MACROS - AUDIO: i a slot, or + NEW); 1: it acts (else drawn dim) */
static int umk_names(uint32_t i, char (*b)[12], int cards)
{
    uint32_t v[4], k;
    const char *u;
    if (ui.hset[0] == 1u) {
        const int16_t *p = uma_p(i);                    /* (+ NEW's: none) */
        for (k = 0; k < 4u; k++)
            b[k][0] = 0;
        if (i == UMA_NEW) {
            str_cpy(b[0], "+ NEW", 12);
            return 0;
        }
        str_cpy(b[0], N_MSRC[clamp(p[0], 0, MS_N - 1)], 12);
        b[1][0] = 'T', b[1][1] = (char)('1' + i / 4u), b[1][2] = 0;
        str_cpy(b[2], mod_dst_name(&trk[i / 4u % NTRK], p[1]), 12);
        param_format(&TP[P_M1AMT], p[2], b[3], &u);    /* (+50: LFO > MOD's) */
        return p[0] && p[1] && p[2];
    }
    umq_get(i, v);
    if (v[0] < UMQ_HIT)                                 /* (OFF, M1 .. M4: the matrix's names) */
        str_cpy(b[0], N_MSRC[v[0] ? MS_M1 + v[0] - UMQ_M1 : MS_OFF], 12);
#if UMQ_SND
    else if (v[0] >= UMQ_LEVEL)
        str_cpy(b[0], v[0] == UMQ_LEVEL ? "LEVEL" : "BEAT", 12);
    else {
        static const char *const SN[3] = {"HIT", "LFO", "STEP"};
        b[0][0] = 'T', b[0][1] = (char)('1' + (v[0] - UMQ_HIT) % NTRK), b[0][2] = ' ';
        str_cpy(b[0] + 3, SN[(v[0] - UMQ_HIT) / NTRK], 9);
    }
#endif
    str_cpy(b[1], umt_tname(umb_pl(), v[1]), 12);
    umq_val(b[2], v[1], v[2]);
    umq_val(b[3], v[1], v[3]);
    if (!v[0] && !cards)                                /* (OFF: in the list, its source alone) */
        b[1][0] = b[2][0] = b[3][0] = 0;
    return v[0] != 0u;
}
static void umk_algo(int32_t s)                         /* ALGORITHM: the route, a detent each */
{
    uint8_t L[NTRK * 4u + 1u];
    uint32_t n;
    if (ui.hset[0] == 1u) {
        n = uma_list(L);
        uma_sel = L[clamp((int32_t)uma_at(L, n) + s, 0, (int32_t)n - 1)];
    } else
        umq_sel = (uint8_t)clamp((int32_t)umq_sel + s, 0, UMQ_NR - 1);
}
static void umk_knob(uint32_t k, int32_t s)
{
    if (ui.hset[0] == 1u)
        uma_knob(k, s);
    else
        umq_knob(k, s);
}
static void umk_cards(void)                             /* KNOB 1..4: the chosen route's columns */
{
    static const char *const LB[2][4] = {{"SOURCE", "TRACK", "DEST", "AMOUNT"}, {"SOURCE", "TARGET", "FROM", "TO"}};
    char b[4][12];
    uint32_t k, au = ui.hset[0] == 1u;
    int on = umk_names(au ? uma_sel : umq_sel, b, 1);
    for (k = 0; k < 4u; k++)
        text_column(k, LB[!au][k], b[k], on || !k ? VAL(k) : T_DIM);
}
/* the routes over the plate, a line each from canvas row y (h rows), each column over its knob's; the chosen one in the
 * theme's colour, scrolled into sight */
static void umk_draw(int32_t x0, int32_t y, int32_t h)
{
    uint8_t L[NTRK * 4u + 1u], ha = cv_halo;
    char b[4][12];
    uint32_t n, i, k, sel, rows = (uint32_t)h / 11u, au = ui.hset[0] == 1u;
    if (au) {
        n = uma_list(L);
        sel = uma_at(L, n);
    } else {
        n = UMQ_NR;
        sel = umq_sel;
    }
    cv_halo = 1;
    for (i = sel >= rows ? sel - rows + 1u : 0u, k = i + rows; i < n && i < k; i++, y += 11) {
        uint32_t c;
        uint16_t col = umk_names(au ? L[i] : i, b, 0) ? T_TEXT : T_DIM;
        if (i == sel)
            col = T_THEME;
        for (c = 0; c < 4u; c++)
            cv_text(4 + 60 * (int32_t)c - x0, y, &AF_X, b[c], col);
    }
    cv_halo = ha;
}
