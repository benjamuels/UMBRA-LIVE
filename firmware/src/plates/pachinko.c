/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* PACHINKO, a plate module (umb_plate.h): the star pachinko of UMBRA OBSCURA, the game. Six rows of pegs, eight cups under
 * them. A note drops a star from the top where its pitch falls (low at the left) in its track's colour; it falls, strikes
 * the pegs (each lights as it is struck, in the star's colour) and lands in a cup, which lights. The kick shakes the
 * table: every star jumps. Line art only: its light is added to the strip, so it lies on a layer as well as it stands
 * alone (REPLACE: black under it).
 * Elements: GRAVTY (how fast they fall), BOUNCE (how lively the pegs are), TILT (the table leaning: 50 level), STARS
 * (the notes that are no drum: a star each), SHAKE (the kick), PEGS and CUPS (their own light).
 * (NEW, API 6) BUMPER (ON, KICK, RING): two round bumpers among the pegs that throw a star off hard and flash its colour;
 * every 2 bars (8 beats) each glides quickly to a new place among the pegs (one in the left half, one in the right).
 * (NEW, API 6) LIGHTS (ON, CHASE, FLASH): a border of bulbs chasing round the table; a star landing flashes them all. */
#include "plate_lib.h"

#define PLP_NB 10                                       /* stars falling at once */
#define PLP_NP 39                                       /* the pegs: six rows, seven and six in turn */
#define PLP_NC 8                                        /* the cups */
typedef struct {
    int16_t x[PLP_NB], y[PLP_NB];                       /* each star: where (px, Q6) .. */
    int16_t vx[PLP_NB], vy[PLP_NB];                     /* .. its speed (px a step, Q8) */
    uint8_t on[PLP_NB], bv[PLP_NB];                     /* .. its track + 1 (0: none), how hard */
    uint8_t pe[PLP_NP], pc[PLP_NP];                     /* each peg: its light, whose (a track) */
    uint8_t ce[PLP_NC], cc[PLP_NC];                     /* each cup's */
    uint32_t sub, rs;                                   /* the steps' clock (ms), the dice */
    uint8_t nb;
    uint8_t be[2], bc[2];                               /* (NEW, API 6) BUMPER: each one's light, whose .. */
    int16_t ux[2], uy[2];                               /* .. where it is (px, Q6) .. */
    uint8_t tx[2], ty[2], nbt, pb;                      /* .. where it goes (px; 0: not yet placed), the beats, the last beat */
    uint8_t fl, fc;                                     /* (NEW, API 6) LIGHTS: the landing's flash, whose */
} plp_t;
#define PLP_Y0 58                                       /* the first row of pegs; a row 22 px */
#define PLP_CY 212                                      /* the cups' rims */
static const uint8_t PLP_BX[2] = {72, 168};             /* (NEW, API 6) BUMPER: the two bumpers' first places (px), between rows */
#define PLP_BY 113
#define PLP_BR(cx) (5 + (cx)->xk[0][2] / 20)            /* .. their radius (px): 5 .. 9 */
static void plp_peg(uint32_t i, int32_t *x, int32_t *y)   /* peg i's place (px) */
{
    uint32_t r = i / 13u * 2u + (i % 13u >= 7u), c = i % 13u >= 7u ? i % 13u - 7u : i % 13u;
    *x = (r & 1u ? 40 : 24) + (int32_t)c * 32;
    *y = PLP_Y0 + (int32_t)r * 22;
}
static void plp_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plp_t *m = st;
    (void)a;
    m->sub = cx->now;
    m->rs = 31u + cx->now;
}
static void plp_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plp_t *m = st;
    uint32_t i;
    int32_t v;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0)   /* SHAKE: every star jumps, every peg glints */
        for (i = 0; i < PLP_NP; i++) {
            if (i < PLP_NB && m->on[i])
                m->vy[i] = (int16_t)(m->vy[i] - v * 3 / 4);   /* (a hop of ~20 px) */
            if (m->pe[i] < v / 3) {
                m->pe[i] = (uint8_t)(v / 3);
                m->pc[i] = 6;                           /* (no track's: white) */
            }
        }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))
        return;
    i = m->nb++ % PLP_NB;
    m->x[i] = (int16_t)((14 + pl_fold(nt->mo[0], pl_pk(nt->note), 256) * 212 / 256) * 64);
    m->y[i] = 30 * 64;
    m->vx[i] = (int16_t)((int32_t)(pl_rnd(&m->rs) % 129u) - 64);
    m->vy[i] = 0;
    m->on[i] = (uint8_t)(nt->track + 1u);
    m->bv[i] = (uint8_t)v;
}
/* the steps owed (125 a second, 10 a frame at most): each star falls, leans with the table, strikes the pegs and the
 * walls, lands in a cup; the pegs' and the cups' light fades */
static void plp_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plp_t *m = st;
    int32_t g = 1 + cx->k[0] * 3 / 50, re = 90 + cx->k[1] * 80 / 50, tilt = ((int32_t)cx->k[2] - 50) / 12, f = (int32_t)cx->dt * 131, n = 0, i, k;
    (void)a;
    for (i = 0; i < PLP_NP; i++)                        /* (0.5 s) */
        m->pe[i] = (uint8_t)pl_dec(m->pe[i], f);
    for (i = 0; i < PLP_NC; i++)
        m->ce[i] = (uint8_t)pl_dec(m->ce[i], f / 2);
    if (cx->xk[0][0] || cx->xk[1][0]) {                 /* (NEW, API 6) BUMPER's light (0.5 s), LIGHTS' flash (1 s) */
        m->be[0] = (uint8_t)pl_dec(m->be[0], f);
        m->be[1] = (uint8_t)pl_dec(m->be[1], f);
        m->fl = (uint8_t)pl_dec(m->fl, f / 2);
    }
    if (cx->xk[0][0]) {                                 /* (NEW, API 6) BUMPER: every 8th beat each to a new place between two
                                                         * rows of pegs (its half of the table), gliding there in ~0.1 s */
        int32_t e = pl_clamp((int32_t)cx->dt * 256 / 90, 0, 256);
        if (cx->beat > m->pb + 64 && ++m->nbt >= 8u)
            m->nbt = 0;
        for (k = 0; k < 2; k++) {
            if (!m->tx[k] || (!m->nbt && cx->beat > m->pb + 64)) {
                uint32_t r = pl_rnd(&m->rs), x = (k ? 136u : 40u) + r % 5u * 16u, y = 69u + (r >> 8) % 5u * 22u;
                if (!m->tx[k]) {
                    x = PLP_BX[k];
                    y = PLP_BY;
                    m->ux[k] = (int16_t)(x * 64u);
                    m->uy[k] = (int16_t)(y * 64u);
                } else if (x == m->tx[k] && y == m->ty[k])
                    y = y > 130u ? y - 44u : y + 44u;   /* (never where it was) */
                m->tx[k] = (uint8_t)x;
                m->ty[k] = (uint8_t)y;
            }
            m->ux[k] = (int16_t)(m->ux[k] + ((m->tx[k] * 64 - m->ux[k]) * e >> 8));
            m->uy[k] = (int16_t)(m->uy[k] + ((m->ty[k] * 64 - m->uy[k]) * e >> 8));
        }
    }
    m->pb = cx->beat;
    while ((uint32_t)(cx->now - m->sub) >= 8u && n++ < 10) {
        m->sub += 8u;
        for (i = 0; i < PLP_NB; i++) {
            int32_t x, y, vx, vy;
            if (!m->on[i])
                continue;
            vx = m->vx[i] + tilt;
            vy = pl_clamp(m->vy[i] + g, -700, 700);
            x = m->x[i] + (vx >> 2);
            y = m->y[i] + (vy >> 2);
            for (k = 0; k < PLP_NP; k++) {              /* a peg within 5 px: thrown back along the line between them */
                int32_t px, py, dx, dy, d2, vn;
                plp_peg((uint32_t)k, &px, &py);
                dx = x - px * 64;
                dy = y - py * 64;
                if (pl_abs(dx) > 320 || pl_abs(dy) > 320 || (d2 = dx * dx + dy * dy) > 320 * 320)
                    continue;
                d2 = (int32_t)pl_isqrt((uint32_t)d2) + 1;
                vn = (vx * dx + vy * dy) / d2;          /* (its speed toward the peg: under 0) */
                if (vn < 0) {
                    vx -= vn * dx / d2 * (256 + re) >> 8;
                    vy -= vn * dy / d2 * (256 + re) >> 8;
                    m->pe[k] = m->bv[i];
                    m->pc[k] = (uint8_t)(m->on[i] - 1u);
                }
                x = px * 64 + dx * 330 / d2;            /* (out of it) */
                y = py * 64 + dy * 330 / d2;
                if (pl_abs(vx) < 24)                    /* (dead on top of one: nudged off) */
                    vx += (int32_t)(pl_rnd(&m->rs) % 97u) - 48;
            }
            if (cx->xk[0][0])                           /* (NEW, API 6) BUMPER: struck, thrown straight out from it, hard */
                for (k = 0; k < 2; k++) {
                    int32_t dx = x - m->ux[k], dy = y - m->uy[k], r = (PLP_BR(cx) + 3) * 64, d, sp = 260 + cx->xk[0][1] * 4;
                    if (pl_abs(dx) >= r || pl_abs(dy) >= r || (d = (int32_t)pl_isqrt((uint32_t)(dx * dx + dy * dy)) + 1) >= r)
                        continue;
                    vx = dx * sp / d;
                    vy = dy * sp / d;
                    x = m->ux[k] + dx * r / d;
                    y = m->uy[k] + dy * r / d;
                    m->be[k] = 255;
                    m->bc[k] = (uint8_t)(m->on[i] - 1u);
                }
            if (y < 24 * 64 && vy < 0)                  /* (the table's top) */
                vy = -vy / 2;
            if (x < 6 * 64 || x > 234 * 64) {
                x = pl_clamp(x, 6 * 64, 234 * 64);
                vx = -vx / 2;
            }
            if (y >= (PLP_CY - 2) * 64) {               /* a cup: lit, the star gone */
                k = pl_clamp((x / 64 - 2) / 30, 0, PLP_NC - 1);
                m->ce[k] = m->bv[i];
                m->cc[k] = (uint8_t)(m->on[i] - 1u);
                if (cx->xk[1][0] && m->bv[i] >= m->fl) {   /* (NEW, API 6) LIGHTS: the landing flashes the border */
                    m->fl = m->bv[i];
                    m->fc = m->cc[k];
                }
                m->on[i] = 0;
                continue;
            }
            m->x[i] = (int16_t)x;
            m->y[i] = (int16_t)y;
            m->vx[i] = (int16_t)pl_clamp(vx, -700, 700);
            m->vy[i] = (int16_t)vy;
        }
    }
    if ((uint32_t)(cx->now - m->sub) >= 8u)
        m->sub = cx->now;                               /* (behind: never a burst) */
}
static void plp_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plp_t *m = st;
    int32_t i, x, y, pl = 110 * cx->k[5] / 50, cl = 80 * cx->k[6] / 50;
    pl_clear(S);
    for (i = 0; i < PLP_NP; i++) {                      /* the pegs: a point; struck, a star in the striker's colour */
        plp_peg((uint32_t)i, &x, &y);
        pl_glint(S, x * 256, y * 256, pl, PL_INK);
        if (m->pe[i])
            pl_star(S, x * 256, y * 256, (2 + m->pe[i] / 60) * 256, m->pe[i], m->pc[i] < UMB_NTRK ? a->colour[(m->pc[i] * 3u) % 8u] : PL_WT);
    }
    for (i = 0; i < PLP_NC; i++) {                      /* the cups: two walls and a floor; lit, its rim and a glow in it */
        const uint8_t *tc = a->colour[(m->cc[i] * 3u) % 8u];
        x = (4 + i * 30) * 256;
        pl_line(S, x, PLP_CY * 256, x + 3 * 256, (PLP_CY + 14) * 256, cl, PL_INK);
        pl_line(S, x + 3 * 256, (PLP_CY + 14) * 256, x + 19 * 256, (PLP_CY + 14) * 256, cl, PL_INK);
        pl_line(S, x + 19 * 256, (PLP_CY + 14) * 256, x + 22 * 256, PLP_CY * 256, cl, PL_INK);
        if (m->ce[i]) {
            pl_line(S, x + 3 * 256, (PLP_CY + 12) * 256, x + 19 * 256, (PLP_CY + 12) * 256, m->ce[i], tc);
            pl_line(S, x + 2 * 256, (PLP_CY + 8) * 256, x + 20 * 256, (PLP_CY + 8) * 256, m->ce[i] / 2, tc);
            pl_star(S, x + 11 * 256, (PLP_CY + 4) * 256, 3 * 256, m->ce[i], PL_WT);
        }
    }
    if (cx->xk[0][0])                                   /* (NEW, API 6) BUMPER: two rings, breathing with the beat; struck, a
                                                         * ring of the striker's colour round them and a star */
        for (i = 0; i < 2; i++) {
            int32_t R = PLP_BR(cx) * 256 + (cx->beat >> 7) * 128, e = m->be[i], X = m->ux[i] * 4, Y = m->uy[i] * 4;
            const uint8_t *tc = a->colour[(m->bc[i] * 3u) % 8u];
            pl_arc(S, a, X, Y, R, 0u, 0xFFFFFFFFu, 150 + (cx->beat >> 2), PL_INK);
            pl_arc(S, a, X, Y, R / 2, 0u, 0xFFFFFFFFu, 90, PL_INK);
            if (e) {
                pl_arc(S, a, X, Y, R + 2 * 256 + (255 - e) * 16, 0u, 0xFFFFFFFFu, e, tc);
                pl_star(S, X, Y, (2 + e / 50) * 256, e, PL_WT);
            }
        }
    if (cx->xk[1][0]) {                                 /* (NEW, API 6) LIGHTS: bulbs every 12 px up the left, across the
                                                         * top, down the right; every fourth lit, chasing; the flash lights
                                                         * them all in the landing's colour */
        uint32_t ph = cx->now / 10u * (uint32_t)(4 + cx->xk[1][1] * 16 / 99) / 100u;   /* (4 .. 20 steps a second) */
        int32_t fl = m->fl * (20 + cx->xk[1][2]) / 119;
        const uint8_t *tc = a->colour[(m->fc * 3u) % 8u];
        for (i = 0; i < 54; i++) {
            int32_t bx = i < 16 ? 4 : i < 38 ? 6 + (i - 16) * 11 : 236, by = i < 16 ? 200 - i * 12 : i < 38 ? 16 : 20 + (i - 38) * 12,
                    on = ((uint32_t)i + ph) % 4u == 0u ? 190 + (cx->beat >> 2) : 40;
            pl_star(S, bx * 256, by * 256, on > 40 ? 3 * 256 : 256, on, PL_WARM[0]);
            if (fl)
                pl_star(S, bx * 256, by * 256, 2 * 256, fl, tc);
        }
    }
    for (i = 0; i < PLP_NB; i++)                        /* the stars: a glint and the way it came */
        if (m->on[i]) {
            const uint8_t *tc = a->colour[((m->on[i] - 1u) * 3u) % 8u];
            pl_line(S, m->x[i] * 4, m->y[i] * 4, m->x[i] * 4 - m->vx[i] * 6, m->y[i] * 4 - m->vy[i] * 6, 60 + m->bv[i] / 3, tc);
            pl_glint(S, m->x[i] * 4, m->y[i] * 4, 130 + m->bv[i] / 2, tc);
            pl_splat(S, m->x[i] * 4, m->y[i] * 4, 160, PL_WT);
        }
}

#include "splash/pachinko.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_pachinko = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plp_t), "PACHINKO",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 7u, {0, 0, 0},
    {{"GRAVTY", 50}, {"BOUNCE", 50}, {"TILT", 50}, {"STARS", 50}, {"SHAKE", 50}, {"PEGS", 50}, {"CUPS", 50}},
    plp_init, plp_note, plp_frame, plp_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Stars fall through pegs into cups", 3u, {0, 0, 0},
    {{"FLOAT", {16, 81, 0, 0, 0, 36, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 40, 0}},
     {"JACKPOT", {81, 0, 0, 96, 0, 0, 91, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), UMB_LINK(UMB_SRC_HIT(0), 4u)}, {99, 99, 60, 50}},
     {"TILT", {0, 0, 86, 0, 81, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 5u), UMB_LINK(UMB_SRC_BEAT, 2u)}, {99, 99, 40, -30}}},
    92u, {0, 0, 0}, SPL_PACHINKO,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"BUMPER", 3u, 1u, {0, 0}, {{"ON", 0}, {"KICK", 50}, {"RING", 50}},
      "Two round bumpers among the pegs throw a star off hard and flash in its colour; every 2 bars they move",
      {"Bumpers on or off", "How hard they throw a star", "How big they are"}},
     {"LIGHTS", 3u, 1u, {0, 0}, {{"ON", 0}, {"CHASE", 50}, {"FLASH", 50}},
      "A border of bulbs chases round the table; every star that lands flashes them all",
      {"Lights on or off", "How fast the lit bulbs chase round", "How bright a landing flashes them"}}}};
