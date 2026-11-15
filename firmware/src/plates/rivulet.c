/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* RIVULET, a plate module (umb_plate.h): rain on a window at night. Drops sit on the glass as beads; one heavy enough
 * lets go and runs down, wandering as it goes, taking the beads it meets and leaving a wet thread behind that dries (a
 * field, 60 x 60, that only fades: 25 steps a second); grown light again it stops, a bead once more. The beads are drawn
 * over the wet: a ring and its glint. A note is a drop where its pitch falls (C2 left .. C7 right), its track its height,
 * the harder the heavier: a hard one runs at once; the kick is a gust that shakes the beads loose; the rest of the kit is
 * mist: small beads anywhere.
 * Elements: RUN (how fast they run), DRY (how long the wet lasts), SIZE (the beads'), DROP (the notes that are no drum),
 * GUST (the kick), MIST (the rest of the kit), WIND (the slant of their way: 50 straight down).
 * (NEW, API 6) STRIKE (ON, CHANCE, FORKS): lightning beyond the glass on the kick, now and then; the beads catch it.
 * (NEW, API 6) WIPER (ON, SWEEP, REACH): a wiper blade swinging over the glass (80 degrees either way), wiping the wet
 * and the beads away; at its longest it reaches the glass's top. */
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
    uint32_t lt, wp;                                    /* (NEW, API 6) STRIKE: since when (0: none); WIPER: its swing */
    uint8_t lv, nk;                                     /* .. how hard, the kicks counted (its own dice) */
} frv_t;
/* (NEW, API 6) WIPER: the blade's way, pivoting under the glass's foot: its heading's sine and cosine (Q15), its two
 * ends' radius (cells, Q8) */
#define FRV_PX (30 * 256)
#define FRV_PY (63 * 256)
static uint32_t frv_mix(uint32_t h)                    /* (NEW, API 6) STRIKE: dice that do not run in step */
{
    h = pl_hash(h);
    h ^= h >> 15;
    h *= 2246822519u;
    return h ^ (h >> 13);
}
static void frv_blade(const frv_t *s, const umb_lctx_t *cx, const umb_api_t *a, int32_t *sn, int32_t *cs, int32_t *r0, int32_t *r1)
{
    int32_t h = (pl_sine(a, s->wp) * 14564) >> 15;      /* (80 degrees either way) */
    *sn = PL_SIN(a, h);
    *cs = PL_COS(a, h);
    *r1 = (36 + cx->xk[1][2] * 30 / 99) * 256;          /* (REACH 99: past the glass's top) */
    *r0 = *r1 / 4;
}

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
    if (cx->xk[0][0] && nt->drum && !nt->lane &&        /* (NEW, API 6) STRIKE: one kick in so many (CHANCE) */
        (frv_mix(s->nk++) >> 24) < 30u + cx->xk[0][1] * 225u / 99u) {
        s->lt = cx->now | 1u;
        s->lv = (uint8_t)(128 + nt->vel);
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
    if (cx->xk[1][0]) {                                 /* (NEW, API 6) WIPER: a swing on; the wet under the blade wiped to
                                                         * a quarter, the beads it meets gone */
        int32_t sn, cs, r0, r1, r;
        s->wp += 43000000u + cx->xk[1][1] * 1300000u;  /* (a swing there and back in 4 .. 1 s) */
        frv_blade(s, cx, a, &sn, &cs, &r0, &r1);
        for (r = r0; r <= r1; r += 256) {
            int32_t x = (FRV_PX + (r * sn >> 15)) >> 8, y = (FRV_PY - (r * cs >> 15)) >> 8, dx, dy;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++)
                    if ((uint32_t)(x + dx) < PL_N && (uint32_t)(y + dy) < PL_N)
                        s->w[(y + dy) * PL_N + x + dx] >>= 2;
        }
        for (i = 0; i < FRV_ND; i++) {
            int32_t dx = s->x[i] - FRV_PX, dy = s->y[i] - FRV_PY, al = (dx * sn - dy * cs) >> 15, ac = pl_abs((dx * cs + dy * sn) >> 15);
            if (s->m[i] && al > r0 - 512 && al < r1 + 512 && ac < 400)
                s->m[i] = s->run[i] = 0;
        }
    }
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
    int32_t i, fl = 0;
    uint32_t la = cx->now - s->lt;
    pl_fdraw(S, s->w, s->cv, a->pal, 0);
    if (cx->xk[0][0] && s->lt && la < 400u) {           /* (NEW, API 6) STRIKE: a bolt from the top down a jagged way, its
                                                         * forks (FORKS: 0 .. 3) thinner; it flickers twice and is gone */
        uint32_t sd = s->lt * 2654435761u, h;
        int32_t X = (40 + (int32_t)(sd >> 8) % 160) * 256, Y = 0, j, k, nf = cx->xk[0][2] * 3 / 99, bx[12], by[12];
        fl = pl_out(s->lv, la, 400u) * (la < 60u || (la > 120u && la < 200u) ? 4 : 1) / 4;
        for (j = 0; j < 12; j++) {
            h = frv_mix(sd + (uint32_t)j);
            bx[j] = X;
            by[j] = Y;
            X += ((int32_t)(h >> 24) - 128) * 40;
            Y += (8 + (int32_t)(h >> 28)) * 256;
            pl_line(S, bx[j], by[j], X, Y, fl, PL_WT);
            pl_line(S, bx[j] + 256, by[j], X + 256, Y, fl / 3, PL_INK);
        }
        for (k = 0; k < nf; k++) {
            int32_t f = 2 + k * 3, fx = bx[f], fy = by[f], dir = k & 1 ? 1 : -1;
            for (j = 0; j < 5; j++) {
                h = frv_mix(sd + 77u + (uint32_t)(k * 8 + j));
                X = fx + dir * (4 + (int32_t)(h >> 29)) * 256 + ((int32_t)(h >> 24 & 15u) - 8) * 64;
                Y = fy + (5 + (int32_t)(h >> 28)) * 256;
                pl_line(S, fx, fy, X, Y, fl * (5 - j) / 8, PL_INK);
                fx = X;
                fy = Y;
            }
        }
    }
    if (cx->xk[1][0]) {                                 /* (NEW, API 6) WIPER: the blade and the arm to it */
        int32_t sn, cs, r0, r1, PX = PL_C2P(FRV_PX), PY = PL_C2P(FRV_PY);
        frv_blade(s, cx, a, &sn, &cs, &r0, &r1);
        r0 *= 4;
        r1 *= 4;
        sn >>= 3;                                       /* (Q12: r1 x 4 x sn in 32 bits) */
        cs >>= 3;
        int32_t ax = PX + (r0 * sn >> 12), ay = PY - (r0 * cs >> 12), bx = PX + (r1 * sn >> 12), by = PY - (r1 * cs >> 12),
                ox = cs >> 3, oy = sn >> 3;                 /* (a px across the blade) */
        pl_line(S, PX, PY, ax, ay, 200, PL_DEEP);
        pl_line(S, PX + ox * 2, PY + oy * 2, ax + ox * 2, ay + oy * 2, 120, PL_DEEP);
        pl_line(S, ax, ay, bx, by, 170 + (cx->beat >> 2), PL_INK);
        pl_line(S, ax + ox, ay + oy, bx + ox, by + oy, 200, PL_WT);
        pl_line(S, ax + ox * 2, ay + oy * 2, bx + ox * 2, by + oy * 2, 90, PL_INK);
    }
    for (i = 0; i < FRV_ND; i++) {                      /* the beads: a ring, a glint up and to the left */
        int32_t m = s->m[i], X = PL_C2P(s->x[i]), Y = PL_C2P(s->y[i]), r = 300 + ((m * (60 + 2 * (int32_t)cx->k[2])) >> 5);
        if (!m)
            continue;
        pl_arc(S, a, X, Y, r, 0u, 0xFFFFFFFFu, 50 + (m >> 1) + (s->run[i] ? 60 : 0), s->c[i] ? pl_ncol(a, s->c[i] - 1u) : PL_INK);
        pl_splat(S, X - r / 3, Y - r / 3, 100 + (m >> 1), PL_WT);
        if (fl)                                         /* (NEW, API 6) STRIKE: the beads catch the flash */
            pl_glint(S, X - r / 3, Y - r / 3, fl, PL_WT);
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
    40u, {0, 0, 0}, SPL_RIVULET,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"STRIKE", 3u, 1u, {0, 0}, {{"ON", 0}, {"CHANCE", 30}, {"FORKS", 50}},
      "Lightning forks beyond the glass now and then on the kick, and every bead catches its flash",
      {"Lightning on or off", "How often a kick brings a strike", "How many forks branch off the bolt (0 to 3)"}},
     {"WIPER", 3u, 1u, {0, 0}, {{"ON", 0}, {"SWEEP", 40}, {"REACH", 50}},
      "A wiper blade swings across the glass, wiping the wet and the beads away",
      {"Wiper on or off", "How fast it swings", "How long the blade is"}}}};
