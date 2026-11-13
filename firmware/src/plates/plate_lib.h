/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
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
/* (a plate short of room in its 4 KiB slot) a function that runs a note or a frame, not a pixel: built smallest */
#define PL_SMALL __attribute__((minsize))
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
/* a glint at x, y (whole px): a point and the four round it a third as bright (pl_glint's pixels there, without
 * pl_splat: for plates short of room in their 4 KiB slot) */
static void pl_dot(const umb_strip_t *s, int32_t x, int32_t y, int32_t a, const uint8_t *c)
{
    int32_t k;
    for (k = 0; k < 5; k++)
        pl_px(s, x + (k == 1) - (k == 2), y + (k == 3) - (k == 4), k ? a * 77 >> 8 : a, c[0], c[1], c[2]);
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
/* (API 3) how hard note n drives control j (as a velocity x 2 + 1: 0 .. 255; 0: not at all): by its tracks as LIVE VIZ
 * CTRLS chose them (umb_lctx_t.et), else own (the plate's own choice); the control's value its strength (50: as played,
 * 0: never, 99: twice) */
static __attribute__((noinline)) int32_t pl_drive(const umb_lctx_t *c, uint32_t j, const umb_note_t *n, int own)
{
    int32_t v = (c->et[j] ? c->et[j] >> n->track & 1u : (uint32_t)own) ? ((int32_t)n->vel * 2 + 1) * c->k[j] / 50 : 0;
    return v > 255 ? 255 : v;
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
/* ---- line art (the plates light enough for a layer: BEACON, ZODIAC, SONAR, MOTHS, MARSH, PACHINKO) ---- */
static const uint8_t PL_INK[3] = {120, 226, 214};       /* the game's ink: teal */
static const uint8_t PL_DEEP[3] = {34, 60, 96};         /* what rests: deep blue */
static int32_t pl_abs(int32_t v) { return v < 0 ? -v : v; }
static void pl_clear(const umb_strip_t *s)             /* the ground: black (in ADD: what is under stays) */
{
    int32_t i;
    if (s->mode == UMB_DRAW_REPLACE)
        for (i = 0; i < s->w * s->h; i++)
            s->px[i] = 0;
}
/* a line from X0, Y0 to X1, Y1 (px, Q8), a (Q8) of colour c: a point a pixel along it, the strip's rows only */
static __attribute__((noinline)) void pl_line(const umb_strip_t *s, int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, int32_t a,
                                              const uint8_t *c)
{
    int32_t dx = X1 - X0, dy = Y1 - Y0, n = (pl_abs(dx) > pl_abs(dy) ? pl_abs(dx) : pl_abs(dy)) >> 8, k0 = 0, k1, k, sx, sy,
            ylo = (s->y0 - 1) * 256, yhi = (s->y0 + s->h) * 256;
    if (a <= 0 || (Y0 < ylo && Y1 < ylo) || (Y0 > yhi && Y1 > yhi))
        return;
    n = n < 1 ? 1 : n;
    sx = dx / n;
    sy = dy / n;
    k1 = n;
    if (sy) {
        int32_t ka = (ylo - Y0) / sy, kb = (yhi - Y0) / sy;
        k0 = pl_clamp(ka < kb ? ka : kb, 0, n);
        k1 = pl_clamp((ka < kb ? kb : ka) + 1, 0, n);
    }
    for (k = k0; k <= k1; k++)
        pl_splat(s, X0 + sx * k, Y0 + sy * k, a, c);
}
/* an arc of the circle round X, Y (px, Q8), radius R (px, Q8): from p0 on for span (2^32 a turn; all of it: ~0), a of
 * colour c, a point a pixel */
static __attribute__((noinline)) void pl_arc(const umb_strip_t *s, const umb_api_t *ap, int32_t X, int32_t Y, int32_t R, uint32_t p0,
                                             uint32_t span, int32_t a, const uint8_t *c)
{
    int32_t n = (int32_t)(((span >> 20) * (uint32_t)(R >> 8) * 201u) >> 17) + 2, k;
    uint32_t st = span / (uint32_t)n;
    if (a <= 0 || Y + R < (s->y0 - 1) * 256 || Y - R > (s->y0 + s->h) * 256)
        return;
    for (k = 0; k <= n; k++) {
        uint32_t ph = p0 + st * (uint32_t)k;
        pl_splat(s, X + (((R >> 2) * (pl_sine(ap, ph + 0x40000000u) >> 3)) >> 10), Y + (((R >> 2) * (pl_sine(ap, ph) >> 3)) >> 10), a, c);
    }
}
static int32_t pl_pol(const umb_api_t *ap, int32_t R, uint32_t ph) { return ((R >> 2) * (pl_sine(ap, ph) >> 3)) >> 10; }   /* R sin(ph) */
/* a star: a point and four short rays, l px long (Q8) */
static void pl_star(const umb_strip_t *s, int32_t X, int32_t Y, int32_t l, int32_t a, const uint8_t *c)
{
    pl_splat(s, X, Y, a, c);
    pl_line(s, X - l, Y, X + l, Y, a >> 1, c);
    pl_line(s, X, Y - l, X, Y + l, a >> 1, c);
}
#define PL_LERP(vv, xi, xf) (((vv)[xi] + ((((vv)[(xi) + 1] - (vv)[xi]) * (xf)) >> 8)) >> 8)
/* ---- the fluid plates (SEPIA, GYRE, WAX, RIVULET, FATHOM, AURORA, BLOT, RAPIDS) and what lies over them ---- */
/* add amt (0..255) to field f (60 x 60 bytes) round cx, cy (cells, Q8) within r (cells, Q8), falling off to the edge
 * (amt below 0: -amt added, falling off as the square: rounder, a body that joins its neighbours) */
static __attribute__((noinline)) void pl_stamp(uint8_t *f, int32_t cx, int32_t cy, int32_t r, int32_t amt)
{
    int32_t x, y, r2 = (r * r) >> 8, q = (1 << 24) / (r2 + 1), m = amt < 0 ? -amt : amt;
    for (y = pl_clamp((cy - r) >> 8, 0, PL_N - 1); y <= pl_clamp((cy + r + 255) >> 8, 0, PL_N - 1); y++)
        for (x = pl_clamp((cx - r) >> 8, 0, PL_N - 1); x <= pl_clamp((cx + r + 255) >> 8, 0, PL_N - 1); x++) {
            int32_t dx = x * 256 - cx, dy = y * 256 - cy, d2 = (dx * dx + dy * dy) >> 8, v;
            if (d2 > r2)
                continue;
            v = ((r2 - d2) * q) >> 16;                  /* (0 at the edge .. 256 in the middle) */
            v = f[y * PL_N + x] + ((m * (amt < 0 ? (v * v) >> 8 : v)) >> 8);
            f[y * PL_N + x] = (uint8_t)(v > 255 ? 255 : v);
        }
}
/* .. where note n's track has moved it (NOTE MOVEMENT: the field 3 .. 57 cells either way) */
static void pl_stamp_at(uint8_t *f, const umb_note_t *n, int32_t x, int32_t y, int32_t r, int32_t amt)
{
    pl_stamp(f, 768 + pl_fold(n->mo[0], x - 768, 13824), 768 + pl_fold(n->mo[1], y - 768, 13824), r, amt);
}
/* a palette of a plate's own, from black through n colours evenly spaced: lut[(x ^ y) & 1][light], as the panel takes it
 * (two phases of dither; black stays black) */
static void pl_lut(uint16_t (*lut)[256], const uint8_t (*st)[3], int32_t n)
{
    int32_t i, k, c[3];
    for (i = 0; i < 256; i++) {
        int32_t p = i * n, g = p >> 8, f = p & 255;
        for (k = 0; k < 3; k++) {
            int32_t c0 = g ? st[g - 1][k] : 0;
            c[k] = c0 + (((st[g][k] - c0) * f) >> 8);
        }
        lut[0][i] = pl_pack(c[0], c[1], c[2], 0);
        lut[1][i] = pl_pack(c[0], c[1], c[2], 8);
    }
}
/* field f (60 x 60) on the strip: each pixel its cells blended, through curve cv (the light, 0..255, of each amount) and
 * a palette: the plate's own (pl_lut; own: 1) or the cold one (umb_api_t.pal) */
static __attribute__((noinline)) void pl_fdraw(const umb_strip_t *S, const uint8_t *f, const uint8_t *cv, const uint16_t (*lut)[256], int own)
{
    uint8_t xi[240], xf[240];
    int32_t vv[PL_N + 1], cx, cy;
    pl_cols(S, xi, xf);
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = S->y0 + cy, vg = S->vg[cy], any = pl_vrow(f, py, vv, 255);
        const uint16_t *L0 = own ? lut[py & 1] : lut[(py & 1) << 1], *L1 = own ? lut[~py & 1] : L0 + 256;
        uint16_t *row = S->px + cy * S->w;
        if (S->x0 & 1) {
            const uint16_t *t = L0;
            L0 = L1;
            L1 = t;
        }
        for (cx = 0; cx < S->w; cx++) {
            int32_t v = any ? PL_LERP(vv, xi[cx], xf[cx]) : 0;
            row[cx] = (cx & 1 ? L1 : L0)[v > 0 ? PL_IX(cv[v], vg) : 0];
        }
    }
}
/* the steps owed since *ms, per ms each: 4 at most (the rest dropped) */
static uint32_t pl_owed(uint32_t now, uint32_t *ms, uint32_t per)
{
    uint32_t n = (uint32_t)(now - *ms) / per;
    if (n > 4u) {
        n = 4u;
        *ms = now;
    } else
        *ms += n * per;
    return n;
}
/* a field's point (cells, Q8) on the screen (px, Q8): a cell's centre is 4 i + 1.5 */
#define PL_C2P(c) ((c) * 4 + 384)
/* a note's own colour: its track's, a drum's its lane's */
static const uint8_t *pl_ncol(const umb_api_t *a, uint32_t track) { return a->colour[(track * 3u) % 8u]; }
/* ---- what lies over them (KOI, BOATS, FIZZ, BOKEH, MEDUSA, RIDGE, QUILL, MURMUR) ---- */
/* light gl of colour b and light e of colour c as one light: its colour into m, how much of it back (255 at most) */
static __attribute__((noinline)) int32_t pl_mix(uint8_t *m, const uint8_t *b, const uint8_t *c, int32_t gl, int32_t e)
{
    int32_t k, t = gl + e;
    for (k = 0; k < 3; k++)
        m[k] = (uint8_t)(t ? (b[k] * gl + c[k] * e) / t : 0);
    return t > 255 ? 255 : t;
}
/* heading h (65536 a turn): its cosine, its sine (Q15) */
#define PL_COS(a, h) pl_sine(a, ((uint32_t)(h) << 16) + 0x40000000u)
#define PL_SIN(a, h) pl_sine(a, (uint32_t)(h) << 16)
/* the screen's first row that is a plate's: all of it in TOTALITY, under the page's head otherwise */
#define PL_TOP(cx) ((cx)->bare ? 0 : 44)
/* a note's place across (px, lo .. hi by its pitch) and down (its track's row of six), where NOTE MOVEMENT put it */
static int32_t pl_nx(const umb_note_t *n, int32_t lo, int32_t hi) { return lo + pl_fold(n->mo[0], pl_pk(n->note), 256) * (hi - lo) / 256; }
static int32_t pl_ny(const umb_note_t *n, int32_t lo, int32_t hi) { return lo + pl_fold(n->mo[1], (int32_t)n->track * 51, 256) * (hi - lo) / 256; }
static uint32_t pl_hash(uint32_t i) { return (i + 1u) * 2654435761u; }   /* (thing i's own dice) */
/* v (0..255) going out over life ms, at age */
static int32_t pl_out(int32_t v, uint32_t age, uint32_t life) { return age >= life ? 0 : v * (int32_t)(life - age) / (int32_t)life; }
#endif
