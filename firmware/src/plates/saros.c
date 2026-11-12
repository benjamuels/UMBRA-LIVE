/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* SAROS as a plate module (umb_plate.h; moved out of ui_umbra_melody.c unchanged, pixel for pixel): the melody as string
 * art on the circle of fifths, the whole plate turning once in 100 s round a dark umbra. A note lands on its pitch class
 * (its track's root at the marked tick), its octave the ring; each melodic track's notes are joined in turn by a thread in
 * its colour. Fifths and fourths stay near the rim, thirds cut across, tritones pass through the umbra and vanish; a phrase
 * played again retraces its threads and burns them brighter (a thread fades in ~6 s). Its light is added to the strip: in
 * REPLACE the strip is cleared to black first (JIANT's), in ADD the threads light whatever is under them.
 * Elements: SPIN, FADE, SIZE (the umbra's); THREAD and POINT (the notes that are no drum: the threads they join, the
 * points they light; each as hard as its value, its tracks LIVE VIZ CTRLS' to choose), DIAL (the dial's ticks and the
 * umbra's rim). */
#include "plate_lib.h"

#define PLS_CX 120                                      /* the centre (px) */
#define PLS_CY 120
#define PLS_TH 48                                       /* threads at most */
#define PLS_ND 12                                       /* .. the notes' points glowing */
static const uint8_t PLS_TEAL[3] = {143, 217, 201}, PLS_REST[3] = {34, 60, 96};
typedef struct {
    uint8_t ta[PLS_TH], tb[PLS_TH], tc[PLS_TH], tf[PLS_TH];   /* a thread's notes (a < b), track, the note before */
    uint16_t te[PLS_TH];                                /* .. its energy (Q8: burned up to 2.5) */
    uint32_t tt[PLS_TH];                                /* .. when it was last played (ms) */
    int32_t lx[PLS_TH], ly[PLS_TH], lsx[PLS_TH], lsy[PLS_TH];   /* .. as this frame draws it: from, a sample's step (px, Q8) */
    int16_t ln[PLS_TH];                                 /* .. its samples */
    uint8_t lc[PLS_TH][3];                              /* .. its light */
    uint8_t nn[PLS_ND], nc[PLS_ND], nv[PLS_ND];         /* the notes' points: note, track, velocity .. */
    uint32_t nt[PLS_ND];                                /* .. when */
    int16_t ux[12], uy[12];                             /* the twelve places on the circle of fifths, turned (Q15) */
    uint8_t nth, nnd, last[UMB_NTRK];                   /* threads in use, the next point; each track's last note + 1 */
    uint32_t th, tms;                                   /* the turn (2^32 a turn) and its clock */
} pls_t;

static int32_t pls_ru(const umb_lctx_t *cx)            /* the umbra's radius (SIZE: 26 px at 50, px Q8) */
{
    int32_t v = (int32_t)cx->k[2];
    return 26 * (v <= 50 ? 64 + v * 192 / 50 : 256 + (v - 50) * 256 / 49);
}
static int32_t pls_fk(const umb_lctx_t *cx) { return 506 / ((int32_t)cx->k[1] + 1); }   /* FADE's rate (45: 11) */
/* note n of track c on the dial (px, Q8): its place on the circle of fifths from the track's root, its octave the ring;
 * NOTE MOVEMENT turns the track's whole web round the dial and moves it out or in */
static void pls_pos(const pls_t *m, const umb_lctx_t *cx, uint32_t c, uint32_t n, int32_t *X, int32_t *Y)
{
    int32_t ru = pls_ru(cx);
    uint32_t k = ((n + 120u - cx->root[c]) % 12u * 7u + (uint32_t)(24 + ((cx->mo[c][0] * 12) >> 8))) % 12u;
    int32_t r = ru + 3584 + pl_fold(cx->mo[c][1], pl_clamp((int32_t)n / 12 - 3, 0, 5), 5) * (23936 - ru) / 5;
    *X = PLS_CX * 256 + ((r * m->ux[k]) >> 15);
    *Y = PLS_CY * 256 + ((r * m->uy[k]) >> 15);
}
static int32_t pls_glow(const pls_t *m, const umb_lctx_t *cx, const umb_api_t *a, uint32_t i)   /* a thread's energy now (Q8) */
{
    uint32_t age = cx->now - m->tt[i];
    return (m->te[i] * pl_exp(a, (int32_t)((age < 1048576u ? age : 1048576u) * (uint32_t)pls_fk(cx) >> 11))) >> 8;
}
static void pls_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    pls_t *m = st;
    (void)a;
    m->tms = cx->now;
}
/* a note played: its point, and the thread from the track's note before (one already there burns) */
static void pls_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    pls_t *m = st;
    uint32_t c = nt->track, n = nt->note, p = m->last[c], i, k, b0, b1;
    int32_t v, vp, lo = 1 << 30;
    if (!nt->vel)
        return;
    v = pl_drive(cx, 3u, nt, !nt->drum);                /* THREAD */
    vp = pl_drive(cx, 4u, nt, !nt->drum);               /* POINT */
    if (!v && !vp)
        return;
    if (vp) {
        m->nn[m->nnd] = (uint8_t)n;
        m->nc[m->nnd] = (uint8_t)c;
        m->nv[m->nnd] = (uint8_t)vp;
        m->nt[m->nnd] = cx->now;
        m->nnd = (uint8_t)((m->nnd + 1u) % PLS_ND);
    }
    m->last[c] = (uint8_t)(n + 1u);
    if (!v || !p-- || p == n)
        return;
    b0 = p < n ? p : n;
    b1 = p < n ? n : p;
    for (i = 0; i < m->nth && (m->ta[i] != b0 || m->tb[i] != b1 || m->tc[i] != c); i++)
        ;
    if (i == m->nth) {                                  /* a new thread (all in use: in place of the faintest) */
        if (m->nth < PLS_TH)
            m->nth++;
        else
            for (k = 0; k < PLS_TH; k++)
                if (pls_glow(m, cx, a, k) < lo) {
                    lo = pls_glow(m, cx, a, k);
                    i = k;
                }
        m->ta[i] = (uint8_t)b0;
        m->tb[i] = (uint8_t)b1;
        m->tc[i] = (uint8_t)c;
        m->te[i] = 0;
    }
    m->te[i] = (uint16_t)pl_clamp(pls_glow(m, cx, a, i) + v, 0, 640);
    m->tt[i] = cx->now;
    m->tf[i] = (uint8_t)p;
}
/* once a frame: the turn, the circle's places, the threads as drawn (a faded one goes) */
static void pls_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    pls_t *m = st;
    uint32_t i, j, k, th;
    m->th += (cx->now - m->tms) * (42950u * cx->k[0] / 30u);   /* (SPIN 30: a turn in 100 s; 0: still; 99: in 30 s) */
    m->tms = cx->now;
    th = m->th - m->th % 5892960u;                     /* (a step at a time: a pixel at the rim, 116 px) */
    for (i = 0; i < 12u; i++) {
        uint32_t ph = i * 357913941u - 0x40000000u + th;   /* (the root at the top, turned) */
        m->ux[i] = (int16_t)pl_sine(a, ph + 0x40000000u);
        m->uy[i] = (int16_t)pl_sine(a, ph);
    }
    for (i = j = 0; i < m->nth; i++) {
        int32_t e = (pl_clamp(pls_glow(m, cx, a, i), 0, 512) + 8) & ~15, X, Y, n, dx, dy;   /* (16 steps a unit) */
        const uint8_t *tc = a->colour[(m->tc[i] * 3u) % 8u];
        if (!e)
            continue;
        m->ta[j] = m->ta[i];
        m->tb[j] = m->tb[i];
        m->tc[j] = m->tc[i];
        m->tf[j] = m->tf[i];
        m->te[j] = m->te[i];
        m->tt[j] = m->tt[i];
        pls_pos(m, cx, m->tc[j], m->ta[j], &m->lx[j], &m->ly[j]);
        pls_pos(m, cx, m->tc[j], m->tb[j], &X, &Y);
        dx = X - m->lx[j];
        dy = Y - m->ly[j];
        n = ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy)) / 256 + 1;
        m->ln[j] = (int16_t)n;
        m->lsx[j] = dx / n;
        m->lsy[j] = dy / n;
        for (k = 0; k < 3u; k++)
            m->lc[j][k] = (uint8_t)pl_clamp((tc[k] * e) >> 8, 0, 255);   /* (its colour once played, brighter burned) */
        j++;
    }
    m->nth = (uint8_t)j;
}
static int pls_umbra(int32_t ru, int32_t X, int32_t Y)  /* behind the umbra */
{
    int32_t dx = (X >> 4) - PLS_CX * 16, dy = (Y >> 4) - PLS_CY * 16;
    return dx * dx + dy * dy < (ru >> 4) * (ru >> 4);
}
static void pls_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const pls_t *m = st;
    int32_t i, k, ru = pls_ru(cx), y0 = S->y0, ylo = (y0 - 2) * 256, yhi = (y0 + S->h + 1) * 256, dl = cx->k[5];   /* (DIAL: 50 as it was) */
    if (S->mode == UMB_DRAW_REPLACE)                    /* (the ground: black, as JIANT's) */
        for (i = 0; i < S->w * S->h; i++)
            S->px[i] = 0;
    if (y0 < PLS_CY + (ru >> 8) + 6 && y0 + S->h > PLS_CY - (ru >> 8) - 6)
        for (i = 0; i < 180; i++) {                     /* the umbra's corona */
            int32_t s = pl_sine(a, (uint32_t)i * 23860929u), c = pl_sine(a, (uint32_t)i * 23860929u + 0x40000000u), r = ru * 55 / 52;
            pl_splat(S, PLS_CX * 256 + ((c * r) >> 15), PLS_CY * 256 + ((s * r) >> 15), 51 * dl / 50, PLS_TEAL);
            r = ru * 60 / 52;
            pl_splat(S, PLS_CX * 256 + ((c * r) >> 15), PLS_CY * 256 + ((s * r) >> 15), 13 * dl / 50, PLS_TEAL);
        }
    for (i = 0; i < 12; i++)                            /* the dial: the root's tick teal */
        pl_splat(S, PLS_CX * 256 + ((29696 * m->ux[i]) >> 15), PLS_CY * 256 + ((29696 * m->uy[i]) >> 15), (i ? 179 : 128) * dl / 50,
                 i ? PLS_REST : PLS_TEAL);
    for (i = 0; i < m->nth; i++) {                      /* the threads, a sample a pixel (only the strip's) */
        int32_t X = m->lx[i], Y = m->ly[i], sx = m->lsx[i], sy = m->lsy[i], n = m->ln[i], k0 = 0, k1 = n;
        uint32_t age = cx->now - m->tt[i];
        if (sy) {
            int32_t ka = (ylo - Y) / sy, kb = (yhi - Y) / sy;
            k0 = ka < kb ? ka : kb;
            k1 = (ka < kb ? kb : ka) + 1;
            k0 = k0 < 0 ? 0 : k0;
            k1 = k1 > n ? n : k1;
        } else if (Y < ylo || Y > yhi)
            continue;
        for (k = k0; k <= k1; k++)
            if (!pls_umbra(ru, X + sx * k, Y + sy * k))
                pl_splat(S, X + sx * k, Y + sy * k, 256, m->lc[i]);
        if (age < 220u) {                               /* a spark runs from the note before to this one */
            int32_t f = (int32_t)(age * 1192u >> 10), Xa = X, Ya = Y, Xb = X + sx * n, Yb = Y + sy * n, x, y;
            if (m->tf[i] != m->ta[i]) {
                Xa = Xb; Ya = Yb; Xb = X; Yb = Y;
            }
            x = Xa + (((Xb - Xa) >> 4) * f >> 4);
            y = Ya + (((Yb - Ya) >> 4) * f >> 4);
            if (!pls_umbra(ru, x, y)) {
                pl_splat(S, x, y, 307, PL_WT);
                for (k = 0; k < 4; k++)
                    pl_splat(S, x + (k == 0 ? 256 : k == 1 ? -256 : 0), y + (k == 2 ? 256 : k == 3 ? -256 : 0), 77, PL_WT);
            }
        }
    }
    for (i = 0; i < PLS_ND; i++) {                      /* the notes' points: a short glow (~1 s) */
        uint32_t age = cx->now - m->nt[i];
        const uint8_t *tc = a->colour[(m->nc[i] * 3u) % 8u];
        int32_t b, X, Y, dx, dy;
        if (!m->nv[i] || age > 3000u)
            continue;
        b = ((((m->nv[i] * pl_exp(a, (int32_t)(age * 73u >> 11))) >> 8) * 230 >> 8) + 8) & ~15;
        pls_pos(m, cx, m->nc[i], m->nn[i], &X, &Y);
        X = (X + 128) >> 8;
        Y = (Y + 128) >> 8;
        if (!b || Y + 3 < y0 || Y - 3 >= y0 + S->h)
            continue;
        for (dy = -3; dy <= 3; dy++)
            for (dx = -3; dx <= 3; dx++) {
                int32_t w = 256 - (dx * dx + dy * dy) * 25;
                if (w > 0)
                    pl_px(S, X + dx, Y + dy, (b * ((w * w) >> 8)) >> 8, (tc[0] + PL_WT[0]) >> 1, (tc[1] + PL_WT[1]) >> 1,
                          (tc[2] + PL_WT[2]) >> 1);
            }
    }
}

UMB_PLATE_HEAD const umb_plate_t umb_plate_saros = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(pls_t), "SAROS",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 6u, {0, 0, 0},
    {{"SPIN", 30}, {"FADE", 45}, {"SIZE", 50}, {"THREAD", 50}, {"POINT", 50}, {"DIAL", 50}},
    pls_init, pls_note, pls_frame, pls_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, 0}};
