/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* BEACON, a plate module (umb_plate.h): the lighthouse of UMBRA OBSCURA, the game, in its line art. A tower on its rock,
 * six windows up it, the lamp room under its roof; the lamp turns all the way round, and its beam (a fan of rays) reaches
 * out to one side, swings toward you (wider and brighter, the lamp flaring into a burst as it faces you), out to the other
 * side, and round behind the tower (narrow and dim, the lamp room hiding its root). Over the sky (the user, 2026-11-15:
 * no more the constellations' shapes and lines) stars strewn as stars are, denser along a band that rises across it and
 * thinner away from it, none over the tower: most faint, a few bright with four short rays, white, warm or cool, each
 * twinkling slowly at a pace of its own (the beat and the level quicken it). Line art only: its light is added to the
 * strip, so it lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: TURN (how fast the lamp turns: 0 still), FADE (how long a window or a star stays lit);
 *   STARS   (the user, 2026-11-13, 2026-11-15) a note lights a scatter of stars in its track's colour, high or low and
 *           left or right by its pitch: held, they come out one by one; let go, they fade. Each track its own scatter, a
 *           drum's lane its own, the whole of it at once (at first: every note)
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
    uint32_t sk;                                        /* the sky's constellations' clock (ms, the beat pushing it) */
} plb_t;
#define PLB_X 120                                       /* the tower's middle */
#define PLB_BY 214                                      /* .. its foot, its top (the gallery), the lamp */
#define PLB_TY 96
#define PLB_LY 84
static const uint8_t PLB_RAY[3] = {200, 240, 236};
static const uint8_t PLB_RAIN[3] = {110, 150, 210};    /* (NEW) the rain's grey-blue */
#define PLB_ST 170                                      /* a star comes out every 170 ms a note is held */
#define PLB_NF 84                                       /* the sky's own stars (some fall on the tower or off the sky: fewer) */
static const uint8_t PLB_SC[3][3] = {{236, 240, 255}, {255, 232, 196}, {196, 214, 255}};   /* a star's light: white, warm, cool */
static uint32_t plb_mix(uint32_t x)                     /* a dice from a number, well stirred (pl_hash's lie in lines) */
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ x >> 16;
}

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
    m->sk += cx->dt * (256u + cx->beat + cx->level / 2u) >> 8;   /* (on the beat, loud: up to twice as fast) */
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
            L = (168 * ac) >> 15, fw = pl_sine(a, m->ang) >> 7, j, i, X = PLB_X * 256, y;
    pl_clear(S);
    /* the sky's own stars (the user, 2026-11-15: elegant, strewn as stars are): half of them along a band rising from the
     * lower left to the upper right (a triangle's spread either side, so they thicken toward its middle), half anywhere in
     * the sky; none over the tower and its lamp room. Each one's brightness its dice cubed (most faint, a few bright: those
     * with four short rays), its light white, warm or cool, twinkling a quarter at most, slowly (2.4 .. 4.8 s) on the
     * sky's clock (the beat and the level quicken it) */
    for (i = 0; i < PLB_NF; i++) {
        uint32_t h = plb_mix((uint32_t)i * 2u + 977u), g = plb_mix(h ^ 0x9E3779B9u), q = h >> 24;
        int32_t x = (int32_t)(h % 236u) + 2, top = PL_TOP(cx) + 4, y,
                b = 16 + (int32_t)(((q * q >> 8) * q >> 8) * 216u >> 8), e;
        if (i & 1)
            y = top + (int32_t)(g % (uint32_t)(196 - top));
        else
            y = 150 - x * 100 / 240 + ((int32_t)(g & 63u) + (int32_t)(g >> 6 & 63u) - 63) * 26 / 63;
        if (y < top || y > 196 || (pl_abs(x - PLB_X) < 30 && y > PLB_TY - 52))
            continue;
        e = b * (224 + (pl_sine(a, m->sk * (900000u + (g >> 22) * 880u) + g) >> 10)) >> 8;
        pl_glint(S, x * 256, y * 256, e, PLB_SC[g >> 30 == 3u ? 0u : g >> 30]);
        if (b > 150)
            pl_star(S, x * 256, y * 256, (b > 200 ? 3 : 2) * 256, e >> 2, PLB_SC[g >> 30 == 3u ? 0u : g >> 30]);
    }
    /* the notes' stars: each a scatter of five round its note's place, its own for each track or lane (pat), as far out as
     * its note has been held, the first white, the rest its track's colour, twinkling */
    for (i = 0; i < PLB_NS; i++) {
        const plb_s_t *q = &m->sky[i];
        const uint8_t *tc = a->colour[(q->tr * 3u) % 8u];
        int32_t sx = (24 + (q->note % 12) * 16 + (q->note / 12 % 2) * 8) * 256, sy = (22 + (q->note / 24 % 3) * 18) * 256;
        for (j = 0; q->lit && j < 5; j++) {
            uint32_t h = plb_mix((uint32_t)q->pat * 8u + (uint32_t)j + 501u);
            int32_t r = j ? (5 + (int32_t)(h >> 28)) * 320 : 0,
                    e = pl_clamp((q->held - j * PLB_ST) * 256 / PLB_ST + (j ? 0 : 256), 0, 256) * q->lit >> 8,
                    tx = sx + pl_pol(a, r, h + 0x40000000u), ty = sy + pl_pol(a, r * 3 / 4, h);
            e = e * (420 + (pl_sine(a, cx->now * 60000u + (uint32_t)(i * 5 + j) * 0x2F000000u) >> 9)) >> 8;   /* (twinkling) */
            if (e)
                pl_glint(S, tx, ty, e > 255 ? 255 : e, j ? tc : PL_WT);
            if (e > 96)
                pl_star(S, tx, ty, (j ? 2 : 3) * 256, e > 255 ? 127 : e >> 1, j ? tc : PL_WT);
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
    /* the beam: a fan of seven rays to the side the lamp faces, as long as it faces that way; flaring. Toward you (fw: 0 ..
     * 256) the fan opens wide and bright; behind the tower (fw below 0) it narrows to a third and dims to under half */
    for (j = -3; j <= 3 && L > 6; j++) {
        int32_t al = ((70 + m->flare / 2) * (80 + ((176 * ac) >> 15)) >> 8) * (5 - pl_abs(j)) / 5;
        al = fw >= 0 ? al * (256 + fw) >> 8 : al * 110 >> 8;
        pl_line(S, X + dir * 11 * 256, PLB_LY * 256, X + dir * (11 + L) * 256, PLB_LY * 256 + j * L * (fw >= 0 ? 18 + fw / 6 : 6), al, PLB_RAY);
    }
    /* the lamp: bright facing you, dim facing away; as it faces you (fw past 0.7) a burst of eight rays and a ring of
     * glow; its flash a star */
    pl_glint(S, X, PLB_LY * 256, 120 + ((135 * fw) >> 8), PL_WT);
    if (fw > 180) {
        int32_t q = (fw - 180) * 256 / 76, r = (6 + q * 34 / 256) * 256;
        for (j = 0; j < 8; j++) {
            uint32_t ph = (uint32_t)j * 0x20000000u + m->ang / 8u;
            pl_line(S, X + pl_pol(a, 2560, ph + 0x40000000u), PLB_LY * 256 + pl_pol(a, 2560, ph), X + pl_pol(a, j & 1 ? r / 2 : r, ph + 0x40000000u),
                    PLB_LY * 256 + pl_pol(a, j & 1 ? r / 2 : r, ph), q * 3 >> 2, PLB_RAY);
        }
        pl_arc(S, a, X, PLB_LY * 256, 1792, 0, ~0u, q, PL_WT);
        pl_arc(S, a, X, PLB_LY * 256, r / 3, 0, ~0u, q >> 2, PLB_RAY);
    }
    if (m->lamp)
        pl_star(S, X, PLB_LY * 256, (4 + m->lamp / 20) * 256, m->lamp, PL_WT);
    /* (NEW, API 6) GULLS: birds wheeling round the lamp, each a flapping V on its own loop; one in the beam's side shines */
    if (cx->xk[0][0]) {
        int32_t n = 1 + cx->xk[0][1] * 11 / 99;
        uint32_t sp = 60000u + cx->xk[0][2] * 9000u;
        for (i = 0; i < n; i++) {
            uint32_t h = pl_hash((uint32_t)i), ph = cx->now * (sp + (h >> 26) * 1500u) + h;
            int32_t rx = (34 + (int32_t)(h >> 24) % 46) * 256, gx = X + pl_pol(a, rx, ph + 0x40000000u),
                    gy = (PLB_LY - 14 - (int32_t)(h >> 20) % 20) * 256 + pl_pol(a, rx / 4, ph),
                    fl = pl_sine(a, cx->now * 1400000u + h) >> 13, lit = (gx - X) * dir > 0 && L > 6 ? 240 : 120;
            pl_line(S, gx - 5 * 256, gy - fl * 64, gx, gy + 256, lit, PL_WT);
            pl_line(S, gx, gy + 256, gx + 5 * 256, gy - fl * 64, lit, PL_WT);
        }
    }
    /* (NEW, API 6) RAIN: slanting streaks across the whole sky, faster with FALL, leaning with SLANT; on the beat they flash */
    if (cx->xk[1][0]) {
        int32_t sl = (cx->xk[1][2] - 50) * 256 / 12, fall = 60 + cx->xk[1][1] * 4, al = 50 + (cx->beat >> 2);
        for (i = 0; i < 46; i++) {
            uint32_t h = pl_hash((uint32_t)i + 77u);
            int32_t x = (int32_t)(h >> 8) % 280 - 20, y = (int32_t)((cx->now * (uint32_t)fall / 256u + (h >> 16)) % 280u) - 20;
            pl_line(S, x * 256, y * 256, x * 256 + sl, (y + 8) * 256, al, PLB_RAIN);
        }
    }
}

#include "splash/beacon.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_beacon = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plb_t), "BEACON",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"TURN", 50}, {"STARS", 50}, {"FADE", 50}, {"BEAM", 50}, {"WINDOW", 50}, {"LAMP", 50}, {"TOWER", 50}, {"SEA", 50}},
    plb_init, plb_note, plb_frame, plb_draw,
    {0, UMB_EL_NOTES, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "A lighthouse, its sky alive with stars", 3u, {0, 0, 0},
    {{"SWEEP", {81, 31, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), 0}, {99, 99, 60, 0}},
     {"FOG", {0, 31, 86, 36, 0, 0, 0, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 7u), UMB_LINK(UMB_SRC_LFO(0), 0u)}, {99, 99, 30, 30}},
     {"SIGNAL", {11, 91, 0, 0, 96, 91, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 3u), UMB_LINK(UMB_SRC_BEAT, 7u)}, {99, 99, 60, 40}}},
    52u, {0, 0, 0}, SPL_BEACON,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"GULLS", 3u, 1u, {0, 0}, {{"ON", 0}, {"FLOCK", 40}, {"WHEEL", 50}},
      "Seabirds wheel round the lamp; the ones on the beam's side catch its light",
      {"Gulls on or off", "How many gulls (1 to 12)", "How fast they circle the tower"}},
     {"RAIN", 3u, 1u, {0, 0}, {{"ON", 0}, {"FALL", 50}, {"SLANT", 65}},
      "Rain streaks across the night, flashing brighter on every beat",
      {"Rain on or off", "How fast the rain falls", "Which way it leans (50 straight down)"}}}};
