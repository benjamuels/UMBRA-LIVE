/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* MARSH, a plate module (umb_plate.h): the marsh of UMBRA OBSCURA, the game: the horizon, water under it in short dashes
 * that shimmer, reeds at both edges swaying, a thin moon. A note lights the water where its pitch falls (low at the left)
 * in its track's colour and drops a ring there; the kick is a gust through the reeds; the rest of the kit are fireflies
 * over the water. Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone
 * (REPLACE: black under it).
 * Elements: WIND (the reeds' sway at rest), FADE (how long the water stays lit), SIZE (the horizon: low .. high),
 * WATER (the notes that are no drum), GUST (the kick), FLIES (the rest of the kit), REEDS and MOON (their own light).
 * (NEW, API 6) HERON (ON, WADE, STRIKE): a heron standing in the shallows, its reflection under it; on the beat it
 * stabs at the water.
 * (NEW, API 6) STARS (ON, COUNT, GLEAM): stars over the marsh, each fading in, shining and fading out on its own clock
 * (only some lit at once), twinkling, brighter on the beat, their light broken on the water. */
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
/* (NEW, API 6) HERON: a line of it from x0, y0 to x1, y1 (px x 4 from its feet), drawn, or mirrored in the water (dim,
 * wavering) */
static void plw_hl(const umb_strip_t *S, const umb_api_t *a, const umb_lctx_t *cx, int32_t X, int32_t W, int32_t x0, int32_t y0,
                   int32_t x1, int32_t y1, int mir)
{
    int32_t d0 = 0, d1 = 0;
    y0 *= 64;
    y1 *= 64;
    if (mir) {
        d0 = pl_sine(a, cx->now * 900000u + (uint32_t)y0 * 40000u) >> 13;
        d1 = pl_sine(a, cx->now * 900000u + (uint32_t)y1 * 40000u) >> 13;
        y0 = -y0 / 2;
        y1 = -y1 / 2;
    }
    pl_line(S, X + (x0 + d0) * 64, W + y0, X + (x1 + d1) * 64, W + y1, mir ? 120 : 190, mir ? PL_DEEP : PL_INK);
}
/* its body and legs (px x 4 from its feet, facing left), then its neck and beak: standing .. stabbing (st 0 .. 256) */
static const int16_t PLW_HB[9][4] = {{-12, 0, -2, -96},     {12, 0, 2, -96},      {-2, -96, -40, -108}, {-40, -108, -24, -126},
                                     {-24, -126, 24, -122}, {24, -122, 64, -96}, {64, -96, 28, -100},  {28, -100, -2, -96},
                                     {24, -122, 54, -112}};
static void plw_heron(const umb_strip_t *S, const umb_api_t *a, const umb_lctx_t *cx, int32_t X, int32_t W, int32_t st)
{
    int32_t m, k, hx = -40 + ((-104 + 40) * st >> 8), hy = -186 + ((-14 + 186) * st >> 8), nx = -24 + ((-72 + 24) * st >> 8),
                  ny = -150 + ((-90 + 150) * st >> 8), bx = -40 + ((-20 + 40) * st >> 8), by = 2 + ((36 - 2) * st >> 8);
    for (m = 0; m < 2; m++) {
        for (k = 0; k < 9; k++)
            plw_hl(S, a, cx, X, W, PLW_HB[k][0], PLW_HB[k][1], PLW_HB[k][2], PLW_HB[k][3], m);
        plw_hl(S, a, cx, X, W, -40, -108, nx, ny, m);   /* (the neck, in two) */
        plw_hl(S, a, cx, X, W, nx, ny, hx, hy, m);
        plw_hl(S, a, cx, X, W, hx, hy, hx + bx, hy + by, m);   /* (the beak, the crest) */
        plw_hl(S, a, cx, X, W, hx, hy, hx + 22, hy - 6, m);
    }
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
    /* (NEW, API 6) STARS: over the marsh (COUNT of them), each on its own clock: it fades in somewhere, shines a while,
     * twinkling, fades out and stays dark, then comes back elsewhere, so only some are lit at once; GLEAM: how much the
     * music brightens them (the beat, the mix); under each a broken streak of its light on the water */
    if (cx->xk[1][0])
        for (i = 0; i < 4 + cx->xk[1][1] * 36 / 99; i++) {
            uint32_t h0 = pl_hash((uint32_t)i + 201u), per = 5000u + (h0 >> 20) % 7000u, t = cx->now + h0 % per, on = per * 2 / 5,
                     h = pl_hash((uint32_t)i * 977u + t / per), h2 = (h ^ (h >> 15)) * 0x2C1B3C6Du;
            int32_t x = (int32_t)((h >> 8) % 236u) + 2, y = PL_TOP(cx) + 4 + (int32_t)((h2 >> 8) % (uint32_t)pl_clamp(hy - 16 - PL_TOP(cx), 1, 240)),
                    tw = (pl_sine(a, cx->now * (200000u + (h >> 24) * 6000u) + h2) + 32768) >> 9, ev,
                    al = 40 + (tw * 3 >> 1) + ((cx->beat + cx->level) * cx->xk[1][2] >> 9);
            t %= per;                                   /* (its life: 0.8 s in, shining, 0.8 s out, then dark) */
            if (t >= on)
                continue;
            ev = (int32_t)(t < on - t ? t : on - t);
            al = al * (ev < 800 ? ev : 800) / 800;
            if (h2 & 1u)
                pl_star(S, x * 256, y * 256, (1 + (al >> 7)) * 256, al, PL_WT);
            else
                pl_glint(S, x * 256, y * 256, al, PL_WT);
            pl_line(S, (x - 2) * 256, (2 * hy - y + 4) * 256, (x + 2) * 256, (2 * hy - y + 4) * 256, al / 3, PL_INK);
        }
    /* (NEW, API 6) HERON: standing in the shallows (WADE: where across), on the beat it stabs at the water, STRIKE: how hard;
     * its reflection under it */
    if (cx->xk[0][0])
        plw_heron(S, a, cx, (34 + cx->xk[0][1] * 172 / 99) * 256, (hy + 30) * 256, (cx->beat * cx->beat >> 8) * cx->xk[0][2] / 99);
}

#include "splash/marsh.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_marsh = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plw_t), "MARSH",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"WIND", 50}, {"FADE", 50}, {"SIZE", 50}, {"WATER", 50}, {"GUST", 50}, {"FLIES", 50}, {"REEDS", 50}, {"MOON", 50}},
    plw_init, plw_note, plw_frame, plw_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Reeds, water and fireflies by a moon", 3u, {0, 0, 0},
    {{"CALM", {16, 0, 0, 31, 0, 36, 0, 91}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 0u), 0}, {99, 99, 20, 0}},
     {"GALE", {91, 0, 0, 0, 96, 0, 81, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 0u), UMB_LINK(UMB_SRC_LEVEL, 3u)}, {99, 99, 40, 50}},
     {"FIREFLY", {0, 81, 0, 0, 0, 100, 31, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 2u), UMB_LINK(UMB_SRC_LEVEL, 7u)}, {99, 99, 25, 30}}},
    18u, {0, 0, 0}, SPL_MARSH,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"HERON", 3u, 1u, {0, 0}, {{"ON", 0}, {"WADE", 30}, {"STRIKE", 60}},
      "A heron stands in the shallows over its reflection, stabbing at the water on the beat",
      {"Heron on or off", "Where it stands, left to right", "How far it stabs on the beat (0 it stands still)"}},
     {"STARS", 3u, 1u, {0, 0}, {{"ON", 0}, {"COUNT", 40}, {"GLEAM", 50}},
      "Stars over the marsh come and go, each fading in and out on its own, brighter on the beat, their light broken on the water",
      {"Stars on or off", "How many stars (4 to 40)", "How much the music brightens them"}}}};
