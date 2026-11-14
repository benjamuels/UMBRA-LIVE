/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* RAPIDS, a plate module (umb_plate.h): a stream seen from above, by the lines its water draws. Forty points ride the
 * flow from left to right (it meanders: whirls laid over the run, drifting; 20 steps a second) and each leaves its
 * light where it has been, in a field (60 x 60) that only fades: threads, bright at the head. A note sets two points
 * down where its pitch lies (low notes at the foot, high at the top), brighter than the stream's own and its track's
 * colour at the head; the kick is a surge: the stream runs up to three times as fast and brightens; the rest of the kit
 * drop eddies in it, whirls that turn the threads round themselves and die away.
 * Elements: SPEED (the stream's), FADE (how long a thread lasts), TURB (how far it meanders: smooth .. broken), STREAK
 * (the notes that are no drum), SURGE (the kick), EDDY (the rest of the kit), THREAD (the stream's own points).
 * (NEW, API 6) STONES (ON, ROCKS, FOAM): rocks in the stream that the threads part round, foam on their upstream side;
 * each a boulder of its own: its size, its squat, its lean and the lumps of its outline its own dice.
 * (NEW, API 6) LEAVES (ON, LEAVES, SPIN): leaves riding the water, each at its own pace, turning, spun by the kit. */
#include "plate_lib.h"

#define FRP_NP 40                                       /* points at once: the first 24 the stream's own (THREAD 50: 6) */
#define FRP_NO 24
#define FRP_NE 3                                        /* eddies at once */
typedef struct {
    uint8_t t[PL_S];                                    /* the threads' light, a byte a cell */
    uint8_t cv[256];                                    /* its curve: a thread bright along its middle */
    int16_t x[FRP_NP], y[FRP_NP];                       /* each point: where (cells, Q8) .. */
    uint8_t b[FRP_NP], c[FRP_NP];                       /* .. how bright (0: none), whose (its track + 1; 0: the stream's) */
    int16_t ex[FRP_NE], ey[FRP_NE], es[FRP_NE];         /* the eddies: where, how strong (signed: which way round) */
    uint16_t surge;                                     /* the kick's surge (Q8) */
    uint8_t np, ne;
    uint32_t ph, rs, ms;                                /* the meanders' drift, the dice, the steps' clock */
    int16_t lx[6], ly[6];                               /* (NEW, API 6) LEAVES: each one's place (cells, Q8; ly 0: not yet
                                                         * on the water) .. */
    uint16_t la[6];                                     /* .. its heading (65536 a turn) */
    uint16_t lsp;                                       /* .. the kit's spin on them (Q8) */
    uint8_t nl;
} frp_t;
#define FRP_NS(cx) (1 + (cx)->xk[0][1] / 20)            /* (NEW, API 6) STONES: how many (1 .. 5) .. */
static const uint8_t FRP_SP[5][2] = {{16, 24}, {38, 38}, {44, 14}, {24, 46}, {30, 10}};   /* .. where (cells) */
#define frp_sx(k) (FRP_SP[k][0] * 256)
#define frp_sy(k) (FRP_SP[k][1] * 256)
#define FRP_SR 640                                      /* .. their radius about (2.5 cells): each its own (frp_rock) */
/* stone k's half width and half height (cells, Q8: 1.5 .. 3.5 cells), its lean (65536 a turn); its outline's lump at
 * point i of 16 (Q8 of its radius: 0.6 .. 1.3, each the mean of its neighbours': lumpy, round) */
static void frp_rock(uint32_t k, int32_t *rx, int32_t *ry, uint32_t *ln)
{
    uint32_t h = pl_hash(k + 77u);
    *rx = 380 + (int32_t)(h >> 24) * 2;
    *ry = 380 + (int32_t)(h >> 16 & 255u) * 2;
    *ln = h & 0x3FFFu;
}
static int32_t frp_lump(uint32_t k, uint32_t i)
{
    int32_t v = 0, j;
    for (j = -1; j <= 1; j++)
        v += (int32_t)(pl_hash(k * 31u + ((i + (uint32_t)j) & 15u)) >> 26) * (j ? 1 : 2);
    return 150 + v * 3 / 4;
}

/* the meanders at x, y (cells, Q8): added to the run u, v (TURB tb) */
static void frp_mea(const frp_t *s, const umb_api_t *a, int32_t x, int32_t y, int32_t tb, int32_t *u, int32_t *v)
{
    *u += (tb * (pl_sine(a, (uint32_t)x * 419430u + s->ph) >> 8) * (pl_sine(a, (uint32_t)y * 524288u + s->ph * 2u + 0x40000000u) >> 8)) >> 15;
    *v -= (tb * (pl_sine(a, (uint32_t)x * 419430u + s->ph + 0x40000000u) >> 8) * (pl_sine(a, (uint32_t)y * 524288u + s->ph * 2u) >> 8)) >> 15;
}
static void frp_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    frp_t *s = st;
    int32_t i;
    (void)a;
    s->rs = 5119u + cx->now;
    s->ms = cx->now;
    for (i = 0; i < 256; i++)
        s->cv[i] = (uint8_t)pl_clamp(i + ((i * i) >> 7), 0, 255);
    for (i = 0; i < FRP_NO; i++) {                      /* the stream's own, anywhere along it to begin with */
        s->x[i] = (int16_t)(pl_rnd(&s->rs) % 15000u);
        s->y[i] = (int16_t)(512u + pl_rnd(&s->rs) % 14336u);
        s->b[i] = (uint8_t)(70u + pl_rnd(&s->rs) % 50u);
    }
}
static void frp_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    frp_t *s = st;
    int32_t v, j;
    uint32_t i;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > s->surge)   /* SURGE (the kick) */
        s->surge = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* EDDY: a whirl, one way then the other */
        i = s->ne++ % FRP_NE;
        s->ex[i] = (int16_t)(10u * 256u + pl_rnd(&s->rs) % 10240u);
        s->ey[i] = (int16_t)(10u * 256u + pl_rnd(&s->rs) % 10240u);
        s->es[i] = (int16_t)(s->ne & 1u ? v * 4 : -v * 4);
    }
    if (cx->xk[1][0] && nt->drum && nt->lane)           /* (NEW, API 6) LEAVES: the kit spins them */
        s->lsp = (uint16_t)pl_clamp(s->lsp + nt->vel * 12, 0, 4000);
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* STREAK: two points, one behind the other */
        return;
    for (j = 0; j < 2; j++) {
        i = FRP_NO + s->np++ % (FRP_NP - FRP_NO);
        s->x[i] = (int16_t)(768 + pl_fold(nt->mo[0], (int32_t)(nt->track * 1500u) + j * 700, 13824));
        s->y[i] = (int16_t)(768 + pl_fold(nt->mo[1], (256 - pl_pk(nt->note)) * 54, 13824));
        s->b[i] = (uint8_t)(120 + (v >> 1));
        s->c[i] = (uint8_t)(nt->track + 1u);
    }
}
/* a step: the light fades (FADE: 0.3 .. 6 s); each point goes with the flow: the run (SPEED; the surge up to three times
 * it), the meanders (TURB), the eddies round themselves; lays its light down round itself (a ridge along its way);
 * off the right edge the stream's own come in again at the left, the notes' are gone */
static void frp_step(frp_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int32_t k1 = (int32_t)cx->k[1], f = 65536 / (6 + k1 * k1 * 114 / 9801), U = ((25 + (int32_t)cx->k[0] * 5 / 2) * (256 + 2 * s->surge)) >> 8,
            tb = (int32_t)cx->k[2] * 2, no = (int32_t)cx->k[6] * FRP_NO / 198, i, j;
    for (i = 0; i < PL_S; i++)
        s->t[i] = (uint8_t)pl_dec(s->t[i], f);
    s->ph += 3000000u + (uint32_t)U * 9000u;
    for (i = 0; i < FRP_NP; i++) {
        int32_t x = s->x[i], y = s->y[i], u = U, v = 0, b = s->b[i];
        if (i < FRP_NO && (i >= no || !b)) {            /* the stream's own: as many as THREAD asks, each from the left */
            s->b[i] = (uint8_t)(i < no ? 70 + pl_rnd(&s->rs) % 50u : 0u);
            s->x[i] = 0;
            s->y[i] = (int16_t)(512u + pl_rnd(&s->rs) % 14336u);
            continue;
        }
        if (!b)
            continue;
        frp_mea(s, a, x, y, tb, &u, &v);
        for (j = 0; j < FRP_NE; j++)
            if (s->es[j]) {
                int32_t dx = (x - s->ex[j]) >> 4, dy = (y - s->ey[j]) >> 4, d2 = ((dx * dx + dy * dy) >> 8) + 24;
                u -= s->es[j] * dy / d2 >> 3;
                v += s->es[j] * dx / d2 >> 3;
            }
        if (cx->xk[0][0])                               /* (NEW, API 6) STONES: near one, pushed out round it */
            for (j = 0; j < FRP_NS(cx); j++) {
                int32_t dx = x - frp_sx((uint32_t)j), dy = y - frp_sy((uint32_t)j), d, rx, ry;
                uint32_t ln;
                frp_rock((uint32_t)j, &rx, &ry, &ln);  /* (its own shape: the distance as if it were round) */
                dx = dx * FRP_SR / rx;
                dy = dy * FRP_SR / ry;
                if (pl_abs(dx) > 2 * FRP_SR || pl_abs(dy) > 2 * FRP_SR || (d = (int32_t)pl_isqrt((uint32_t)(dx * dx + dy * dy)) + 1) > 2 * FRP_SR)
                    continue;
                u += dx * (2 * FRP_SR - d) / d / 3;
                v += dy * (2 * FRP_SR - d) / d / 3 + (dy < 0 ? -40 : 40);
            }
        x += pl_clamp(u, -250, 250);
        y += pl_clamp(v, -250, 250);
        if (x >= 59 * 256 || x < 0 || y < 0 || y >= 59 * 256) {
            s->b[i] = 0;
            continue;
        }
        s->x[i] = (int16_t)x;
        s->y[i] = (int16_t)y;
        pl_stamp(s->t, x, y, 290, (b * (256 + s->surge)) >> (i < FRP_NO ? 11 : 10));
        if (i >= FRP_NO && !(s->b[i] = (uint8_t)(s->b[i] - 1u)))
            s->c[i] = 0;
    }
    for (j = 0; j < FRP_NE; j++)                        /* the eddies die away (1.5 s) */
        s->es[j] = (int16_t)(s->es[j] - s->es[j] / 26 - (s->es[j] > 0) + (s->es[j] < 0));
    s->surge = pl_dec(s->surge, 8192);                  /* (0.4 s) */
    if (cx->xk[1][0]) {                                 /* (NEW, API 6) LEAVES: with the run and the meanders, at half the
                                                         * pace; turning; off the right, a new one at the left */
        for (i = 0; i < 1 + cx->xk[1][1] / 20; i++) {
            int32_t u = U, v = 0;
            if (!s->ly[i] || s->lx[i] >= 59 * 256) {
                uint32_t h = pl_hash(s->nl++ + 300u), g = pl_hash(h);
                s->lx[i] = (int16_t)(s->ly[i] ? 0 : (h >> 8) % 12000u);
                s->ly[i] = (int16_t)(1024u + (g >> 8) % 13000u);
            }
            frp_mea(s, a, s->lx[i], s->ly[i], tb, &u, &v);
            for (j = 0; cx->xk[0][0] && j < FRP_NS(cx); j++) {   /* (round the stones too) */
                int32_t dx = s->lx[i] - frp_sx((uint32_t)j), dy = s->ly[i] - frp_sy((uint32_t)j);
                if (pl_abs(dx) < 2 * FRP_SR && pl_abs(dy) < 2 * FRP_SR)
                    v += dy < 0 ? -60 : 60;
            }
            s->lx[i] = (int16_t)pl_clamp(s->lx[i] + pl_clamp(u, -250, 250) * (3 + i) / 10, 0, 59 * 256);   /* (each its own pace) */
            s->ly[i] = (int16_t)pl_clamp(s->ly[i] + pl_clamp(v, -250, 250) / 2, 512, 58 * 256);
            s->la[i] = (uint16_t)(s->la[i] + (uint32_t)((cx->xk[1][2] * 12 + s->lsp) * (i & 1 ? 1 : -1)));
        }
        s->lsp = pl_dec(s->lsp, 8192);
    }
}
static void frp_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    frp_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 50u);
    while (n--)
        frp_step(s, cx, a);
}
static void frp_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const frp_t *s = st;
    int32_t i, k;
    pl_fdraw(S, s->t, s->cv, a->pal, 0);
    if (cx->xk[0][0])                                   /* (NEW, API 6) STONES: a boulder's lumpy outline and, inside, the
                                                         * line of its shoulder (its upper half, smaller); foam on the upstream
                                                         * side, whiter with the surge and the level */
        for (k = 0; k < FRP_NS(cx); k++) {
            int32_t X = PL_C2P(frp_sx((uint32_t)k)), Y = PL_C2P(frp_sy((uint32_t)k)), fo = (40 + s->surge / 2 + cx->level / 4) * (20 + cx->xk[0][2]) / 119,
                    rx, ry, px = 0, py = 0, qx = 0, qy = 0, j;
            uint32_t ln;
            frp_rock((uint32_t)k, &rx, &ry, &ln);
            for (j = 0; j <= 16; j++) {
                int32_t l = frp_lump((uint32_t)k, (uint32_t)j), ex = (PL_COS(a, j * 4096) >> 7) * rx >> 6, ey = (PL_SIN(a, j * 4096) >> 7) * ry >> 6,
                        c = PL_COS(a, ln) >> 7, sn = PL_SIN(a, ln) >> 7, ux = ((ex * c - ey * sn) >> 8) * l >> 8, uy = ((ex * sn + ey * c) >> 8) * l >> 8;
                if (j) {
                    pl_line(S, X + px, Y + py, X + ux, Y + uy, 150, PL_INK);
                    if (j > 8)
                        pl_line(S, X + qx, Y + qy - 256, X + ux * 5 / 8, Y + uy * 5 / 8 - 256, 80, PL_INK);
                }
                px = ux;
                py = uy;
                qx = ux * 5 / 8;
                qy = uy * 5 / 8;
            }
            pl_arc(S, a, X, Y, rx * 4 + 256, 0x50000000u, 0x60000000u, fo, PL_WT);
            pl_arc(S, a, X, Y, rx * 4 + 1024 + s->surge * 4, 0x58000000u, 0x50000000u, fo * 2 / 3, PL_WT);
        }
    if (cx->xk[1][0])                                   /* (NEW, API 6) LEAVES: a leaf's outline and its rib, warm */
        for (k = 0; k < 1 + cx->xk[1][1] / 20; k++) {
            int32_t X = PL_C2P(s->lx[k]), Y = PL_C2P(s->ly[k]), cs = PL_COS(a, s->la[k]) >> 7, sn = PL_SIN(a, s->la[k]) >> 7,
                    ax = cs * 5, ay = sn * 5, bx = -sn * 2, by = cs * 2;   /* (half its length along, its half width across) */
            const uint8_t *c = PL_WARM[k & 1];
            if (!s->ly[k])
                continue;
            pl_line(S, X - ax, Y - ay, X + bx, Y + by, 220, c);
            pl_line(S, X + bx, Y + by, X + ax, Y + ay, 220, c);
            pl_line(S, X + ax, Y + ay, X - bx, Y - by, 220, c);
            pl_line(S, X - bx, Y - by, X - ax, Y - ay, 220, c);
            pl_line(S, X - ax * 7 / 5, Y - ay * 7 / 5, X + ax / 2, Y + ay / 2, 160, c);
        }
    for (i = 0; i < FRP_NP; i++)                        /* the heads */
        if (s->b[i])
            pl_splat(S, PL_C2P(s->x[i]), PL_C2P(s->y[i]), s->b[i] + 40, s->c[i] ? pl_ncol(a, s->c[i] - 1u) : PL_WT);
}

#include "splash/rapids.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_rapids = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(frp_t), "RAPIDS",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 7u, {0, 0, 0},
    {{"SPEED", 50}, {"FADE", 50}, {"TURB", 50}, {"STREAK", 50}, {"SURGE", 50}, {"EDDY", 50}, {"THREAD", 50}},
    frp_init, frp_note, frp_frame, frp_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "A stream drawn by the lines of its flow", 3u, {0, 0, 0},
    {{"RUN", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 50, 30}},
     {"BROOK", {21, 86, 16, 31, 0, 31, 81, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 25, 0}},
     {"TORRENT", {91, 31, 96, 96, 96, 91, 100, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 2u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 30, 40}}},
    70u, {0, 0, 0}, SPL_RAPIDS,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"STONES", 3u, 1u, {0, 0}, {{"ON", 0}, {"ROCKS", 50}, {"FOAM", 50}},
      "Rocks stand in the stream; the threads part round them and foam whitens on the kick",
      {"Stones on or off", "How many rocks (1 to 5)", "How white the foam on their upstream side"}},
     {"LEAVES", 3u, 1u, {0, 0}, {{"ON", 0}, {"LEAVES", 40}, {"SPIN", 30}},
      "Autumn leaves ride the water, turning as they go; the drums set them spinning",
      {"Leaves on or off", "How many leaves (1 to 5)", "How fast they turn on their own"}}}};
