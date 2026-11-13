/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* AURORA, a plate module (umb_plate.h): the northern lights. Three curtains hang across the sky, each a folded line (two
 * waves through each other) with rays standing up from it, brightest at the foot and going out with the height; along
 * each runs its light, a line of 64 points that drifts sideways, one curtain against the next, through fine rays that
 * stand where they are (the noise table). Its own palette, by the light alone: the foot pale green, the rays green,
 * their tops violet. A third of one curtain is worked out afresh at a time, 40 times a second, each in its turn (the
 * columns a note has just lit at once), and the folds stand where the plate's start put them (a fold that crept redrew every ray it
 * moved a pixel: most of the screen, for a motion too slow to see; WAVE moves them, by hand or by a link): at rest the
 * curtains stand, and a note changes only where its light is. A note lights its track's
 * curtain where its pitch falls (C2 left .. C7 right) and the light drifts off with it; the kick flares the foot of one
 * curtain after another; the rest of the kit are stars that glint above.
 * Elements: DRIFT (how fast the light runs along), FADE (how long the notes' light lasts), HEIGHT (the rays'), RAY (the
 * notes that are no drum), FLARE (the kick), GLINT (the rest of the kit), WAVE (how deep the curtains fold), REST (their
 * light with no note). */
#include "plate_lib.h"

#define FAU_NC 3                                        /* curtains */
#define FAU_NI 64                                       /* a curtain's points of light */
#define FAU_NG 8                                        /* stars glinting at once */
static const uint8_t FAU_ST[4][3] = {{62, 20, 120}, {16, 170, 110}, {110, 255, 150}, {236, 255, 236}};   /* its palette */
typedef struct {
    uint16_t lut[2][256];                               /* the palette as the panel takes it */
    uint8_t I[FAU_NC][FAU_NI];                          /* the notes' light along each curtain */
    uint8_t yb[FAU_NC][240], am[FAU_NC][240];           /* this frame's, a column: each curtain's foot (its row), its light */
    uint32_t off[FAU_NC], tl[FAU_NC];                   /* how far each has drifted (Q16 of a point); when it was last
                                                         * worked out */
    uint32_t gt[FAU_NG];                                /* the stars: since when .. */
    uint8_t gx[FAU_NG], gy[FAU_NG], gv[FAU_NG], ng;     /* .. where, how hard */
    uint16_t flare;                                     /* the kick's flare (Q8) .. */
    uint8_t fk, turn, all;                              /* .. on this curtain; the third of a curtain whose turn it was
                                                         * (0 .. 8); 1: all are to be worked out (the plate has just started) */
    uint8_t d0[FAU_NC], d1[FAU_NC];                     /* each curtain's columns a note has just lit (d0 .. d1 - 1; none:
                                                         * d1 0): worked out at once, the rest in the curtain's turn */
    uint32_t rs, ms, fp;                                /* the dice; the steps' clock; where the folds stand (its own each
                                                         * time the plate starts) */
} fau_t;

static void fau_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fau_t *s = st;
    (void)a;
    s->rs = 1913u + cx->now;
    s->ms = cx->now;
    s->fp = pl_rnd(&s->rs);
    s->all = 1u;                                        /* (each worked out once before it is drawn) */
    pl_lut(s->lut, FAU_ST, 4);
}
static void fau_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    fau_t *s = st;
    int32_t v, j;
    uint32_t i, k = nt->track % FAU_NC, x;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* FLARE (the kick): the next curtain's foot */
        s->flare = (uint16_t)v;
        s->fk = (uint8_t)((s->fk + 1u) % FAU_NC);
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* GLINT: a star above */
        i = s->ng++ % FAU_NG;
        s->gx[i] = (uint8_t)(pl_rnd(&s->rs) % 240u);
        s->gy[i] = (uint8_t)(pl_rnd(&s->rs) % 110u);
        s->gv[i] = (uint8_t)v;
        s->gt[i] = cx->now | 1u;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* RAY: its track's curtain lit there, five points wide */
        return;
    i = (uint32_t)(pl_fold(nt->mo[0], pl_pk(nt->note), 256) * (FAU_NI - 1) / 256);
    x = i * 240u / FAU_NI;                              /* (its columns: three points either way, and the drift's part of one) */
    if (!s->d1[k] || x < s->d0[k] + 16u)
        s->d0[k] = (uint8_t)(x > 16u ? x - 16u : 0u);
    if (x + 16u > s->d1[k])
        s->d1[k] = (uint8_t)(x + 16u > 240u ? 240u : x + 16u);
    i += s->off[k] >> 16;
    for (j = -2; j <= 2; j++) {
        uint8_t *p = &s->I[k][(i + (uint32_t)j) % FAU_NI];
        int32_t e = *p + v * (3 - pl_abs(j)) / 3;
        *p = (uint8_t)(e > 255 ? 255 : e);
    }
}
/* curtain k's columns x0 .. x1 - 1: its foot (the fold: WAVE) and its light (at rest, and the notes' through the fine
 * rays) */
static void fau_cols(fau_t *s, const umb_lctx_t *cx, const umb_api_t *a, int32_t k, int32_t x0, int32_t x1)
{
    int32_t wa = (int32_t)cx->k[6] * 16 / 50, y0 = (cx->bare ? 84 : 104) + k * 50, x;
    uint32_t ph = (uint32_t)k * 1431655765u + s->fp;
    for (x = x0; x < x1; x++) {
        uint32_t p = (uint32_t)x * (FAU_NI * 65536u / 240u) + s->off[k];
        int32_t i0 = (int32_t)((p >> 16) % FAU_NI), i1 = (i0 + 1) % FAU_NI, fr = (int32_t)(p >> 8 & 255u),
                e = (int32_t)cx->k[7] * 9 / 5 + s->I[k][i0] + (((s->I[k][i1] - s->I[k][i0]) * fr) >> 8);
        s->yb[k][x] = (uint8_t)pl_clamp(y0 + ((wa * pl_sine(a, (uint32_t)x * 17895697u + ph)) >> 15) + ((wa * pl_sine(a, (uint32_t)x * 46385646u + ph * 3u)) >> 16), 6, 236);
        s->am[k][x] = (uint8_t)pl_clamp((e * (90 + ((166 * (pl_tbil(a, x * 150, k * 2560) >> 8)) >> 8))) >> 8, 0, 255);
    }
}
/* the frame: the flare falls; the columns a note has just lit are worked out at once; every 25 ms a third of one curtain
 * in its turn (the rest stays as it is drawn: each curtain whole every 225 ms, never more than a third at a time), and as
 * its turn comes round the curtain's notes' light fades (FADE: 0.3 .. 8 s) and drifts (DRIFT) */
static void fau_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fau_t *s = st;
    int32_t k1 = (int32_t)cx->k[1], k, i;
    uint32_t n;
    s->flare = pl_dec(s->flare, (int32_t)cx->dt * 187);  /* (0.35 s) */
    for (k = 0; k < FAU_NC; k++)
        if (s->d1[k]) {
            fau_cols(s, cx, a, k, s->d0[k], s->d1[k]);
            s->d1[k] = 0;
        }
    n = pl_owed(cx->now, &s->ms, 25u);
    if (s->all)
        n = 3u * FAU_NC;
    s->all = 0;
    while (n--) {
        s->turn = (uint8_t)((s->turn + 1u) % (3u * FAU_NC));
        k = s->turn / 3u;
        if (!(s->turn % 3u)) {
            int32_t dt = (int32_t)pl_clamp((int32_t)(cx->now - s->tl[k]), 0, 500), f = pl_clamp(dt * 65536 / (300 + k1 * k1 * 4 / 5), 0, 65535);
            s->tl[k] = cx->now;
            for (i = 0; i < FAU_NI; i++)
                s->I[k][i] = (uint8_t)pl_dec(s->I[k][i], f);
            s->off[k] += (uint32_t)((k & 1 ? -1 : 1) * (dt * (int32_t)cx->k[0] * (5 + 2 * k)));
        }
        fau_cols(s, cx, a, k, (int32_t)(s->turn % 3u) * 80, (int32_t)(s->turn % 3u) * 80 + 80);
    }
}
static void fau_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const fau_t *s = st;
    int32_t H = 12 + (int32_t)cx->k[2] * 60 / 99, hs = 32 * 256 / H, reach = H * 4, wa = (int32_t)cx->k[6] * 24 / 50 + 2, cxx, cy, k, i;
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = S->y0 + cy, vg = S->vg[cy], on = 0;
        uint16_t *row = S->px + cy * S->w;
        for (k = 0; k < FAU_NC; k++)                    /* the curtains that reach this row */
            on |= (py > (cx->bare ? 84 : 104) + k * 50 - wa - reach && py < (cx->bare ? 84 : 104) + k * 50 + wa + 4) << k;   /* (its folds: 1.5 WAVE either way) */
        for (cxx = 0; cxx < S->w; cxx++) {
            int32_t px = S->x0 + cxx, b = 0;
            for (k = 0; on >> k; k++) {
                int32_t dy = s->yb[k][px] - py;         /* (above its foot: the ray, going out; under it: its edge, 3 px) */
                if (on >> k & 1 && dy > -4)
                    b += dy >= 0 ? ((s->am[k][px] * pl_exp(a, (dy * hs) >> 8)) >> 8) + (k == s->fk && dy < 16 ? (s->flare * (16 - dy)) >> 5 : 0)
                                 : (s->am[k][px] * (4 + dy)) >> 3;
            }
            b = b > 261 ? 255 : b - 6;                  /* (its faintest reach cut: the sky above stays black, and still) */
            row[cxx] = b > 0 ? s->lut[(px ^ py) & 1][PL_IX(b, vg)] : 0;
        }
    }
    for (i = 0; i < FAU_NG; i++) {                      /* the stars glinting: 0.5 s */
        int32_t f = s->gt[i] ? pl_fade(cx->now - s->gt[i], 500u, 8u) : 0;
        if (f)
            pl_glint(S, s->gx[i] * 256, s->gy[i] * 256, (f * (110 + (s->gv[i] >> 1))) >> 8, PL_WT);
    }
}

#include "splash/aurora.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_aurora = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(fau_t), "AURORA",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 8u, {0, 0, 0},
    {{"DRIFT", 50}, {"FADE", 50}, {"HEIGHT", 30}, {"RAY", 50}, {"FLARE", 50}, {"GLINT", 50}, {"WAVE", 50}, {"REST", 30}},
    fau_init, fau_note, fau_frame, fau_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Curtains of northern light, drifting", 3u, {0, 0, 0},
    {{"POLAR", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 7u)}, {99, 99, 50, 40}},
     {"HUSH", {16, 91, 81, 31, 0, 26, 26, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 6u), 0}, {99, 99, 30, 0}},
     {"STORM", {91, 26, 96, 91, 96, 0, 91, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 6u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 40, 30}}},
    35u, {0, 0, 0}, SPL_AURORA};
