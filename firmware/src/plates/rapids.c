/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* RAPIDS, a plate module (umb_plate.h): a stream seen from above, by the lines its water draws. Forty points ride the
 * flow from left to right (it meanders: whirls laid over the run, drifting; 20 steps a second) and each leaves its
 * light where it has been, in a field (60 x 60) that only fades: threads, bright at the head. A note sets two points
 * down where its pitch lies (low notes at the foot, high at the top), brighter than the stream's own and its track's
 * colour at the head; the kick is a surge: the stream runs up to three times as fast and brightens; the rest of the kit
 * drop eddies in it, whirls that turn the threads round themselves and die away.
 * Elements: SPEED (the stream's), FADE (how long a thread lasts), TURB (how far it meanders: smooth .. broken), STREAK
 * (the notes that are no drum), SURGE (the kick), EDDY (the rest of the kit), THREAD (the stream's own points). */
#include "plate_lib.h"

#define FRP_NP 40                                       /* points at once: the first 24 the stream's own (THREAD 50: 6) */
#define FRP_NO 24
#define FRP_NE 3                                        /* eddies at once */
typedef struct {
    uint8_t t[PL_S];                                    /* the threads' light, a byte a cell */
    uint8_t cv[256];                                    /* its curve: a thread bright along its middle */
    int16_t x[FRP_NP], y[FRP_NP];                       /* each point: where (cells, Q8) .. */
    uint8_t b[FRP_NP], c[FRP_NP];                       /* .. how bright (0: none), whose (its track + 1; 0: the stream's) */
    int16_t ex[FRP_NE], ey[FRP_NE], es[FRP_NE];         /* the eddies: where, how strong (signed: which way round) */
    uint16_t surge;                                     /* the kick's surge (Q8) */
    uint8_t np, ne;
    uint32_t ph, rs, ms;                                /* the meanders' drift, the dice, the steps' clock */
} frp_t;

static void frp_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    frp_t *s = st;
    int32_t i;
    (void)a;
    s->rs = 5119u + cx->now;
    s->ms = cx->now;
    for (i = 0; i < 256; i++)
        s->cv[i] = (uint8_t)pl_clamp(i + ((i * i) >> 7), 0, 255);
    for (i = 0; i < FRP_NO; i++) {                      /* the stream's own, anywhere along it to begin with */
        s->x[i] = (int16_t)(pl_rnd(&s->rs) % 15000u);
        s->y[i] = (int16_t)(512u + pl_rnd(&s->rs) % 14336u);
        s->b[i] = (uint8_t)(70u + pl_rnd(&s->rs) % 50u);
    }
}
static void frp_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    frp_t *s = st;
    int32_t v, j;
    uint32_t i;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > s->surge)   /* SURGE (the kick) */
        s->surge = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* EDDY: a whirl, one way then the other */
        i = s->ne++ % FRP_NE;
        s->ex[i] = (int16_t)(10u * 256u + pl_rnd(&s->rs) % 10240u);
        s->ey[i] = (int16_t)(10u * 256u + pl_rnd(&s->rs) % 10240u);
        s->es[i] = (int16_t)(s->ne & 1u ? v * 4 : -v * 4);
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* STREAK: two points, one behind the other */
        return;
    for (j = 0; j < 2; j++) {
        i = FRP_NO + s->np++ % (FRP_NP - FRP_NO);
        s->x[i] = (int16_t)(768 + pl_fold(nt->mo[0], (int32_t)(nt->track * 1500u) + j * 700, 13824));
        s->y[i] = (int16_t)(768 + pl_fold(nt->mo[1], (256 - pl_pk(nt->note)) * 54, 13824));
        s->b[i] = (uint8_t)(120 + (v >> 1));
        s->c[i] = (uint8_t)(nt->track + 1u);
    }
}
/* a step: the light fades (FADE: 0.3 .. 6 s); each point goes with the flow: the run (SPEED; the surge up to three times
 * it), the meanders (TURB), the eddies round themselves; lays its light down round itself (a ridge along its way);
 * off the right edge the stream's own come in again at the left, the notes' are gone */
static void frp_step(frp_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int32_t k1 = (int32_t)cx->k[1], f = 65536 / (6 + k1 * k1 * 114 / 9801), U = ((25 + (int32_t)cx->k[0] * 5 / 2) * (256 + 2 * s->surge)) >> 8,
            tb = (int32_t)cx->k[2] * 2, no = (int32_t)cx->k[6] * FRP_NO / 198, i, j;
    for (i = 0; i < PL_S; i++)
        s->t[i] = (uint8_t)pl_dec(s->t[i], f);
    s->ph += 3000000u + (uint32_t)U * 9000u;
    for (i = 0; i < FRP_NP; i++) {
        int32_t x = s->x[i], y = s->y[i], u = U, v = 0, b = s->b[i];
        if (i < FRP_NO && (i >= no || !b)) {            /* the stream's own: as many as THREAD asks, each from the left */
            s->b[i] = (uint8_t)(i < no ? 70 + pl_rnd(&s->rs) % 50u : 0u);
            s->x[i] = 0;
            s->y[i] = (int16_t)(512u + pl_rnd(&s->rs) % 14336u);
            continue;
        }
        if (!b)
            continue;
        u += (tb * (pl_sine(a, (uint32_t)x * 419430u + s->ph) >> 8) * (pl_sine(a, (uint32_t)y * 524288u + s->ph * 2u + 0x40000000u) >> 8)) >> 15;
        v -= (tb * (pl_sine(a, (uint32_t)x * 419430u + s->ph + 0x40000000u) >> 8) * (pl_sine(a, (uint32_t)y * 524288u + s->ph * 2u) >> 8)) >> 15;
        for (j = 0; j < FRP_NE; j++)
            if (s->es[j]) {
                int32_t dx = (x - s->ex[j]) >> 4, dy = (y - s->ey[j]) >> 4, d2 = ((dx * dx + dy * dy) >> 8) + 24;
                u -= s->es[j] * dy / d2 >> 3;
                v += s->es[j] * dx / d2 >> 3;
            }
        x += pl_clamp(u, -250, 250);
        y += pl_clamp(v, -250, 250);
        if (x >= 59 * 256 || x < 0 || y < 0 || y >= 59 * 256) {
            s->b[i] = 0;
            continue;
        }
        s->x[i] = (int16_t)x;
        s->y[i] = (int16_t)y;
        pl_stamp(s->t, x, y, 290, (b * (256 + s->surge)) >> (i < FRP_NO ? 11 : 10));
        if (i >= FRP_NO && !(s->b[i] = (uint8_t)(s->b[i] - 1u)))
            s->c[i] = 0;
    }
    for (j = 0; j < FRP_NE; j++)                        /* the eddies die away (1.5 s) */
        s->es[j] = (int16_t)(s->es[j] - s->es[j] / 26 - (s->es[j] > 0) + (s->es[j] < 0));
    s->surge = pl_dec(s->surge, 8192);                  /* (0.4 s) */
}
static void frp_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    frp_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 50u);
    while (n--)
        frp_step(s, cx, a);
}
static void frp_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const frp_t *s = st;
    int32_t i;
    (void)cx;
    pl_fdraw(S, s->t, s->cv, a->pal, 0);
    for (i = 0; i < FRP_NP; i++)                        /* the heads */
        if (s->b[i])
            pl_splat(S, PL_C2P(s->x[i]), PL_C2P(s->y[i]), s->b[i] + 40, s->c[i] ? pl_ncol(a, s->c[i] - 1u) : PL_WT);
}

#include "splash/rapids.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_rapids = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(frp_t), "RAPIDS",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 7u, {0, 0, 0},
    {{"SPEED", 50}, {"FADE", 50}, {"TURB", 50}, {"STREAK", 50}, {"SURGE", 50}, {"EDDY", 50}, {"THREAD", 50}},
    frp_init, frp_note, frp_frame, frp_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "A stream drawn by the lines of its flow", 3u, {0, 0, 0},
    {{"RUN", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 50, 30}},
     {"BROOK", {21, 86, 16, 31, 0, 31, 81, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 25, 0}},
     {"TORRENT", {91, 31, 96, 96, 96, 91, 100, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 2u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 30, 40}}},
    70u, {0, 0, 0}, SPL_RAPIDS};
