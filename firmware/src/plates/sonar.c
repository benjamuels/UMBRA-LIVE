/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* SONAR, a plate module (umb_plate.h): the sounding and the tape of UMBRA OBSCURA, the game: six rings round a centre, a
 * ring a track (track 1 the innermost), and a hand that sweeps them. A note leaves a mark on its track's ring where the
 * hand is, in the track's colour, and sends a ping out from the centre as far as that ring; the marks dim as the hand
 * leaves them and light again as it comes round, a radar's afterglow, so a bar of the song stands on the rings as a
 * pattern. Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone (REPLACE:
 * black under it).
 * Elements: TURN (the hand: 50 a turn in 4 s, 0 still), FADE (how many seconds a mark lasts), SIZE (the rings'),
 * MARKS and PINGS (the notes, at first every one; each as hard as its value; PINGS below 50 ever gentler: dimmer, slower,
 * softer, swelling in and going out only part of the way), RINGS and HAND (their own light).
 * (NEW, API 6) TARGETS: contacts creeping round the rings, a diamond and its wake, lit as the hand finds them (ON, COUNT,
 * DRIFT); SHOCK: the kick's echo, a doubled ring running out from the centre through the rings (ON, REACH, ECHOES). */
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
    uint8_t kv;                                         /* (NEW, API 6) SHOCK: the last kick: how hard .. */
    uint32_t kt;                                        /* .. when */
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
    if (nt->drum && !nt->lane) {                        /* (NEW, API 6) SHOCK: the kick kept */
        m->kt = cx->now;
        m->kv = nt->vel;
    }
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
            life = 600 + cx->k[1] * 110,                /* (SIZE 50: as drawn, 0: 0.4, 99: 1.6; FADE 50: 6 s) */
            g = pl_clamp(50 - cx->k[4], 0, 50), pl = 500 + g * 20, pi = 1 + g * 6, e;   /* (the pings' gentleness) */
    uint32_t t;
    pl_clear(S);
    /* the rings; a ping: a circle out to its ring, 0.5 s; PINGS below 50 gentler (g 0 .. 50): up to 1.5 s, swelling in over
     * up to 0.3 s and held, going out as far as half way at the least, softened by a fainter circle inside it (they are
     * dimmer already: each ping as hard as PINGS) */
    for (t = 0; t < UMB_NTRK; t++) {
        uint32_t age = cx->now - m->pt[t];
        r = (18 + 16 * (int32_t)t) * sc;
        pl_arc(S, a, X, Y, r, 0u, 0xFFFFFFFFu, 96 * cx->k[5] / 50, PL_DEEP);
        if (m->pv[t] && age < (uint32_t)pl) {
            i = (int32_t)age;
            e = m->pv[t] * pl_clamp((pl - i) * (100 + 2 * g) / 100, 0, pl) / pl;   /* (gentle: held longer, then out) */
            if (i < pi)
                e = e * i / pi;
            i = r * (100 - g) / 100 * i / pl;
            pl_arc(S, a, X, Y, i, 0u, 0xFFFFFFFFu, e, a->colour[(t * 3u) % 8u]);
            if (g && i > 3 * 256)
                pl_arc(S, a, X, Y, i - 3 * 256, 0u, 0xFFFFFFFFu, e * g / 80, a->colour[(t * 3u) % 8u]);
        }
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
    /* (NEW, API 6) TARGETS: contacts creeping round (each its own ring and pace, some against the hand), a diamond and a wake
     * of dots behind it; dim until the hand passes, then bright and a small ring round it, fading over the turn */
    if (cx->xk[0][0]) {
        int32_t n = 1 + cx->xk[0][1] * 7 / 99, j;
        for (i = 0; i < n; i++) {
            uint32_t h = pl_hash((uint32_t)i + 31u), sp = (uint32_t)(4 + cx->xk[0][2]) * (1500u + (h >> 28) * 300u),
                     ph = h + (h & 1u ? cx->now * sp : 0u - cx->now * sp);
            int32_t rr = (26 + (int32_t)(h >> 9) % 70) * sc + pl_pol(a, 6 * sc, cx->now * 90000u + h), ex, ey, e,
                    glow = 256 - (int32_t)((m->hand - ph) >> 24), d = 3 * 256;
            ex = X + pl_pol(a, rr, ph + 0x40000000u);
            ey = Y + pl_pol(a, rr, ph);
            e = 70 + ((185 * glow * glow) >> 16);
            pl_line(S, ex - d, ey, ex, ey - d, e, PL_WT);
            pl_line(S, ex, ey - d, ex + d, ey, e, PL_WT);
            pl_line(S, ex + d, ey, ex, ey + d, e, PL_WT);
            pl_line(S, ex, ey + d, ex - d, ey, e, PL_WT);
            if (glow > 200)
                pl_arc(S, a, ex, ey, (6 + (256 - glow) / 6) * 256, 0u, 0xFFFFFFFFu, (glow - 200) * 4, PL_INK);
            for (j = 1; j <= 5; j++) {                  /* its wake: where it was, 0.3 s apart */
                uint32_t q = h + (h & 1u ? (cx->now - 300u * (uint32_t)j) * sp : 0u - (cx->now - 300u * (uint32_t)j) * sp);
                pl_splat(S, X + pl_pol(a, rr, q + 0x40000000u), Y + pl_pol(a, rr, q), e * (6 - j) / 7, PL_INK);
            }
        }
    }
    /* (NEW, API 6) SHOCK: on the kick, a doubled ring runs out from the centre through the rings (REACH: how far, past the
     * last one at the most), ECHOES of it following, thinning as they go */
    if (cx->xk[1][0] && m->kv) {
        uint32_t age = cx->now - m->kt, life = 450u + cx->xk[1][1] * 6u;
        int32_t ne = 1 + cx->xk[1][2] * 3 / 99, j, r1 = (40 + cx->xk[1][1] * 90 / 99) * sc, e;
        for (j = 0; j < ne; j++) {
            uint32_t ag = age - (uint32_t)j * 120u;
            if (age < (uint32_t)j * 120u || ag >= life)
                continue;
            r = 8 * 256 + (int32_t)((r1 - 8 * 256) / 64 * (int32_t)(ag * 64u / life));
            e = (m->kv + 90) * (int32_t)(life - ag) / (int32_t)life * (4 - j) / 4;
            pl_arc(S, a, X, Y, r, 0u, 0xFFFFFFFFu, e, j ? PL_INK : PL_WT);
            pl_arc(S, a, X, Y, r - 3 * 256, 0u, 0xFFFFFFFFu, e / 3, PL_INK);
        }
    }
}

#include "splash/sonar.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_sonar = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plo_t), "SONAR",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 7u, {0, 0, 0},
    {{"TURN", 50}, {"FADE", 50}, {"SIZE", 50}, {"MARKS", 50}, {"PINGS", 50}, {"RINGS", 50}, {"HAND", 50}},
    plo_init, plo_note, plo_frame, plo_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Rings and a sweeping hand find notes", 3u, {0, 0, 0},
    {{"SCAN", {81, 0, 0, 0, 0, 31, 91, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), 0}, {99, 99, 50, 0}},
     {"DEEP", {21, 91, 76, 81, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 5u), UMB_LINK(UMB_SRC_LFO(0), 2u)}, {99, 99, 40, 20}},
     {"CONTACT", {0, 31, 0, 96, 96, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 6u), UMB_LINK(UMB_SRC_BEAT, 2u)}, {99, 99, 50, 15}}},
    58u, {0, 0, 0}, SPL_SONAR,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"TARGETS", 3u, 1u, {0, 0}, {{"ON", 0}, {"COUNT", 40}, {"DRIFT", 30}},
      "Contacts creep round the rings, each a diamond with a wake, lit as the hand sweeps over them",
      {"Targets on or off", "How many contacts (1 to 8)", "How fast they creep round"}},
     {"SHOCK", 3u, 1u, {0, 0}, {{"ON", 0}, {"REACH", 60}, {"ECHOES", 50}},
      "Every kick sends a doubled ring racing out from the centre through the rings, echoes behind it",
      {"Shock rings on or off", "How far the rings run (at most past the last ring)", "How many rings follow each kick (1 to 4)"}}}};
