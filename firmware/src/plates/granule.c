/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* GRANULE as a plate module (umb_plate.h; moved out of ui_umbra_field.c unchanged, pixel for pixel): the sun's surface.
 * 12 x 12 granules (a granule and a profile byte each 4 x 4 px block: lanes dark, hearts bright), each glowing faintly; a
 * note lights its own (its place on the circle of fifths and its octave across, its track's band down) in its track's
 * colour; the kick a cluster, the snare three, the hats bright points in the lanes. Elements: BOIL (how often a granule
 * dies and is born nearby, its neighbourhood remapped), FADE, GLOW (the glow at rest); GRAIN (the notes that are no drum),
 * BURST (the kick's cluster, the snare's three), POINT (the rest of the kit): each as hard as its value, its tracks LIVE
 * VIZ CTRLS' to choose.
 * (NEW, API 6) CRACKS (ON, MANY, SPEED): fissures of light tracing the lanes between the granules: each starts on a lane,
 * walks along it (the line between two hearts' halves), turning where three meet, then holds and goes out; brighter on
 * the beat and with the mix.
 * (NEW, API 6) ECHO (ON, DELAY, FLASH): a granule lit and fading away flashes once more after DELAY, its own shape cut
 * hard: its heart bright and crisp, a black ring round it, short. */
#include "plate_lib.h"

#define PLG_G 12u                                       /* 12 x 12 granules .. */
#define PLG_N (PLG_G * PLG_G)
#define PLG_C 20                                        /* .. 20 px apart */
#define PLG_NB 12u                                      /* .. bright points at most */
#define PLG_NK 8                                        /* (NEW, API 6) CRACKS at most .. */
#define PLG_NP 48                                       /* .. their points (a step 2 px) */
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
    /* (NEW, API 6) ECHO: each granule's: lit enough to echo (its light then, /2), its echo's strength, this frame's light,
     * the ms to its echo's end */
    uint8_t ek[PLG_N], ef[PLG_N], ev[PLG_N];
    uint16_t ec[PLG_N];
    /* (NEW, API 6) CRACKS: each one's tip (px, Q4), its heading (2 px a step, Q4), its points (px) and how many of how many,
     * its clock (ms), when it was done (0: growing) */
    int16_t kx[PLG_NK], ky[PLG_NK], kdx[PLG_NK], kdy[PLG_NK];
    uint8_t kp[PLG_NK][PLG_NP][2], kn[PLG_NK], km[PLG_NK];
    uint16_t kt[PLG_NK];
    uint32_t kd[PLG_NK];
} plg_t;

PL_SMALL static void plg_seed(plg_t *s, uint32_t g, uint32_t now)   /* a granule (re)born: its heart in its cell, +- 7 px */
{
    s->sx[g] = (uint8_t)((g % PLG_G) * PLG_C + 3u + pl_rnd(&s->rs) % 15u);
    s->sy[g] = (uint8_t)((g / PLG_G) * PLG_C + 3u + pl_rnd(&s->rs) % 15u);
    s->amb[g] = (uint8_t)(23u + pl_rnd(&s->rs) % 28u);
    s->born[g] = now;
}
/* the blocks bx0 .. bx1, by0 .. by1: each one's granule (the nearest heart) and profile: dark in the lanes, brightest at
 * the heart */
PL_SMALL static void plg_map(plg_t *s, const umb_api_t *a, int32_t bx0, int32_t by0, int32_t bx1, int32_t by1)
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
PL_SMALL static void plg_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
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
PL_SMALL static void plg_light(plg_t *s, uint32_t g, int32_t e, const uint8_t *c)   /* granule g lit e (Q8) in colour c */
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
PL_SMALL static uint32_t plg_at(const umb_note_t *n, uint32_t row, uint32_t col)
{
    return (uint32_t)pl_fold(n->mo[1], (int32_t)row, PLG_G - 1) * PLG_G + (uint32_t)pl_fold(n->mo[0], (int32_t)col, PLG_G - 1);
}
PL_SMALL static void plg_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plg_t *s = st;
    static const int8_t PLG_K[5] = {0, -1, 1, -(int8_t)PLG_G, (int8_t)PLG_G};
    uint32_t g, k, c = nt->track, n = nt->note, lane = nt->lane, o = n / 12u,
             nt6 = cx->tracks < 1u ? 1u : cx->tracks > UMB_NTRK ? UMB_NTRK : cx->tracks;
    int32_t v;
    const uint8_t *col = a->colour[nt->drum ? PL_LANE_POS[lane] : (c * 3u) % 8u];
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && lane < 3u)) != 0) {   /* BURST */
        if (!lane) {                                    /* the kick (and what is no drum): a cluster */
            uint32_t row = 3u + pl_rnd(&s->rs) % 7u;
            g = plg_at(nt, row, 2u + pl_rnd(&s->rs) % 8u);
            for (k = 0; k < 5; k++)                     /* (its heart, then the four round it) */
                plg_light(s, g + (uint32_t)PLG_K[k], k ? (90 * v) >> 8 : 102 + ((205 * v) >> 8), col);
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
/* (NEW, API 6) CRACKS: crack i a step on along its lane: onto the line between the two nearest hearts, then 2 px along it
 * (onward, the way it was heading); 0: off the screen */
PL_SMALL static int plg_step(plg_t *s, int32_t i)
{
    int32_t X = s->kx[i], Y = s->ky[i], gx = pl_clamp((X >> 4) / PLG_C, 0, PLG_G - 1), gy = pl_clamp((Y >> 4) / PLG_C, 0, PLG_G - 1),
            f1 = 1 << 30, f2 = 1 << 30, dx, dy, nx, ny, nn, d, L, tx, ty;
    uint32_t g1 = 0, g2 = 0;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++) {
            int32_t gX = gx + dx, gY = gy + dy, ex, ey;
            uint32_t g;
            if (gX < 0 || gY < 0 || gX >= (int32_t)PLG_G || gY >= (int32_t)PLG_G)
                continue;
            g = (uint32_t)(gY * (int32_t)PLG_G + gX);
            ex = X - s->sx[g] * 16;
            ey = Y - s->sy[g] * 16;
            d = (ex * ex + ey * ey) >> 4;
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
    nx = s->sx[g1] - s->sx[g2];
    ny = s->sy[g1] - s->sy[g2];
    nn = nx * nx + ny * ny;
    nn = nn ? nn : 1;
    d = (X - (s->sx[g1] + s->sx[g2]) * 8) * nx + (Y - (s->sy[g1] + s->sy[g2]) * 8) * ny;
    X -= nx * d / nn;
    Y -= ny * d / nn;
    L = (int32_t)pl_isqrt((uint32_t)nn);
    tx = -ny * 32 / (L ? L : 1);
    ty = nx * 32 / (L ? L : 1);
    if (tx * s->kdx[i] + ty * s->kdy[i] < 0) {
        tx = -tx;
        ty = -ty;
    }
    X += tx;
    Y += ty;
    if (X < 16 || Y < 16 || X > 238 * 16 || Y > 238 * 16)
        return 0;
    s->kx[i] = (int16_t)X;
    s->ky[i] = (int16_t)Y;
    s->kdx[i] = (int16_t)tx;
    s->kdy[i] = (int16_t)ty;
    s->kp[i][s->kn[i]][0] = (uint8_t)(X >> 4);
    s->kp[i][s->kn[i]++][1] = (uint8_t)(Y >> 4);
    return 1;
}
/* (NEW, API 6) CRACKS: MANY of them (1 .. 8), each growing a step every 25 .. 320 ms (SPEED), 20 .. 47 steps, then held
 * 0.4 s, gone out in 1.2 s and started afresh on another lane */
PL_SMALL static void plg_crack(plg_t *s, const umb_lctx_t *cx)
{
    int32_t i, T = 25 + (99 - cx->xk[0][2]) * 3;
    for (i = 0; i < PLG_NK; i++) {
        if (!cx->xk[0][0] || i > cx->xk[0][1] * 7 / 99 || (s->kd[i] && cx->now - s->kd[i] > 1600u)) {
            s->kn[i] = 0;
            s->kd[i] = 0;
            continue;
        }
        if (s->kd[i])
            continue;
        if (!s->kn[i]) {                                /* (a start: between a heart and the one beside it) */
            uint32_t g = pl_rnd(&s->rs) % PLG_N, b = g % PLG_G < PLG_G - 1u ? g + 1u : g - 1u, q = pl_rnd(&s->rs);
            s->kx[i] = (int16_t)((s->sx[g] + s->sx[b]) * 8);
            s->ky[i] = (int16_t)((s->sy[g] + s->sy[b]) * 8);
            s->kdx[i] = (int16_t)(q & 1u ? 32 : -32);
            s->kdy[i] = (int16_t)(q & 2u ? 32 : -32);
            s->km[i] = (uint8_t)(20u + (q >> 8) % 28u);
            s->kt[i] = 0;
            s->kp[i][0][0] = (uint8_t)(s->kx[i] >> 4);
            s->kp[i][0][1] = (uint8_t)(s->ky[i] >> 4);
            s->kn[i] = 1;
        }
        s->kt[i] = (uint16_t)(s->kt[i] + cx->dt);
        while (!s->kd[i] && s->kt[i] >= T) {
            s->kt[i] = (uint16_t)(s->kt[i] - T);
            if (s->kn[i] >= s->km[i] || !plg_step(s, i))
                s->kd[i] = cx->now | 1u;
        }
    }
}
/* once a frame: the light fades (FADE: 0.3 .. 4.2 s), granules reborn (BOIL: 0.3 .. 6 a second), this frame's levels */
PL_SMALL static void plg_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
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
        /* (NEW, API 6) ECHO: lit (past a third), then gone dim: its echo DELAY on (0 .. 0.9 s), 0.3 s long in 4 hard steps */
        if (!cx->xk[1][0])
            s->ek[g] = s->ev[g] = 0, s->ec[g] = 0;
        else {
            if (s->E[g] > 136u && s->E[g] / 2u > s->ek[g])
                s->ek[g] = (uint8_t)(s->E[g] / 2u);
            else if (s->ek[g] && s->E[g] < 40u) {
                s->ec[g] = (uint16_t)(300u + cx->xk[1][1] * 9u);
                s->ef[g] = s->ek[g];
                s->ek[g] = 0;
            }
            s->ec[g] = s->ec[g] > cx->dt ? (uint16_t)(s->ec[g] - cx->dt) : 0;
            s->ev[g] = (uint8_t)pl_clamp(s->ec[g] && s->ec[g] <= 300u ? (int32_t)(s->ef[g] * ((s->ec[g] + 74u) / 75u) * (30u + cx->xk[1][2]) / 256u) : 0, 0, 255);
        }
    }
    plg_crack(s, cx);
}
/* (NEW, API 6) CRACKS: each a line through its points, the newest brightest, its tip a glint while it grows; brighter
 * on the beat and with the mix; held, then going out */
PL_SMALL __attribute__((noinline)) static void plg_xdraw(const plg_t *s, const umb_lctx_t *cx, const umb_strip_t *S)
{
    int32_t i, k;
    for (i = 0; i < PLG_NK; i++) {
        int32_t n = s->kn[i], al = 200 + (cx->beat >> 1) + (cx->level >> 1), age = s->kd[i] ? (int32_t)(cx->now - s->kd[i]) : 0;
        if (n < 2)
            continue;
        if (age > 400)
            al = al * (1600 - age) / 1200;
        for (k = 1; k < n; k++)
            pl_line(S, s->kp[i][k - 1][0] * 256 + 128, s->kp[i][k - 1][1] * 256 + 128, s->kp[i][k][0] * 256 + 128, s->kp[i][k][1] * 256 + 128,
                    al * (96 + 160 * k / n) >> 8, k & 4 ? PL_INK : PL_WT);
        if (!s->kd[i])
            pl_dot(S, s->kp[i][n - 1][0], s->kp[i][n - 1][1], 255, PL_WT);
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
            if (s->ev[g] && p > 40) {                   /* (NEW, API 6) ECHO: its shape cut hard, a black ring round it */
                e = p > 110 ? s->ev[g] : 0;
                row[cxx] = pl_rgb(a, 0, (s->col[g][0] + PL_WT[0]) * e >> 8, (s->col[g][1] + PL_WT[1]) * e >> 8, (s->col[g][2] + PL_WT[2]) * e >> 8, vg, 0);
                qg = PLG_N;
                continue;
            }
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
            pl_dot(S, s->bx[i], s->by[i], (f * (128 + (s->bv[i] >> 1)) * 384) >> 16,
                     s->bc[i] == 4u ? PL_WT : a->colour[PL_LANE_POS[s->bc[i] % PL_NLANE]]);
    }
    plg_xdraw(s, cx, S);                                /* (NEW, API 6) CRACKS */
}

#include "splash/granule.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_granule = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plg_t), "GRANULE",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 6u, {0, 0, 0},
    {{"BOIL", 35}, {"FADE", 45}, {"GLOW", 50}, {"GRAIN", 50}, {"BURST", 50}, {"POINT", 50}},
    plg_init, plg_note, plg_frame, plg_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES},
    "The sun's surface, a cell lit a note", 3u, {0, 0, 0},
    {{"SIMMER", {16, 81, 36, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 0u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 25, 40}},
     {"FLARES", {61, 0, 0, 31, 96, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 2u), UMB_LINK(UMB_SRC_BEAT, 0u)}, {99, 99, 50, 30}},
     {"PULSE", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 2u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 70, 40}}},
    72u, {0, 0, 0}, SPL_GRANULE,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"CRACKS", 3u, 1u, {0, 0}, {{"ON", 0}, {"MANY", 60}, {"SPEED", 60}},
      "Cracks of light trace the lanes between the cells, turning where they meet, brighter on the beat",
      {"Cracks on or off", "How many trace at once (1 to 8)", "How fast they trace"}},
     {"ECHO", 3u, 1u, {0, 0}, {{"ON", 0}, {"DELAY", 30}, {"FLASH", 60}},
      "As a lit cell fades away, a sharper, harder copy of it flashes once in its place",
      {"Echo on or off", "How long after the fade it flashes (0 to 0.9 s)", "How bright the echo flashes"}}}};
