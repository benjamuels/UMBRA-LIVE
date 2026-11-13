/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* FATHOM, a plate module (umb_plate.h): the sea seen from the side, at night. Above, black; a bright line where the
 * water meets it, never still: a slow swell of two waves through each other, and on it the ripples of a string of 120
 * points (the wave equation along one line, 20 steps a second, its ends walls; the picture moves only on a step). Below, light let down in slanting shafts
 * that go out with the depth: brighter under a crest, darker under a trough, so they sway as the waves pass. A note is a
 * stone let fall where its pitch lies (C2 left .. C7 right): a dent, its ripples both ways, a little spray; the kick heaves
 * a wave in from one side, then the other; the rest of the kit throws spray off the top.
 * Elements: SWELL (the swell's height), CALM (how soon the ripples die), LEVEL (the sea's: low .. high), DROP (the notes
 * that are no drum), HEAVE (the kick), SPRAY (the rest of the kit), SHAFT (how much the waves gather the light). */
#include "plate_lib.h"

#define FFA_N 120                                       /* the string's points: one to two pixels */
#define FFA_NS 10                                       /* drops of spray at once */
#define FFA_NX 320                                      /* the columns the light is worked out for */
typedef struct {
    int16_t h[FFA_N], v[FFA_N];                         /* the ripples: how high (px, Q5), how fast up (a step) */
    uint8_t ys[240], sh[FFA_NX];                        /* this frame's, a column: the surface's row; the light let down
                                                         * under it (and past the edge: the shafts slant) */
    int16_t sx[FFA_NS], sy[FFA_NS], svx[FFA_NS], svy[FFA_NS];   /* the spray: where (px, Q4), how fast (a step) .. */
    uint8_t sl[FFA_NS];                                 /* .. the steps left to it (0: none) */
    uint8_t ns, side, lv;                               /* the next drop of spray; the kick's side; the sea's level (row) */
    uint32_t rs, ms;                                    /* the dice, the steps' clock */
} ffa_t;

static void ffa_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ffa_t *s = st;
    (void)a;
    s->rs = 733u + cx->now;
    s->ms = cx->now;
}
static void ffa_spray(ffa_t *s, int32_t x, int32_t v)  /* a drop of spray off the surface at column x */
{
    uint32_t i = s->ns++ % FFA_NS;
    s->sx[i] = (int16_t)(x * 16);
    s->sy[i] = (int16_t)(s->ys[pl_clamp(x, 0, 239)] * 16);
    s->svx[i] = (int16_t)((int32_t)(pl_rnd(&s->rs) % 41u) - 20);
    s->svy[i] = (int16_t)(-(30 + (v >> 2) + (int32_t)(pl_rnd(&s->rs) % 30u)));
    s->sl[i] = 32;
}
static void ffa_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    ffa_t *s = st;
    int32_t v, i, j;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* HEAVE (the kick): a wave from one side */
        s->side ^= 1u;
        for (j = 0; j < 24; j++)                        /* (up by the wall, down beside it: no water added) */
            s->v[s->side ? FFA_N - 1 - j : j] += (int16_t)((v * (12 - j)) >> 5);
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0)   /* SPRAY */
        for (j = nt->lane < 3u ? 3 : 1; j > 0; j--)
            ffa_spray(s, (int32_t)(8u + pl_rnd(&s->rs) % 224u), v);
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* DROP */
        return;
    i = pl_fold(nt->mo[0], pl_pk(nt->note), 256) * (FFA_N - 19) / 256 + 9;
    for (j = -8; j <= 8; j++)                           /* (down in the middle, up at its shoulders: none taken) */
        s->v[i + j] -= (int16_t)((v * (4 - pl_abs(j))) >> 5);
    ffa_spray(s, 2 * i - 3, v);
    ffa_spray(s, 2 * i + 3, v);
}
static void ffa_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ffa_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 50u);
    int16_t sf[FFA_NX + 16];
    int32_t damp = 2 + (int32_t)cx->k[1] * 14 / 99, A = (int32_t)cx->k[0] * 224 / 50, i, x, q;
    if (!n && s->lv)
        return;
    while (n--) {                                       /* a step: the string twice over (its ends walls), dragged (its
                                                         * sharp bends most: it runs smooth), drawn back to rest */
        for (q = 0; q < 2; q++) {
            for (i = 0; i < FFA_N; i++) {
                int32_t l = i ? i - 1 : 1, r = i < FFA_N - 1 ? i + 1 : FFA_N - 2, c = s->h[l] + s->h[r] - 2 * s->h[i];
                s->v[i] = (int16_t)pl_clamp(s->v[i] + ((c * 150) >> 8) + (s->v[l] + s->v[r] - 2 * s->v[i]) / 6 - s->v[i] * damp / 256 - s->h[i] / 512, -3000, 3000);
            }
            for (i = 0; i < FFA_N; i++) {
                s->h[i] = (int16_t)pl_clamp(s->h[i] + s->v[i], -2400, 2400);
                if (pl_abs(s->h[i]) < 24 && pl_abs(s->v[i]) < 3)
                    s->h[i] = s->v[i] = 0;              /* (it stills) */
            }
        }
        for (i = 0; i < FFA_NS; i++)                    /* the spray: thrown, falling */
            if (s->sl[i]) {
                s->sx[i] = (int16_t)(s->sx[i] + s->svx[i]);
                s->sy[i] = (int16_t)(s->sy[i] + s->svy[i]);
                s->svy[i] = (int16_t)(s->svy[i] + 8);
                s->sl[i]--;
            }
    }
    s->lv = (uint8_t)(190 - (int32_t)cx->k[2] * 140 / 99);
    for (x = 0; x < FFA_NX + 16; x++) {                 /* the surface a column: the level, the ripples, the swell */
        int32_t j = pl_clamp(x - 8, 0, 2 * FFA_N - 2), hh = (s->h[j >> 1] + s->h[(j + 1) >> 1]) >> 1;
        sf[x] = (int16_t)(s->lv * 32 - hh - ((A * pl_sine(a, (uint32_t)x * 21474836u - cx->now * 537000u)) >> 15) -
                          ((A * pl_sine(a, (uint32_t)x * 51745389u + cx->now * 866000u)) >> 16));
    }
    for (x = 0; x < FFA_NX; x++) {                      /* .. its row; the light under it by how it bends (a crest gathers) */
        if (x < 240)
            s->ys[x] = (uint8_t)pl_clamp(sf[x + 8] >> 5, 4, 236);
        s->sh[x] = (uint8_t)pl_clamp(64 + (((sf[x] + sf[x + 16] - 2 * sf[x + 8]) * (int32_t)cx->k[6]) >> 5), 14, 255);
    }
}
static void ffa_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const ffa_t *s = st;
    int32_t cxx, cy, i;
    (void)cx;
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = S->y0 + cy, vg = S->vg[cy], sl = ((py - s->lv) * 24) >> 6;
        const uint16_t *L = a->pal[(py & 1) << 1];
        uint16_t *row = S->px + cy * S->w;
        for (cxx = 0; cxx < S->w; cxx++) {
            int32_t px = S->x0 + cxx, d = py - s->ys[px], b;
            if (d < -1) {
                row[cxx] = 0;
                continue;
            }
            b = d < 0 ? 60 : !d ? 255 : d < 2 ? 190   /* the line; under it its glow, and the shafts, going out with the depth */
                : (pl_clamp(((s->sh[pl_clamp(px + sl, 0, FFA_NX - 1)] * pl_exp(a, (d * 205) >> 8)) >> 8) - 6, 0, 255) & ~3) + ((140 * pl_exp(a, d * 10)) >> 8);
            row[cxx] = L[(px & 1) * 256 + PL_IX(b > 255 ? 255 : b, vg)];
        }
    }
    for (i = 0; i < FFA_NS; i++)
        if (s->sl[i])
            pl_splat(S, s->sx[i] * 16, s->sy[i] * 16, 60 + s->sl[i] * 5, PL_WT);
}

#include "splash/fathom.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_fathom = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(ffa_t), "FATHOM",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 7u, {0, 0, 0},
    {{"SWELL", 50}, {"CALM", 40}, {"LEVEL", 50}, {"DROP", 50}, {"HEAVE", 50}, {"SPRAY", 50}, {"SHAFT", 50}},
    ffa_init, ffa_note, ffa_frame, ffa_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "The sea from the side, light let down", 3u, {0, 0, 0},
    {{"TIDE", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 50, 20}},
     {"GLASS", {6, 91, 0, 31, 0, 21, 91, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 15, 0}},
     {"GALE", {96, 11, 0, 81, 96, 96, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 0u), UMB_LINK(UMB_SRC_LEVEL, 6u)}, {99, 99, 30, 40}}},
    26u, {0, 0, 0}, SPL_FATHOM};
