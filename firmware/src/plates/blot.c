/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* BLOT, a plate module (umb_plate.h): ink let fall on wet paper. A field of ink (60 x 60, 12 steps a second: ink creeps) in which
 * every cell is drawn up toward its fullest neighbour less what the paper keeps on the way (more here, less there: the
 * noise table), so a drop bleeds out into a blot with a ragged edge and stops; all of it dries at one rate, so the edge
 * draws back in until the blot is gone. It is seen through a curve with a bright rim where the ink runs out (as a stain
 * dries darkest at its edge) and through the paper's grain (the noise table again, long across). Its own palettes: the
 * notes' ink violet, the drums' each their own (the kick rose, the snare and clap teal, the hats gold, the toms, rim and
 * bell ice blue), the rim pale; a cell keeps the ink it was given, and ink that bleeds into bare paper takes its fullest
 * neighbour's (bare paper forgets). A note is a drop where its pitch falls (C2 left .. C7 right), its track its height; the kick a
 * splat: a large drop and what it throws off; the rest of the kit flecks.
 * Elements: BLEED (how far the ink runs), DRY (how soon it is gone), GRAIN (the paper's), BLOT (the notes that are no
 * drum), SPLAT (the kick), FLECK (the rest of the kit), RIM (the edge's light).
 * (NEW, API 6) PEN (on/off; SPEED: how fast the nib travels; NIB: how wide it writes, pressing harder as the music
 * swells): a pen wandering over the wet paper in loops, its line bleeding as it goes. DRIP (on/off; RUN: how fast a
 * drip runs down; INK: how far it gets): a note's drop runs down the paper in a thin streak. */
#include "plate_lib.h"

static const uint8_t FBL_ST[5][4][3] = {               /* its palettes: the notes' ink; the kick's, snare's, hats', the rest's */
    {{34, 12, 84}, {92, 50, 178}, {156, 112, 236}, {240, 226, 255}}, {{70, 6, 36}, {180, 24, 84}, {250, 96, 150}, {255, 226, 236}},
    {{6, 44, 62}, {14, 140, 160}, {80, 220, 224}, {226, 255, 250}}, {{62, 30, 4}, {176, 104, 16}, {248, 184, 64}, {255, 244, 212}},
    {{12, 30, 84}, {36, 100, 210}, {110, 180, 255}, {232, 244, 255}}};
typedef struct {
    uint8_t w[PL_S];                                    /* the ink, a byte a cell */
    uint8_t rp[PL_N], rc[PL_N];                         /* the step's rows as they were: the one above, this one */
    uint8_t h[PL_S];                                    /* whose ink each cell's is (0 the notes', 1 .. 4 the drums') */
    uint16_t lut[2][256];                               /* the notes' palette as the panel takes it .. */
    uint16_t dk[4][2][128];                             /* .. the drums', half as fine */
    uint8_t cv[256];                                    /* the ink's light by how much there is .. */
    uint8_t kr;                                         /* .. as RIM was when it was made (+ 1) */
    uint8_t dq;                                         /* the drying owed (Q8 of a level) */
    uint32_t rs, ms;                                    /* the dice, the steps' clock */
    uint32_t xr, pp;                                    /* (NEW, API 6) DRIP's dice; PEN: how far round its loops (Q16 ..) */
    int32_t px, py;                                     /* .. where its nib was (cells, Q8; px 0: not yet down) */
    uint16_t dx[6], dy[6];                              /* DRIP: each one where (cells, Q8) .. */
    uint8_t dl[6], dw[6], nd;                           /* .. the ink left to it (0: none), its wander; the next */
} fbl_t;

static void fbl_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fbl_t *s = st;
    int32_t k, i;
    (void)a;
    s->rs = 2791u + cx->now;
    s->ms = cx->now;
    s->xr = 5309u + cx->now;                            /* (NEW, API 6) DRIP's dice, apart from the plate's own */
    pl_lut(s->lut, FBL_ST[0], 4);
    for (k = 0; k < 4; k++) {                           /* (the drums' made in the ink's field, bare as yet, then halved) */
        uint16_t (*t)[256] = (uint16_t (*)[256])(void *)s->w;
        pl_lut(t, FBL_ST[k + 1], 4);
        for (i = 0; i < 256; i++)
            s->dk[k][i >> 7][i & 127] = t[i >> 7][(i & 127) * 2 + 1];
    }
    for (i = 0; i < 1024; i++)
        s->w[i] = 0;
}
/* the cells round X, Y (cells, Q8; where note n's track has moved it) within r given ink hu */
static void fbl_hue(fbl_t *s, const umb_note_t *n, int32_t X, int32_t Y, int32_t r, uint32_t hu)
{
    int32_t cx = 768 + pl_fold(n->mo[0], X - 768, 13824), cy = 768 + pl_fold(n->mo[1], Y - 768, 13824), x, y;
    for (y = pl_clamp((cy - r) >> 8, 0, PL_N - 1); y <= pl_clamp((cy + r + 255) >> 8, 0, PL_N - 1); y++)
        for (x = pl_clamp((cx - r) >> 8, 0, PL_N - 1); x <= pl_clamp((cx + r + 255) >> 8, 0, PL_N - 1); x++)
            if ((x * 256 - cx) * (x * 256 - cx) + (y * 256 - cy) * (y * 256 - cy) <= r * r)
                s->h[y * PL_N + x] = (uint8_t)hu;
}
static void fbl_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    fbl_t *s = st;
    int32_t v, k, i;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* SPLAT (the kick): a large drop, three thrown off it */
        int32_t X = (int32_t)(18u * 256u + pl_rnd(&s->rs) % 6144u), Y = (int32_t)(18u * 256u + pl_rnd(&s->rs) % 6144u);
        pl_stamp_at(s->w, nt, X, Y, 620, 120 + ((135 * v) >> 8));
        fbl_hue(s, nt, X, Y, 620, 1u);
        for (i = 0; i < 3; i++) {
            uint32_t ph = pl_rnd(&s->rs);
            int32_t R = (5 + (int32_t)(pl_rnd(&s->rs) % 12u)) * 256;
            pl_stamp_at(s->w, nt, X + pl_pol(a, R, ph + 0x40000000u), Y + pl_pol(a, R, ph), 260, 60 + ((120 * v) >> 8));
            fbl_hue(s, nt, X + pl_pol(a, R, ph + 0x40000000u), Y + pl_pol(a, R, ph), 260, 1u);
        }
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0)   /* FLECK (its ink: the snare and clap's, the hats', the rest's) */
        for (i = nt->lane < 3u ? 3 : 1; i > 0; i--) {
            int32_t X = (int32_t)(4u * 256u + pl_rnd(&s->rs) % 13312u), Y = (int32_t)(4u * 256u + pl_rnd(&s->rs) % 13312u);
            pl_stamp_at(s->w, nt, X, Y, 300, 50 + ((90 * v) >> 8));
            fbl_hue(s, nt, X, Y, 300, !nt->drum ? 0u : nt->lane < 3u ? 2u : nt->lane < 5u ? 3u : 4u);
        }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* BLOT */
        return;
    k = pl_pk(nt->note);
    pl_stamp_at(s->w, nt, 768 + k * 54, (10 + 8 * (int32_t)nt->track) * 256, 420 + (((256 - k) * 300) >> 8), 110 + ((145 * v) >> 8));
    fbl_hue(s, nt, 768 + k * 54, (10 + 8 * (int32_t)nt->track) * 256, 420 + (((256 - k) * 300) >> 8), nt->drum ? 1u + (nt->lane > 0u) + (nt->lane > 2u) + (nt->lane > 4u) : 0u);
    if (cx->xk[1][0]) {                                 /* (NEW, API 6) DRIP: the drop starts a run down from under it */
        i = s->nd++ % 6u;
        s->dx[i] = (uint16_t)(768 + pl_fold(nt->mo[0], k * 54, 13824));
        s->dy[i] = (uint16_t)(768 + pl_fold(nt->mo[1], (12 + 8 * (int32_t)nt->track) * 256 - 768, 13824));
        s->dl[i] = (uint8_t)(40 + cx->xk[1][2] * 2 + v / 4);
        s->dw[i] = (uint8_t)pl_rnd(&s->xr);
    }
}
/* a step: each cell dries (DRY: 1 .. 20 s from full), then comes half way up to its fullest neighbour as it was, less
 * what the paper keeps on the way there (BLEED; one to three times it by the paper's noise) */
static void fbl_step(fbl_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int32_t d0 = 6 + (99 - (int32_t)cx->k[0]) * 30 / 99, k1 = (int32_t)cx->k[1], dry = s->dq + 218 + k1 * k1 * 4134 / 9801, x, y;
    s->dq = (uint8_t)dry;
    dry >>= 8;
    pl_cpy(s->rp, s->w, PL_N);
    for (y = 0; y < PL_N; y++) {
        uint8_t *hr = s->w + y * PL_N;
        const uint8_t *nx = y < PL_N - 1 ? hr + PL_N : s->rc, *tn = a->noise + (y & (PL_T - 1)) * PL_T;
        pl_cpy(s->rc, hr, PL_N);
        for (x = 0; x < PL_N; x++) {
            int32_t xl = x ? x - 1 : 1, xr = x < PL_N - 1 ? x + 1 : PL_N - 2, d = d0 + ((tn[x & (PL_T - 1)] * d0) >> 7),
                    m = s->rc[xl] > s->rc[xr] ? s->rc[xl] : s->rc[xr], q = s->rp[xl] > s->rp[xr] ? s->rp[xl] : s->rp[xr], c = s->rc[x] - dry;
            m = m > s->rp[x] ? m : s->rp[x];
            m = (m > nx[x] ? m : nx[x]) - d;
            q = q > nx[xl] ? q : nx[xl];                /* (the four at its corners: 1.4 times as far) */
            q = (q > nx[xr] ? q : nx[xr]) - ((d * 362) >> 8);
            m = m > q ? m : q;
            c = c < 0 ? 0 : c;
            if (m > c && c < 8) {                       /* (bleeding into bare paper: its fullest neighbour's ink) */
                int32_t b = xl, bv = s->rc[xl];
                if (s->rc[xr] > bv)
                    bv = s->rc[xr], b = xr;
                if (s->rp[x] > bv)
                    bv = s->rp[x], b = x - (y ? PL_N : 0);
                if (nx[x] > bv)
                    b = x + (y < PL_N - 1 ? PL_N : 0);
                s->h[y * PL_N + x] = s->h[y * PL_N + b];
            }
            hr[x] = (uint8_t)(m > c ? c + ((m - c + 1) >> 1) : c);
            if (!hr[x])
                s->h[y * PL_N + x] = 0;
        }
        pl_cpy(s->rp, s->rc, PL_N);
    }
}
static void fbl_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fbl_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 80u);
    int32_t i, pk = 84 + (int32_t)cx->k[6] * 171 / 99;
    if (s->kr != cx->k[6] + 1u) {                       /* the curve: up to the rim in 12 levels, down to the body by 44,
                                                         * the body one level (drying, only a blot's edge changes) */
        s->kr = (uint8_t)(cx->k[6] + 1u);
        for (i = 0; i < 256; i++)
            s->cv[i] = (uint8_t)(i < 12 ? pk * i / 12 : i < 44 ? pk - (pk - 84) * (i - 12) / 32 : 84);
    }
    while (n--)
        fbl_step(s, cx, a);
    /* (NEW, API 6) PEN: the nib goes round two loops through each other (SPEED), the line laid from where it was to where
     * it is, a dot each cell; NIB its width, the level how hard it presses */
    if (cx->xk[0][0]) {
        int32_t X, Y, dx, dy, m, j;
        s->pp += cx->dt * (uint32_t)(20 + cx->xk[0][1] * 3);
        X = 30 * 256 + pl_pol(a, 22 * 256, s->pp * 9000u) + pl_pol(a, 5 * 256, s->pp * 23000u + 0x40000000u);
        Y = 30 * 256 + pl_pol(a, 20 * 256, s->pp * 13000u + 0x40000000u) + pl_pol(a, 6 * 256, s->pp * 31000u);
        if (!s->px)
            s->px = X, s->py = Y;
        dx = X - s->px;
        dy = Y - s->py;
        m = (pl_abs(dx) > pl_abs(dy) ? pl_abs(dx) : pl_abs(dy)) / 256 + 1;
        for (j = 1; j <= m && m < 40; j++)
            pl_stamp(s->w, s->px + dx * j / m, s->py + dy * j / m, 200 + cx->xk[0][2] * 3, 12 + cx->xk[0][2] / 4 + cx->level / 12);
        s->px = X;
        s->py = Y;
    } else
        s->px = 0;
    /* (NEW, API 6) DRIP: each run goes down (RUN: 3 .. 20 cells a second), wandering a little, a thin line of ink, until its
     * ink is spent or it is off the paper */
    for (i = 0; cx->xk[1][0] && i < 6; i++)
        if (s->dl[i]) {
            int32_t y = s->dy[i] + (int32_t)cx->dt * (3 + cx->xk[1][1] * 17 / 99) * 256 / 1000, x = s->dx[i] + (pl_sine(a, (uint32_t)y * 2400000u + s->dw[i] * 16843009u) >> 12);
            pl_stamp(s->w, x, y, 220, 12 + s->dl[i] / 8);
            s->dx[i] = (uint16_t)pl_clamp(x, 0, (PL_N - 1) * 256);
            s->dy[i] = (uint16_t)y;
            s->dl[i] = (uint8_t)(y >= (PL_N - 1) * 256 || s->dl[i] < 2 ? 0 : s->dl[i] - 1 - (int32_t)(cx->dt > 60) );
        }
}
static void fbl_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const fbl_t *s = st;
    uint8_t xi[240], xf[240];
    int32_t vv[PL_N + 1], x0 = S->x0, gr = (int32_t)cx->k[2] * 2, cxx, cy;
    pl_cols(S, xi, xf);
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = S->y0 + cy, vg = S->vg[cy], any = pl_vrow(s->w, py, vv, 255);
        int32_t ht = pl_clamp((2 * py - 3) << 5, 0, (PL_N - 1) * 256 - 1);   /* (the nearest cell's ink, dithered between) */
        const uint8_t *hr = s->h + (ht >> 8) * PL_N;
        uint16_t *row = S->px + cy * S->w;
        uint32_t u = (uint32_t)(x0 * 150 + py * 13), w = (uint32_t)(py * 46 - x0 * 9);   /* (the grain: long across) */
        for (cxx = 0; cxx < S->w; cxx++, u += 150u, w -= 9u) {
            int32_t v = any ? PL_LERP(vv, xi[cxx], xf[cxx]) : 0, b;
            if (v <= 0) {
                row[cxx] = 0;
                continue;
            }
            b = (s->cv[v] * (256 - ((gr * (255 - (pl_tbil(a, (int32_t)u, (int32_t)w) >> 8))) >> 8))) >> 8;
            b = PL_IX(b, vg);
            v = (cxx + py) & 1 ? 80 : 176;
            v = hr[((ht & 255) > v) * PL_N + xi[cxx] + (xf[cxx] > 256 - v)];
            row[cxx] = v ? s->dk[v - 1][(x0 + cxx ^ py) & 1][b >> 1] : s->lut[(x0 + cxx ^ py) & 1][b];
        }
    }
}

#include "splash/blot.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_blot = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(fbl_t), "BLOT",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 7u, {0, 0, 0},
    {{"BLEED", 50}, {"DRY", 45}, {"GRAIN", 50}, {"BLOT", 50}, {"SPLAT", 50}, {"FLECK", 50}, {"RIM", 70}},
    fbl_init, fbl_note, fbl_frame, fbl_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "Ink bleeds into wet paper and dries", 3u, {0, 0, 0},
    {{"STAIN", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 6u)}, {99, 99, 50, 30}},
     {"WASH", {96, 16, 26, 31, 0, 0, 21, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 0u), 0}, {99, 99, 20, 0}},
     {"SPATTER", {21, 86, 81, 0, 96, 96, 100, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 6u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 40, 30}}},
    18u, {0, 0, 0}, SPL_BLOT,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"PEN", 3u, 1u, {0, 0}, {{"ON", 0}, {"SPEED", 40}, {"NIB", 40}}, "A pen writes loops across the wet paper, its line bleeding as it goes",
      {"Pen on or off", "How fast the nib travels", "How wide it writes (it presses harder as the music swells)"}},
     {"DRIP", 3u, 1u, {0, 0}, {{"ON", 0}, {"RUN", 50}, {"INK", 50}}, "Each note's drop runs down the paper in a thin streak",
      {"Drips on or off", "How fast a drip runs down", "How far it gets before its ink is spent"}}}};
