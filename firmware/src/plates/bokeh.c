/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* BOKEH, a plate module (umb_plate.h): a city's lights at night through a lens out of focus; made to lie over RIVULET,
 * the rain on the glass in front of them. Sixteen lights, each a disc with a brighter edge, as a lens draws a point it
 * cannot resolve: at rest only their edges, faint, a third of them sodium yellow, breathing. A note lights the one
 * nearest its place (its pitch across, its track down) in its track's colour: the whole disc, going out. The kick pulls
 * the focus: every disc draws in, sharper and brighter, and lets go. The rest of the kit are cars along the bottom: two
 * headlamps one way, two red lamps the other. Its light is added to the strip, so it lies on a layer as well as it stands
 * alone (REPLACE: black under it).
 * Elements: BLUR (the discs' size), FADE (how long a light stays lit), RIM (how much brighter the edge), LAMPS (the notes
 * that are no drum), FOCUS (the kick), CARS (the rest of the kit), REST (the lights' own), DRIFT (how far they sway).
 * (NEW, API 6) SPIKES (on/off; POINTS: four .. eight; REACH: how far): the lens's star, rays out of every lit light,
 * longest as a note lights it and as the kick pulls the focus. TRAIN (on/off; PACE: how fast; COACH: how many coaches): an
 * elevated train crossing the city, its windows soft lights brightening with the level. SHUFFLE (on/off; RATE: how often
 * a light moves; GLIDE: 0 a jump .. 99 the whole while on its way): the lights leave their places for new ones anywhere
 * in the city, each in its own time, and glide there (easing out and in); off, they are back where they were. */
#include "plate_lib.h"

#define OBK_N 16                                        /* the lights */
#define OBK_NC 3                                        /* cars at once */
typedef struct {
    uint8_t e[OBK_N], c[OBK_N];                         /* each light: the notes' on it, whose (its track) */
    int16_t cx[OBK_NC];                                 /* each car: where (px, Q4) .. */
    uint8_t cv[OBK_NC], cd[OBK_NC];                     /* .. how hard (0: none), which way (1: leftward) */
    uint16_t foc;                                       /* the pull on the focus (Q8) */
    uint8_t nc;
    uint32_t tp;                                        /* (NEW, API 6) TRAIN: how far it has gone (px, Q8) */
    uint32_t xr;                                        /* (NEW, API 6) SHUFFLE: its dice; each light: where from, where to
                                                         * (across, down: Q16 of the city), since when (ms); 1: under way */
    uint16_t fx[OBK_N], fy[OBK_N], tx[OBK_N], ty[OBK_N], ms[OBK_N];
    uint8_t sh;
} obk_t;
static const uint8_t OBK_RED[3] = {255, 56, 28};        /* a car's lamps from behind */

/* (NEW, API 6) SHUFFLE: how long light i (its dice h) stays on each move (RATE: 8 .. 0.6 s, its own 0.75 .. 1.25 of it) */
static int32_t obk_T(const umb_lctx_t *cx, uint32_t h) { return (8000 - cx->xk[2][1] * 75) * (192 + (int32_t)(h >> 25)) >> 8; }
/* light i: its place (px) and its size, as DRIFT and BLUR (and SHUFFLE) have them */
static void obk_at(const obk_t *s, const umb_lctx_t *cx, const umb_api_t *a, uint32_t i, int32_t *x, int32_t *y, int32_t *r)
{
    uint32_t h = pl_hash(i + 40u), g = (h ^ h >> 13) * 2246822519u, u = h >> 16, v = (g >> 16) ^ (g & 0xFFFFu);   /* (h's top
                                                         * bits go evenly round: the lights spread across) */
    int32_t top = PL_TOP(cx);
    if (s->sh) {                                        /* (NEW, API 6) SHUFFLE: on its way (GLIDE: its share of the while) */
        int32_t G = obk_T(cx, h) * cx->xk[2][2] / 99, t = s->ms[i] >= G ? 256 : s->ms[i] * 256 / (G + 1);
        t = (t * t * (768 - 2 * t)) >> 16;              /* (eased out and in) */
        u = (uint32_t)(s->fx[i] + (((s->tx[i] - s->fx[i]) * t) >> 8));
        v = (uint32_t)(s->fy[i] + (((s->ty[i] - s->fy[i]) * t) >> 8));
    }
    *x = 14 + (int32_t)((u * 212u) >> 16) + (pl_pol(a, cx->k[7] * 40, cx->now * (40000u + (g >> 20 & 63u) * 900u) + g) >> 8);
    *y = top + 18 + (int32_t)((v * (uint32_t)(150 - top / 2)) >> 16);
    *r = (5 + (int32_t)(g >> 4 & 15u) * 11 / 15) * (25 + cx->k[0]) / 75;   /* (BLUR 50: 5 .. 16 px) */
}
/* a disc round X, Y (px) of radius R: light e all over it, rim more on its outer two pixels */
static void obk_disc(const umb_strip_t *S, int32_t X, int32_t Y, int32_t R, int32_t e, int32_t rim, const uint8_t *c)
{
    int32_t x, y, y1 = Y + R < S->y0 + S->h - 1 ? Y + R : S->y0 + S->h - 1;
    for (y = Y - R > S->y0 ? Y - R : S->y0; y <= y1; y++) {
        int32_t dy = y - Y, w = (int32_t)pl_isqrt((uint32_t)(R * R - dy * dy)), q = (R - 2) * (R - 2) - dy * dy,
                wi = q > 0 ? (int32_t)pl_isqrt((uint32_t)q) : -1;
        for (x = -w; x <= w; x++)
            pl_px(S, X + x, y, pl_abs(x) > wi ? e + rim : e, c[0], c[1], c[2]);
    }
}
static void obk_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    (void)a;
    ((obk_t *)st)->xr = 6271u + cx->now;                /* (NEW, API 6) SHUFFLE's dice */
}
static void obk_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    obk_t *s = st;
    int32_t v, x, y, r, nx, ny, bd = 1 << 30;
    uint32_t i, best = 0;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > s->foc)   /* FOCUS (the kick) */
        s->foc = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* CARS: one in at an edge, the next the other way */
        for (i = 0; i < OBK_NC && s->cv[i]; i++)
            ;
        if (i < OBK_NC) {                               /* (the road full: none) */
            s->cd[i] = ++s->nc & 1u;
            s->cx[i] = (int16_t)(s->cd[i] ? 250 * 16 : -20 * 16);
            s->cv[i] = (uint8_t)v;
        }
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* LAMPS: the light nearest the note's place */
        return;
    nx = pl_nx(nt, 14, 226);
    ny = pl_ny(nt, PL_TOP(cx) + 18, 168);
    for (i = 0; i < OBK_N; i++) {
        obk_at(s, cx, a, i, &x, &y, &r);
        if (pl_abs(x - nx) + pl_abs(y - ny) < bd) {
            bd = pl_abs(x - nx) + pl_abs(y - ny);
            best = i;
        }
    }
    if (v > s->e[best])
        s->e[best] = (uint8_t)v;
    s->c[best] = nt->track;
}
static void obk_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    obk_t *s = st;
    int32_t dt = (int32_t)cx->dt, k1 = cx->k[1], i;
    (void)a;
    for (i = 0; i < OBK_N; i++)
        s->e[i] = (uint8_t)pl_dec(s->e[i], dt * 65536 / (250 + k1 * k1 * 4 / 5));   /* (FADE 50: 2.2 s) */
    for (i = 0; i < OBK_NC; i++)                        /* the cars: 90 px a second, off the far edge */
        if (s->cv[i]) {
            s->cx[i] = (int16_t)(s->cx[i] + (s->cd[i] ? -1 : 1) * ((dt * 23) >> 4));
            if (s->cx[i] < -24 * 16 || s->cx[i] > 256 * 16)
                s->cv[i] = 0;
        }
    s->foc = pl_dec(s->foc, dt * 190);                  /* (0.35 s) */
    s->tp += (uint32_t)(dt * (8 + cx->xk[1][1]) * 256 / 500);   /* (NEW, API 6) TRAIN: 16 .. 214 px a second (PACE) */
    /* (NEW, API 6) SHUFFLE: turned on, every light sets off from where it stands; each moves on, its while up (obk_T), to a
     * new place, from where it had got to */
    if (!cx->xk[2][0])
        s->sh = 0;
    for (i = 0; cx->xk[2][0] && i < OBK_N; i++) {
        uint32_t h = pl_hash((uint32_t)i + 40u), g = (h ^ h >> 13) * 2246822519u;
        int32_t T = obk_T(cx, h);
        if (!s->sh) {
            s->fx[i] = s->tx[i] = (uint16_t)(h >> 16);
            s->fy[i] = s->ty[i] = (uint16_t)((g >> 16) ^ (g & 0xFFFFu));
            s->ms[i] = (uint16_t)T;
        }
        s->ms[i] = (uint16_t)(s->ms[i] + dt > 60000 ? 60000 : s->ms[i] + dt);
        if (s->ms[i] >= T) {
            s->fx[i] = s->tx[i];
            s->fy[i] = s->ty[i];
            s->tx[i] = (uint16_t)pl_rnd(&s->xr);
            s->ty[i] = (uint16_t)pl_rnd(&s->xr);
            s->ms[i] = 0;
        }
    }
    s->sh = cx->xk[2][0];
}
static void obk_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const obk_t *s = st;
    uint32_t i;
    pl_clear(S);
    for (i = 0; i < OBK_N; i++) {
        int32_t x, y, r, e = s->e[i], rest = (cx->k[6] * (190 + (pl_sine(a, cx->now * 130000u + pl_hash(i)) >> 9))) >> 7, rim;
        uint8_t m[3];
        obk_at(s, cx, a, i, &x, &y, &r);
        r = pl_clamp((r * (256 - s->foc * 150 / 255)) >> 8, 2, 40);
        if (y + r < S->y0 || y - r >= S->y0 + S->h)
            continue;
        rim = pl_mix(m, i % 3u ? PL_INK : PL_WARM[0], pl_ncol(a, s->c[i]), rest + s->foc / 4, e * cx->k[2] / 50);
        obk_disc(S, x, y, r, 0, rim, m);
        if (e)
            obk_disc(S, x, y, r, (e * (256 + s->foc)) >> 9, 0, pl_ncol(a, s->c[i]));
        /* (NEW, API 6) SPIKES: POINTS rays through the light, as long as it is lit (REACH), brightest at its rim */
        if (cx->xk[0][0]) {
            int32_t g = e + s->foc / 2 + rest / 3, L = (r + 4 + ((g * (10 + cx->xk[0][2] * 60 / 99)) >> 8)) * 256, np = 2 + cx->xk[0][1] * 3 / 100, k;
            for (k = 0; g > 24 && k < np; k++) {
                uint32_t ph = (uint32_t)k * (0x80000000u / (uint32_t)np) + 0x08000000u;
                int32_t ux = pl_pol(a, L, ph + 0x40000000u), uy = pl_pol(a, L, ph);
                pl_line(S, x * 256 - ux, y * 256 - uy, x * 256 + ux, y * 256 + uy, g > 255 ? 110 : g * 110 / 255, e ? pl_ncol(a, s->c[i]) : m);
                pl_line(S, x * 256 - ux / 2, y * 256 - uy / 2, x * 256 + ux / 2, y * 256 + uy / 2, g > 255 ? 120 : g * 120 / 255, PL_WT);
            }
        }
    }
    /* (NEW, API 6) TRAIN: COACH coaches (one .. four) on a line across the city, right to left, again and again: each a
     * faint outline and eight windows, soft lights brighter with the level, the front one a lamp */
    if (cx->xk[1][0]) {
        int32_t nk = 1 + cx->xk[1][2] * 4 / 100, len = nk * 64, y = 182, x0 = 240 - (int32_t)((s->tp >> 8) % (uint32_t)(len + 360)), k, j,
                w = 70 + cx->level / 2;
        pl_line(S, 0, (y + 10) * 256, 240 * 256, (y + 10) * 256, 40, PL_DEEP);   /* (the line it runs on) */
        for (k = 0; k < nk; k++) {
            int32_t cxl = x0 + k * 64;
            if (cxl > 250 || cxl + 64 < -10)
                continue;
            pl_line(S, cxl * 256, (y - 8) * 256, (cxl + 60) * 256, (y - 8) * 256, 70, PL_INK);
            pl_line(S, cxl * 256, (y + 7) * 256, (cxl + 60) * 256, (y + 7) * 256, 70, PL_INK);
            for (j = 0; j < 7; j++)
                obk_disc(S, cxl + 6 + j * 8, y, 3, (w * (200 + (pl_hash((uint32_t)(k * 8 + j)) >> 26) * 4)) >> 8, 40, (j + k) % 3 ? PL_WARM[0] : PL_INK);
        }
        if (x0 > -10 && x0 < 250)
            pl_star(S, (x0 - 2) * 256, y * 256, 1536, 230, PL_WT);
    }
    for (i = 0; i < OBK_NC; i++)
        if (s->cv[i]) {
            obk_disc(S, s->cx[i] >> 4, s->cd[i] ? 206 : 218, 3, s->cv[i], 0, s->cd[i] ? OBK_RED : PL_WARM[0]);
            obk_disc(S, (s->cx[i] >> 4) + 10, s->cd[i] ? 206 : 218, 3, s->cv[i], 0, s->cd[i] ? OBK_RED : PL_WARM[0]);
        }
}

#include "splash/bokeh.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_bokeh = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(obk_t), "BOKEH",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"BLUR", 50}, {"FADE", 50}, {"RIM", 50}, {"LAMPS", 50}, {"FOCUS", 50}, {"CARS", 50}, {"REST", 40}, {"DRIFT", 30}},
    obk_init, obk_note, obk_frame, obk_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "City lights out of focus, lit by notes", 3u, {0, 0, 0},
    {{"AVENUE", {0, 0, 0, 0, 0, 100, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 6u)}, {99, 99, 40, 30}},
     {"MACRO", {96, 86, 16, 0, 31, 1, 61, 71}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 0u), 0}, {99, 99, 25, 0}},
     {"SIGNAL", {21, 21, 100, 100, 100, 0, 11, 1}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 4u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 50, 40}}},
    38u, {0, 0, 0}, SPL_BOKEH,
    /* (API 6, the test) its new elements */
    3u, {0, 0, 0},
    {{"SPIKES", 3u, 1u, {0, 0}, {{"ON", 0}, {"POINTS", 30}, {"REACH", 50}}, "A star filter: rays through every lit light, longest as notes light them and the kick pulls focus",
      {"Spikes on or off", "How many rays (four .. eight)", "How far they reach"}},
     {"TRAIN", 3u, 1u, {0, 0}, {{"ON", 0}, {"PACE", 40}, {"COACH", 50}}, "An elevated train crosses the city, its windows brightening with the level",
      {"Train on or off", "How fast it goes", "How many coaches (one .. four)"}},
     {"SHUFFLE", 3u, 1u, {0, 0}, {{"ON", 0}, {"RATE", 40}, {"GLIDE", 60}}, "The lights wander the city: each leaves its place for a new one and glides there",
      {"Shuffle on or off", "How often a light moves (every 8 s .. 0.6 s)", "How it gets there (0 a jump .. 99 gliding all the way)"}}}};
