/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* GYRE, a plate module (umb_plate.h): a whirlpool. Foam on dark water (a field, 60 x 60) is carried round the middle,
 * as fast far out as near (so the nearer goes round the sooner; within its core as one body), and drawn slowly in
 * (20 steps a second), so whatever falls on it is wound into arms: three of its own, from foam let in at three places
 * on the rim that go slowly round; in the middle the eye, black, with a thin bright rim that answers the mix's level. Over the foam, streaks: points of spray riding the same turn, each drawn as the arc it
 * has just travelled. A note is foam where its pitch falls (low notes at the rim, high by the eye), its track where
 * round, and two streaks of its colour; the kick is a surge (it turns faster, the rim flares); the rest of the kit is
 * spray.
 * Elements: SPIN (how fast it turns), PULL (how fast it draws in), EYE (the eye's size), FOAM (the notes that are no
 * drum), SURGE (the kick), SPRAY (the rest of the kit), FADE (how long the foam lasts), REST (its own three arms and
 * the spray that falls on it unasked). */
#include "plate_lib.h"

#define FGY_NP 40                                       /* streaks at once */
#define FGY_C 7552                                      /* the middle (cells, Q8: 29.5) */
typedef struct {
    uint8_t d[PL_S];                                    /* the foam, a byte a cell */
    uint8_t rp[PL_N], rc[PL_N];                         /* the step's rows as they were: the one above, this one */
    uint8_t cv[256];                                    /* the foam's light by how much there is */
    int16_t W[56], K[56];                               /* by the distance (half cells, squared, >> 7): the turn and the
                                                         * pull a step (Q16 of the distance) */
    uint16_t pa[FGY_NP], pr[FGY_NP];                    /* each streak: where round (65536 a turn), how far out (px, Q6) */
    uint8_t pv[FGY_NP], pc[FGY_NP];                     /* .. how bright (0: none), whose (its track + 1; 0: spray) */
    uint16_t surge;                                     /* the kick's surge (Q8) */
    uint16_t e2;                                        /* the eye: three quarters of the core (half cells, squared) */
    uint8_t np;                                         /* the next streak */
    uint16_t arm;                                       /* where its own arms begin (65536 a turn) */
    uint32_t rs, ms;                                    /* the dice, the steps' clock */
} fgy_t;

PL_SMALL static void fgy_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fgy_t *s = st;
    int32_t i;
    (void)a;
    s->rs = 911u + cx->now;
    s->ms = cx->now;
    for (i = 0; i < 256; i++)                           /* (the glow soft, the crests bright) */
        s->cv[i] = (uint8_t)pl_clamp((i * (i + 300)) / 280, 0, 255);
}
PL_SMALL static void fgy_streak(fgy_t *s, uint32_t ang, int32_t r, int32_t v, uint32_t c)   /* a streak at ang, r px out */
{
    uint32_t i = s->np++ % FGY_NP;
    s->pa[i] = (uint16_t)ang;
    s->pr[i] = (uint16_t)(r << 6);
    s->pv[i] = (uint8_t)pl_clamp(v, 1, 255);
    s->pc[i] = (uint8_t)c;
}
/* foam amt within r (cells, Q8) at ang (65536 a turn), R cells (Q8) out */
PL_SMALL static void fgy_foam(fgy_t *s, const umb_api_t *a, uint32_t ang, int32_t R, int32_t r, int32_t amt)
{
    pl_stamp(s->d, FGY_C + pl_pol(a, R, (ang << 16) + 0x40000000u), FGY_C + pl_pol(a, R, ang << 16), r, amt);
}
PL_SMALL static void fgy_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    fgy_t *s = st;
    int32_t v, k, R, i;
    uint32_t ang;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* SURGE (the kick): faster, and foam thrown round the eye */
        if (v > s->surge)
            s->surge = (uint16_t)v;
        for (i = 0; i < 3; i++)
            fgy_foam(s, a, pl_rnd(&s->rs), (12 + (int32_t)(pl_rnd(&s->rs) % 8u)) * 256, 480, (140 * v) >> 8);
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0)   /* SPRAY: the snare, the clap three streaks; the rest one */
        for (i = nt->lane < 3u ? 3 : 1; i > 0; i--)
            fgy_streak(s, pl_rnd(&s->rs), 44 + (int32_t)(pl_rnd(&s->rs) % 72u), 90 + (v >> 1), 0u);
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* FOAM */
        return;
    k = pl_pk(nt->note);
    ang = nt->track * 10923u + (uint32_t)(nt->mo[0] * 128) + (uint32_t)k * 40u;
    R = (27 - ((k * 17) >> 8)) * 256 + nt->mo[1] * 6;   /* (27 .. 10 cells out) */
    fgy_foam(s, a, ang, R, 560 + (v << 1), 130 + ((125 * v) >> 8));
    for (i = 0; i < 2; i++)
        fgy_streak(s, ang + (i ? -900u : 600u), (R >> 6) + (i ? -4 : 5), 110 + (v >> 1), nt->track + 1u);
}
/* a step: the turn and the pull by the distance (SPIN, PULL, EYE; the surge: up to twice as fast); each cell takes the
 * foam from where they bring it (less than a cell away: the rows as they were), thinned (FADE: 1 .. 12 s); the eye
 * drains; the streaks ride on; REST lets spray fall at the rim */
/* (the step's bookkeeping, built small: the speeds by the distance before the cells move, the streaks and the arms after) */
static void fgy_aux(fgy_t *s, const umb_lctx_t *cx, const umb_api_t *a, int32_t rc, int after)
{
    int32_t vm = pl_clamp(((int32_t)cx->k[0] * 230 / 99) * (256 + s->surge) >> 8, 0, 235), vr = 8 + (int32_t)cx->k[1] * 75 / 99, x, q;
    if (!after) {
        for (q = 0; q < 56; q++) {
            int32_t r = (int32_t)(pl_isqrt((uint32_t)(q * 128 + 128) << 16) >> 1), m = r > rc ? r : rc;   /* (its far edge: never more
                                                             * than a cell a step) */
            s->W[q] = (int16_t)((vm << 16) / m);
            s->K[q] = (int16_t)((vr << 16) / m);
        }
        s->e2 = (uint16_t)((rc * rc * 9) >> 18);
        return;
    }
    for (x = 0; x < FGY_NP; x++) {                      /* the streaks: round and in; gone at the eye */
        int32_t r = s->pr[x] >> 6;
        if (!s->pv[x])
            continue;
        q = pl_clamp((r * r) >> 9, 0, 55);              /* ((r / 2)^2 >> 7: 2 px a half cell) */
        s->pa[x] = (uint16_t)(s->pa[x] + ((s->W[q] * 10430) >> 16));
        s->pr[x] = (uint16_t)(s->pr[x] - ((s->K[q] * s->pr[x]) >> 16) - 1);
        if (r * 256 < rc * 3 || !--s->pv[x])
            s->pv[x] = 0;
    }
    s->arm = (uint16_t)(s->arm + 50u);                  /* REST: its own arms (round in a minute), spray now and then */
    for (x = 0; x < 3; x++)
        fgy_foam(s, a, s->arm + (uint32_t)x * 21845u, 27 * 256, 480, (int32_t)cx->k[7] / 2);
    if (pl_rnd(&s->rs) % 100u < (uint32_t)cx->k[7] / 2u)
        fgy_streak(s, pl_rnd(&s->rs), 60 + (int32_t)(pl_rnd(&s->rs) % 56u), 70 + (int32_t)(pl_rnd(&s->rs) % 60u), 0u);
    s->surge = pl_dec(s->surge, 6554);                  /* (0.5 s) */
}
static __attribute__((noinline)) void fgy_step(fgy_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int32_t rc = (2 + (int32_t)cx->k[2] * 8 / 99) * 256, k6 = (int32_t)cx->k[6], kd = 4096 - 4096 / (20 + k6 * k6 * 220 / 9801), x, y, q;
    fgy_aux(s, cx, a, rc, 0);
    pl_cpy(s->rp, s->d, PL_N);
    for (y = 0; y < PL_N; y++) {
        uint8_t *hr = s->d + y * PL_N;
        const uint8_t *rw[3];
        int32_t py = 2 * y - 59;
        pl_cpy(s->rc, hr, PL_N);
        rw[0] = s->rp;
        rw[1] = s->rc;
        rw[2] = y < PL_N - 1 ? hr + PL_N : s->rc;
        for (x = 0; x < PL_N; x++) {
            int32_t px = 2 * x - 59, d2 = px * px + py * py, w, k, X, Y, x0, fx, t, b;
            const uint8_t *r0, *r1;
            q = d2 >> 7;
            w = s->W[q];
            k = s->K[q];
            X = pl_clamp(x * 256 + ((w * py + k * px) >> 9), 0, (PL_N - 1) * 256 - 1);
            Y = pl_clamp(y * 256 + pl_clamp((k * py - w * px) >> 9, -255, 255), 0, (PL_N - 1) * 256 - 1);   /* (the rows kept: this one,
                                                         * the one above, the one below) */
            x0 = X >> 8;
            fx = X & 255;
            r0 = rw[(Y >> 8) - y + 1];
            r1 = rw[(Y >> 8) - y + 2];
            t = r0[x0] + (((r0[x0 + 1] - r0[x0]) * fx) >> 8);
            b = r1[x0] + (((r1[x0 + 1] - r1[x0]) * fx) >> 8);
            hr[x] = (uint8_t)(d2 < s->e2 ? 0 : ((t + (((b - t) * (Y & 255)) >> 8)) * kd) >> 12);
        }
        hr[0] = hr[PL_N - 1] = 0;
        pl_cpy(s->rp, s->rc, PL_N);
    }
    for (x = 0; x < PL_N; x++)
        s->d[x] = s->d[PL_S - PL_N + x] = 0;
    fgy_aux(s, cx, a, rc, 1);
}
PL_SMALL static void fgy_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fgy_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 50u);
    while (n--)
        fgy_step(s, cx, a);
}
static void fgy_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const fgy_t *s = st;
    int32_t C = PL_C2P(FGY_C), re = (2 + (int32_t)cx->k[2] * 8 / 99) * 4 * 256, i, k, rg = pl_clamp(70 + cx->level / 2 + s->surge, 0, 255);
    pl_fdraw(S, s->d, s->cv, a->pal, 0);
    pl_arc(S, a, C, C, re * 3 / 4, 0u, 0xFFFFFFFFu, rg, PL_WT);   /* the eye's rim (one ring: its dim second one gone for
                                                         * the 4 KiB slot, 2026-11-13) */
    for (i = 0; i < FGY_NP; i++) {                      /* the streaks: the arc of the last half second */
        int32_t r = s->pr[i] >> 6, q = pl_clamp((r * r) >> 9, 0, 55);
        uint32_t sp = (uint32_t)((s->W[q] * 10430) >> 16) * 11u + 500u;
        const uint8_t *c = s->pc[i] ? pl_ncol(a, s->pc[i] - 1u) : PL_WT;
        if (!s->pv[i])
            continue;
        for (k = 1; k < 4; k += 2)                      /* (dim its whole length, bright its last third) */
            pl_arc(S, a, C, C, s->pr[i] << 2, ((uint32_t)s->pa[i] - sp / (uint32_t)k) << 16, (sp / (uint32_t)k) << 16, s->pv[i] >> (k == 1), c);
    }
}

#include "splash/gyre.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_gyre = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(fgy_t), "GYRE",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 8u, {0, 0, 0},
    {{"SPIN", 60}, {"PULL", 40}, {"EYE", 50}, {"FOAM", 50}, {"SURGE", 50}, {"SPRAY", 50}, {"FADE", 60}, {"REST", 50}},
    fgy_init, fgy_note, fgy_frame, fgy_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "A whirlpool winds foam round its eye", 3u, {0, 0, 0},
    {{"VORTEX", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 50, 25}},
     {"DRAIN", {41, 91, 21, 0, 0, 31, 31, 71}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 30, 0}},
     {"GALAXY", {26, 11, 31, 81, 0, 91, 96, 100}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 5u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 60, 30}}},
    55u, {0, 0, 0}, SPL_GYRE};
