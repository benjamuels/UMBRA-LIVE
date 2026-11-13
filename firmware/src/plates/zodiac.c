/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* ZODIAC, a plate module (umb_plate.h): six signs from UMBRA OBSCURA, the game (the bird, the cricket and the dog the
 * Undecided is made of, the crab, the bull, the scorpion: their stars and joins as the game drew them), one a track, wheeling
 * round the pole. At rest the stars are faint and the joins fainter; a note lights one of its track's stars (its pitch
 * which) in the track's colour, and the joins that meet there with it, so a melody draws its sign. Line art only: its light
 * is added to the strip, so it lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: TURN (the sky's wheel: 0 still), FADE (how long a star stays lit), SIZE (the signs'), STARS (the notes, at
 * first every one; as hard as its value), JOINS (the joins' own light), SPREAD (how far from the pole the signs lie). */
#include "plate_lib.h"

#define PLZ_NS 75                                       /* the stars, all six signs' */
#define PLZ_NJ 75                                       /* .. their joins */
static const int8_t PLZ_P[PLZ_NS][2] = {               /* each star from its sign's middle (px at SIZE 100) */
    {-68, -23}, {-50, -31}, {-32, -17}, {-24, 7}, {4, 19}, {46, 21}, {66, 33}, {8, -13}, {28, -47}, {48, -27}, {-12, 35}, {-20, 45},   /* the bird */
    {-44, 4}, {-66, -26}, {-74, -10}, {-18, -2}, {26, 2}, {66, 10}, {36, -16}, {38, -38}, {58, 36}, {-30, 32}, {-4, 34}, {8, -20},   /* the cricket */
    {-72, -14}, {-52, -26}, {-40, -46}, {-30, -10}, {-26, 18}, {-32, 52}, {18, -14}, {48, -10}, {72, -40}, {54, 22}, {48, 52}, {14, 20},   /* the dog */
    {-14, 4}, {0, -4}, {14, 4}, {0, 15}, {-34, -10}, {-42, -30}, {-25, -30}, {34, -10}, {42, -30}, {25, -30}, {-36, 21}, {36, 21}, {-20, 38}, {20, 38},   /* the crab */
    {-56, 2}, {-46, 10}, {-30, -2}, {-12, -6}, {6, -2}, {20, 8}, {32, -8}, {38, -22}, {36, 2}, {46, -8}, {-34, 16}, {4, 16},   /* the bull */
    {-50, 4}, {-48, -8}, {-38, 0}, {-26, -4}, {-14, -6}, {-2, -4}, {8, 2}, {16, 12}, {26, 16}, {36, 10}, {38, -2}, {32, -10}, {26, -14},   /* the scorpion */};
static const uint8_t PLZ_J[PLZ_NJ][2] = {              /* each join: two of its sign's stars */
    {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {2, 7}, {7, 5}, {7, 8}, {8, 9}, {9, 5}, {4, 10}, {10, 11},   /* the bird */
    {0, 1}, {0, 2}, {0, 3}, {3, 4}, {4, 5}, {3, 11}, {11, 6}, {6, 5}, {4, 7}, {7, 8}, {3, 9}, {3, 10},   /* the cricket */
    {0, 1}, {1, 2}, {2, 3}, {1, 3}, {3, 4}, {4, 5}, {3, 6}, {6, 7}, {7, 8}, {7, 9}, {9, 10}, {4, 11}, {11, 7},   /* the dog */
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {0, 4}, {4, 5}, {4, 6}, {2, 7}, {7, 8}, {7, 9}, {0, 10}, {2, 11}, {3, 12}, {3, 13},   /* the crab */
    {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {5, 8}, {8, 9}, {2, 10}, {4, 11},   /* the bull */
    {0, 2}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 10}, {10, 11}, {11, 12},   /* the scorpion */
};
static const uint8_t PLZ_PO[UMB_NTRK + 1] = {0, 12, 24, 36, 50, 62, 75}, PLZ_JO[UMB_NTRK + 1] = {0, 13, 25, 38, 52, 63, 75};   /* each sign's first star, first join */
typedef struct {
    uint32_t ang;                                       /* the sky's turn (2^32 a turn) */
    int16_t x[PLZ_NS], y[PLZ_NS];                       /* this frame's: each star on the screen (px, Q4) */
    uint8_t e[PLZ_NS];                                  /* .. its light (0..255) */
} plz_t;

static void plz_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    (void)st;
    (void)cx;
    (void)a;
}
static void plz_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plz_t *m = st;
    uint32_t t = nt->track % UMB_NTRK, n = PLZ_PO[t + 1u] - PLZ_PO[t], i;
    int32_t v = nt->vel ? pl_drive(cx, 3u, nt, 1) : 0;
    (void)a;
    if (!v)
        return;
    i = PLZ_PO[t] + (nt->drum ? nt->lane * 5u : nt->note) % n;
    if (v > m->e[i])
        m->e[i] = (uint8_t)v;
}
static void plz_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plz_t *m = st;
    int32_t k1 = cx->k[1], f = (int32_t)cx->dt * 65536 / (300 + k1 * k1), sc = 13 + cx->k[2] * 19 / 50,   /* (FADE 50: 2.8 s; */
            rr = (20 + cx->k[5]) * 256, cy = cx->bare ? 120 : 142;                                     /* SIZE 50: 0.5) */
    uint32_t t, i;
    m->ang += cx->dt * 1432u * cx->k[0];                /* (TURN 30: a turn in 100 s) */
    for (t = 0; t < UMB_NTRK; t++) {
        uint32_t ph = m->ang + t * 715827883u;
        int32_t co = pl_sine(a, ph + 0x40000000u) >> 3, si = pl_sine(a, ph) >> 3,
                ox = 120 * 256 + ((rr >> 2) * co >> 10), oy = cy * 256 + ((rr >> 2) * si >> 10);
        for (i = PLZ_PO[t]; i < PLZ_PO[t + 1u]; i++) {  /* (the sign turned with the sky: it wheels, feet to the pole) */
            int32_t px = PLZ_P[i][0] * sc, py = PLZ_P[i][1] * sc;   /* (px, Q6) */
            m->x[i] = (int16_t)((ox + ((-px * si - py * co) >> 10)) >> 4);
            m->y[i] = (int16_t)((oy + ((px * co - py * si) >> 10)) >> 4);
            m->e[i] = (uint8_t)pl_dec(m->e[i], f);
        }
    }
}
static void plz_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plz_t *m = st;
    uint32_t t, i;
    pl_clear(S);
    for (t = 0; t < UMB_NTRK && t < cx->tracks; t++) {
        const uint8_t *tc = a->colour[(t * 3u) % 8u];
        for (i = PLZ_JO[t]; i < PLZ_JO[t + 1u]; i++) {
            uint32_t p = PLZ_PO[t] + PLZ_J[i][0], q = PLZ_PO[t] + PLZ_J[i][1];
            int32_t e = m->e[p] > m->e[q] ? m->e[p] : m->e[q];
            pl_line(S, m->x[p] * 16, m->y[p] * 16, m->x[q] * 16, m->y[q] * 16, (e ? 30 + e / 2 : 70) * cx->k[4] / 50, e ? tc : PL_DEEP);
        }
        for (i = PLZ_PO[t]; i < PLZ_PO[t + 1u]; i++) {
            int32_t e = m->e[i];
            if (e)
                pl_star(S, m->x[i] * 16, m->y[i] * 16, (1 + e / 50) * 256, 60 + e * 3 / 4, tc);
            else
                pl_glint(S, m->x[i] * 16, m->y[i] * 16, 110, PL_INK);
        }
    }
}

#include "splash/zodiac.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_zodiac = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plz_t), "ZODIAC",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 6u, {0, 0, 0},
    {{"TURN", 30}, {"FADE", 50}, {"SIZE", 60}, {"STARS", 50}, {"JOINS", 50}, {"SPREAD", 55}},
    plz_init, plz_note, plz_frame, plz_draw,
    {0, 0, 0, UMB_EL_NOTES, 0, 0},
    "Six constellations, a sign a track", 3u, {0, 0, 0},
    {{"WHEEL", {76, 0, 0, 0, 0, 81, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), 0}, {99, 99, 50, 0}},
     {"CHART", {6, 0, 76, 36, 96, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 3u), 0}, {99, 99, 50, 0}},
     {"TWINKLE", {0, 26, 0, 96, 11, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 2u), UMB_LINK(UMB_SRC_LFO(0), 5u)}, {99, 99, 20, 30}}},
    32u, {0, 0, 0}, SPL_ZODIAC};
