/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* BEACON, a plate module (umb_plate.h): the lighthouse of UMBRA OBSCURA, the game, in its line art. A tower on its rock,
 * six windows up it, the lamp room under its roof; the lamp turns, and its beam (a fan of rays) reaches out to one side,
 * shortens as it swings toward you, and comes out again on the other. Line art only: its light is added to the strip, so it
 * lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: TURN (how fast the lamp turns: 0 still), FADE (how long a window or a star stays lit);
 *   STARS   (the user, 2026-11-13) a note puts its instrument's constellation in the sky, high or low and left or right
 *           by its pitch: held, its stars come out one by one, then the lines between them; let go, it fades. Each track
 *           its own shape, a drum's lane its own, the whole of it at once (at first: every note)
 *   BEAM    the beam flares (at first: the kick)
 *   WINDOW  a track's window lights in its colour, track 1's the lowest (at first: every note that is no drum)
 *   LAMP    the lamp flashes, a star (at first: the rest of the kit)
 *   TOWER   the tower's own lines      SEA  the water under it, restless with the level */
#include "plate_lib.h"

#define PLB_NS 10                                       /* the constellations at once */
typedef struct {
    uint8_t tr, note, pat, on;                          /* its track, its note, its shape; still held */
    uint16_t held, lit;                                 /* how long so far (ms), its light (Q8) */
} plb_s_t;
typedef struct {
    plb_s_t sky[PLB_NS];
    uint32_t ang;                                       /* the lamp's turn (2^32 a turn) */
    uint16_t win[UMB_NTRK];                             /* each window's light (Q8) */
    uint16_t flare, lamp;                               /* the beam's flare, the lamp's flash (Q8) */
} plb_t;
#define PLB_X 120                                       /* the tower's middle */
#define PLB_BY 214                                      /* .. its foot, its top (the gallery), the lamp */
#define PLB_TY 96
#define PLB_LY 84
static const uint8_t PLB_RAY[3] = {200, 240, 236};
#define PLB_ST 170                                      /* a star comes out every 170 ms a note is held */
/* the shapes, five stars each (px from the note's place): a track's 0 .. 5, the kit's lanes 6 .. 13 by lane */
static const int8_t PLB_PAT[14][5][2] = {
    {{-14, 4}, {-7, -2}, {0, 0}, {7, -2}, {14, 4}},     /* a bow */
    {{-10, 8}, {0, -10}, {10, 8}, {-10, 8}, {0, 2}},    /* a triangle, its heart */
    {{-16, -4}, {-8, 6}, {0, -4}, {8, 6}, {16, -4}},    /* a W */
    {{0, -12}, {0, 0}, {0, 12}, {-10, 0}, {10, 0}},     /* a cross */
    {{-14, -6}, {-6, -8}, {2, -4}, {8, 4}, {16, 2}},    /* a dipper */
    {{0, -11}, {10, -3}, {6, 9}, {-6, 9}, {-10, -3}},   /* a ring */
    {{-12, 6}, {-4, 2}, {4, 2}, {12, 6}, {0, -8}},      /* KICK: an arch and its keystone */
    {{-8, -8}, {8, 8}, {0, 0}, {-8, 8}, {8, -8}},       /* SNARE: an X */
    {{-6, 0}, {6, 0}, {-3, -6}, {3, -6}, {0, 6}},       /* CLAP */
    {{0, -6}, {4, 0}, {0, 6}, {-4, 0}, {0, -6}},        /* HAT: a diamond */
    {{-12, 0}, {-6, 0}, {0, 0}, {6, 0}, {12, 0}},       /* OPEN HAT: a belt */
    {{-9, 5}, {-3, -5}, {3, 5}, {9, -5}, {15, 5}},      /* TOM: a zigzag */
    {{-3, -3}, {3, 3}, {-3, 3}, {3, -3}, {0, 0}},       /* RIM: a knot */
    {{0, -9}, {3, -3}, {9, 0}, {3, 3}, {0, 9}}};        /* BELL: a sickle */

PL_SMALL static void plb_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    (void)st;
    (void)cx;
    (void)a;
}
PL_SMALL static void plb_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plb_t *m = st;
    int32_t v;
    uint32_t i, b = 0;
    (void)a;
    if (!nt->vel) {                                     /* (let go: its constellation fades) */
        for (i = 0; i < PLB_NS; i++)
            if (m->sky[i].on && m->sky[i].tr == nt->track && m->sky[i].note == nt->note)
                m->sky[i].on = 0;
        return;
    }
    if ((v = pl_drive(cx, 1u, nt, 1)) != 0) {          /* STARS: in the place of the faintest not held */
        for (i = 1; i < PLB_NS; i++)
            if ((m->sky[i].on ? 512u : m->sky[i].lit) < (m->sky[b].on ? 512u : m->sky[b].lit))
                b = i;
        m->sky[b].tr = nt->track;
        m->sky[b].note = nt->note;
        m->sky[b].pat = (uint8_t)(nt->drum ? 6u + nt->lane % 8u : nt->track % 6u);
        m->sky[b].on = (uint8_t)!nt->drum;
        m->sky[b].held = (uint16_t)(nt->drum ? 6 * PLB_ST : 0);   /* (a drum: all of it at once) */
        m->sky[b].lit = (uint16_t)v;
    }
    if ((v = pl_drive(cx, 3u, nt, nt->drum && !nt->lane)) > m->flare)
        m->flare = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) > m->lamp)
        m->lamp = (uint16_t)v;
    if ((v = pl_drive(cx, 4u, nt, !nt->drum)) > m->win[nt->track % UMB_NTRK])
        m->win[nt->track % UMB_NTRK] = (uint16_t)v;
}
PL_SMALL static void plb_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plb_t *m = st;
    int32_t k2 = cx->k[2], f = (int32_t)cx->dt * 65536 / (200 + k2 * k2 * 3 / 5);   /* (FADE 50: 1.7 s) */
    uint32_t t;
    (void)a;
    m->ang += cx->dt * 10737u * cx->k[0];               /* (TURN 50: a turn in 8 s) */
    for (t = 0; t < UMB_NTRK; t++)
        m->win[t] = pl_dec(m->win[t], f);
    for (t = 0; t < PLB_NS; t++) {
        plb_s_t *q = &m->sky[t];
        if (q->on)
            q->held = (uint16_t)(q->held + cx->dt > 8000u ? 8000u : q->held + cx->dt);
        else
            q->lit = pl_dec(q->lit, f);
    }
    m->flare = pl_dec(m->flare, (int32_t)cx->dt * 164);   /* (0.4 s) */
    m->lamp = pl_dec(m->lamp, (int32_t)cx->dt * 262);     /* (0.25 s) */
}
static void plb_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plb_t *m = st;
    int32_t tw = 96 * cx->k[6] / 50, sea = cx->k[7], c = pl_sine(a, m->ang + 0x40000000u), ac = pl_abs(c), dir = c < 0 ? -1 : 1,
            L = (168 * ac) >> 15, j, i, X = PLB_X * 256, y;
    pl_clear(S);
    /* the stars: each constellation as far out as its note has been held, the lines once all five are */
    for (i = 0; i < PLB_NS; i++) {
        const plb_s_t *q = &m->sky[i];
        const uint8_t *tc = a->colour[(q->tr * 3u) % 8u];
        int32_t sx = (24 + (q->note % 12) * 16 + (q->note / 12 % 2) * 8) * 256, sy = (22 + (q->note / 24 % 3) * 18) * 256;
        int32_t px = 0, py = 0;
        for (j = 0; q->lit && j < 5; j++) {
            int32_t e = pl_clamp((q->held - j * PLB_ST) * 256 / PLB_ST + (j ? 0 : 256), 0, 256) * q->lit >> 8,
                    ln = pl_clamp((q->held - 5 * PLB_ST) * 256 / 400, 0, 256) * q->lit >> 9,
                    tx = sx + PLB_PAT[q->pat][j][0] * 384, ty = sy + PLB_PAT[q->pat][j][1] * 384;
            e = e * (420 + (pl_sine(a, cx->now * 60000u + (uint32_t)(i * 5 + j) * 0x2F000000u) >> 9)) >> 8;   /* (twinkling) */
            if (j && ln)
                pl_line(S, px, py, tx, ty, ln, tc);
            if (e)
                pl_glint(S, tx, ty, e > 255 ? 255 : e, j ? tc : PL_WT);
            if (e > 96)
                pl_star(S, tx, ty, (j ? 2 : 4) * 256, e > 255 ? 127 : e >> 1, j ? tc : PL_WT);
            px = tx, py = ty;
        }
    }
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
        pl_line(S, X + dir * 11 * 256, PLB_LY * 256, X + dir * (11 + L) * 256, PLB_LY * 256 + j * L * 18, al, PLB_RAY);
    }
    /* the lamp: brighter as it faces you; its flash a star */
    pl_glint(S, X, PLB_LY * 256, 150 + ((105 * (32768 - ac)) >> 15), PL_WT);
    if (m->lamp)
        pl_star(S, X, PLB_LY * 256, (4 + m->lamp / 20) * 256, m->lamp, PL_WT);
}

#include "splash/beacon.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_beacon = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plb_t), "BEACON",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"TURN", 50}, {"STARS", 50}, {"FADE", 50}, {"BEAM", 50}, {"WINDOW", 50}, {"LAMP", 50}, {"TOWER", 50}, {"SEA", 50}},
    plb_init, plb_note, plb_frame, plb_draw,
    {0, UMB_EL_NOTES, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "A lighthouse, its sky full of the notes' stars", 3u, {0, 0, 0},
    {{"SWEEP", {81, 31, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), 0}, {99, 99, 60, 0}},
     {"FOG", {0, 31, 86, 36, 0, 0, 0, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 7u), UMB_LINK(UMB_SRC_LFO(0), 0u)}, {99, 99, 30, 30}},
     {"SIGNAL", {11, 91, 0, 0, 96, 91, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 3u), UMB_LINK(UMB_SRC_BEAT, 7u)}, {99, 99, 60, 40}}},
    52u, {0, 0, 0}, SPL_BEACON};
