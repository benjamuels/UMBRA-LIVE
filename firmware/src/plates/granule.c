/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* GRANULE as a plate module (umb_plate.h; moved out of ui_umbra_field.c unchanged, pixel for pixel): the sun's surface.
 * 12 x 12 granules (a granule and a profile byte each 4 x 4 px block: lanes dark, hearts bright), each glowing faintly; a
 * note lights its own (its place on the circle of fifths and its octave across, its track's band down) in its track's
 * colour; the kick a cluster, the snare three, the hats bright points in the lanes. Elements: BOIL (how often a granule
 * dies and is born nearby, its neighbourhood remapped), FADE, GLOW (the glow at rest); GRAIN (the notes that are no drum),
 * BURST (the kick's cluster, the snare's three), POINT (the rest of the kit): each as hard as its value, its tracks LIVE
 * VIZ CTRLS' to choose. */
#include "plate_lib.h"

#define PLG_G 12u                                       /* 12 x 12 granules .. */
#define PLG_N (PLG_G * PLG_G)
#define PLG_C 20                                        /* .. 20 px apart */
#define PLG_NB 12u                                      /* .. bright points at most */
typedef struct {
    uint8_t lab[PL_S], prof[PL_S];                      /* each 4 x 4 px block's granule, and its light's profile there */
    uint8_t sx[PLG_N], sy[PLG_N], amb[PLG_N], col[PLG_N][3];   /* a granule: its heart (px), its glow at rest, its colour */
    uint16_t E[PLG_N];                                  /* .. its light (Q8, up to 1.6) */
    uint32_t born[PLG_N];                               /* .. when it was born (it fades in) */
    uint8_t gb[PLG_N];                                  /* .. this frame's: the glow, */
    uint16_t gl[PLG_N];                                 /* .. the light (in 16 steps) */
    uint8_t bx[PLG_NB], by[PLG_NB], bv[PLG_NB], bc[PLG_NB];   /* the bright points (the hats): where, how hard, the lane */
    uint32_t bt[PLG_NB];                                /* .. when */
    uint8_t nb;
    uint32_t tb;                                        /* the boil's clock */
    uint32_t rs;                                        /* the dice */
} plg_t;

static void plg_seed(plg_t *s, uint32_t g, uint32_t now)   /* a granule (re)born: its heart in its cell, +- 7 px */
{
    s->sx[g] = (uint8_t)((g % PLG_G) * PLG_C + 3u + pl_rnd(&s->rs) % 15u);
    s->sy[g] = (uint8_t)((g / PLG_G) * PLG_C + 3u + pl_rnd(&s->rs) % 15u);
    s->amb[g] = (uint8_t)(23u + pl_rnd(&s->rs) % 28u);
    s->born[g] = now;
}
/* the blocks bx0 .. bx1, by0 .. by1: each one's granule (the nearest heart) and profile: dark in the lanes, brightest at
 * the heart */
static void plg_map(plg_t *s, const umb_api_t *a, int32_t bx0, int32_t by0, int32_t bx1, int32_t by1)
{
    int32_t bx, by, dx, dy;
    for (by = pl_clamp(by0, 0, PL_N - 1); by <= pl_clamp(by1, 0, PL_N - 1); by++)
        for (bx = pl_clamp(bx0, 0, PL_N - 1); bx <= pl_clamp(bx1, 0, PL_N - 1); bx++) {
            int32_t px = bx * 4 + 2, py = by * 4 + 2, gx = px / PLG_C, gy = py / PLG_C, f1 = 1 << 30, f2 = 1 << 30, db, t, k;
            uint32_t g1 = 0, g2 = 0, sep;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    int32_t X = gx + dx, Y = gy + dy, d, ex, ey;
                    uint32_t g;
                    if (X < 0 || Y < 0 || X >= (int32_t)PLG_G || Y >= (int32_t)PLG_G)
                        continue;
                    g = (uint32_t)(Y * (int32_t)PLG_G + X);
                    ex = px - s->sx[g];
                    ey = py - s->sy[g];
                    d = ex * ex + ey * ey;
                    if (d < f1) {
                        f2 = f1;
                        g2 = g1;
                        f1 = d;
                        g1 = g;
                    } else if (d < f2) {
                        f2 = d;
                        g2 = g;
                    }
                }
            dx = s->sx[g1] - s->sx[g2];
            dy = s->sy[g1] - s->sy[g2];
            sep = pl_isqrt((uint32_t)(dx * dx + dy * dy));
            db = ((f2 - f1) << 3) / (int32_t)(sep ? sep : 1u);   /* (px to the lane, Q4) */
            t = pl_clamp((db - 16) * 16 / 3, 0, 256);
            k = pl_clamp(256 - ((f1 * 376) >> 8), 0, 256);   /* (the heart's dome: 1 - (r / 13 px)^2) */
            k = (k * (179 + ((77 * (pl_tbil(a, px * 77 + 1408, py * 77 + 576) >> 8)) >> 8))) >> 8;   /* (its grain) */
            s->lab[by * PL_N + bx] = (uint8_t)g1;
            s->prof[by * PL_N + bx] = (uint8_t)pl_clamp((((t * t * (768 - 2 * t)) >> 16) * k) >> 8, 0, 255);
        }
}
static void plg_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plg_t *s = st;
    uint32_t g;
    s->rs = 218u + cx->now;
    for (g = 0; g < PLG_N; g++) {
        plg_seed(s, g, cx->now);
        s->born[g] = cx->now - 2000u;
        s->col[g][0] = 143;
        s->col[g][1] = 217;
        s->col[g][2] = 201;
    }
    plg_map(s, a, 0, 0, PL_N - 1, PL_N - 1);
}
static void plg_light(plg_t *s, uint32_t g, int32_t e, const uint8_t *c)   /* granule g lit e (Q8) in colour c */
{
    int32_t E, k, j;
    if (g >= PLG_N)
        return;
    E = s->E[g] + e;
    E = E > 410 ? 410 : E;
    s->E[g] = (uint16_t)E;
    k = E ? e * 256 / E : 256;
    for (j = 0; j < 3; j++)
        s->col[g][j] = (uint8_t)(s->col[g][j] + (((c[j] - s->col[g][j]) * k) >> 8));
}
/* the granule at row, col (0 .. 11) where note n's track has moved it (NOTE MOVEMENT) */
static uint32_t plg_at(const umb_note_t *n, uint32_t row, uint32_t col)
{
    return (uint32_t)pl_fold(n->mo[1], (int32_t)row, PLG_G - 1) * PLG_G + (uint32_t)pl_fold(n->mo[0], (int32_t)col, PLG_G - 1);
}
static void plg_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plg_t *s = st;
    uint32_t g, c = nt->track, n = nt->note, lane = nt->lane, o = n / 12u,
             nt6 = cx->tracks < 1u ? 1u : cx->tracks > UMB_NTRK ? UMB_NTRK : cx->tracks;
    int32_t v;
    const uint8_t *col = a->colour[nt->drum ? PL_LANE_POS[lane] : (c * 3u) % 8u];
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && lane < 3u)) != 0) {   /* BURST */
        if (!lane) {                                    /* the kick (and what is no drum): a cluster */
            uint32_t row = 3u + pl_rnd(&s->rs) % 7u;
            g = plg_at(nt, row, 2u + pl_rnd(&s->rs) % 8u);
            plg_light(s, g, 102 + ((205 * v) >> 8), col);
            plg_light(s, g - 1u, (90 * v) >> 8, col);
            plg_light(s, g + 1u, (90 * v) >> 8, col);
            plg_light(s, g - PLG_G, (90 * v) >> 8, col);
            plg_light(s, g + PLG_G, (90 * v) >> 8, col);
        } else                                          /* the snare, the clap: three */
            for (g = 0; g < 3u; g++)
                plg_light(s, pl_rnd(&s->rs) % PLG_N, 128 + ((102 * v) >> 8), col);
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && lane >= 3u)) != 0) {   /* POINT: a bright point in a lane (between two hearts) */
        uint32_t p = pl_rnd(&s->rs) % PLG_N, b = p % PLG_G < PLG_G - 1u ? p + 1u : p - 1u, i = s->nb++ % PLG_NB;
        s->bx[i] = (uint8_t)((s->sx[p] + s->sx[b]) >> 1);
        s->by[i] = (uint8_t)((s->sy[p] + s->sy[b]) >> 1);
        s->bv[i] = (uint8_t)v;
        s->bc[i] = (uint8_t)lane;
        s->bt[i] = cx->now;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* GRAIN */
        return;
    g = plg_at(nt, (c * PLG_G / nt6 + (o & 1u)) % PLG_G, (pl_fifth(cx, c, n) * 5u + o * 3u) % PLG_G);
    plg_light(s, g, 115 + ((154 * v) >> 8), col);
}
/* once a frame: the light fades (FADE: 0.3 .. 4.2 s), granules reborn (BOIL: 0.3 .. 6 a second), this frame's levels */
static void plg_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plg_t *s = st;
    int32_t k0 = (int32_t)cx->k[0], k1 = (int32_t)cx->k[1], life = 300 + k1 * k1 * 2 / 5, f = (int32_t)cx->dt * 65536 / life;
    uint32_t g, gw = cx->k[2];                          /* (GLOW: the glow at rest, 0 none .. 99 twice) */
    s->tb += cx->dt * (uint32_t)(k0 ? 77 + k0 * 1536 / 99 : 0);
    while (s->tb >= 256000u) {
        int32_t gx, gy, t;
        s->tb -= 256000u;
        g = pl_rnd(&s->rs) % PLG_N;
        for (t = 0; t < 6 && s->E[g] > 31u; t++)        /* (a quiet one) */
            g = pl_rnd(&s->rs) % PLG_N;
        plg_seed(s, g, cx->now);
        gx = (int32_t)(g % PLG_G) * PLG_C;
        gy = (int32_t)(g / PLG_G) * PLG_C;
        plg_map(s, a, (gx - 30) >> 2, (gy - 30) >> 2, (gx + 50) >> 2, (gy + 50) >> 2);
    }
    for (g = 0; g < PLG_N; g++) {
        uint32_t age = cx->now - s->born[g];
        s->E[g] = pl_dec(s->E[g], f);
        s->gb[g] = (uint8_t)(s->amb[g] * (age >= 1600u ? 8u : age / 200u) * gw / 400u);   /* (born: it fades in, 8 steps) */
        s->gl[g] = (uint16_t)(s->E[g] & ~15u);
    }
}
static void plg_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plg_t *s = st;
    uint8_t xi[240], xf[240];
    int32_t vv[PL_N + 1], i, x0 = S->x0, cxx, cy;
    pl_cols(S, xi, xf);
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = S->y0 + cy, vg = S->vg[cy], dq = (py & 1) << 1;
        const uint8_t *lab = s->lab + (py >> 2) * PL_N;
        const uint16_t *L = a->pal[dq];
        uint16_t *row = S->px + cy * S->w;
        uint32_t qg = PLG_N;                            /* (the pair's even pixel: lit, granule qg) */
        pl_vrow(s->prof, py, vv, 255);
        for (cxx = 0; cxx < S->w; cxx++) {
            int32_t px = x0 + cxx, p = PL_LERP(vv, xi[cxx], xf[cxx]), e, R, G, B;
            uint32_t g = lab[px >> 2];
            if (p < 3 || !s->gl[g]) {                   /* (dark, or a granule at rest: one read) */
                row[cxx] = L[(px & 1) * 256 + (p < 3 ? 0 : PL_IX((p * s->gb[g]) >> 8, vg))];
                qg = PLG_N;
                continue;
            }
            if ((px & 1) && g == qg) {                  /* (lit: worked out a pixel pair at a time) */
                row[cxx] = row[cxx - 1];
                continue;
            }
            qg = g;
            e = ((((p * p) >> 8) * s->gl[g]) * 410) >> 16;   /* (lit: a dome, brightest at the heart) */
            R = (s->col[g][0] * e) >> 8;
            G = (s->col[g][1] * e) >> 8;
            B = (s->col[g][2] * e) >> 8;
            if (e > 115) {
                R += (PL_WT[0] * (e - 115) * 230) >> 16;
                G += (PL_WT[1] * (e - 115) * 230) >> 16;
                B += (PL_WT[2] * (e - 115) * 230) >> 16;
            }
            row[cxx] = pl_rgb(a, (p * s->gb[g]) >> 8, R, G, B, vg, PL_D2[dq | (px & 1)]);
        }
    }
    for (i = 0; i < (int32_t)PLG_NB; i++) {             /* the bright points: 0.4 s, 8 steps */
        int32_t f = pl_fade(cx->now - s->bt[i], 400u, 8u);
        if (f && s->bv[i])
            pl_glint(S, s->bx[i] * 256, s->by[i] * 256, (f * (128 + (s->bv[i] >> 1)) * 384) >> 16,
                     s->bc[i] == 4u ? PL_WT : a->colour[PL_LANE_POS[s->bc[i] % PL_NLANE]]);
    }
}

UMB_PLATE_HEAD const umb_plate_t umb_plate_granule = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plg_t), "GRANULE",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 6u, {0, 0, 0},
    {{"BOIL", 35}, {"FADE", 45}, {"GLOW", 50}, {"GRAIN", 50}, {"BURST", 50}, {"POINT", 50}},
    plg_init, plg_note, plg_frame, plg_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES},
    "The sun's surface, a cell lit a note", 3u, {0, 0, 0},
    {{"SIMMER", {16, 81, 36, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 0u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 25, 40}},
     {"FLARES", {61, 0, 0, 31, 96, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 2u), UMB_LINK(UMB_SRC_BEAT, 0u)}, {99, 99, 50, 30}},
     {"PULSE", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 2u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 70, 40}}}};
