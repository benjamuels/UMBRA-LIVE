/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* BLOT, a plate module (umb_plate.h): ink let fall on wet paper. A field of ink (60 x 60, 12 steps a second: ink creeps) in which
 * every cell is drawn up toward its fullest neighbour less what the paper keeps on the way (more here, less there: the
 * noise table), so a drop bleeds out into a blot with a ragged edge and stops; all of it dries at one rate, so the edge
 * draws back in until the blot is gone. It is seen through a curve with a bright rim where the ink runs out (as a stain
 * dries darkest at its edge) and through the paper's grain (the noise table again, long across). Its own palette: violet
 * ink, the rim pale. A note is a drop where its pitch falls (C2 left .. C7 right), its track its height; the kick a
 * splat: a large drop and what it throws off; the rest of the kit flecks.
 * Elements: BLEED (how far the ink runs), DRY (how soon it is gone), GRAIN (the paper's), BLOT (the notes that are no
 * drum), SPLAT (the kick), FLECK (the rest of the kit), RIM (the edge's light). */
#include "plate_lib.h"

static const uint8_t FBL_ST[4][3] = {{34, 12, 84}, {92, 50, 178}, {156, 112, 236}, {240, 226, 255}};   /* its palette */
typedef struct {
    uint8_t w[PL_S];                                    /* the ink, a byte a cell */
    uint8_t rp[PL_N], rc[PL_N];                         /* the step's rows as they were: the one above, this one */
    uint16_t lut[2][256];                               /* the palette as the panel takes it */
    uint8_t cv[256];                                    /* the ink's light by how much there is .. */
    uint8_t kr;                                         /* .. as RIM was when it was made (+ 1) */
    uint8_t dq;                                         /* the drying owed (Q8 of a level) */
    uint32_t rs, ms;                                    /* the dice, the steps' clock */
} fbl_t;

static void fbl_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fbl_t *s = st;
    (void)a;
    s->rs = 2791u + cx->now;
    s->ms = cx->now;
    pl_lut(s->lut, FBL_ST, 4);
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
        for (i = 0; i < 3; i++) {
            uint32_t ph = pl_rnd(&s->rs);
            int32_t R = (5 + (int32_t)(pl_rnd(&s->rs) % 12u)) * 256;
            pl_stamp_at(s->w, nt, X + pl_pol(a, R, ph + 0x40000000u), Y + pl_pol(a, R, ph), 260, 60 + ((120 * v) >> 8));
        }
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0)   /* FLECK */
        for (i = nt->lane < 3u ? 3 : 1; i > 0; i--) {
            int32_t X = (int32_t)(4u * 256u + pl_rnd(&s->rs) % 13312u);
            pl_stamp_at(s->w, nt, X, (int32_t)(4u * 256u + pl_rnd(&s->rs) % 13312u), 300, 50 + ((90 * v) >> 8));
        }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* BLOT */
        return;
    k = pl_pk(nt->note);
    pl_stamp_at(s->w, nt, 768 + k * 54, (10 + 8 * (int32_t)nt->track) * 256, 420 + (((256 - k) * 300) >> 8), 110 + ((145 * v) >> 8));
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
            hr[x] = (uint8_t)(m > c ? c + ((m - c + 1) >> 1) : c);
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
}
static void fbl_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const fbl_t *s = st;
    uint8_t xi[240], xf[240];
    int32_t vv[PL_N + 1], x0 = S->x0, gr = (int32_t)cx->k[2] * 2, cxx, cy;
    pl_cols(S, xi, xf);
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = S->y0 + cy, vg = S->vg[cy], any = pl_vrow(s->w, py, vv, 255);
        uint16_t *row = S->px + cy * S->w;
        uint32_t u = (uint32_t)(x0 * 150 + py * 13), w = (uint32_t)(py * 46 - x0 * 9);   /* (the grain: long across) */
        for (cxx = 0; cxx < S->w; cxx++, u += 150u, w -= 9u) {
            int32_t v = any ? PL_LERP(vv, xi[cxx], xf[cxx]) : 0, b;
            if (v <= 0) {
                row[cxx] = 0;
                continue;
            }
            b = (s->cv[v] * (256 - ((gr * (255 - (pl_tbil(a, (int32_t)u, (int32_t)w) >> 8))) >> 8))) >> 8;
            row[cxx] = s->lut[(x0 + cxx ^ py) & 1][PL_IX(b, vg)];
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
    18u, {0, 0, 0}, SPL_BLOT};
