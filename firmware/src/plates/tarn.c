/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* TARN as a plate module (umb_plate.h; moved out of ui_umbra_field.c unchanged, pixel for pixel): the eclipse in dark
 * water. A ripple tank (48 x 48, 24 steps a second) bends the reflection (a pixel offset, no resampling) and its stars;
 * the crests catch a teal sheen; it stills exactly (a dead band). A note a drop (pitch across, track down), the kick a heavy
 * one by the moon, the snare one, the hats a star's glint. Controls: DRAG, BEND, SIZE (the reflection nearer or further). */
#include "plate_lib.h"

#define PLT_N 48                                        /* the water 48 x 48 cells of 5 x 5 px */
#define PLT_RX 120                                      /* the reflection's centre (px) and its moon's radius */
#define PLT_RY 126
#define PLT_RR 30
static const uint8_t PLT_SH[3] = {70, 150, 160};        /* the water's sheen */
typedef struct {
    int16_t h[2][PLT_N * PLT_N];                        /* its height, now and a step before (swapped) */
    uint8_t rl[1024];                                   /* the reflection by r^2 / 32 */
    int16_t gr[2][PLT_N * 2];                           /* two rows of slopes (by parity), x and y */
    int16_t grj[2];
    uint8_t cur;                                        /* which of h is now */
    uint8_t tx[8], ty[8], tv[8], nt;                    /* stars glinting (the hats) */
    uint32_t tt[8];
    uint32_t rs, ms;                                    /* the dice, the steps' clock */
} plt_t;

static void plt_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plt_t *s = st;
    int32_t q;
    s->rs = 218u + cx->now;
    s->ms = cx->now;
    for (q = 0; q < 1024; q++) {                        /* the eclipse by r^2: the disc, the corona (r^-2.6), the limb's ring */
        int32_t r16 = (int32_t)pl_isqrt((uint32_t)(q * 32 + 16) << 8), r = r16 >> 4, v;
        if (r16 < PLT_RR * 16 - 8) {
            s->rl[q] = 6;
            continue;
        }
        v = (179 * pl_exp(a, ((a->log[(r > 175 ? 175 : r < 16 ? 16 : r) - 16] - a->log[PLT_RR - 16]) * 83) >> 9) +
             230 * pl_exp(a, ((r16 - PLT_RR * 16) * 20) >> 4)) >> 8;
        v = (v * pl_clamp((r16 - PLT_RR * 16 + 8) * 16, 0, 256)) >> 8;
        s->rl[q] = (uint8_t)pl_clamp(((v - 15) * 272) >> 8, 0, 255);   /* (its faint tail taken off) */
    }
    s->grj[0] = s->grj[1] = -99;
}
/* a drop at cx, cy (cells, Q8), r (cells, Q8), amp deep */
static void plt_drop(plt_t *s, int32_t cx, int32_t cy, int32_t r, int32_t amp)
{
    int16_t *h = s->h[s->cur];
    int32_t x, y, r2 = (r * r) >> 8;
    for (y = pl_clamp((cy - r) >> 8, 1, PLT_N - 2); y <= pl_clamp((cy + r + 255) >> 8, 1, PLT_N - 2); y++)
        for (x = pl_clamp((cx - r) >> 8, 1, PLT_N - 2); x <= pl_clamp((cx + r + 255) >> 8, 1, PLT_N - 2); x++) {
            int32_t dx = x * 256 - cx, dy = y * 256 - cy, d2 = (dx * dx + dy * dy) >> 8;
            if (d2 <= r2)
                h[y * PLT_N + x] = (int16_t)pl_clamp(h[y * PLT_N + x] - amp * (r2 - d2) / (r2 + 1), -32000, 32000);
        }
}
/* a drop where note n's track has moved it (NOTE MOVEMENT: the water 3 .. 45 cells either way) */
static void plt_at(plt_t *s, const umb_note_t *n, int32_t x, int32_t y, int32_t r, int32_t amp)
{
    plt_drop(s, 768 + pl_fold(n->mo[0], x - 768, 10752), 768 + pl_fold(n->mo[1], y - 768, 10752), r, amp);
}
static void plt_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plt_t *s = st;
    int32_t k, v = nt->vel ? (int32_t)nt->vel * 2 + 1 : 0;
    (void)a;
    if (!v)
        return;
    if (nt->drum) {
        if (!nt->lane) {                                /* the kick: a heavy drop by the moon */
            int32_t X = (int32_t)(20u * 256u + pl_rnd(&s->rs) % 2048u);
            plt_at(s, nt, X, (int32_t)(21u * 256u + pl_rnd(&s->rs) % 2048u), 666, (2600 * v) >> 8);
        } else if (nt->lane < 3u) {                     /* the snare, the clap: one */
            int32_t X = (int32_t)(4u * 256u + pl_rnd(&s->rs) % 10240u);
            plt_at(s, nt, X, (int32_t)(4u * 256u + pl_rnd(&s->rs) % 10240u), 461, (1700 * v) >> 8);
        } else {                                        /* the rest: a star's glint */
            uint32_t i = s->nt++ % 8u;
            s->tx[i] = (uint8_t)(pl_rnd(&s->rs) % 240u);
            s->ty[i] = (uint8_t)(pl_rnd(&s->rs) % 240u);
            s->tv[i] = (uint8_t)v;
            s->tt[i] = cx->now;
        }
        return;
    }
    k = pl_pk(nt->note);
    plt_at(s, nt, 768 + k * 42, (42 - 5 * (int32_t)nt->track) * 256, 307 + (((256 - k) * 410) >> 8), 700 + ((1500 * v) >> 8));
}
/* a step: the wave (h' = (the four round) / 2 - h before), dragged (DRAG), stilled below a dead band */
static void plt_step(plt_t *s, const umb_lctx_t *cx)
{
    const int16_t *c = s->h[s->cur];
    int16_t *p = s->h[s->cur ^ 1u];
    int32_t k0 = (int32_t)cx->k[0], dk = 82 + k0 * k0 * 600 / 9801, x, y;
    for (y = 0; y < PLT_N; y++) {
        const int16_t *r0 = c + y * PLT_N, *ru = c + (y ? y - 1 : 1) * PLT_N, *rd = c + (y < PLT_N - 1 ? y + 1 : PLT_N - 2) * PLT_N;
        for (x = 0; x < PLT_N; x++) {
            int32_t nv = ((r0[x ? x - 1 : 1] + r0[x < PLT_N - 1 ? x + 1 : PLT_N - 2] + ru[x] + rd[x]) >> 1) - p[y * PLT_N + x];
            nv -= nv * dk / 4096;
            p[y * PLT_N + x] = (int16_t)(nv > -12 && nv < 12 ? 0 : pl_clamp(nv, -32000, 32000));
        }
    }
    s->cur ^= 1u;
    s->grj[0] = s->grj[1] = -99;
}
static void plt_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)   /* the steps owed: 24 a second, 4 at most */
{
    plt_t *s = st;
    uint32_t n = 0;
    (void)a;
    while ((uint32_t)(cx->now - s->ms) >= 42u && n++ < 4u) {
        s->ms += 42u;
        plt_step(s, cx);
    }
    if ((uint32_t)(cx->now - s->ms) >= 42u)
        s->ms = cx->now;
}
static const int16_t *plt_grow(plt_t *s, int32_t j)    /* row j's slopes (x, y), two rows kept (by parity) */
{
    const int16_t *c = s->h[s->cur], *r0 = c + j * PLT_N, *ru = c + (j ? j - 1 : j) * PLT_N, *rd = c + (j < PLT_N - 1 ? j + 1 : j) * PLT_N;
    int16_t *g = s->gr[j & 1];
    int32_t x;
    if (s->grj[j & 1] != j) {
        s->grj[j & 1] = (int16_t)j;
        for (x = 0; x < PLT_N; x++) {
            g[2 * x] = (int16_t)pl_clamp(r0[x < PLT_N - 1 ? x + 1 : x] - r0[x ? x - 1 : x], -32000, 32000);
            g[2 * x + 1] = (int16_t)pl_clamp(rd[x] - ru[x], -32000, 32000);
        }
    }
    return g;
}
static void plt_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    plt_t *s = st;
    uint8_t xi[240], xf[240], still[PLT_N];
    int32_t k2 = (int32_t)cx->k[2], sc = k2 <= 50 ? 64 + k2 * 192 / 50 : 256 + (k2 - 50) * 256 / 49;
    int32_t m = ((102 + 819 * (int32_t)cx->k[1] / 99) * 164) >> 8, i, sj = -1;   /* (BEND: px a slope unit, Q16) */
    int32_t q = (1 << 24) / (sc * sc);                  /* (SIZE: the reflection read nearer or further, Q8) */
    int32_t x0 = S->x0, cxx, cy;
    for (cxx = 0; cxx < S->w; cxx++) {
        int32_t t = pl_clamp(((x0 + cxx - 2) * 256) / 5, 0, (PLT_N - 1) * 256 - 1);
        xi[cxx] = (uint8_t)(t >> 8);
        xf[cxx] = (uint8_t)(t & 255);
    }
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = S->y0 + cy, vg = S->vg[cy], t = pl_clamp(((py - 2) * 256) / 5, 0, (PLT_N - 1) * 256 - 1), fy = t & 255,
                b = py - PLT_RY, b2 = b * b, k;
        const int16_t *g0 = plt_grow(s, t >> 8), *g1 = plt_grow(s, (t >> 8) + 1);
        const uint16_t *L = a->pal[(py & 1) << 1];
        uint16_t *row = S->px + cy * S->w;
        uint32_t hx = (uint32_t)x0 * 73856093u, hy = (uint32_t)py * 19349663u;
        int32_t ox = 0, oy = 0, hl = -1;                /* (the pair's offset and sheen; -1: none yet) */
        if (sj != t >> 8) {                             /* (which cells' corners are still: nothing bent there) */
            sj = t >> 8;
            for (k = 0; k < PLT_N - 1; k++)
                still[k] = !(g0[2 * k] | g0[2 * k + 1] | g0[2 * k + 2] | g0[2 * k + 3] | g1[2 * k] | g1[2 * k + 1] | g1[2 * k + 2] |
                             g1[2 * k + 3]);
        }
        for (cxx = 0; cxx < S->w; cxx++, hx += 73856093u) {
            int32_t px = x0 + cxx, p, c, X, Y, v, fx;
            uint32_t hh;
            k = 2 * xi[cxx];
            if (still[xi[cxx]]) {                       /* (still water: the reflection as it is) */
                hl = -1;
                p = px - PLT_RX;
                v = s->rl[pl_clamp((p * p + b2) * q >> 13, 0, 1023)];
                hh = hx ^ hy;
                if (!((hh >> 20) & 1023u))
                    v += ((int32_t)((hh >> 8) & 127u) + 40) * 154 >> 8;
                row[cxx] = L[(px & 1) * 256 + PL_IX(v > 255 ? 255 : v, vg)];
                continue;
            }
            if (!(px & 1) || hl < 0) {                  /* (the slope a pixel pair at a time) */
                int32_t gx, gy;
                fx = xf[cxx];
                p = g0[k] + (((g0[k + 2] - g0[k]) * fx) >> 8);
                c = g1[k] + (((g1[k + 2] - g1[k]) * fx) >> 8);
                gx = p + (((c - p) * fy) >> 8);
                p = g0[k + 1] + (((g0[k + 3] - g0[k + 1]) * fx) >> 8);
                c = g1[k + 1] + (((g1[k + 3] - g1[k + 1]) * fx) >> 8);
                gy = p + (((c - p) * fy) >> 8);
                ox = (gx * m + 32768) >> 16;            /* (whole pixels: still water, the same pixels) */
                oy = (gy * m + 32768) >> 16;
                hl = pl_clamp((((-gx * 154 - gy * 205) * 36) >> 16) - 20, 0, 256) & ~31;   /* (the sheen: 8 steps) */
                hl = (hl * 154) >> 8;
            }
            X = px + ox;
            Y = py + oy;
            p = X - PLT_RX;
            c = Y - PLT_RY;
            v = s->rl[pl_clamp((p * p + c * c) * q >> 13, 0, 1023)];
            if ((uint32_t)X < 240u && (uint32_t)Y < 240u) {   /* (the stars: one pixel in 1024) */
                hh = (uint32_t)X * 73856093u ^ (uint32_t)Y * 19349663u;
                if (!((hh >> 20) & 1023u))
                    v += ((int32_t)((hh >> 8) & 127u) + 40) * 154 >> 8;
            }
            row[cxx] = hl ? pl_rgb(a, v > 255 ? 255 : v, (PLT_SH[0] * hl) >> 8, (PLT_SH[1] * hl) >> 8, (PLT_SH[2] * hl) >> 8, vg,
                                   PL_D2[((py & 1) << 1) | (px & 1)])
                          : L[(px & 1) * 256 + PL_IX(v > 255 ? 255 : v, vg)];
        }
    }
    for (i = 0; i < 8; i++) {                           /* the stars glinting: 0.35 s, 6 steps */
        int32_t f = s->tv[i] ? pl_fade(cx->now - s->tt[i], 350u, 6u) : 0;
        if (f)
            pl_glint(S, s->tx[i] * 256, s->ty[i] * 256, (f * (128 + (s->tv[i] >> 1)) * 230) >> 16, PL_WT);
    }
}

UMB_PLATE_HEAD const umb_plate_t umb_plate_tarn = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plt_t), "TARN",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 3u, {0, 0, 0},
    {{"DRAG", 55}, {"BEND", 50}, {"SIZE", 50}},
    plt_init, plt_note, plt_frame, plt_draw};
