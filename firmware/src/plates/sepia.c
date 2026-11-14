/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* SEPIA, a plate module (umb_plate.h): ink let into still water, as paper is marbled. A field of ink (60 x 60) is carried
 * round by two slow eddies that drift through each other (a flow that neither gathers nor thins it: each cell takes what
 * lay a little upstream, 20 steps a second) and thins away. It is seen in rings: the light goes up and down with how much
 * ink there is, so a drop is a set of rings, the flow pulls them into marbling, and as the ink thins the rings draw in
 * and close. A note is a drop where its pitch falls (C2 left .. C7 right), its track its height; the kick stirs the water;
 * the rest of the kit spatters it.
 * Elements: FLOW (how fast the water turns), FADE (how long the ink lasts), RINGS (how many: 0 plain ink), DROP (the
 * notes that are no drum), STIR (the kick), SPLASH (the rest of the kit), EDDY (the eddies' size).
 * (NEW, API 6) PIPETTE: a dropper wandering over the water lets a drop fall on the beat, rings the flow marbles (ON, DRIPS,
 * BLOT); COLOR: a second colour of ink (ON, HUE, SHARE): a share of the drops (SHARE: of every hundred) let in in a
 * colour of HUE's (amber, rose, violet, sea green, gold, orange across 0..99) instead of the water's own; the flow carries
 * which colour each cell holds with its ink, so the two marble into each other and mix where they meet. */
#include "plate_lib.h"

typedef struct {
    uint8_t d[PL_S];                                    /* the ink, a byte a cell */
    uint8_t rp[PL_N], rc[PL_N];                         /* the step's rows as they were: the one above, this one */
    uint8_t cv[256];                                    /* the rings: the light of each amount of ink .. */
    uint8_t kr;                                         /* .. as RINGS was when they were made (+ 1) */
    uint8_t rsv;
    uint16_t stir;                                      /* the kick's stir (Q8) */
    uint32_t ph[4];                                     /* the two eddies' places (2^32 a wave), across and down */
    uint32_t rs, ms;                                    /* the dice, the steps' clock */
    uint8_t lb, bn;                                     /* (NEW, API 6) PIPETTE: the beat as it was, the beats counted */
    uint8_t q[PL_S];                                    /* (NEW, API 6) COLOR: each cell's ink's share of the second colour .. */
    uint8_t qp[PL_N], qc[PL_N];                         /* .. the step's rows as they were */
} fse_t;
/* (NEW, API 6) COLOR: the second colours, HUE across them */
static const uint8_t FSE_HUE[6][3] = {{255, 150, 50}, {255, 80, 120}, {190, 100, 255}, {60, 230, 150}, {255, 215, 70}, {255, 110, 30}};
/* ink amt into the water round X, Y (cells, Q8; where note nt's NOTE MOVEMENT puts it, if a note's) within r; (COLOR on)
 * in the second colour or the first by the dice, each cell's share mixed by how much ink was there and how much came */
static void fse_ink(fse_t *s, const umb_lctx_t *cx, const umb_note_t *nt, int32_t X, int32_t Y, int32_t r, int32_t amt)
{
    uint8_t old[20 * 20];
    int32_t x, y, x0, y0, x1, y1, t;
    if (nt) {
        X = 768 + pl_fold(nt->mo[0], X - 768, 13824);
        Y = 768 + pl_fold(nt->mo[1], Y - 768, 13824);
    }
    x0 = pl_clamp((X - r) >> 8, 0, PL_N - 1);
    y0 = pl_clamp((Y - r) >> 8, 0, PL_N - 1);
    x1 = pl_clamp((X + r + 255) >> 8, 0, PL_N - 1);
    y1 = pl_clamp((Y + r + 255) >> 8, 0, PL_N - 1);
    if (!cx->xk[1][0] || x1 - x0 >= 20 || y1 - y0 >= 20) {
        pl_stamp(s->d, X, Y, r, amt);
        return;
    }
    t = (int32_t)(pl_rnd(&s->rs) % 100u) < cx->xk[1][2] ? 255 : 0;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            old[(y - y0) * 20 + x - x0] = s->d[y * PL_N + x];
    pl_stamp(s->d, X, Y, r, amt);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            int32_t o = old[(y - y0) * 20 + x - x0], n = s->d[y * PL_N + x];
            if (n > o)
                s->q[y * PL_N + x] = (uint8_t)((s->q[y * PL_N + x] * o + t * (n - o)) / n);
        }
}

static void fse_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fse_t *s = st;
    uint32_t i;
    (void)a;
    s->rs = 4099u + cx->now;
    s->ms = cx->now;
    for (i = 0; i < 4u; i++)
        s->ph[i] = pl_rnd(&s->rs);
}
static void fse_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    fse_t *s = st;
    int32_t v, k, i;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > s->stir)   /* STIR (the kick) */
        s->stir = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* SPLASH: the snare, the clap a spatter; the rest a drop */
        int32_t X = (int32_t)(8u * 256u + pl_rnd(&s->rs) % 11264u), Y = (int32_t)(8u * 256u + pl_rnd(&s->rs) % 11264u);
        for (i = nt->lane < 3u ? 5 : 1; i > 0; i--)
            fse_ink(s, cx, nt, X + (int32_t)(pl_rnd(&s->rs) % 2560u) - 1280, Y + (int32_t)(pl_rnd(&s->rs) % 2560u) - 1280,
                        nt->lane < 3u ? 400 : 520, 90 + ((150 * v) >> 8));
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* DROP */
        return;
    k = pl_pk(nt->note);
    fse_ink(s, cx, nt, 768 + k * 54, (9 + 8 * (int32_t)nt->track) * 256, 700 + (((256 - k) * 640) >> 8) + v, 150 + ((105 * v) >> 8));
}
/* a step: each cell takes the ink from where the flow brings it (less than a cell away: the rows as they were), thinned
 * (FADE: 0.4 .. 6 s); the edge cells stay clear. The flow: two eddies' worth of whirls (sin sin: its curl), each drifting */
static void fse_step(fse_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int8_t sx[2][PL_N], cxs[2][PL_N], sy[2][PL_N], cy[2][PL_N];
    int32_t k1 = (int32_t)cx->k[1], n = 8 + k1 * k1 * 112 / 9801, kd = 4096 - 4096 / n, x, y, e, cq = cx->xk[1][0],
            A = pl_clamp((int32_t)cx->k[0] * 2 + ((s->stir * 3) >> 2), 0, 250);
    uint32_t w = 0xFFFFFFFFu / (uint32_t)(30 + (int32_t)cx->k[6] * 60 / 99), sp = 160000u + (uint32_t)A * 10000u;
    s->ph[0] += sp * 7u;
    s->ph[1] -= sp * 5u;
    s->ph[2] -= sp * 11u;
    s->ph[3] += sp * 9u;
    for (e = 0; e < 2; e++, w = w / 8u * 15u)
        for (x = 0; x < PL_N; x++) {
            sx[e][x] = (int8_t)(pl_sine(a, (uint32_t)x * w + s->ph[2 * e]) >> 8);
            cxs[e][x] = (int8_t)(pl_sine(a, (uint32_t)x * w + s->ph[2 * e] + 0x40000000u) >> 8);
            sy[e][x] = (int8_t)(pl_sine(a, (uint32_t)x * w + s->ph[2 * e + 1]) >> 8);
            cy[e][x] = (int8_t)(pl_sine(a, (uint32_t)x * w + s->ph[2 * e + 1] + 0x40000000u) >> 8);
        }
    pl_cpy(s->rp, s->d, PL_N);
    pl_cpy(s->qp, s->q, PL_N);
    for (y = 0; y < PL_N; y++) {
        uint8_t *hr = s->d + y * PL_N, *qr = s->q + y * PL_N;
        const uint8_t *rw[3], *qw[3];
        pl_cpy(s->rc, hr, PL_N);
        pl_cpy(s->qc, qr, PL_N);
        rw[0] = s->rp;
        rw[1] = s->rc;
        rw[2] = y < PL_N - 1 ? hr + PL_N : s->rc;
        qw[0] = s->qp;                                  /* (NEW, API 6) COLOR: the colour goes with the ink */
        qw[1] = s->qc;
        qw[2] = y < PL_N - 1 ? qr + PL_N : s->qc;
        for (x = 0; x < PL_N; x++) {
            int32_t u = (A * (sx[0][x] * cy[0][y] + ((sx[1][x] * cy[1][y] * 3) >> 2))) >> 15,
                    v = (A * (cxs[0][x] * sy[0][y] + ((cxs[1][x] * sy[1][y] * 3) >> 2))) >> 15;
            int32_t X = pl_clamp(x * 256 - u, 0, (PL_N - 1) * 256 - 1), Y = pl_clamp(y * 256 + v, 0, (PL_N - 1) * 256 - 1),
                    x0 = X >> 8, fx = X & 255;
            const uint8_t *r0 = rw[(Y >> 8) - y + 1], *r1 = rw[(Y >> 8) - y + 2];
            int32_t t = r0[x0] + (((r0[x0 + 1] - r0[x0]) * fx) >> 8), b = r1[x0] + (((r1[x0 + 1] - r1[x0]) * fx) >> 8);
            hr[x] = (uint8_t)(((t + (((b - t) * (Y & 255)) >> 8)) * kd) >> 12);
            if (cq) {
                r0 = qw[(Y >> 8) - y + 1];
                r1 = qw[(Y >> 8) - y + 2];
                t = r0[x0] + (((r0[x0 + 1] - r0[x0]) * fx) >> 8);
                b = r1[x0] + (((r1[x0 + 1] - r1[x0]) * fx) >> 8);
                qr[x] = (uint8_t)(t + (((b - t) * (Y & 255)) >> 8));
            }
        }
        hr[0] = hr[PL_N - 1] = 0;
        pl_cpy(s->rp, s->rc, PL_N);
        pl_cpy(s->qp, s->qc, PL_N);
    }
    for (x = 0; x < PL_N; x++)
        s->d[x] = s->d[PL_S - PL_N + x] = 0;
    s->stir = pl_dec(s->stir, 6554);                    /* (0.5 s) */
}
static void fse_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    fse_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 50u), i;
    if (cx->xk[0][0] && cx->beat > s->lb + 60u && (++s->bn & (cx->xk[0][1] < 33 ? 3u : cx->xk[0][1] < 66 ? 1u : 0u)) == 0) {
        /* (NEW, API 6) PIPETTE: on the beat (DRIPS: every 4th, 2nd, every one) a drop where the dropper has wandered */
        int32_t X = 7680 + (pl_sine(a, cx->now * 52000u) >> 7) * 22, Y = 7680 + (pl_sine(a, cx->now * 33000u + 0x40000000u) >> 7) * 22;
        fse_ink(s, cx, 0, X, Y, 500 + cx->xk[0][2] * 14, 120 + cx->level / 2);
    }
    s->lb = cx->beat;
    if (s->kr != cx->k[2] + 1u) {                       /* the rings (RINGS 50: three of them from clear to full) */
        s->kr = (uint8_t)(cx->k[2] + 1u);
        for (i = 0; i < 256u; i++) {
            int32_t e = (int32_t)(i * 3u > 255u ? 255u : i * 3u), r = cx->k[2] ? (pl_sine(a, i * cx->k[2] * 1016801u) + 32768) >> 8 : 255;
            s->cv[i] = (uint8_t)((e * (40 + ((215 * r) >> 8))) >> 8);
        }
    }
    while (n--)
        fse_step(s, cx, a);
}
static void fse_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const fse_t *s = st;
    if (!cx->xk[1][0])
        pl_fdraw(S, s->d, s->cv, a->pal, 0);
    else {                                              /* (NEW, API 6) COLOR: each pixel's light its share of the second
                                                         * colour, the rest the water's own (as pl_wpx, but straight:
                                                         * faint rings keep their colour) */
        uint8_t xi[240], xf[240], C[3];
        int32_t vv[PL_N + 1], qq[PL_N + 1], x, y, k, p = cx->xk[1][1] * 1293 / 100, h = p >> 8, f = p & 255;
        for (k = 0; k < 3; k++)
            C[k] = (uint8_t)(FSE_HUE[h][k] + (((FSE_HUE[h < 5 ? h + 1 : 5][k] - FSE_HUE[h][k]) * f) >> 8));
        pl_cols(S, xi, xf);
        for (y = 0; y < S->h; y++) {
            int32_t py = S->y0 + y, vg = S->vg[y], any = pl_vrow(s->d, py, vv, 255);
            uint16_t *row = S->px + y * S->w;
            pl_vrow(s->q, py, qq, 255);
            for (x = 0; x < S->w; x++) {
                int32_t v = any ? PL_LERP(vv, xi[x], xf[x]) : 0, b = v > 0 ? s->cv[v] : 0, w = (b * PL_LERP(qq, xi[x], xf[x])) >> 8,
                        kk = w * 300;
                row[x] = pl_rgb(a, b - w, (C[0] * kk) >> 16, (C[1] * kk) >> 16, (C[2] * kk) >> 16, vg, PL_D2[(py & 1) * 2 + ((S->x0 + x) & 1)]);
            }
        }
    }
    if (cx->xk[0][0]) {                                 /* (NEW, API 6) PIPETTE: the dropper, a faint ring, bright as it drips */
        int32_t X = 7680 + (pl_sine(a, cx->now * 52000u) >> 7) * 22, Y = 7680 + (pl_sine(a, cx->now * 33000u + 0x40000000u) >> 7) * 22;
        pl_arc(S, a, PL_C2P(X), PL_C2P(Y), (5 * 256) - cx->beat * 4, 0u, 0xFFFFFFFFu, 60 + (cx->beat >> 1), PL_WT);
    }
}

#include "splash/sepia.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_sepia = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(fse_t), "SEPIA",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 7u, {0, 0, 0},
    {{"FLOW", 50}, {"FADE", 45}, {"RINGS", 50}, {"DROP", 50}, {"STIR", 50}, {"SPLASH", 50}, {"EDDY", 50}},
    fse_init, fse_note, fse_frame, fse_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "Ink in water, pulled into marbled rings", 3u, {0, 0, 0},
    {{"MARBLE", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 50, 30}},
     {"STILL", {16, 86, 21, 0, 21, 31, 81, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 6u), 0}, {99, 99, 30, 0}},
     {"SQUALL", {91, 36, 91, 0, 96, 81, 21, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 4u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 60, 40}}},
    30u, {0, 0, 0}, SPL_SEPIA,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"PIPETTE", 3u, 1u, {0, 0}, {{"ON", 0}, {"DRIPS", 50}, {"BLOT", 40}},
      "A dropper wanders over the water and lets a drop fall on the beat, rings for the flow to marble",
      {"Pipette on or off", "How often it drips: every 4th beat, 2nd, every beat", "How big each drop spreads"}},
     {"COLOR", 3u, 1u, {0, 0}, {{"ON", 0}, {"HUE", 50}, {"SHARE", 40}},
      "A second colour of ink: a share of the drops falls in it, and the flow marbles the two together",
      {"Second colour on or off", "Which colour: amber, rose, violet, sea green, gold, orange", "How many of the drops take it"}}}};
