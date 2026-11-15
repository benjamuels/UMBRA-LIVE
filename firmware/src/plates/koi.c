/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* KOI, a plate module (umb_plate.h): fish in a pond, seen from above; made to lie over SEPIA's ink. Each is drawn in
 * line: a pointed head, a body that narrows to a forked tail, two fins, a patch of orange on its back; its tail beats and
 * the beat runs down its body. Left alone they wander. A note is something let fall on the water where its pitch lies
 * (C2 left .. C7 right), its track its row: a ring spreads there, and the nearest fish with nothing to go after turns and
 * darts to it, its line taking the note's colour. The kick frightens them all: each starts aside and bolts a little way.
 * The rest of the kit are rings on the water. Line art only: its light is added to the strip, so it lies on a layer as well as it
 * stands alone (REPLACE: black under it).
 * Elements: SWIM (how fast they go: 0 a slow drift), FADE (how long a fish keeps a note's colour), SIZE (theirs), LURE (the notes that
 * are no drum), FRIGHT (the kick), RIPPLE (the rest of the kit), SHOAL (how many: 1 .. 7), GLOW (their own light).
 * (NEW, API 6) LILIES (ON, PADS, BLOOM): lily pads drifting on the pond, bobbing on the beat, lotus flowers opening
 * with the mix.
 * (NEW, API 6) WAKE (ON, TRAIL, FAN): a V of ripples behind each fish, longer the faster it goes, flaring at a fright. */
#include "plate_lib.h"

#define OKO_N 7                                         /* fish at once */
#define OKO_NR 4                                        /* rings on the water at once */
typedef struct {
    int16_t x[OKO_N], y[OKO_N];                         /* each fish: its head's place (px, Q6) .. */
    uint16_t h[OKO_N], wg[OKO_N];                       /* .. its heading (65536 a turn), where in its tail's beat */
    int16_t lx[OKO_N], ly[OKO_N];                       /* .. the note it is after: its place (px, Q6) .. */
    uint16_t lt[OKO_N];                                 /* .. and the ms left to reach it (0: none, it wanders) */
    uint8_t e[OKO_N], c[OKO_N];                         /* .. the note's light on it, whose (its track) */
    uint32_t rt[OKO_NR];                                /* the rings: since when (0: none) .. */
    uint8_t rx[OKO_NR], ry[OKO_NR], rv[OKO_NR], rc[OKO_NR];   /* .. where (px), how hard, whose (a track; 7: none's) */
    uint32_t wp, rs;                                    /* where they wander (2^32 a round), the dice */
    uint16_t fr;                                        /* the fright (Q8) */
    uint8_t nr;
} oko_t;
static const uint8_t OKO_W[6] = {0, 141, 180, 128, 56, 0};   /* half a fish's width down its back (Q8 of a joint's length) */

PL_SMALL static void oko_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    oko_t *s = st;
    uint32_t i;
    (void)a;
    s->rs = 5501u + cx->now;
    for (i = 0; i < OKO_N; i++) {
        s->x[i] = (int16_t)((20u + pl_rnd(&s->rs) % 200u) * 64u);
        s->y[i] = (int16_t)((40u + pl_rnd(&s->rs) % 180u) * 64u);
        s->h[i] = (uint16_t)pl_rnd(&s->rs);
    }
}
PL_SMALL static void oko_ring(oko_t *s, const umb_lctx_t *cx, int32_t x, int32_t y, int32_t v, uint32_t c)
{
    uint32_t i = s->nr++ % OKO_NR;
    s->rt[i] = cx->now | 1u;
    s->rx[i] = (uint8_t)x;
    s->ry[i] = (uint8_t)y;
    s->rv[i] = (uint8_t)v;
    s->rc[i] = (uint8_t)c;
}
PL_SMALL static void oko_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    oko_t *s = st;
    int32_t v, x, y, i, best = 0, bd = 1 << 30, n = 1 + cx->k[6] * 6 / 99, top = PL_TOP(cx);
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* FRIGHT (the kick): each starts aside, up to a quarter turn */
        for (i = 0; i < n; i++)
            s->h[i] = (uint16_t)(s->h[i] + (int32_t)(pl_rnd(&s->rs) % (uint32_t)(v * 128 + 1)) - v * 64);
        if (v > s->fr)
            s->fr = (uint16_t)v;
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0)   /* RIPPLE */
        oko_ring(s, cx, (int32_t)(12u + pl_rnd(&s->rs) % 216u), top + 12 + (int32_t)(pl_rnd(&s->rs) % (uint32_t)(216 - top)), v, 7u);
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* LURE */
        return;
    x = pl_nx(nt, 16, 224);
    y = pl_ny(nt, top + 20, 220);
    for (i = 0; i < n; i++) {                           /* the nearest fish, one with nothing to go after first */
        int32_t d = pl_abs(s->x[i] / 64 - x) + pl_abs(s->y[i] / 64 - y) + (s->lt[i] ? 400 : 0);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    s->lx[best] = (int16_t)(x * 64);
    s->ly[best] = (int16_t)(y * 64);
    s->lt[best] = 1800;
    s->e[best] = (uint8_t)v;
    s->c[best] = nt->track;
    oko_ring(s, cx, x, y, v, nt->track);
}
PL_SMALL static void oko_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    oko_t *s = st;
    int32_t dt = (int32_t)cx->dt, k0 = cx->k[0], k1 = cx->k[1], n = 1 + cx->k[6] * 6 / 99, i;
    int32_t sp0 = k0 < 50 ? 3 + k0 * 29 / 50 : 12 + k0 * 2 / 5;   /* (SWIM: px a second, 3 at 0 .. 32 at 50 .. 51) */
    s->wp += (uint32_t)dt * (uint32_t)(sp0 * 2300);
    for (i = 0; i < n; i++) {
        int32_t cs = PL_COS(a, s->h[i]) >> 3, sn = PL_SIN(a, s->h[i]) >> 3, tx, ty, cr, dot, sp = sp0, turn = 10 + sp0, mv;
        if (s->lt[i]) {                          /* after a note: it darts */
            tx = s->lx[i];
            ty = s->ly[i];
            sp = sp * 2 + 10 + sp0 * 5 / 8;           /* (SWIM 50: 94 px a second) */
            turn = 110;
            s->lt[i] = (uint16_t)(s->lt[i] > dt ? s->lt[i] - dt : 0);
        } else {                                        /* its own way: after a point that goes round the pond */
            tx = 120 * 64 + pl_pol(a, 96 * 64, s->wp * (2u + (uint32_t)i % 3u) + (uint32_t)i * 0x24924924u);
            ty = (PL_TOP(cx) + 240) * 32 + pl_pol(a, 88 * 64, s->wp * (3u + (uint32_t)i % 2u) + (uint32_t)i * 0x5A5A5A5Au);
        }
        tx -= s->x[i];
        ty -= s->y[i];
        cr = (cs * ty - sn * tx) >> 12;                 /* (how far off to its side, how far ahead: px, Q6) */
        dot = (cs * tx + sn * ty) >> 12;
        if (s->lt[i] && pl_abs(tx) + pl_abs(ty) < 10 * 64)
            s->lt[i] = 0;                               /* (there) */
        s->h[i] = (uint16_t)(s->h[i] + dt * pl_clamp(dot < 0 ? (cr < 0 ? -turn : turn) : cr / 16, -turn, turn));
        mv = (sp + s->fr / 2) * dt * 64 / 1000;         /* (frightened: a burst the way it faces) */
        s->x[i] = (int16_t)pl_clamp(s->x[i] + ((cs * mv) >> 12), -20 * 64, 260 * 64);
        s->y[i] = (int16_t)pl_clamp(s->y[i] + ((sn * mv) >> 12), -20 * 64, 260 * 64);
        s->wg[i] = (uint16_t)(s->wg[i] + dt * (50 + sp * 2));
        s->e[i] = (uint8_t)pl_dec(s->e[i], dt * 65536 / (300 + k1 * k1 * 4 / 5));   /* (FADE 50: 2.3 s) */
    }
    s->fr = pl_dec(s->fr, dt * 262);                    /* (0.25 s) */
}
static void oko_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const oko_t *s = st;
    int32_t L = 640 + cx->k[2] * 18, n = 1 + cx->k[6] * 6 / 99, gl = 30 + cx->k[7] * 2, i, k;   /* (SIZE 50: a fish 30 px) */
    pl_clear(S);
    for (i = 0; i < OKO_NR; i++) {                      /* the rings: widening over 0.7 s */
        uint32_t age = cx->now - s->rt[i];
        if (s->rt[i] && age < 700u)
            pl_arc(S, a, s->rx[i] * 256, s->ry[i] * 256, (3 + (int32_t)age / 28) * 256, 0u, 0xFFFFFFFFu, pl_out(s->rv[i], age, 700u) / 2,
                   s->rc[i] < UMB_NTRK ? pl_ncol(a, s->rc[i]) : PL_WT);
    }
    /* (NEW, API 6) LILIES: pads (a ring with its notch) drifting slowly, each its own turn, swelling on the beat; on some a
     * lotus, its petals open with BLOOM and the mix's level */
    if (cx->xk[0][0])
        for (i = 0; i < 1 + cx->xk[0][1] * 9 / 99; i++) {
            uint32_t h = pl_hash((uint32_t)i + 11u), h2 = (h ^ (h >> 15)) * 0x2C1B3C6Du, ph = h + cx->now * (40000u + (h >> 26) * 4000u);
            int32_t R = (8 + (int32_t)(h >> 29) * 2) * 256 + cx->beat * 3, top = PL_TOP(cx) + 14,
                    X = (int32_t)(((h2 >> 8) % 280u + cx->now / (90u + (h >> 27))) % 280u) * 256 - 20 * 256,
                    Y = (top + (int32_t)((h2 >> 20) % (uint32_t)(226 - top))) * 256 + pl_pol(a, 3 * 256, cx->now * 200000u + h);
            pl_arc(S, a, X, Y, R, ph + 0x0C000000u, 0xE8000000u, 110, PL_INK);
            pl_line(S, X, Y, X + pl_pol(a, R, ph + 0x4C000000u), Y + pl_pol(a, R, ph + 0x0C000000u), 80, PL_INK);
            pl_line(S, X, Y, X + pl_pol(a, R, ph + 0x34000000u), Y + pl_pol(a, R, ph - 0x0C000000u), 80, PL_INK);
            if (h & 0x100u && cx->xk[0][2]) {           /* (its lotus: six petals, opening) */
                int32_t l = (2 + (cx->xk[0][2] * (64 + cx->level) >> 11)) * 256;
                for (k = 0; k < 6; k++) {
                    uint32_t q = ph * 3u + (uint32_t)k * 715827883u;
                    pl_line(S, X, Y, X + pl_pol(a, l, q + 0x40000000u), Y + pl_pol(a, l, q), 200, a->colour[6]);
                }
                pl_dot(S, X >> 8, Y >> 8, 230, PL_WARM[0]);
            }
        }
    for (i = 0; i < n; i++) {
        int32_t X = s->x[i] * 4, Y = s->y[i] * 4, cs = PL_COS(a, s->h[i]) >> 3, sn = PL_SIN(a, s->h[i]) >> 3, px[6], py[6], sx[2], sy[2], e,
                ox, oy;
        uint8_t m[3];
        e = pl_mix(m, PL_WT, pl_ncol(a, s->c[i]), gl, s->e[i]);
        for (k = 0; k < 6; k++) {                       /* its back: six joints from the head, the tail's beat growing down it */
            int32_t w = pl_pol(a, k * L / 3, ((uint32_t)s->wg[i] << 16) - (uint32_t)k * 0x18000000u), al = k * L;
            px[k] = X - ((cs * al) >> 12) - ((sn * w) >> 12);
            py[k] = Y - ((sn * al) >> 12) + ((cs * w) >> 12);
        }
        sx[0] = sx[1] = px[0];
        sy[0] = sy[1] = py[0];
        for (k = 1; k < 7; k++) {                       /* its two sides, from the nose to the tail's root (j: left, right),
                                                         * its fins out and back from the shoulders; past the last joint the
                                                         * tail, a fork along its bend */
            int32_t hw = k < 6 ? (OKO_W[k] * L) >> 8 : L * 5 / 8, j;
            ox = -(sn * hw) >> 12;
            oy = (cs * hw) >> 12;
            for (j = 0; j < 2; j++, ox = -ox, oy = -oy) {
                if (k == 6) {
                    pl_line(S, px[5], py[5], 2 * px[5] - px[4] + ox, 2 * py[5] - py[4] + oy, e, m);
                    continue;
                }
                pl_line(S, sx[j], sy[j], px[k] + ox, py[k] + oy, e, m);
                sx[j] = px[k] + ox;
                sy[j] = py[k] + oy;
                if (k == 1)
                    pl_line(S, sx[j], sy[j], sx[j] + 2 * ox - ((cs * L) >> 13), sy[j] + 2 * oy - ((sn * L) >> 13), e / 2, m);
            }
        }
        pl_dot(S, px[2] >> 8, py[2] >> 8, e, PL_WARM[i & 1]);   /* its patch */
        /* (NEW, API 6) WAKE: from its tail a V of ripples, four pairs of arcs left behind and widening, bright (TRAIL: how
         * far back, FAN: how wide), longer and brighter the faster it goes: after a note, frightened */
        if (cx->xk[1][0]) {
            int32_t len = L * (2 + cx->xk[1][1] / 8) * (3 + (s->lt[i] != 0) * 2 + (s->fr >> 6)) / 6,
                    fan = 60 + cx->xk[1][2] * 4, al = 200 + (s->fr >> 1) + (s->lt[i] ? 60 : 0);
            for (k = 0; k < 4; k++) {                   /* (four ripples, left behind it and widening as they fade: on each
                                                         * side an arc, swept back) */
                int32_t f = (int32_t)(((uint32_t)s->wg[i] >> 7) + (uint32_t)k * 256u) % 1024, d = len * f >> 10,
                        bx = px[5] - ((cs * d) >> 12), by = py[5] - ((sn * d) >> 12), r = L / 4 + ((d * fan) >> 8),
                        a2 = pl_clamp(al, 0, 255) * (1024 - f) >> 10;
                uint32_t hh = (uint32_t)s->h[i] << 16;
                pl_arc(S, a, bx, by, r, hh + 0x34000000u, 0x30000000u, a2, PL_WT);
                pl_arc(S, a, bx, by, r, hh - 0x64000000u, 0x30000000u, a2, PL_WT);
            }
        }
    }
}

#include "splash/koi.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_koi = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(oko_t), "KOI",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"SWIM", 50}, {"FADE", 50}, {"SIZE", 50}, {"LURE", 50}, {"FRIGHT", 50}, {"RIPPLE", 50}, {"SHOAL", 50}, {"GLOW", 50}},
    oko_init, oko_note, oko_frame, oko_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Koi that dart to each note you play", 3u, {0, 0, 0},
    {{"POND", {21, 81, 0, 0, 26, 0, 36, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 7u), 0}, {99, 99, 30, 0}},
     {"FEED", {71, 41, 0, 100, 0, 0, 100, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 4u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 50, 40}},
     {"GHOST", {0, 91, 71, 0, 0, 81, 0, 6}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 7u), 0}, {99, 99, 25, 0}}},
    45u, {0, 0, 0}, SPL_KOI,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"LILIES", 3u, 1u, {0, 0}, {{"ON", 0}, {"PADS", 40}, {"BLOOM", 50}},
      "Lily pads drift on the pond, bobbing on the beat; lotus flowers on some open with the mix",
      {"Lily pads on or off", "How many pads (1 to 10)", "How far the lotus flowers open (0 none)"}},
     {"WAKE", 3u, 1u, {0, 0}, {{"ON", 0}, {"TRAIL", 50}, {"FAN", 40}},
      "Each fish leaves a V of ripples behind it, longer and brighter when it darts or takes fright",
      {"Wakes on or off", "How far back the ripples run", "How wide the V opens"}}}};
