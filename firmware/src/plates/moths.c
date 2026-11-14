/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* MOTHS, a plate module (umb_plate.h): the Keeper's lamp and its moths, from UMBRA OBSCURA, the game. A lamp in the
 * middle; a note lets a moth in from the dark at the edge (its pitch where round the lamp, its track its colour), and it
 * spirals in to circle the lamp, wings beating, until it tires and fades. The kick scatters them outward; they come back.
 * Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone (REPLACE: black under
 * it).
 * Elements: FLIGHT (how fast they circle), FADE (how long a moth lasts), SIZE (the wings'), MOTHS (the notes that are no
 * drum: a moth each), SCATTR (the kick: they scatter), LAMP (the rest of the kit: the lamp flares), ORBIT (how far out
 * they circle).
 * (NEW, API 6) WEB (ON, SPAN, DEW): a spider's web in a corner, lit by the lamp, trembling with the kick, its dew
 * glinting with the kit; every 2 bars it moves to the other corner of its diagonal (top right, bottom left).
 * (NEW, API 6) WEB2 (ON, SPAN, BARS): a second web on the other diagonal (top left, bottom right), moving every BARS
 * bars (its dew: WEB's DEW). A move with lively music (loud, or many notes of late) is a cut, the web gone from one
 * corner and in the other; with quiet music it fades from corner to corner over 1.5 s. */
#include "plate_lib.h"

#define PLM_N 16                                        /* moths at once */
typedef struct {
    uint32_t t0[PLM_N];                                 /* each moth: since when (0: none) .. */
    uint16_t a[PLM_N];                                  /* .. where round the lamp (65536 a turn) */
    int16_t r[PLM_N];                                   /* .. how far out (px, Q6) */
    uint8_t v[PLM_N], c[PLM_N], sd[PLM_N];              /* .. how hard, its track, its own dice */
    uint16_t lamp, scat;                                /* the lamp's flare, the scatter (Q8) */
    uint8_t n;
    uint8_t pb, wc[2], wx[2];                           /* (NEW, API 6) WEB, WEB2: the beat before, each web's corner (0, 1),
                                                         * how it moved there (1: a cut) .. */
    uint16_t nb, en;                                    /* .. the beats counted, the music's liveliness (the notes of late) */
    uint32_t wt[2];                                     /* .. since when there */
} plm_t;

static void plm_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    (void)st;
    (void)cx;
    (void)a;
}
static void plm_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plm_t *m = st;
    uint32_t i;
    int32_t v;
    (void)a;
    if (!nt->vel)
        return;
    m->en = (uint16_t)(m->en < 60000u ? m->en + 40u : m->en);
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > m->scat)
        m->scat = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) > m->lamp)
        m->lamp = (uint16_t)v;
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))
        return;
    i = m->n++ % PLM_N;
    m->t0[i] = cx->now | 1u;
    m->a[i] = (uint16_t)(pl_fold(nt->mo[0], pl_pk(nt->note), 256) * 255 + nt->track * 10923u);
    m->r[i] = 118 * 64;
    m->v[i] = (uint8_t)v;
    m->c[i] = nt->track;
    m->sd[i] = (uint8_t)(nt->note * 37u + m->n * 101u);
}
static void plm_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plm_t *m = st;
    int32_t dt = (int32_t)cx->dt, life = 1200 + cx->k[1] * 76, i;   /* (FADE 50: 5 s) */
    (void)a;
    for (i = 0; i < PLM_N; i++) {
        int32_t home = (18 + cx->k[6] * 30 / 50 + (m->sd[i] & 15)) * 64, r = m->r[i];   /* (ORBIT 50: 48 .. 63 px) */
        if (!m->t0[i])
            continue;
        if (cx->now - m->t0[i] >= (uint32_t)life) {
            m->t0[i] = 0;
            continue;
        }
        r += (home - r) * dt / 450 + (m->scat * dt * 3 >> 6);   /* (in to its orbit in ~0.5 s; the scatter: ~30 px out) */
        m->r[i] = (int16_t)pl_clamp(r, 8 * 64, 150 * 64);
        m->a[i] = (uint16_t)(m->a[i] + (uint32_t)dt * cx->k[0] * (20u + (m->sd[i] >> 4)) / 50u);   /* (FLIGHT 50: a turn in 2 .. 3 s) */
    }
    m->scat = pl_dec(m->scat, dt * 262);                /* (0.25 s) */
    m->en = pl_dec(m->en, dt * 33);                     /* (2 s: a note a second keeps it near 80) */
    if (cx->beat > m->pb + 64u) {                       /* (NEW, API 6) a beat: WEB moves every 2 bars, WEB2 every BARS */
        m->nb++;
        for (i = 0; i < 2; i++)
            if (m->nb % (uint32_t)(i ? 4 * (1 + cx->xk[1][2] * 7 / 99) : 8) == 0) {
                m->wc[i] ^= 1u;
                m->wt[i] = cx->now;
                m->wx[i] = cx->level + m->en / 4 >= 280;   /* (lively: a cut) */
            }
    }
    m->pb = cx->beat;
    m->lamp = pl_dec(m->lamp, dt * 164);                /* (0.4 s) */
}
/* (NEW, API 6) WEB, WEB2: an orb web hung in corner c (0 top right, 1 top left, 2 bottom right, 3 bottom left: drawn for
 * the top right and mirrored), R wide (SPAN), ten threads out from its hub and a spiral round them, lit al (as the lamp
 * is: the mix, the kit's flare); the kick sets it trembling; dew (WEB's DEW): drops on it that glint */
static void plm_web(const umb_strip_t *S, const umb_api_t *a, const umb_lctx_t *cx, const plm_t *m, uint32_t c, int32_t R, int32_t al)
{
    int32_t mx = c & 1u ? -1 : 1, my = c & 2u ? -1 : 1, HX = mx > 0 ? 236 * 256 - R * 3 / 4 : 4 * 256 + R * 3 / 4,
            HY = my > 0 ? (PL_TOP(cx) + 4) * 256 + R * 3 / 4 : 236 * 256 - R * 3 / 4, tr = pl_pol(a, m->scat * 3, cx->now * 25000000u),
            k, j, pX = 0, pY = 0;
    if (al <= 0)
        return;
    HY += tr;
    for (k = 0; k < 10; k++) {                          /* (its threads, to the corner's edges) */
        uint32_t ph = (uint32_t)k * 429496730u + 0x08000000u;
        int32_t l = ph >= 0xB0000000u || ph < 0x10000000u ? R * 3 : R * 5 / 4;   /* (up and right: to the edges, held there) */
        pl_line(S, HX, HY, HX + mx * pl_pol(a, l, ph + 0x40000000u), HY + my * pl_pol(a, l, ph) - tr, al, PL_INK);
    }
    for (j = 0; j <= 10 * 7; j++) {                     /* (its spiral: seven rounds, a thread to the next, sagging in) */
        uint32_t ph = (uint32_t)(j % 10) * 429496730u + 0x08000000u;
        int32_t r = R * (6 + j) / 80, nX = HX + mx * pl_pol(a, r, ph + 0x40000000u), nY = HY + my * pl_pol(a, r, ph),
                mX = (pX + nX) / 2 - (((pX + nX) / 2 - HX) >> 4), mY = (pY + nY) / 2 - (((pY + nY) / 2 - HY) >> 4);
        if (j) {
            pl_line(S, pX, pY, mX, mY, al * 3 / 4, PL_WT);
            pl_line(S, mX, mY, nX, nY, al * 3 / 4, PL_WT);
        }
        pX = nX;
        pY = nY;
    }
    for (k = 0; k < cx->xk[0][2] * 24 / 99; k++) {      /* (the dew: on the spiral, each glinting in turn, all on the kit's flare) */
        uint32_t h = pl_hash((uint32_t)k + 91u + c * 7u), j2 = (h >> 8) % 70u, ph = j2 % 10u * 429496730u + 0x08000000u;
        int32_t r = R * (6 + (int32_t)j2) / 80, g = pl_sine(a, cx->now * 700000u + h);
        pl_glint(S, HX + mx * pl_pol(a, r, ph + 0x40000000u), HY + my * pl_pol(a, r, ph),
                 (60 + (g > 26000 ? (g - 26000) / 30 : 0) + m->lamp / 2) * al / 90, PL_WT);
    }
}
static void plm_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plm_t *m = st;
    int32_t X = 120 * 256, Y = (cx->bare ? 120 : 142) * 256, i, life = 1200 + cx->k[1] * 76, w = (3 + cx->k[2] * 5 / 50) * 256,
            lg = 90 + cx->level / 3 + m->lamp / 2;
    pl_clear(S);
    pl_arc(S, a, X, Y, 6 * 256, 0u, 0xFFFFFFFFu, lg, PL_WT);   /* the lamp: its glass, its light, eight rays */
    pl_glint(S, X, Y, 150 + lg / 2, PL_WARM[0]);
    for (i = 0; i < 8; i++) {
        uint32_t ph = (uint32_t)i * 536870912u + cx->now * 20000u;
        pl_line(S, X + pl_pol(a, 10 * 256, ph + 0x40000000u), Y + pl_pol(a, 10 * 256, ph), X + pl_pol(a, (13 + lg / 16) * 256, ph + 0x40000000u),
                Y + pl_pol(a, (13 + lg / 16) * 256, ph), lg / 2, PL_WARM[0]);
    }
    for (i = 0; i < PLM_N; i++) {                       /* the moths: a body, two wings beating, its wobble its own */
        uint32_t age = cx->now - m->t0[i], ph = (uint32_t)m->a[i] << 16, hd, fl;
        const uint8_t *tc;
        int32_t r, x, y, e, wa;
        if (!m->t0[i] || age >= (uint32_t)life)
            continue;
        tc = a->colour[(m->c[i] * 3u) % 8u];
        r = m->r[i] * 4 + pl_pol(a, 3 * 256, age * 9000000u + m->sd[i] * 16843009u);
        x = X + pl_pol(a, r, ph + 0x40000000u);
        y = Y + pl_pol(a, r, ph);
        e = m->v[i] * (age * 3u > (uint32_t)life * 2u ? 3 * (int32_t)((uint32_t)life - age) : life) / life;   /* (its last third: fading) */
        e = 70 + e * 3 / 4;
        hd = ph + 0x80000000u;                          /* (its heading: along its circle) */
        fl = age * 60000000u + m->sd[i] * 7777777u;     /* (14 beats a second) */
        wa = 0x18000000 + (pl_sine(a, fl) >> 4) * 0x40000;   /* (the wings' angle from its back: 34 +- 45 degrees) */
        pl_splat(S, x, y, e + 60, PL_WT);
        pl_line(S, x, y, x + pl_pol(a, w, hd + (uint32_t)wa + 0x40000000u), y + pl_pol(a, w, hd + (uint32_t)wa), e, tc);
        pl_line(S, x, y, x + pl_pol(a, w, hd - (uint32_t)wa + 0x40000000u), y + pl_pol(a, w, hd - (uint32_t)wa), e, tc);
    }
    /* (NEW, API 6) WEB, WEB2: each in its corner, cut or fading over from the last (WEB: top right / bottom left, WEB2: top
     * left / bottom right) */
    for (i = 0; i < 2; i++)
        if (cx->xk[i][0]) {
            uint32_t age = cx->now - m->wt[i], c = (uint32_t)i ^ (m->wc[i] ? 3u : 0u);   /* (corner: 1 left, 2 bottom) */
            int32_t R = (24 + cx->xk[i][1] * 40 / 99) * 256, al = 50 + cx->level / 5 + m->lamp / 3,
                    f = m->wx[i] || !m->wt[i] || age >= 1500u ? 256 : (int32_t)age * 256 / 1500;
            plm_web(S, a, cx, m, c, R, al * f >> 8);
            if (f < 256)
                plm_web(S, a, cx, m, c ^ 3u, R, al * (256 - f) >> 8);
        }
}

#include "splash/moths.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_moths = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plm_t), "MOTHS",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 7u, {0, 0, 0},
    {{"FLIGHT", 50}, {"FADE", 50}, {"SIZE", 50}, {"MOTHS", 50}, {"SCATTR", 50}, {"LAMP", 50}, {"ORBIT", 50}},
    plm_init, plm_note, plm_frame, plm_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "A lamp at night, a moth for each note", 3u, {0, 0, 0},
    {{"DUSK", {26, 81, 0, 0, 0, 36, 71, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 6u), UMB_LINK(UMB_SRC_LEVEL, 5u)}, {99, 99, 30, 50}},
     {"SWARM", {86, 0, 0, 96, 61, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), UMB_LINK(UMB_SRC_HIT(0), 4u)}, {99, 99, 60, 40}},
     {"FLICKER", {0, 26, 71, 0, 0, 91, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, -70, 40}}},
    28u, {0, 0, 0}, SPL_MOTHS,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"WEB", 3u, 1u, {0, 0}, {{"ON", 0}, {"SPAN", 50}, {"DEW", 40}},
      "A spider's web lit by the lamp, trembling with the kick; every 2 bars it moves between top right and bottom left, a cut when the music is lively, a slow fade when quiet",
      {"Web on or off", "How wide the web is", "How many dew drops glint on the webs (0 none)"}},
     {"WEB2", 3u, 1u, {0, 0}, {{"ON", 0}, {"SPAN", 50}, {"BARS", 30}},
      "A second web on the other diagonal, moving between top left and bottom right on its own rhythm",
      {"Second web on or off", "How wide the web is", "How many bars between its moves (1 to 8)"}}}};
