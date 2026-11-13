/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* WAX, a plate module (umb_plate.h): a lamp of molten wax. Blobs warm at the foot, rise, cool at the top and sink again;
 * where two meet they run together, and part again with a neck. Each blob is a round body of light in a field (60 x 60,
 * made afresh every step, 20 a second: only the blobs are kept); the field is seen through a curve that is dark outside
 * a level, brightest on it and glowing within: a rim and a body, whatever the blobs' shape. Its own palette: ember red
 * round the wax, orange inside, the rim pale. A note lets a hot blob go from the foot where its pitch falls (low notes
 * larger); the kick heaves them all up and swells them; the rest of the kit are beads that hurry up and go.
 * Elements: HEAT (how hard the lamp drives them), THICK (how the wax resists), SIZE (the blobs'), BLOB (the notes that
 * are no drum), HEAVE (the kick), BEAD (the rest of the kit), GLOW (the light round them and their rim). */
#include "plate_lib.h"

#define FWX_NR 6                                        /* blobs always there (the last of them a small one); then as many
                                                         * of the notes', and beads */
#define FWX_NB (3 * FWX_NR)
static const uint8_t FWX_ST[4][3] = {{104, 10, 0}, {218, 62, 0}, {255, 150, 22}, {255, 238, 176}};   /* its palette */
typedef struct {
    uint8_t f[PL_S];                                    /* the field, a byte a cell */
    uint16_t lut[2][256];                               /* the palette as the panel takes it */
    uint8_t cv[256];                                    /* the light of each level of the field .. */
    uint8_t kg;                                         /* .. as GLOW was when it was made (+ 1) */
    uint8_t nn, nb;                                     /* the next note's blob, the next bead */
    uint8_t rsv;
    int16_t x[FWX_NB], y[FWX_NB], vy[FWX_NB];           /* each blob: where (cells, Q8), how fast down (Q8 a step) .. */
    uint8_t T[FWX_NB], R[FWX_NB], hx[FWX_NB], life[FWX_NB];   /* .. how warm, its reach (cells, Q4; 0: none), where
                                                         * across it keeps to (cells), the steps left to it (0: for good) */
    uint16_t heave;                                     /* the kick's heave (Q8) */
    uint32_t rs, ms, t;                                 /* the dice, the steps' clock, the steps */
} fwx_t;

static void fwx_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fwx_t *s = st;
    uint32_t i;
    (void)a;
    s->rs = 3301u + cx->now;
    s->ms = cx->now;
    pl_lut(s->lut, FWX_ST, 4);
    for (i = 0; i < FWX_NR; i++) {
        s->hx[i] = (uint8_t)(7u + i * 9u + pl_rnd(&s->rs) % 5u);
        s->x[i] = (int16_t)(s->hx[i] * 256);
        s->y[i] = (int16_t)((10u + pl_rnd(&s->rs) % 40u) * 256u);
        s->T[i] = (uint8_t)pl_rnd(&s->rs);
        s->R[i] = (uint8_t)(i == FWX_NR - 1u ? 76u : 104u + pl_rnd(&s->rs) % 64u);
    }
}
static void fwx_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    fwx_t *s = st;
    int32_t v, k;
    uint32_t i;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* HEAVE (the kick) */
        if (v > s->heave)
            s->heave = (uint16_t)v;
        for (i = 0; i < FWX_NB; i++)
            s->vy[i] = (int16_t)(s->vy[i] - (v >> 2));
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* BEAD */
        i = 2u * FWX_NR + s->nb++ % FWX_NR;
        s->hx[i] = (uint8_t)(4u + pl_rnd(&s->rs) % 52u);
        s->R[i] = (uint8_t)(34 + (v >> 4));
        s->vy[i] = -130;
        s->life[i] = 64;
        goto go;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* BLOB */
        return;
    k = pl_pk(nt->note);
    i = FWX_NR + s->nn++ % FWX_NR;
    s->hx[i] = (uint8_t)((768 + pl_fold(nt->mo[0], k * 54, 13824)) >> 8);
    s->R[i] = (uint8_t)(70 + (((256 - k) * 56) >> 8) + (v >> 3));   /* (4.4 .. 9.9 cells) */
    s->vy[i] = (int16_t)(-60 - (v >> 2));
    s->life[i] = 176;
go:
    s->x[i] = (int16_t)(s->hx[i] * 256);
    s->y[i] = 57 * 256;
    s->T[i] = 255;
}
/* a step: each blob warms toward what its height gives (the foot hot, the top cold), is driven up or down by it against
 * the wax, sways about its own place; the field is made of them */
static void fwx_step(fwx_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int32_t heat = 1 + (int32_t)cx->k[0] * 15 / 99, g = 3 + (int32_t)cx->k[0] * 12 / 99, drag = 10 + (int32_t)cx->k[1] * 70 / 99,
            sz = (128 + (int32_t)cx->k[2] * 256 / 99) * (512 + s->heave) >> 9, i;
    s->t++;
    for (i = 0; i < PL_S; i++)
        s->f[i] = 0;
    for (i = 0; i < FWX_NB; i++) {
        int32_t y = s->y[i], vy = s->vy[i], T = s->T[i], tg = pl_clamp(((y >> 4) - 128) * 255 / 704, 0, 255), r;
        if (!s->R[i])
            continue;
        T += ((tg - T) * heat + (tg > T ? 255 : 0)) >> 8;
        vy += ((128 - T) * g) >> 7;
        vy -= (vy * drag) / 256;
        y += vy;
        if (y < 768 || y > 57 * 256) {
            y = y < 768 ? 768 : 57 * 256;
            vy = 0;
        }
        s->y[i] = (int16_t)y;
        s->vy[i] = (int16_t)vy;
        s->T[i] = (uint8_t)T;
        s->x[i] = (int16_t)(s->x[i] + ((s->hx[i] * 256 + (pl_sine(a, s->t * 11250000u + (uint32_t)i * 715827883u) >> 6) - s->x[i]) >> 4));
        r = (s->R[i] * 16 * sz) >> 8;
        if (s->life[i] && --s->life[i] < 16u) {         /* (its last steps: it draws in) */
            r = (r * s->life[i]) >> 4;
            if (!s->life[i])
                s->R[i] = 0;
        }
        if (r > 64)
            pl_stamp(s->f, s->x[i], y, r, -235);
    }
    s->heave = pl_dec(s->heave, 10923);                 /* (0.3 s) */
}
static void fwx_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fwx_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 50u);
    int32_t i, g = (int32_t)cx->k[6];
    if (s->kg != cx->k[6] + 1u) {                       /* the curve: the glow outside (in steps: a blob moving changes
                                                         * little of it), the rim at 96, the body within and its core, each
                                                         * one level: only their edges change as the wax moves */
        s->kg = (uint8_t)(cx->k[6] + 1u);
        for (i = 0; i < 256; i++) {
            int32_t h = ((pl_clamp(i, 0, 80) * pl_clamp(i, 0, 80) * g) >> 13) & ~7, pk = 160 + g;
            s->cv[i] = (uint8_t)pl_clamp(i < 80 ? h : i < 96 ? h + (pk - h) * (i - 80) / 16 : i < 112 ? pk - (pk - 150) * (i - 96) / 16
                                         : i < 196 ? 150 : i < 208 ? 150 + (i - 196) * 5 : 210, 0, 255);
        }
    }
    while (n--)
        fwx_step(s, cx, a);
}
static void fwx_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const fwx_t *s = st;
    (void)cx;
    (void)a;
    pl_fdraw(S, s->f, s->cv, s->lut, 1);
}

#include "splash/wax.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_wax = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(fwx_t), "WAX",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 7u, {0, 0, 0},
    {{"HEAT", 40}, {"THICK", 50}, {"SIZE", 50}, {"BLOB", 50}, {"HEAVE", 50}, {"BEAD", 50}, {"GLOW", 50}},
    fwx_init, fwx_note, fwx_frame, fwx_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "A lamp of wax: blobs rise, join, part", 3u, {0, 0, 0},
    {{"LAMP", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 6u)}, {99, 99, 50, 40}},
     {"TAR", {11, 96, 81, 31, 21, 0, 26, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 25, 0}},
     {"BOIL", {96, 11, 31, 0, 91, 96, 81, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 2u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 30, 40}}},
    22u, {0, 0, 0}, SPL_WAX};
