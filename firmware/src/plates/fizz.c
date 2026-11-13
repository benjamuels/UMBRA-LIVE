/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* FIZZ, a plate module (umb_plate.h): bubbles going up; made to lie over WAX. Each is a ring with a shine inside it, up
 * and to the left; it rises (the larger the faster), swaying, and bursts at the top: eight short rays flying out. A few
 * rise on their own. A note lets one go from the bottom where its pitch lies (C2 left .. C7 right), in its track's
 * colour, the lower and the harder the larger. The kick bursts the two highest and jolts the rest upward.
 * The rest of the kit let go a thread of fine beads, as in a glass. Line art only: its light is added to the strip, so
 * it lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: RISE (how fast), WOBBLE (how far they sway), SIZE (theirs), BUBBLE (the notes that are no drum), POP (the
 * kick), FIZZ (the rest of the kit), SHINE (the shine's light), SIMMER (how many rise on their own: 0 none). */
#include "plate_lib.h"

#define OFZ_N 14                                        /* bubbles at once */
#define OFZ_NF 12                                       /* beads of fizz at once */
typedef struct {
    int16_t x[OFZ_N], y[OFZ_N];                         /* each bubble: where (px, Q6) .. */
    uint16_t pop[OFZ_N];                                /* .. bursting: the ms left of it (0: whole) */
    uint8_t r[OFZ_N], v[OFZ_N], c[OFZ_N], sd[OFZ_N];    /* .. its size (px; 0: none), how hard, its track (7: none's), its dice */
    int16_t fx[OFZ_NF], fy[OFZ_NF];                     /* each bead: where (px, Q6; fy 0: none) */
    uint32_t rs;
    uint16_t jolt, own;                                 /* the kick's jolt (Q8); the ms to the next bubble of its own */
    uint8_t n, nf;
} ofz_t;

static void ofz_let(ofz_t *s, int32_t x, int32_t r, int32_t v, uint32_t c)   /* a bubble let go at the bottom */
{
    uint32_t i = s->n++ % OFZ_N;
    s->x[i] = (int16_t)(x * 64);
    s->y[i] = (int16_t)((244 + r) * 64);
    s->pop[i] = 0;
    s->r[i] = (uint8_t)r;
    s->v[i] = (uint8_t)v;
    s->c[i] = (uint8_t)c;
    s->sd[i] = (uint8_t)pl_rnd(&s->rs);
}
static void ofz_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ofz_t *s = st;
    (void)a;
    s->rs = 811u + cx->now;
}
static void ofz_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    ofz_t *s = st;
    int32_t v, i, k;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* POP (the kick) */
        if (v > s->jolt)
            s->jolt = (uint16_t)v;
        for (k = 0; k < 2; k++) {
            int32_t hi = -1;
            for (i = 0; i < OFZ_N; i++)
                if (s->r[i] && !s->pop[i] && (hi < 0 || s->y[i] < s->y[hi]))
                    hi = i;
            if (hi >= 0 && s->y[hi] < 200 * 64)
                s->pop[hi] = 300;
        }
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* FIZZ: the snare, the clap four beads, the rest two */
        int32_t x = (int32_t)(10u + pl_rnd(&s->rs) % 220u);
        for (k = nt->lane < 3u ? 4 : 2; k > 0; k--) {
            i = s->nf++ % OFZ_NF;
            s->fx[i] = (int16_t)((x + (int32_t)(pl_rnd(&s->rs) % 7u) - 3) * 64);
            s->fy[i] = (int16_t)((236 + k * 6) * 64);
        }
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* BUBBLE */
        return;
    ofz_let(s, pl_nx(nt, 14, 226), pl_clamp(3 + ((v * (5 + (((256 - pl_pk(nt->note)) * 9) >> 8)) * (10 + cx->k[2])) >> 14), 3, 24), v, nt->track);
}
static void ofz_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ofz_t *s = st;
    int32_t dt = (int32_t)cx->dt, top = PL_TOP(cx) + 4, i;
    (void)a;
    if (cx->k[7]) {                                     /* its own: one every 2.3 s or so (SIMMER 50) */
        if (s->own > dt)
            s->own = (uint16_t)(s->own - dt);
        else {
            ofz_let(s, (int32_t)(14u + pl_rnd(&s->rs) % 212u), (int32_t)(3u + pl_rnd(&s->rs) % 5u), 110, 7u);
            s->own = (uint16_t)(250u + (99u - cx->k[7]) * 40u + pl_rnd(&s->rs) % 500u);
        }
    }
    for (i = 0; i < OFZ_N; i++) {
        int32_t r = s->r[i];
        if (!r)
            continue;
        if (s->pop[i]) {
            if (s->pop[i] > dt)
                s->pop[i] = (uint16_t)(s->pop[i] - dt);
            else
                s->pop[i] = s->r[i] = 0;
            continue;
        }
        s->y[i] = (int16_t)(s->y[i] - (8 + cx->k[0] * (8 + r) / 32) * dt * 64 / 1000 - ((s->jolt * dt) >> 4));   /* (RISE 50: 27 .. 50 px a second) */
        if (s->y[i] < (top + r) * 64)
            s->pop[i] = 300;
    }
    for (i = 0; i < OFZ_NF; i++)                        /* the beads: 80 px a second, each as far as its own height */
        if (s->fy[i]) {
            s->fy[i] = (int16_t)(s->fy[i] - dt * 5);
            if (s->fy[i] < (top + 20 + i * 37 % 110) * 64)
                s->fy[i] = 0;
        }
    s->jolt = pl_dec(s->jolt, dt * 262);                /* (0.25 s) */
}
static void ofz_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const ofz_t *s = st;
    int32_t i, k;
    pl_clear(S);
    for (i = 0; i < OFZ_N; i++) {
        int32_t r = s->r[i], X, Y = s->y[i] * 4, e = 50 + s->v[i] * 3 / 4;
        const uint8_t *col = s->c[i] < UMB_NTRK ? pl_ncol(a, s->c[i]) : PL_INK;
        if (!r || Y + (r + 9) * 256 < S->y0 * 256 || Y - (r + 9) * 256 > (S->y0 + S->h) * 256)
            continue;
        X = s->x[i] * 4 + pl_pol(a, cx->k[1] * r * 3, cx->now * (900000u + s->sd[i] * 4000u) + s->sd[i] * 16843009u);
        if (s->pop[i]) {                                /* bursting: eight rays flying out, going out */
            int32_t r0 = (r + (300 - s->pop[i]) / 20) * 256, r1 = r0 + (2 + r / 3) * 256;
            for (k = 0; k < 8; k++) {
                uint32_t ph = (uint32_t)k * 536870912u + s->sd[i] * 16843009u;
                pl_line(S, X + pl_pol(a, r0, ph + 0x40000000u), Y + pl_pol(a, r0, ph), X + pl_pol(a, r1, ph + 0x40000000u), Y + pl_pol(a, r1, ph),
                        e * s->pop[i] / 300, col);
            }
            continue;
        }
        pl_arc(S, a, X, Y, r * 256, 0u, 0xFFFFFFFFu, e, col);
        if (r > 4)                                      /* its shine: a short arc inside, up and to the left */
            pl_arc(S, a, X, Y, r * 160, 0x90000000u, 0x28000000u, cx->k[6] * 2, PL_WT);
    }
    for (i = 0; i < OFZ_NF; i++)
        if (s->fy[i])
            pl_splat(S, s->fx[i] * 4 + pl_pol(a, 300, cx->now * 6000000u + (uint32_t)i * 0x30000000u), s->fy[i] * 4, 130, PL_WT);
}

#include "splash/fizz.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_fizz = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(ofz_t), "FIZZ",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"RISE", 50}, {"WOBBLE", 50}, {"SIZE", 50}, {"BUBBLE", 50}, {"POP", 50}, {"FIZZ", 50}, {"SHINE", 50}, {"SIMMER", 50}},
    ofz_init, ofz_note, ofz_frame, ofz_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Bubbles going up, a note in each", 3u, {0, 0, 0},
    {{"SODA", {71, 31, 26, 0, 0, 100, 0, 91}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 7u), UMB_LINK(UMB_SRC_BEAT, 6u)}, {99, 99, 40, 30}},
     {"DEEP", {16, 81, 91, 0, 31, 0, 81, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 1u), 0}, {99, 99, 30, 0}},
     {"BOIL", {96, 61, 0, 100, 100, 81, 0, 100}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 0u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 30, 40}}},
    65u, {0, 0, 0}, SPL_FIZZ};
