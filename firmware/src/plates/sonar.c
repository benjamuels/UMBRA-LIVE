/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* SONAR, a plate module (umb_plate.h): the sounding and the tape of UMBRA OBSCURA, the game: six rings round a centre, a
 * ring a track (track 1 the innermost), and a hand that sweeps them. A note leaves a mark on its track's ring where the
 * hand is, in the track's colour, and sends a ping out from the centre as far as that ring; the marks dim as the hand
 * leaves them and light again as it comes round, a radar's afterglow, so a bar of the song stands on the rings as a
 * pattern. Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone (REPLACE:
 * black under it).
 * Elements: TURN (the hand: 50 a turn in 4 s, 0 still), FADE (how many seconds a mark lasts), SIZE (the rings'),
 * MARKS and PINGS (the notes, at first every one; each as hard as its value), RINGS and HAND (their own light). */
#include "plate_lib.h"

#define PLO_NM 48                                       /* the marks kept */
typedef struct {
    uint32_t hand;                                      /* the hand's angle (2^32 a turn) */
    uint32_t mt[PLO_NM];                                /* each mark: when (0: none) .. */
    uint16_t ma[PLO_NM];                                /* .. where on its ring (65536 a turn) */
    uint8_t mr[PLO_NM], mv[PLO_NM];                     /* .. its track, how hard */
    uint32_t pt[UMB_NTRK];                              /* each track's last ping: when .. */
    uint8_t pv[UMB_NTRK];                               /* .. how hard (0: none) */
    uint8_t nm;
} plo_t;

static void plo_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    (void)st;
    (void)cx;
    (void)a;
}
static void plo_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plo_t *m = st;
    uint32_t t = nt->track % UMB_NTRK, i;
    int32_t v;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 3u, nt, 1)) != 0) {
        i = m->nm++ % PLO_NM;
        m->mt[i] = cx->now | 1u;
        m->ma[i] = (uint16_t)(m->hand >> 16);
        m->mr[i] = (uint8_t)t;
        m->mv[i] = (uint8_t)v;
    }
    if ((v = pl_drive(cx, 4u, nt, 1)) != 0) {
        m->pt[t] = cx->now;
        m->pv[t] = (uint8_t)v;
    }
}
static void plo_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plo_t *m = st;
    (void)a;
    m->hand += cx->dt * 21475u * cx->k[0];              /* (TURN 50: a turn in 4 s) */
}
static void plo_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plo_t *m = st;
    int32_t k2 = cx->k[2], sc = 96 + k2 * 160 / 50, X = 120 * 256, Y = (cx->bare ? 120 : 142) * 256, i, r,
            life = 600 + cx->k[1] * 110;                /* (SIZE 50: as drawn, 0: 0.4, 99: 1.6; FADE 50: 6 s) */
    uint32_t t;
    pl_clear(S);
    for (t = 0; t < UMB_NTRK; t++) {                    /* the rings; a ping: a circle out to its ring, 0.5 s */
        uint32_t age = cx->now - m->pt[t];
        r = (18 + 16 * (int32_t)t) * sc;
        pl_arc(S, a, X, Y, r, 0u, 0xFFFFFFFFu, 96 * cx->k[5] / 50, PL_DEEP);
        if (m->pv[t] && age < 500u)
            pl_arc(S, a, X, Y, r * (int32_t)age / 500, 0u, 0xFFFFFFFFu, (m->pv[t] * (int32_t)(500u - age)) / 500, a->colour[(t * 3u) % 8u]);
    }
    pl_line(S, X, Y, X + pl_pol(a, 106 * sc, m->hand + 0x40000000u), Y + pl_pol(a, 106 * sc, m->hand), 110 * cx->k[6] / 50, PL_INK);
    pl_glint(S, X, Y, 200, PL_WT);
    for (i = 0; i < PLO_NM; i++) {                      /* the marks: fading over their life, lit again as the hand passes */
        uint32_t age = cx->now - m->mt[i], ph = (uint32_t)m->ma[i] << 16;
        int32_t e, glow = 256 - (int32_t)(((m->hand - ph) >> 24));   /* (the turn since the hand was there: 256 .. 0) */
        if (!m->mt[i] || age >= (uint32_t)life)
            continue;
        e = (m->mv[i] * (int32_t)((uint32_t)life - age) / life) * (70 + ((186 * glow) >> 8)) >> 8;
        r = (18 + 16 * m->mr[i]) * sc;
        pl_star(S, X + pl_pol(a, r, ph + 0x40000000u), Y + pl_pol(a, r, ph), (2 + (glow >> 6)) * 256, e, a->colour[(m->mr[i] * 3u) % 8u]);
    }
}

UMB_PLATE_HEAD const umb_plate_t umb_plate_sonar = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plo_t), "SONAR",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 7u, {0, 0, 0},
    {{"TURN", 50}, {"FADE", 50}, {"SIZE", 50}, {"MARKS", 50}, {"PINGS", 50}, {"RINGS", 50}, {"HAND", 50}},
    plo_init, plo_note, plo_frame, plo_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Rings and a sweeping hand find notes", 3u, {0, 0, 0},
    {{"SCAN", {81, 0, 0, 0, 0, 31, 91, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), 0}, {99, 99, 50, 0}},
     {"DEEP", {21, 91, 76, 81, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 5u), UMB_LINK(UMB_SRC_LFO(0), 2u)}, {99, 99, 40, 20}},
     {"CONTACT", {0, 31, 0, 96, 96, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 6u), UMB_LINK(UMB_SRC_BEAT, 2u)}, {99, 99, 50, 15}}}};
