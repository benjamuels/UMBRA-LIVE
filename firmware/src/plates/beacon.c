/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* BEACON, a plate module (umb_plate.h): the lighthouse of UMBRA OBSCURA, the game, in its line art. A tower on its rock,
 * six windows up it, the lamp room under its roof; the lamp turns, and its beam (a fan of rays) reaches out to one side,
 * shortens as it swings toward you, and comes out again on the other. Line art only: its light is added to the strip, so it
 * lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: TURN (how fast the lamp turns: 0 still), SPREAD (the fan's width), FADE (how long a window stays lit);
 *   BEAM    the beam flares (at first: the kick)
 *   WINDOW  a track's window lights in its colour, track 1's the lowest (at first: every note that is no drum)
 *   LAMP    the lamp flashes, a star (at first: the rest of the kit)
 *   TOWER   the tower's own lines      SEA  the water under it, restless with the level */
#include "plate_lib.h"

typedef struct {
    uint32_t ang;                                       /* the lamp's turn (2^32 a turn) */
    uint16_t win[UMB_NTRK];                             /* each window's light (Q8) */
    uint16_t flare, lamp;                               /* the beam's flare, the lamp's flash (Q8) */
} plb_t;
#define PLB_X 120                                       /* the tower's middle */
#define PLB_BY 214                                      /* .. its foot, its top (the gallery), the lamp */
#define PLB_TY 96
#define PLB_LY 84
static const uint8_t PLB_RAY[3] = {200, 240, 236};

static void plb_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    (void)st;
    (void)cx;
    (void)a;
}
static void plb_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plb_t *m = st;
    int32_t v;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 3u, nt, nt->drum && !nt->lane)) > m->flare)
        m->flare = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) > m->lamp)
        m->lamp = (uint16_t)v;
    if ((v = pl_drive(cx, 4u, nt, !nt->drum)) > m->win[nt->track % UMB_NTRK])
        m->win[nt->track % UMB_NTRK] = (uint16_t)v;
}
static void plb_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plb_t *m = st;
    int32_t k2 = cx->k[2], f = (int32_t)cx->dt * 65536 / (200 + k2 * k2 * 3 / 5);   /* (FADE 50: 1.7 s) */
    uint32_t t;
    (void)a;
    m->ang += cx->dt * 10737u * cx->k[0];               /* (TURN 50: a turn in 8 s) */
    for (t = 0; t < UMB_NTRK; t++)
        m->win[t] = pl_dec(m->win[t], f);
    m->flare = pl_dec(m->flare, (int32_t)cx->dt * 164);   /* (0.4 s) */
    m->lamp = pl_dec(m->lamp, (int32_t)cx->dt * 262);     /* (0.25 s) */
}
static void plb_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plb_t *m = st;
    int32_t tw = 96 * cx->k[6] / 50, sea = cx->k[7], c = pl_sine(a, m->ang + 0x40000000u), ac = pl_abs(c), dir = c < 0 ? -1 : 1,
            L = (168 * ac) >> 15, j, i, X = PLB_X * 256, y;
    pl_clear(S);
    /* the sea: the horizon behind the tower, dashes under it that shimmer (more with the level) */
    pl_line(S, 0, 204 * 256, 239 * 256, 204 * 256, 60 * sea / 50, PL_DEEP);
    for (i = 0; i < 14; i++) {
        uint32_t h = (uint32_t)i * 2654435761u;
        int32_t x = (int32_t)(h >> 8) % 220 + 10, w = 4 + (int32_t)(h >> 28), yy = 210 + (int32_t)((h >> 16) % 26u),
                sh = 110 + ((pl_sine(a, cx->now * (90000u + (h >> 20) * 40u) + h) >> 9) * (64 + cx->level)) / 96;
        if (pl_abs(x - PLB_X) > 34 || yy > PLB_BY + 6)
            pl_line(S, (x - w) * 256, yy * 256, (x + w) * 256, yy * 256, sh * sea / 50, PL_DEEP);
    }
    /* the tower: its sides, the gallery, the lamp room and its roof, the door, the rock */
    pl_line(S, X - 15 * 256, PLB_TY * 256, X - 27 * 256, PLB_BY * 256, tw, PL_INK);
    pl_line(S, X + 15 * 256, PLB_TY * 256, X + 27 * 256, PLB_BY * 256, tw, PL_INK);
    pl_line(S, X - 21 * 256, PLB_TY * 256, X + 21 * 256, PLB_TY * 256, tw, PL_INK);
    pl_line(S, X - 21 * 256, (PLB_TY - 3) * 256, X + 21 * 256, (PLB_TY - 3) * 256, tw >> 1, PL_INK);
    pl_line(S, X - 10 * 256, (PLB_TY - 3) * 256, X - 10 * 256, (PLB_TY - 24) * 256, tw, PL_INK);
    pl_line(S, X + 10 * 256, (PLB_TY - 3) * 256, X + 10 * 256, (PLB_TY - 24) * 256, tw, PL_INK);
    pl_line(S, X - 14 * 256, (PLB_TY - 24) * 256, X + 14 * 256, (PLB_TY - 24) * 256, tw, PL_INK);
    pl_line(S, X - 14 * 256, (PLB_TY - 24) * 256, X, (PLB_TY - 38) * 256, tw, PL_INK);
    pl_line(S, X + 14 * 256, (PLB_TY - 24) * 256, X, (PLB_TY - 38) * 256, tw, PL_INK);
    pl_line(S, X, (PLB_TY - 38) * 256, X, (PLB_TY - 45) * 256, tw, PL_INK);
    pl_line(S, X - 6 * 256, PLB_BY * 256, X - 6 * 256, (PLB_BY - 12) * 256, tw, PL_INK);
    pl_line(S, X + 6 * 256, PLB_BY * 256, X + 6 * 256, (PLB_BY - 12) * 256, tw, PL_INK);
    pl_arc(S, a, X, (PLB_BY - 12) * 256, 6 * 256, 0x80000000u, 0x80000000u, tw, PL_INK);
    pl_line(S, X - 44 * 256, (PLB_BY + 5) * 256, X - 27 * 256, PLB_BY * 256, tw, PL_INK);
    pl_line(S, X + 27 * 256, PLB_BY * 256, X + 50 * 256, (PLB_BY + 6) * 256, tw, PL_INK);
    pl_line(S, X - 27 * 256, PLB_BY * 256, X + 27 * 256, PLB_BY * 256, tw >> 1, PL_INK);
    /* the windows: track 1's the lowest, left and right in turn; lit, filled with its track's colour */
    for (i = 0; i < (int32_t)UMB_NTRK; i++) {
        int32_t wx = (PLB_X + (i & 1 ? 4 : -10)) * 256, wy = (PLB_BY - 30 - i * 16) * 256, e = m->win[i];
        const uint8_t *tc = a->colour[(i * 3) % 8];
        pl_line(S, wx, wy, wx + 6 * 256, wy, tw >> 1, PL_INK);
        pl_line(S, wx, wy + 9 * 256, wx + 6 * 256, wy + 9 * 256, tw >> 1, PL_INK);
        pl_line(S, wx, wy, wx, wy + 9 * 256, tw >> 1, PL_INK);
        pl_line(S, wx + 6 * 256, wy, wx + 6 * 256, wy + 9 * 256, tw >> 1, PL_INK);
        for (y = 1; e && y < 9; y++)
            pl_line(S, wx + 256, wy + y * 256, wx + 5 * 256, wy + y * 256, e, tc);
    }
    /* the beam: a fan of seven rays to the side the lamp faces, as long as it faces that way; flaring */
    for (j = -3; j <= 3 && L > 6; j++) {
        int32_t al = ((70 + m->flare / 2) * (80 + ((176 * ac) >> 15)) >> 8) * (5 - pl_abs(j)) / 5;
        pl_line(S, X + dir * 11 * 256, PLB_LY * 256, X + dir * (11 + L) * 256, PLB_LY * 256 + j * L * (6 + cx->k[1] * 12 / 50), al, PLB_RAY);
    }
    /* the lamp: brighter as it faces you; its flash a star */
    pl_glint(S, X, PLB_LY * 256, 150 + ((105 * (32768 - ac)) >> 15), PL_WT);
    if (m->lamp)
        pl_star(S, X, PLB_LY * 256, (4 + m->lamp / 20) * 256, m->lamp, PL_WT);
}

UMB_PLATE_HEAD const umb_plate_t umb_plate_beacon = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plb_t), "BEACON",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"TURN", 50}, {"SPREAD", 50}, {"FADE", 50}, {"BEAM", 50}, {"WINDOW", 50}, {"LAMP", 50}, {"TOWER", 50}, {"SEA", 50}},
    plb_init, plb_note, plb_frame, plb_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0}};
