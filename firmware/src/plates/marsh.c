/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* MARSH, a plate module (umb_plate.h): the marsh of UMBRA OBSCURA, the game: the horizon, water under it in short dashes
 * that shimmer, reeds at both edges swaying, a thin moon. A note lights the water where its pitch falls (low at the left)
 * in its track's colour and drops a ring there; the kick is a gust through the reeds; the rest of the kit are fireflies
 * over the water. Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone
 * (REPLACE: black under it).
 * Elements: WIND (the reeds' sway at rest), FADE (how long the water stays lit), SIZE (the horizon: low .. high),
 * WATER (the notes that are no drum), GUST (the kick), FLIES (the rest of the kit), REEDS and MOON (their own light). */
#include "plate_lib.h"

#define PLW_ND 44                                       /* the water's dashes */
#define PLW_NR 28                                       /* the reeds */
#define PLW_NF 10                                       /* fireflies at once */
#define PLW_NP 4                                        /* rings at once */
typedef struct {
    uint8_t d[PLW_ND], dc[PLW_ND];                      /* each dash: the notes' light on it, whose (its track) */
    uint32_t ft[PLW_NF], pt[PLW_NP];                    /* the fireflies, the rings: since when .. */
    uint8_t fx[PLW_NF], fy[PLW_NF], fv[PLW_NF];         /* .. where, how hard */
    uint8_t px[PLW_NP], pv[PLW_NP], pc[PLW_NP];
    uint16_t gust;                                      /* the gust (Q8) */
    uint32_t rs;
    uint8_t nf, np;
} plw_t;
static uint32_t plw_h(uint32_t i) { return (i + 1u) * 2654435761u; }   /* (a dash's, a reed's own dice) */
static int32_t plw_hy(const umb_lctx_t *cx) { return 176 - cx->k[2] * 46 / 50; }   /* the horizon (SIZE 50: row 130) */
static int32_t plw_dx(uint32_t i) { return (int32_t)((plw_h(i) >> 8) % 240u); }

static void plw_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plw_t *m = st;
    (void)a;
    m->rs = 97u + cx->now;
}
static void plw_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plw_t *m = st;
    uint32_t i;
    int32_t v, x;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > m->gust)
        m->gust = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {
        i = m->nf++ % PLW_NF;
        m->ft[i] = cx->now | 1u;
        m->fx[i] = (uint8_t)(10u + pl_rnd(&m->rs) % 220u);
        m->fy[i] = (uint8_t)(pl_rnd(&m->rs) % 60u);
        m->fv[i] = (uint8_t)v;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))
        return;
    x = 12 + pl_fold(nt->mo[0], pl_pk(nt->note), 256) * 216 / 256;
    for (i = 0; i < PLW_ND; i++) {                      /* the dashes within 36 px of it, the nearer the more */
        int32_t dx = pl_abs(plw_dx(i) - x), e = dx < 36 ? v * (36 - dx) / 36 : 0;
        if (e > m->d[i]) {
            m->d[i] = (uint8_t)e;
            m->dc[i] = nt->track;
        }
    }
    i = m->np++ % PLW_NP;
    m->pt[i] = cx->now | 1u;
    m->px[i] = (uint8_t)x;
    m->pv[i] = (uint8_t)v;
    m->pc[i] = nt->track;
}
static void plw_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plw_t *m = st;
    int32_t k1 = cx->k[1], f = (int32_t)cx->dt * 65536 / (300 + k1 * k1 * 4 / 5), i;   /* (FADE 50: 2.3 s) */
    (void)a;
    for (i = 0; i < PLW_ND; i++)
        m->d[i] = (uint8_t)pl_dec(m->d[i], f);
    m->gust = pl_dec(m->gust, (int32_t)cx->dt * 82);    /* (0.8 s) */
}
static void plw_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plw_t *m = st;
    int32_t hy = plw_hy(cx), i, rl = 110 * cx->k[6] / 50, wind = 40 + cx->k[0] * 216 / 50 + m->gust * 6;
    pl_clear(S);
    pl_line(S, 0, hy * 256, 239 * 256, hy * 256, 34, PL_DEEP);   /* the horizon */
    if (cx->k[7]) {                                     /* the moon: a thin one, two arcs */
        pl_arc(S, a, 196 * 256, (hy - 78) * 256, 11 * 256, 0xB0000000u, 0x90000000u, 70 * cx->k[7] / 50, PL_WT);
        pl_arc(S, a, 192 * 256, (hy - 80) * 256, 11 * 256, 0xC8000000u, 0x68000000u, 44 * cx->k[7] / 50, PL_INK);
    }
    for (i = 0; i < PLW_ND; i++) {                      /* the water: longer dashes nearer, shimmering; lit by the notes */
        uint32_t h = plw_h((uint32_t)i);
        int32_t y = hy + 4 + (int32_t)((h >> 20) % (uint32_t)(236 - hy)), x = plw_dx((uint32_t)i), w = 3 + (int32_t)((h >> 28) * (uint32_t)(y - hy) / 28u),
                sh = 26 + ((pl_sine(a, cx->now * (150000u + (h >> 12 & 255u) * 900u) + h) + 32768) * 30 >> 16);
        pl_line(S, (x - w) * 256, y * 256, (x + w) * 256, y * 256, sh, PL_DEEP);
        if (m->d[i])
            pl_line(S, (x - w) * 256, y * 256, (x + w) * 256, y * 256, m->d[i], a->colour[(m->dc[i] * 3u) % 8u]);
    }
    for (i = 0; i < PLW_NP; i++) {                      /* the rings: an ellipse on the water, widening over 0.9 s */
        uint32_t age = cx->now - m->pt[i], k;
        int32_t r, e;
        if (!m->pt[i] || age >= 900u)
            continue;
        r = (4 + (int32_t)age / 22) * 256;
        e = m->pv[i] * (int32_t)(900u - age) / 900;
        for (k = 0; k < 40u; k++)
            pl_splat(S, m->px[i] * 256 + pl_pol(a, r, k * 107374182u + 0x40000000u), (hy + 22) * 256 + pl_pol(a, r / 4, k * 107374182u), e,
                     a->colour[(m->pc[i] * 3u) % 8u]);
    }
    for (i = 0; i < PLW_NR; i++) {                      /* the reeds: at both edges, bending with the wind (two strokes each) */
        uint32_t h = plw_h((uint32_t)i + 77u);
        int32_t x = (i & 1 ? 238 - (int32_t)((h >> 8) % 52u) : 2 + (int32_t)((h >> 8) % 52u)) * 256, ht = (18 + (int32_t)((h >> 16) % 44u)) * 256,
                by = (hy + 3 + (int32_t)((h >> 24) % 12u)) * 256,
                sw = ((pl_sine(a, cx->now * 480000u + (uint32_t)i * 1160000000u) >> 7) * 3 + (pl_sine(a, cx->now * 190000u + h) >> 7) * 2) * wind >> 8;
        pl_line(S, x, by, x + sw * 3 / 10, by - ht * 6 / 10, rl, PL_DEEP);
        pl_line(S, x + sw * 3 / 10, by - ht * 6 / 10, x + sw, by - ht, rl, PL_DEEP);
    }
    for (i = 0; i < PLW_NF; i++) {                      /* the fireflies: a warm point over the water, 0.7 s */
        uint32_t age = cx->now - m->ft[i];
        if (m->ft[i] && age < 700u)
            pl_glint(S, m->fx[i] * 256 + pl_pol(a, 4 * 256, age * 5000000u), (hy - 8 - m->fy[i]) * 256, m->fv[i] * (int32_t)(700u - age) / 700,
                     PL_WARM[0]);
    }
}

UMB_PLATE_HEAD const umb_plate_t umb_plate_marsh = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plw_t), "MARSH",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"WIND", 50}, {"FADE", 50}, {"SIZE", 50}, {"WATER", 50}, {"GUST", 50}, {"FLIES", 50}, {"REEDS", 50}, {"MOON", 50}},
    plw_init, plw_note, plw_frame, plw_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0}};
