/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* MEDUSA, a plate module (umb_plate.h): jellyfish; made to lie over FATHOM, under its sea. Each is drawn in line: a
 * bell, the line of its rim, a smaller arc inside, five threads trailing under it that sway, each a little behind the
 * last. Each has a depth of its own (DEPTH: at 50 they keep under FATHOM's surface) and sinks toward it, the faster the
 * higher over it, and beats: the bell draws in and is thrown upward, then opens again. Each beats on its own every few
 * seconds, enough to hold its depth. A note is a beat of the one nearest its pitch (C2 left .. C7 right), harder
 * than its own, and its lines take the note's colour; the kick is a beat of all of them at once; the rest of the kit are
 * sparks in the water round them. One that leaves the top comes in again from below. Line art only: its light is added to
 * the strip, so it lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: DEPTH (how deep they keep: 0 the top .. 99 the bottom), FADE (how long one keeps a note's colour), SIZE (theirs), PULSE (the notes that
 * are no drum), THROB (the kick), SPARKS (the rest of the kit), BLOOM (how many: 1 .. 6), TRAIL (the threads' length). */
#include "plate_lib.h"

#define OME_N 6                                         /* jellyfish at once */
#define OME_NS 10                                       /* sparks at once */
typedef struct {
    int16_t x[OME_N], y[OME_N];                         /* each: its rim's middle (px, Q6) .. */
    uint16_t p[OME_N], ph[OME_N];                       /* .. its beat (65535: just struck, falling to 0); where in its own
                                                         * slow round (65536 a round: a beat as it comes round) */
    uint8_t e[OME_N], c[OME_N];                         /* .. the note's light on it, whose (its track) */
    uint32_t st[OME_NS];                                /* the sparks: since when (0: none) .. */
    uint8_t sx[OME_NS], sy[OME_NS], sv[OME_NS];         /* .. where (px), how hard */
    uint32_t rs;
    uint8_t ns;
} ome_t;

static void ome_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ome_t *s = st;
    uint32_t i;
    (void)a;
    s->rs = 4703u + cx->now;
    for (i = 0; i < OME_N; i++) {
        s->x[i] = (int16_t)((20u + (i * 40503u & 0xFFFFu) * 200u / 65536u) * 64u);   /* (spread across, however many there are) */
        s->y[i] = (int16_t)((150u + pl_rnd(&s->rs) % 60u) * 64u);
        s->ph[i] = (uint16_t)pl_rnd(&s->rs);
    }
}
static void ome_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    ome_t *s = st;
    int32_t v, x, i, best = 0, bd = 1 << 30, n = 1 + cx->k[6] * 5 / 99, top = PL_TOP(cx);
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0)   /* THROB (the kick): all of them, as hard as their own beat */
        for (i = 0; i < n; i++)
            if (v * 157 > s->p[i])
                s->p[i] = (uint16_t)(v * 157);
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* SPARKS */
        i = s->ns++ % OME_NS;
        s->st[i] = cx->now | 1u;
        s->sx[i] = (uint8_t)(8u + pl_rnd(&s->rs) % 224u);
        s->sy[i] = (uint8_t)((uint32_t)top + 12u + pl_rnd(&s->rs) % (uint32_t)(220 - top));
        s->sv[i] = (uint8_t)v;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* PULSE: the one nearest the note across */
        return;
    x = pl_nx(nt, 20, 220) * 64;
    for (i = 0; i < n; i++)
        if (pl_abs(s->x[i] - x) < bd) {
            bd = pl_abs(s->x[i] - x);
            best = i;
        }
    if (v * 257 > s->p[best])
        s->p[best] = (uint16_t)(v * 257);
    s->e[best] = (uint8_t)v;
    s->c[best] = nt->track;
}
static void ome_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ome_t *s = st;
    int32_t dt = (int32_t)cx->dt, k1 = cx->k[1], n = 1 + cx->k[6] * 5 / 99, i;
    (void)a;
    for (i = 0; i < n; i++) {
        uint16_t ph = (uint16_t)(s->ph[i] + dt * (14 + i * 5 % 9));   /* (its own beat: every 3 .. 4.7 s) */
        int32_t y, up = pl_clamp(70 + cx->k[0] * 9 / 5 + i * 9, 0, 215) * 64 - s->y[i];   /* (how far over its own depth,
                                                         * DEPTH 50: rows 160 .. 205: px, Q6) */
        if (ph < s->ph[i] && s->p[i] < 40000u)
            s->p[i] = 40000u;
        s->ph[i] = ph;
        /* thrown up by its beat (its own: ~5 px, a note's: up to 14); sinking (3 px a second at its own depth, and as
         * much again for each 3 px over it: a steady kick holds it some 8 px higher, no more) */
        y = s->y[i] - ((s->p[i] * dt * 3) >> 16) + (int32_t)((uint32_t)((19200 + (up > 0 ? up * 100 : 0)) * dt) + pl_rnd(&s->rs) % 100000u) / 100000;
        if (y < (PL_TOP(cx) - 44) * 64) {               /* (out at the top: in again from below, somewhere else) */
            y = 262 * 64;
            s->x[i] = (int16_t)((24u + pl_rnd(&s->rs) % 192u) * 64u);
        }
        s->y[i] = (int16_t)(y > 266 * 64 ? 266 * 64 : y);
        s->p[i] = (uint16_t)(s->p[i] > dt * 109 ? s->p[i] - dt * 109 : 0);   /* (0.6 s) */
        s->e[i] = (uint8_t)pl_dec(s->e[i], dt * 65536 / (300 + k1 * k1 * 4 / 5));   /* (FADE 50: 2.3 s) */
    }
}
static void ome_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const ome_t *s = st;
    int32_t n = 1 + cx->k[6] * 5 / 99, len = (3 + cx->k[7] * 4 / 50) * 256, i, k, t;
    pl_clear(S);
    for (i = 0; i < OME_NS; i++) {                      /* the sparks: 0.8 s, rising a little */
        uint32_t age = cx->now - s->st[i];
        if (s->st[i] && age < 800u)
            pl_glint(S, s->sx[i] * 256, s->sy[i] * 256 - (int32_t)age * 4, pl_out(s->sv[i], age, 800u), PL_WT);
    }
    for (i = 0; i < n; i++) {
        int32_t w = (9 + cx->k[2] * 9 / 50) * (8 + i * 3 % 5) / 10, q = s->p[i] >> 8, rx = w * (256 - ((q * 77) >> 8)), ry = w * (230 + ((q * 40) >> 8)),
                X = s->x[i] * 4 + pl_pol(a, 3 * 256, cx->now * 200000u + (uint32_t)i * 0x3C6EF372u), Y = s->y[i] * 4, e, x0, y0;
        uint8_t m[3];
        if (Y + 5 * len + 1024 < S->y0 * 256 || Y - ry - 512 > (S->y0 + S->h) * 256)
            continue;
        e = pl_mix(m, PL_INK, pl_ncol(a, s->c[i]), 70 + q / 4, s->e[i]);
        x0 = X - rx;                                    /* the bell: half a round over its rim, drawn in by the beat */
        y0 = Y;
        for (k = 1; k <= 12; k++) {
            uint32_t ph = 0x80000000u + (uint32_t)k * 178956970u;
            int32_t x1 = X + pl_pol(a, rx, ph + 0x40000000u), y1 = Y + pl_pol(a, ry, ph);
            pl_line(S, x0, y0, x1, y1, e, m);
            x0 = x1;
            y0 = y1;
        }
        pl_line(S, X - rx, Y, X + rx, Y, e / 2, m);     /* its rim; what is inside it */
        pl_arc(S, a, X, Y - ry / 3, ry / 3, 0x80000000u, 0x80000000u, e / 2, m);
        for (t = 0; t < 5; t++) {                       /* its threads: five joints each, the sway running down them */
            x0 = X + (t - 2) * rx * 2 / 5;
            y0 = Y;
            for (k = 1; k <= 5; k++) {
                int32_t x1 = X + (t - 2) * rx * 2 / 5 +
                             pl_pol(a, k * (90 + q), cx->now * 1500000u - (uint32_t)k * 0x20000000u + (uint32_t)(t + 3 * i) * 0x33333333u),
                        y1 = Y + k * len;
                pl_line(S, x0, y0, x1, y1, e * (7 - k) / 8, m);
                x0 = x1;
                y0 = y1;
            }
        }
    }
}

#include "splash/medusa.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_medusa = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(ome_t), "MEDUSA",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"DEPTH", 50}, {"FADE", 50}, {"SIZE", 50}, {"PULSE", 50}, {"THROB", 50}, {"SPARKS", 50}, {"BLOOM", 50}, {"TRAIL", 50}},
    ome_init, ome_note, ome_frame, ome_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Jellyfish that beat with the notes", 3u, {0, 0, 0},
    {{"BLOOM", {0, 0, 31, 0, 0, 0, 100, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 7u)}, {99, 99, 40, 30}},
     {"ABYSS", {86, 91, 91, 0, 26, 100, 1, 100}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 7u), 0}, {99, 99, 30, 0}},
     {"SURGE", {16, 21, 0, 100, 100, 0, 61, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 4u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 50, 30}}},
    30u, {0, 0, 0}, SPL_MEDUSA};
