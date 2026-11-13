/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* RIVULET, a plate module (umb_plate.h): rain on a window at night. Drops sit on the glass as beads; one heavy enough
 * lets go and runs down, wandering as it goes, taking the beads it meets and leaving a wet thread behind that dries (a
 * field, 60 x 60, that only fades: 25 steps a second); grown light again it stops, a bead once more. The beads are drawn
 * over the wet: a ring and its glint. A note is a drop where its pitch falls (C2 left .. C7 right), its track its height,
 * the harder the heavier: a hard one runs at once; the kick is a gust that shakes the beads loose; the rest of the kit is
 * mist: small beads anywhere.
 * Elements: RUN (how fast they run), DRY (how long the wet lasts), SIZE (the beads'), DROP (the notes that are no drum),
 * GUST (the kick), MIST (the rest of the kit), WIND (the slant of their way: 50 straight down). */
#include "plate_lib.h"

#define FRV_ND 24                                       /* drops at once */
typedef struct {
    uint8_t w[PL_S];                                    /* the wet left behind, a byte a cell */
    uint8_t cv[256];                                    /* its light: a thread bright along its middle */
    int16_t x[FRV_ND], y[FRV_ND];                       /* each drop: where (cells, Q8) .. */
    uint8_t m[FRV_ND], c[FRV_ND], sd[FRV_ND], run[FRV_ND];   /* .. how heavy (0: none), whose (its track + 1; 0: mist),
                                                         * its own dice, running */
    uint16_t gust;                                      /* the kick's gust (Q8) */
    uint8_t nd;
    uint32_t rs, ms, T;                                 /* the dice, the steps' clock, the steps */
} frv_t;

static void frv_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    frv_t *s = st;
    int32_t i;
    (void)a;
    s->rs = 1277u + cx->now;
    s->ms = cx->now;
    for (i = 0; i < 256; i++)
        s->cv[i] = (uint8_t)pl_clamp(((i * i) >> 10) + (i > 120 ? (i - 120) * 2 : 0), 0, 255);
}
/* a drop of mass m at x, y (cells, Q8), track c's (0: mist): into the bead already there (within two cells), else a free
 * place, else the lightest bead's */
static void frv_drop(frv_t *s, int32_t x, int32_t y, int32_t m, uint32_t c)
{
    uint32_t i, b = 0;
    for (i = 0; i < FRV_ND; i++) {
        if (s->m[i] && !s->run[i] && pl_abs(s->x[i] - x) < 512 && pl_abs(s->y[i] - y) < 512) {
            s->m[i] = (uint8_t)pl_clamp(s->m[i] + m, 0, 255);
            if (c)
                s->c[i] = (uint8_t)c;
            return;
        }
        if (s->run[b] || (!s->run[i] && s->m[i] < s->m[b]))
            b = i;
    }
    s->x[b] = (int16_t)x;
    s->y[b] = (int16_t)y;
    s->m[b] = (uint8_t)m;
    s->c[b] = (uint8_t)c;
    s->sd[b] = (uint8_t)pl_rnd(&s->rs);
    s->run[b] = 0;
}
static void frv_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    frv_t *s = st;
    int32_t v, i;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > s->gust)   /* GUST (the kick) */
        s->gust = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0)   /* MIST */
        for (i = 0; i < 2; i++) {
            int32_t X = (int32_t)(3u * 256u + pl_rnd(&s->rs) % 13824u);
            frv_drop(s, X, (int32_t)(3u * 256u + pl_rnd(&s->rs) % 12800u), 22 + (v >> 3), 0u);
        }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* DROP */
        return;
    frv_drop(s, 768 + pl_fold(nt->mo[0], pl_pk(nt->note) * 54, 13824),
             768 + pl_fold(nt->mo[1], (int32_t)(nt->track * 1280u + pl_rnd(&s->rs) % 1536u), 13824), 70 + ((130 * v) >> 8), nt->track + 1u);
}
/* a step: the wet dries (DRY: 0.5 .. 9 s); a bead heavy enough (the gust: lighter ones too) runs: down by its weight
 * (RUN), aside by its own wandering (the noise along its way) and the wind, wetting where it goes, thinning as it does,
 * taking the beads it meets; too light, it stops; at the foot it is gone */
static void frv_step(frv_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int32_t k1 = (int32_t)cx->k[1], f = 65536 / (12 + k1 * k1 * 213 / 9801), i, j;
    s->T++;
    for (i = 0; i < PL_S; i++)
        s->w[i] = (uint8_t)pl_dec(s->w[i], f);
    for (i = 0; i < FRV_ND; i++) {
        int32_t m = s->m[i], vy, x, y;
        if (!m)
            continue;
        if (!s->run[i]) {
            s->run[i] = (m * (256 + 2 * s->gust)) >> 8 >= 110;
            continue;
        }
        vy = 30 + ((m * (40 + 2 * (int32_t)cx->k[0])) >> 7);
        x = s->x[i] + ((((pl_tbil(a, s->y[i] >> 1, s->sd[i] * 256) >> 8) - 128 + ((int32_t)cx->k[6] - 50) * 3) * vy) >> 8);
        y = s->y[i] + vy;
        pl_stamp(s->w, x, y, 420, 24 + (m >> 2));
        for (j = 0; j < FRV_ND; j++)
            if (j != i && s->m[j] && !s->run[j] && pl_abs(s->x[j] - x) < 400 && pl_abs(s->y[j] - y) < 400) {
                m = pl_clamp(m + (s->m[j] >> 1), 0, 255);
                s->m[j] = 0;
            }
        m -= (int32_t)(s->T & 1u);
        if (m < 50)
            s->run[i] = 0;
        if (y > 59 * 256 || x < 0 || x > 59 * 256)
            m = s->run[i] = 0;
        s->x[i] = (int16_t)x;
        s->y[i] = (int16_t)y;
        s->m[i] = (uint8_t)m;
    }
    s->gust = pl_dec(s->gust, 10486);                   /* (0.25 s) */
}
static void frv_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    frv_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 40u);
    while (n--)
        frv_step(s, cx, a);
}
static void frv_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const frv_t *s = st;
    int32_t i;
    pl_fdraw(S, s->w, s->cv, a->pal, 0);
    for (i = 0; i < FRV_ND; i++) {                      /* the beads: a ring, a glint up and to the left */
        int32_t m = s->m[i], X = PL_C2P(s->x[i]), Y = PL_C2P(s->y[i]), r = 300 + ((m * (60 + 2 * (int32_t)cx->k[2])) >> 5);
        if (!m)
            continue;
        pl_arc(S, a, X, Y, r, 0u, 0xFFFFFFFFu, 50 + (m >> 1) + (s->run[i] ? 60 : 0), s->c[i] ? pl_ncol(a, s->c[i] - 1u) : PL_INK);
        pl_splat(S, X - r / 3, Y - r / 3, 100 + (m >> 1), PL_WT);
    }
}

#include "splash/rivulet.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_rivulet = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(frv_t), "RIVULET",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 7u, {0, 0, 0},
    {{"RUN", 50}, {"DRY", 50}, {"SIZE", 50}, {"DROP", 50}, {"GUST", 50}, {"MIST", 50}, {"WIND", 50}},
    frv_init, frv_note, frv_frame, frv_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "Rain on glass: beads gather and run", 3u, {0, 0, 0},
    {{"SHOWER", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 50, 30}},
     {"DRIZZLE", {21, 91, 26, 31, 0, 91, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 6u), 0}, {99, 99, 25, 0}},
     {"STORM", {91, 31, 71, 96, 96, 0, 81, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 6u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 40, 30}}},
    40u, {0, 0, 0}, SPL_RIVULET};
