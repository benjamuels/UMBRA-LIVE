/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* What the plate modules share (CINDER, GRANULE, TARN, SAROS): their copies of the firmware's plate helpers (ui_umbra.c,
 * ui_umbra_melody.c, ui_umbra_field.c), the same arithmetic step for step, so a plate moved out draws as it did. A plate
 * file is built alone (tools/build.py plate_files): everything here comes with it, nothing reaches the firmware but through
 * umb_api_t. Built into the emulator (ui_umbra.c, every plate resident), these are included once for all of them. */
#ifndef PLATE_LIB_H
#define PLATE_LIB_H
#include "umb_plate.h"

#define PL_N 60                                         /* a field's side: 60 x 60 cells of 4 x 4 px */
#define PL_S (PL_N * PL_N)
#define PL_T 32                                         /* the noise table's side */
#define PL_NLANE 8u                                     /* a kit's lanes */
static const int8_t PL_D2[4] = {-8, 0, 4, -4};          /* the dither, 2 x 2: [(y & 1) x 2 + (x & 1)] */
static const uint8_t PL_WT[3] = {236, 250, 246};        /* white-teal */
static const uint8_t PL_WARM[2][3] = {{255, 206, 64}, {255, 122, 28}};   /* the warm tints: yellow, orange */
static const uint8_t PL_LANE_POS[PL_NLANE] = {3, 5, 6, 2, 1, 4, 7, 0};  /* a kit's lanes' colours (KICK ice, SNARE violet ..) */
#define PL_WLO 20                                       /* (light under it black either way: no tint) */

static int32_t pl_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline __attribute__((always_inline)) int32_t pl_ix(int32_t i) { return i < 0 ? 0 : i > 255 ? 255 : i; }
#define PL_IX(v, vg) pl_ix(((v) * (vg)) >> 8)
static void pl_cpy(uint8_t *d, const uint8_t *s, uint32_t n)   /* (no memcpy: a plate links nothing) */
{
    while (n--)
        *d++ = *s++;
}
static uint16_t pl_swap(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }
/* r g b (0..255, or past it: held there) dithered by d (-8..7), as the panel takes it */
static uint16_t pl_pack(int32_t R, int32_t G, int32_t B, int32_t d)
{
    R += d >> 1;
    G += d >> 2;
    B += d >> 1;
    R = R < 0 ? 0 : R > 255 ? 255 : R;
    G = G < 0 ? 0 : G > 255 ? 255 : G;
    B = B < 0 ? 0 : B > 255 ? 255 : B;
    return pl_swap((uint32_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
/* the panel's pixel q (its colour read back, half a step up) and the light r g b (0..255) x w / 256 added, dithered by d */
static __attribute__((noinline)) uint16_t pl_add(uint32_t q, int32_t r, int32_t g, int32_t b, int32_t w, int32_t d)
{
    q = pl_swap(q);
    return pl_pack((int32_t)((q >> 8) & 0xF8u) + 4 + ((r * w) >> 8), (int32_t)((q >> 3) & 0xFCu) + 2 + ((g * w) >> 8),
                   (int32_t)((q << 3) & 0xF8u) + 4 + ((b * w) >> 8), d);
}
/* the palette at v (its plain entry) and the light r g b added, darkened by vg (Q8), dithered by d */
#define pl_rgb(a, v, r, g, b, vg, d) pl_add((a)->pal[1][PL_IX(v, vg)], r, g, b, vg, d)
/* add the light r g b (0..255) x w / 256 to the screen's pixel x, y if the strip has it (its row's light applied) */
static void pl_px(const umb_strip_t *s, int32_t x, int32_t y, int32_t w, int32_t r, int32_t g, int32_t b)
{
    uint16_t *p;
    if (w <= 0 || (uint32_t)(x - s->x0) >= (uint32_t)s->w || (uint32_t)(y - s->y0) >= (uint32_t)s->h)
        return;
    w = (w * s->vg[y - s->y0]) >> 8;
    p = s->px + (uint32_t)(y - s->y0) * (uint32_t)s->w + (uint32_t)(x - s->x0);
    *p = pl_add(*p, r, g, b, w, 0);
}
/* a point at X, Y (px, Q8), a (Q8) of colour c, shared among the four pixels round it */
static void pl_splat(const umb_strip_t *s, int32_t X, int32_t Y, int32_t a, const uint8_t *c)
{
    int32_t x = X >> 8, y = Y >> 8, fx = X & 255, fy = Y & 255, ax = (a * (256 - fx)) >> 8, bx = (a * fx) >> 8;
    if (y + 1 < s->y0 || y >= s->y0 + s->h)
        return;
    pl_px(s, x, y, (ax * (256 - fy)) >> 8, c[0], c[1], c[2]);
    pl_px(s, x + 1, y, (bx * (256 - fy)) >> 8, c[0], c[1], c[2]);
    pl_px(s, x, y + 1, (ax * fy) >> 8, c[0], c[1], c[2]);
    pl_px(s, x + 1, y + 1, (bx * fy) >> 8, c[0], c[1], c[2]);
}
/* a glint at X, Y (px, Q8): a point and the four round it a third as bright */
static void pl_glint(const umb_strip_t *s, int32_t X, int32_t Y, int32_t a, const uint8_t *c)
{
    pl_splat(s, X, Y, a, c);
    pl_splat(s, X + 256, Y, a * 77 >> 8, c);
    pl_splat(s, X - 256, Y, a * 77 >> 8, c);
    pl_splat(s, X, Y + 256, a * 77 >> 8, c);
    pl_splat(s, X, Y - 256, a * 77 >> 8, c);
}
static int32_t pl_sine(const umb_api_t *a, uint32_t ph)   /* the sine, between the table's points (dsp.c sine_i) */
{
    uint32_t i = ph >> 22;
    int32_t p = a->sine[i], q = a->sine[(i + 1u) & 1023u];
    return p + (((q - p) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}
static int32_t pl_exp(const umb_api_t *a, int32_t i) { return i < 0 ? 255 : i > 255 ? 0 : a->fade[i]; }   /* 256 exp(-i / 32) */
/* the noise table at X, Y (texels, Q8; it wraps): its value x 256 */
static int32_t pl_tbil(const umb_api_t *a, int32_t X, int32_t Y)
{
    const uint8_t *tn = a->noise;
    int32_t x0 = (X >> 8) & (PL_T - 1), y0 = (Y >> 8) & (PL_T - 1), fx = X & 255, fy = Y & 255, x1 = (x0 + 1) & (PL_T - 1),
            y1 = (y0 + 1) & (PL_T - 1);
    int32_t p = tn[y0 * PL_T + x0], b = tn[y0 * PL_T + x1], c = tn[y1 * PL_T + x0], d = tn[y1 * PL_T + x1];
    return (p << 8) + (b - p) * fx + (c - p) * fy + (((p - b - c + d) * fx * fy) >> 8);
}
static uint32_t pl_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}
static uint32_t pl_rnd(uint32_t *rs)                    /* mulberry32 */
{
    uint32_t t = *rs += 0x6D2B79F5u;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
/* v faded by f (Q16 of it), rounded up: it reaches 0 */
static uint16_t pl_dec(uint32_t v, int32_t f) { return (uint16_t)(v - ((v * (uint32_t)f + 65535u) >> 16)); }
/* a fade in steps: of n, at age (ms) of life (ms), Q8 (0 when gone) */
static int32_t pl_fade(uint32_t age, uint32_t life, uint32_t n) { return age >= life ? 0 : (int32_t)((n - age * n / life) * 256u / n); }
/* a pitch across the keyboard, 0..256 (C2 .. C7); its place on the circle of fifths from track c's root (0..11) */
static int32_t pl_pk(uint32_t n) { return pl_clamp(((int32_t)n - 36) * 4369 >> 10, 0, 256); }
static uint32_t pl_fifth(const umb_lctx_t *cx, uint32_t c, uint32_t n) { return (n + 120u - cx->root[c]) % 12u * 7u % 12u; }
/* v (0 .. span) moved by offset o (Q8 of the span), folded back into 0 .. span (ui_umbra.c umn_fold) */
static int32_t pl_fold(int32_t o, int32_t v, int32_t span)
{
    int32_t p = 2 * span;
    if (!o)
        return v;
    v = (v + ((o * span) >> 8)) % p;
    v += v < 0 ? p : 0;
    return v > span ? p - v : v;
}
/* the warm tints (CINDER): a track's own (3 yellow, 4 orange; 0: none), as a tint byte: its share (0..127), which (128) */
static uint32_t pl_tint(uint32_t c) { return c == 2u ? 127u : c == 3u ? 255u : 0u; }
/* a pixel of light b (0..255) under tint t: its share the tint's colour, the rest the cold palette's */
static __attribute__((noinline)) uint16_t pl_wpx(const umb_api_t *a, int32_t b, uint32_t t, int32_t vg, int32_t d)
{
    const uint8_t *C = PL_WARM[t >> 7];
    int32_t w = (b * (int32_t)(t & 127u)) >> 7, k = (w * w * 541) >> 8;
    return pl_rgb(a, b - w, (C[0] * k) >> 16, (C[1] * k) >> 16, (C[2] * k) >> 16, vg, d);
}
/* a 60 x 60 field (cells 4 px, centres 4 i + 2) across the strip: each column's cell and blend */
static void pl_cols(const umb_strip_t *s, uint8_t *xi, uint8_t *xf)
{
    int32_t x;
    for (x = 0; x < s->w; x++) {
        int32_t t = pl_clamp((2 * (s->x0 + x) - 3) << 5, 0, (PL_N - 1) * 256 - 1);
        xi[x] = (uint8_t)(t >> 8);
        xf[x] = (uint8_t)(t & 255);
    }
}
/* pixel row py of field f (its bits m) blended up and down into vv (x 256, 61: the last again); 0: all of it 0 */
static int32_t pl_vrow(const uint8_t *f, int32_t py, int32_t *vv, int32_t m)
{
    int32_t t = pl_clamp((2 * py - 3) << 5, 0, (PL_N - 1) * 256 - 1), fy = t & 255, k, any = 0;
    const uint8_t *r0 = f + (t >> 8) * PL_N, *r1 = r0 + PL_N;
    for (k = 0; k < PL_N; k++)
        any |= vv[k] = ((r0[k] & m) << 8) + ((r1[k] & m) - (r0[k] & m)) * fy;
    vv[PL_N] = vv[PL_N - 1];
    return any;
}
#define PL_LERP(vv, xi, xf) (((vv)[xi] + ((((vv)[(xi) + 1] - (vv)[xi]) * (xf)) >> 8)) >> 8)
#endif
