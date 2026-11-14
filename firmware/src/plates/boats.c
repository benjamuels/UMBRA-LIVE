/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* BOATS, a plate module (umb_plate.h): paper boats on a whirlpool; made to lie over GYRE. Each is the folded kind, drawn
 * in line: a hull, a peak, a lamp at its top. The water carries them round, the faster the nearer the middle, and draws
 * them in; at the middle a boat tips over and goes down. Three are the water's own: one that sinks is let in again at
 * the rim. A note lets one in at the rim, where round by its pitch, in its track's colour. The kick is a swell from the
 * middle outwards: a ring, the boats rock and are lifted out a way and let back. The rest of the kit flare the lamps. Seen tilted
 * (TILT): the far side of the round is higher and its boats smaller. Line art only: its light is added to the strip, so
 * it lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: DRIFT (how fast they go round), PULL (how fast they are drawn in), SIZE (theirs), BOATS (the notes that are
 * no drum), SWELL (the kick), LAMPS (the rest of the kit), TILT (0: from straight above), WAKE (their wakes' light).
 * (NEW, API 6) SPIRAL (on/off; ARMS: two .. six; TWIST: how far round each winds): lines of foam winding in to the
 * middle the way the boats go round, trailing like a whirlpool's, turning with the water, brighter on the beat. FISH (on/off; LEAP: how high; SCHOOL: how many a kick): a kick
 * throws fish up out of the water, each an arc and a splash where it falls back. */
#include "plate_lib.h"

#define OBO_N 12                                        /* boats at once .. */
#define OBO_NO 3                                        /* .. the first of them the water's own */
typedef struct {
    uint16_t a[OBO_N];                                  /* each boat: where round (65536 a turn) .. */
    int16_t r[OBO_N];                                   /* .. how far out (px, Q6; 0: none) */
    uint16_t sk[OBO_N];                                 /* .. going down: the ms left of it (0: afloat) */
    uint8_t v[OBO_N], c[OBO_N], sd[OBO_N];              /* .. how hard, its track (7: none's), its own dice */
    uint32_t kt, rs;                                    /* the swell: since when (0: none); the dice */
    uint16_t lamp;                                      /* the lamps' flare (Q8) */
    uint8_t kv, n;                                      /* the swell: how hard; the next boat */
    uint32_t xr, ft[6], sp;                             /* (NEW, API 6) FISH: its own dice; each one since when (0: none);
                                                         * SPIRAL: how far round (Q16 of a turn ..) */
    uint8_t fa[6], fr[6], fd[6], nf;                    /* .. where it leaps from: round, out (px); its dice; the next */
} obo_t;
static const int8_t OBO_P[7][2] = {{-16, 0}, {16, 0}, {9, 8}, {-9, 8}, {-8, 0}, {0, -15}, {8, 0}};   /* a boat: the hull's
                                                         * four corners, the peak's three (16: half its length) */
static const uint8_t OBO_L[6][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}};   /* .. and its lines */
static const int8_t OBO_FP[6][2] = {{10, 0}, {0, -4}, {-7, 0}, {0, 4}, {-12, -4}, {-12, 4}};   /* (NEW, API 6) a fish: nose,
                                                         * back, tail, belly, the fin's two tips (half px) */

static void obo_let(obo_t *s, uint32_t i, uint32_t ang, int32_t v, uint32_t c)   /* a boat in at the rim */
{
    s->a[i] = (uint16_t)ang;
    s->r[i] = 114 * 64;
    s->sk[i] = 0;
    s->v[i] = (uint8_t)v;
    s->c[i] = (uint8_t)c;
    s->sd[i] = (uint8_t)pl_rnd(&s->rs);
}
static int32_t obo_swell(const obo_t *s, const umb_lctx_t *cx) { return s->kt ? pl_out(s->kv, cx->now - s->kt, 480u) : 0; }
static void obo_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    obo_t *s = st;
    uint32_t i;
    (void)a;
    s->rs = 2203u + cx->now;
    s->xr = 4517u + cx->now;                            /* (NEW, API 6) FISH's dice, apart from the plate's own */
    for (i = 0; i < OBO_NO; i++) {
        obo_let(s, i, pl_rnd(&s->rs), 150, 7u);
        s->r[i] = (int16_t)((48u + 30u * i) * 64u);
    }
}
static void obo_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    obo_t *s = st;
    int32_t v;
    (void)a;
    if (!nt->vel)
        return;
    if (cx->xk[1][0] && nt->drum && !nt->lane)          /* (NEW, API 6) FISH: a kick throws up one .. three */
        for (v = 0; v < 1 + cx->xk[1][2] * 3 / 100; v++) {
            uint32_t i = s->nf++ % 6u;
            s->fa[i] = (uint8_t)pl_rnd(&s->xr);
            s->fr[i] = (uint8_t)(36u + pl_rnd(&s->xr) % 70u);
            s->fd[i] = (uint8_t)pl_rnd(&s->xr);
            s->ft[i] = (cx->now - (uint32_t)v * 90u) | 1u;
        }
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* SWELL (the kick) */
        s->kt = cx->now | 1u;
        s->kv = (uint8_t)v;
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) > s->lamp)   /* LAMPS */
        s->lamp = (uint16_t)v;
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* BOATS */
        return;
    obo_let(s, OBO_NO + s->n++ % (OBO_N - OBO_NO), (uint32_t)pl_fold(nt->mo[0], pl_pk(nt->note), 256) * 255u + nt->track * 10923u, v, nt->track);
}
static void obo_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    obo_t *s = st;
    int32_t dt = (int32_t)cx->dt, i;
    (void)a;
    for (i = 0; i < OBO_N; i++) {
        int32_t r = s->r[i];
        if (!r)
            continue;
        s->a[i] = (uint16_t)(s->a[i] + dt * cx->k[0] * 11 / (r >> 6));   /* (DRIFT 50: round in 12 s at the rim, 2 s by the middle) */
        if (s->sk[i]) {                                 /* going down; then gone, or (the water's own) in again */
            if (s->sk[i] > dt)
                s->sk[i] = (uint16_t)(s->sk[i] - dt);
            else if (i < OBO_NO)
                obo_let(s, (uint32_t)i, pl_rnd(&s->rs), 150, 7u);
            else
                s->r[i] = 0;
            continue;
        }
        r -= cx->k[1] ? r * dt * cx->k[1] / 400000 + 1 : 0;   /* (PULL 50: in from the rim in ~17 s) */
        if (r < 14 * 64)
            s->sk[i] = 600;
        s->r[i] = (int16_t)pl_clamp(r, 64, 118 * 64);
    }
    s->lamp = pl_dec(s->lamp, dt * 164);                /* (0.4 s) */
    s->sp += (uint32_t)(dt * cx->k[0] * 3);             /* (NEW, API 6) SPIRAL: it turns with DRIFT (50: round in 7 s) */
}
static void obo_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const obo_t *s = st;
    int32_t X = 120 * 256, Y = (cx->bare ? 120 : 142) * 256, fl = 256 - cx->k[6] * 154 / 99, sz = 5 + cx->k[2] * 5 / 50, sw = obo_swell(s, cx), i,
            k;
    pl_clear(S);
    /* (NEW, API 6) SPIRAL: ARMS lines of foam, each from the rim in to the middle, winding TWIST (a quarter .. one and a
     * half turns) the way the water goes: each arm the path a boat takes in, its inner end ahead the way they go round, so
     * the arms trail like a whirlpool's; the whole turning that way as fast as a boat half way in; faint, brighter on the
     * beat, going out toward the middle */
    if (cx->xk[0][0]) {
        int32_t na = 2 + cx->xk[0][1] * 5 / 100, tw = 64 + cx->xk[0][2] * 320 / 99, al = 110 + cx->beat / 2, j, r;
        for (k = 0; k < na; k++) {
            int32_t ox = 0, oy = 0;
            for (j = 0, r = 114; r >= 10; r -= 2, j++) {
                uint32_t ph = (uint32_t)k * (0xFFFFFFFFu / (uint32_t)na) + (s->sp << 12) + (uint32_t)((114 - r) * tw / 104) * 16777216u;
                int32_t x = X + pl_pol(a, r * 256, ph + 0x40000000u), y = Y + ((pl_pol(a, r * 256, ph) * fl) >> 8);
                if (j)
                    pl_line(S, ox, oy, x, y, (al * (r - 4)) / 110, PL_INK);
                ox = x;
                oy = y;
            }
        }
    }
    if (sw)                                             /* the swell: a ring from the middle to the rim */
        for (k = 0; k < 48; k++) {
            int32_t R = (int32_t)(cx->now - s->kt) * 64;
            pl_splat(S, X + pl_pol(a, R, (uint32_t)k * 89478485u + 0x40000000u), Y + ((pl_pol(a, R, (uint32_t)k * 89478485u) * fl) >> 8), sw / 2, PL_WT);
        }
    for (i = 0; i < OBO_N; i++) {
        uint32_t ph = (uint32_t)s->a[i] << 16, dth;
        int32_t r = s->r[i] * 4 + sw * 12, bx, by, u, t, e, dn = s->sk[i] ? 600 - s->sk[i] : 0, p[7][2];   /* (the swell: out 12 px and back) */
        const uint8_t *col = s->c[i] < UMB_NTRK ? pl_ncol(a, s->c[i]) : PL_WT;
        if (!s->r[i])
            continue;
        bx = X + pl_pol(a, r, ph + 0x40000000u);
        by = Y + ((pl_pol(a, r, ph) * fl) >> 8);
        /* (its scale, Q8 px a 16th of its half length: SIZE; smaller on the far side, and as it nears the middle) */
        u = (sz * (256 + ((pl_pol(a, 256, ph) * (256 - fl)) >> 9)) * (128 + (r >> 8))) >> 12;
        if (by + 20 * u < S->y0 * 256 || by - 20 * u > (S->y0 + S->h) * 256)
            continue;
        t = pl_pol(a, 40 + sw / 2, cx->now * 2500000u + s->sd[i] * 16843009u) + dn * 256 / 500;   /* it rocks; going down, it tips */
        by += pl_pol(a, 256 + sw * 4, cx->now * 1700000u + s->sd[i] * 33686018u) + dn * u / 40;
        e = (60 + s->v[i] * 3 / 4) * (600 - dn) / 600;
        for (k = 0; k < 7; k++) {
            p[k][0] = bx + (OBO_P[k][0] + ((OBO_P[k][1] * t) >> 8)) * u;
            p[k][1] = by + (OBO_P[k][1] - ((OBO_P[k][0] * t) >> 8)) * u;
        }
        for (k = 0; k < 6; k++)
            pl_line(S, p[OBO_L[k][0]][0], p[OBO_L[k][0]][1], p[OBO_L[k][1]][0], p[OBO_L[k][1]][1], e, col);
        pl_splat(S, p[5][0], p[5][1] - 512, (50 + s->lamp / 2) * (600 - dn) / 600, PL_WARM[0]);   /* its lamp */
        if (s->lamp > 40 && !dn)
            pl_star(S, p[5][0], p[5][1] - 512, s->lamp * 6, s->lamp, PL_WARM[0]);
        dth = 3418000000u / (uint32_t)(r >> 8);         /* its wake: four points behind it, 5 px apart */
        for (k = 1; k <= 4 && !dn; k++)
            pl_splat(S, X + pl_pol(a, r, ph - (uint32_t)k * dth + 0x40000000u), Y + ((pl_pol(a, r, ph - (uint32_t)k * dth) * fl) >> 8) + 9 * u,
                     cx->k[7] * 2 * (5 - k) / 5, PL_INK);
    }
    /* (NEW, API 6) FISH: each leaps in an arc (0.7 s; LEAP its height), head along its way, a tail fin behind; a ring of
     * splash where it falls back in */
    for (i = 0; cx->xk[1][0] && i < 6; i++)
        if (s->ft[i] && cx->now - s->ft[i] < 1000u) {
            uint32_t ph = (uint32_t)s->fa[i] << 24;
            int32_t age = (int32_t)(cx->now - s->ft[i]), t = age * 256 / 700, H = 10 + cx->xk[1][1] * 30 / 99, w = s->fd[i] & 1 ? 50 : -50,
                    bx = X + pl_pol(a, s->fr[i] * 256, ph + 0x40000000u), by = Y + ((pl_pol(a, s->fr[i] * 256, ph) * fl) >> 8), fx, fy, ux, uy, n,
                    q[6][2];
            if (age >= 700) {                            /* (the splash: a ring, out and gone) */
                int32_t R = (age - 700) * 40, e = pl_out(150, (uint32_t)(age - 700), 300u);
                pl_arc(S, a, bx + w * 256, by, R + 512, 0u, 0xFFFFFFFFu, e, PL_WT);
                continue;
            }
            fx = bx + w * t;                             /* (across: w px over the leap; up: 4 H t (1 - t)) */
            fy = by - ((4 * H * t * (256 - t)) >> 8);
            ux = w * 4;
            uy = -2 * H * (256 - 2 * t) / 64;                 /* (its slope halved: a fish leaps flatter than it flies) */
            n = (int32_t)pl_isqrt((uint32_t)(ux * ux + uy * uy)) + 1;
            ux = ux * 256 / n;
            uy = uy * 256 / n;
            for (k = 0; k < 6; k++) {
                q[k][0] = fx + (OBO_FP[k][0] * ux - OBO_FP[k][1] * uy) / 2;
                q[k][1] = fy + (OBO_FP[k][0] * uy + OBO_FP[k][1] * ux) / 2;
            }
            for (k = 0; k < 4; k++)
                pl_line(S, q[k][0], q[k][1], q[(k + 1) & 3][0], q[(k + 1) & 3][1], 220, PL_WT);
            pl_line(S, q[2][0], q[2][1], q[4][0], q[4][1], 180, PL_WT);
            pl_line(S, q[2][0], q[2][1], q[5][0], q[5][1], 180, PL_WT);
            pl_line(S, q[4][0], q[4][1], q[5][0], q[5][1], 120, PL_WT);
        }
}

#include "splash/boats.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_boats = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(obo_t), "BOATS",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"DRIFT", 50}, {"PULL", 50}, {"SIZE", 50}, {"BOATS", 50}, {"SWELL", 50}, {"LAMPS", 50}, {"TILT", 50}, {"WAKE", 50}},
    obo_init, obo_note, obo_frame, obo_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Paper boats circling down a whirlpool", 3u, {0, 0, 0},
    {{"REGATTA", {0, 31, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), UMB_LINK(UMB_SRC_LEVEL, 7u)}, {99, 99, 50, 40}},
     {"LANTERN", {21, 16, 71, 0, 26, 100, 81, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 5u), 0}, {99, 99, 60, 0}},
     {"WRECK", {91, 96, 0, 0, 100, 0, 1, 91}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 0u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 40, 30}}},
    25u, {0, 0, 0}, SPL_BOATS,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"SPIRAL", 3u, 1u, {0, 0}, {{"ON", 0}, {"ARMS", 40}, {"TWIST", 50}}, "Lines of foam wind in to the middle, turning with the water, brighter on the beat",
      {"Spiral on or off", "How many arms (two .. six)", "How far round each winds (a quarter .. one and a half turns)"}},
     {"FISH", 3u, 1u, {0, 0}, {{"ON", 0}, {"LEAP", 50}, {"SCHOOL", 30}}, "A kick throws fish up out of the whirlpool; each splashes back in",
      {"Fish on or off", "How high they leap", "How many each kick throws (one .. three)"}}}};
