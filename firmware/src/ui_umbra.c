/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* UMBRA OBSCURA: HOME's plates in place of the ecosystem. Included by ui_lzhome.c before eco_draw.
 *
 * KILN (after paketa12's "lamp is lava" in TouchDesigner): a Belousov-Zhabotinsky cellular automaton (three chemicals,
 * each step averaged over the Moore neighbourhood, a -> b -> c -> a) whose field is re-read every step at places pushed
 * by its own slope (turned by SWIRL: the recursive displacement) and by the curl of two blended noise layers (COARSE and
 * FINE set each layer's scale, BLEND mixes them, GRAIN how much they push and texture the surface), then lit as a relief
 * in the cold palette (near-black, deep blue, pale teal).
 * The six tracks are bodies on a slow ring: each feeds one chemical where it sits, with its level and its hits; the
 * first track's hits push the whole flow. Muted tracks feed nothing.
 * Drawn as the backdrop of HOME's ECO page (SKIN AIR / HUD: every strip draws the part of the screen under it, the
 * words outlined over it; ui_graph.c lz_begin), stepped at 20 Hz of fm1_ms, once a UI frame at most twice.
 *
 * PREVIEW: float maths and full-size buffers, built into the browser emulator only (FELUCCA_WEB). The FM-1 build keeps
 * Jiant's ecosystem until KILN's fixed-point version (8-bit fields, a rolling window of rows, ~25 KB) is written. */
#ifdef FELUCCA_WEB
#include <math.h>
#define UMB_ON 1
#define UMB_N 80                                        /* the field: 80 x 80 cells, a cell 3 x 3 px */
#define UMB_S (UMB_N * UMB_N)
#define UMB_T 64                                        /* the noise table: 64 x 64, tiling */
enum { UK_RATE, UK_FLOW, UK_SWIRL, UK_RELIEF, UK_COARSE, UK_FINE, UK_BLEND, UK_GRAIN, UK_COUNT };
static uint8_t umb_p[UK_COUNT] = {45, 50, 35, 60, 35, 55, 40, 35};   /* 0..99 (web_umb sets them) */
static struct {
    float a[UMB_S], b[UMB_S], c[UMB_S], sa[UMB_S], sb[UMB_S], sc[UMB_S];
    float h[UMB_S], gx[UMB_S], gy[UMB_S], nf[UMB_S], nfx[UMB_S], nfy[UMB_S];
    float tn[UMB_T * UMB_T], tx[UMB_T * UMB_T], ty[UMB_T * UMB_T];
    float lut[256 * 3];
    float en[NTRK], pen[NTRK], pulse, t;
    uint8_t hit[NTRK], init;
    uint32_t f0, ms;
} umb;

static uint32_t umb_hash(int32_t x, int32_t y, int32_t s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)s * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static float umb_rand(uint32_t *s)                     /* mulberry32 */
{
    uint32_t t = *s += 0x6D2B79F5u;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (float)((t ^ (t >> 14)) >> 8) * (1.0f / 16777216.0f);
}
static float umb_smooth(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3 - 2 * t);
}
/* bilinear sample of an n x n field that wraps */
static float umb_bil(const float *f, int32_t n, float x, float y)
{
    int32_t xi = (int32_t)floorf(x), yi = (int32_t)floorf(y);
    float fx = x - (float)xi, fy = y - (float)yi;
    int32_t x0 = ((xi % n) + n) % n, y0 = ((yi % n) + n) % n, x1 = x0 + 1 == n ? 0 : x0 + 1, y1 = y0 + 1 == n ? 0 : y0 + 1;
    float a = f[y0 * n + x0], b = f[y0 * n + x1], c = f[y1 * n + x0], d = f[y1 * n + x1];
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
static void umb_moore(const float *s, float *d, int32_t n)
{
    int32_t x, y;
    for (y = 0; y < n; y++) {
        int32_t ym = ((y + n - 1) % n) * n, y0 = y * n, yp = ((y + 1) % n) * n;
        for (x = 0; x < n; x++) {
            int32_t xm = (x + n - 1) % n, xp = (x + 1) % n;
            d[y0 + x] = (s[ym + xm] + s[ym + x] + s[ym + xp] + s[y0 + xm] + s[y0 + x] + s[y0 + xp] + s[yp + xm] + s[yp + x] +
                         s[yp + xp]) * (1.0f / 9.0f);
        }
    }
}
static void umb_grad(const float *f, float *gx, float *gy, int32_t n)
{
    int32_t x, y;
    for (y = 0; y < n; y++)
        for (x = 0; x < n; x++) {
            gx[y * n + x] = (f[y * n + (x + 1) % n] - f[y * n + (x + n - 1) % n]) * 0.5f;
            gy[y * n + x] = (f[((y + 1) % n) * n + x] - f[((y + n - 1) % n) * n + x]) * 0.5f;
        }
}
/* the noise table: two octaves of value noise that tile (lattices of 8 and 16), and its slopes */
static float umb_oct(int32_t x, int32_t y, int32_t per, int32_t s)
{
    float cell = (float)UMB_T / (float)per, gx = (float)x / cell, gy = (float)y / cell;
    int32_t xi = (int32_t)floorf(gx), yi = (int32_t)floorf(gy);
    float xf = gx - (float)xi, yf = gy - (float)yi, u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    int32_t x0 = xi % per, y0 = yi % per, x1 = (xi + 1) % per, y1 = (yi + 1) % per;
    float a = (float)umb_hash(x0, y0, s) / 4294967296.0f, b = (float)umb_hash(x1, y0, s) / 4294967296.0f;
    float c = (float)umb_hash(x0, y1, s) / 4294967296.0f, d = (float)umb_hash(x1, y1, s) / 4294967296.0f;
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
/* the cold palette: near-black, deep blue, pale teal, white-teal */
static void umb_palette(void)
{
    static const float st[4][4] = {{0, 5, 5, 8}, {0.38f, 24, 44, 74}, {0.78f, 143, 217, 201}, {1, 236, 250, 246}};
    uint32_t i, k, ch;
    for (i = 0; i < 256u; i++) {
        float v = (float)i / 255.0f, f;
        k = 0;
        while (k < 2u && v > st[k + 1][0])
            k++;
        f = (v - st[k][0]) / (st[k + 1][0] - st[k][0]);
        f = f < 0 ? 0 : f > 1 ? 1 : f;
        for (ch = 0; ch < 3u; ch++)
            umb.lut[i * 3u + ch] = st[k][ch + 1] + (st[k + 1][ch + 1] - st[k][ch + 1]) * f;
    }
}
static void umb_start(void)
{
    uint32_t seed = 218u + fm1_ms, i;
    int32_t x, y, ns = (int32_t)(umb_rand(&seed) * 1000.0f);
    for (y = 0; y < UMB_T; y++)
        for (x = 0; x < UMB_T; x++)
            umb.tn[y * UMB_T + x] = umb_oct(x, y, 8, ns) * 0.62f + umb_oct(x, y, 16, ns + 1) * 0.38f;
    umb_grad(umb.tn, umb.tx, umb.ty, UMB_T);
    for (i = 0; i < UMB_S; i++) {
        umb.a[i] = umb_rand(&seed);
        umb.b[i] = umb_rand(&seed);
        umb.c[i] = umb_rand(&seed);
    }
    umb_palette();
    umb_moore(umb.a, umb.h, UMB_N);
    umb_grad(umb.h, umb.gx, umb.gy, UMB_N);
    umb.ms = fm1_ms;
    umb.init = 1;
}
/* a body's place (cells): six on a slow ring */
static void umb_body(uint32_t i, float *x, float *y)
{
    float an = (float)i * 1.0471976f + umb.t * 0.03f + 0.5f;
    *x = (0.5f + cosf(an) * 0.27f) * UMB_N;
    *y = (0.5f + sinf(an) * 0.27f) * UMB_N;
}
static void umb_stamp(float *f, float cx, float cy, float r, float amt)
{
    int32_t x, y;
    float r2 = r * r;
    for (y = (int32_t)floorf(cy - r); y <= (int32_t)ceilf(cy + r); y++)
        for (x = (int32_t)floorf(cx - r); x <= (int32_t)ceilf(cx + r); x++) {
            float d2 = ((float)x - cx) * ((float)x - cx) + ((float)y - cy) * ((float)y - cy);
            int32_t k = (((y % UMB_N) + UMB_N) % UMB_N) * UMB_N + (((x % UMB_N) + UMB_N) % UMB_N);
            if (d2 > r2)
                continue;
            f[k] += amt * (1 - d2 / r2);
            if (f[k] > 1)
                f[k] = 1;
        }
}
#define UMB_M(k) ((float)umb_p[k] * (1.0f / 99.0f))
/* the tracks as bodies: each one's level (its part's peak, slewed as eco_step's: a hit at once, a quick fall) */
static void umb_levels(void)
{
    uint32_t c;
    for (c = 0; c < NTRK; c++) {
        int32_t v = trk[c].peak * 256 / 7000;
        float e;
        if (v > 256)
            v = 256;
        trk[c].peak = 0;
        umb.pen[c] = umb.en[c];
        e = (float)v / 256.0f;
        umb.en[c] = e > umb.en[c] ? e : umb.en[c] + (e - umb.en[c]) * 0.25f;
        umb.hit[c] = umb.en[c] - umb.pen[c] > 0.19f;
        if (eco_off(c))
            umb.en[c] = 0, umb.hit[c] = 0;
    }
}
static void umb_step(void)
{
    float *f3[3] = {umb.a, umb.b, umb.c};
    float sA = 0.12f + UMB_M(UK_COARSE) * 0.5f, sB = 0.7f + UMB_M(UK_FINE) * 2.6f, bl = UMB_M(UK_BLEND), gr = UMB_M(UK_GRAIN);
    float oA = umb.t * 0.35f, oB = umb.t * 0.8f, k, th, ct, st, s, gamp;
    uint32_t i;
    int32_t x, y;
    for (i = 0; i < NTRK; i++) {                        /* the bodies feed their chemical */
        float bx, by, e = umb.en[i];
        if (e < 0.03f && !umb.hit[i])
            continue;
        umb_body(i, &bx, &by);
        umb_stamp(f3[i % 3u], bx, by, 1.2f + e * 2.5f + (umb.hit[i] ? 1.2f : 0), e * 0.08f + (umb.hit[i] ? 0.3f : 0));
        if (i == 0u && umb.hit[i])
            umb.pulse = 1;
    }
    umb.pulse *= 0.8f;
    for (y = 0; y < UMB_N; y++)                         /* the two noise layers, blended (paketa12's two Noise TOPs) */
        for (x = 0; x < UMB_N; x++)
            umb.nf[y * UMB_N + x] = (1 - bl) * umb_bil(umb.tn, UMB_T, (float)x * sA + oA, (float)y * sA) +
                                    bl * umb_bil(umb.tn, UMB_T, (float)x * sB - oB, (float)y * sB + 17.3f);
    umb_grad(umb.nf, umb.nfx, umb.nfy, UMB_N);
    umb_moore(umb.a, umb.sa, UMB_N);
    umb_moore(umb.b, umb.sb, UMB_N);
    umb_moore(umb.c, umb.sc, UMB_N);
    k = (0.4f + UMB_M(UK_FLOW) * 3.2f) * (1 + umb.pulse);
    th = UMB_M(UK_SWIRL) * 1.9f;
    ct = cosf(th);
    st = sinf(th);
    s = 0.3f + UMB_M(UK_RATE) * 0.8f;
    gamp = gr * 8;
    for (y = 0; y < UMB_N; y++) {
        int32_t ym = ((y + UMB_N - 1) % UMB_N) * UMB_N, y0 = y * UMB_N, yp = ((y + 1) % UMB_N) * UMB_N;
        for (x = 0; x < UMB_N; x++) {
            int32_t j = y0 + x;
            float ax = umb.sa[y0 + (x + 1) % UMB_N] - umb.sa[y0 + (x + UMB_N - 1) % UMB_N], ay = umb.sa[yp + x] - umb.sa[ym + x];
            float dx = (ax * ct - ay * st) * k - umb.nfy[j] * gamp;   /* its own slope, turned; the noise's curl */
            float dy = (ax * st + ay * ct) * k + umb.nfx[j] * gamp;
            float X, Y, A, B, C, na, nb, nc;
            dx = dx > 2 ? 2 : dx < -2 ? -2 : dx;        /* (+-2 cells: the FM-1's version keeps a 5-row window) */
            dy = dy > 2 ? 2 : dy < -2 ? -2 : dy;
            X = (float)x - dx;
            Y = (float)y - dy;
            A = umb_bil(umb.sa, UMB_N, X, Y);
            B = umb_bil(umb.sb, UMB_N, X, Y);
            C = umb_bil(umb.sc, UMB_N, X, Y);
            na = A + s * A * (1.2f * B - C) + (umb.nf[j] - 0.5f) * 0.04f * gr;
            nb = B + s * B * (C - 1.2f * A);
            nc = C + s * C * (A - B);
            umb.a[j] = na < 0 ? 0 : na > 1 ? 1 : na;
            umb.b[j] = nb < 0 ? 0 : nb > 1 ? 1 : nb;
            umb.c[j] = nc < 0 ? 0 : nc > 1 ? 1 : nc;
        }
    }
    umb_moore(umb.a, umb.h, UMB_N);
    umb_grad(umb.h, umb.gx, umb.gy, UMB_N);
}
/* once a UI frame: the levels, then the steps owed at 20 Hz (two at most) */
static void umb_tick(void)
{
    uint32_t n = 0;
    if (!umb.init)
        umb_start();
    if (umb.f0 == ui.frame)
        return;
    umb.f0 = ui.frame;
    umb_levels();
    while ((uint32_t)(fm1_ms - umb.ms) >= 50u && n < 2u) {
        umb.ms += 50u;
        umb.t += 0.05f;
        umb_step();
        n++;
    }
    if ((uint32_t)(fm1_ms - umb.ms) >= 50u)
        umb.ms = fm1_ms;                                /* (behind: drop the steps, never catch up in a burst) */
}
/* the canvas as the screen's x0, y0 .. : KILN lit, RGB565 with a 4 x 4 ordered dither */
static void umb_backdrop(int32_t x0, int32_t y0)
{
    static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    float sA = 0.12f + UMB_M(UK_COARSE) * 0.5f, sB = 0.7f + UMB_M(UK_FINE) * 2.6f, bl = UMB_M(UK_BLEND), gr = UMB_M(UK_GRAIN);
    float oA = umb.t * 0.35f, oB = umb.t * 0.8f, depth = 3 + UMB_M(UK_RELIEF) * 22, gd = gr * 7;
    const float lx = -0.5f, ly = -0.62f, lz = 0.6f, hx = -0.2793f, hy = -0.3463f, hz = 0.8953f;   /* light; half vector */
    uint32_t cx, cy;
    umb_tick();
    for (cy = 0; cy < cv_h; cy++) {
        int32_t py = y0 + (int32_t)cy;
        float Y = ((float)py + 0.5f) / 3 - 0.5f, va = Y * sA, vb = Y * sB + 17.3f;
        float vg = py < 56 ? 0.42f : py < 76 ? 0.42f + (float)(py - 56) * 0.029f : py >= 226 ? 0.5f : 1;   /* darker under
                                                         * the knob row and the footer, so the words read */
        uint16_t *row = cv_px + cy * cv_w;
        for (cx = 0; cx < cv_w; cx++) {
            int32_t px = x0 + (int32_t)cx;
            float X = ((float)px + 0.5f) / 3 - 0.5f, ua = X * sA + oA, ub = X * sB - oB;
            float nn = (1 - bl) * umb_bil(umb.tn, UMB_T, ua, va) + bl * umb_bil(umb.tn, UMB_T, ub, vb);
            float ngx = (1 - bl) * umb_bil(umb.tx, UMB_T, ua, va) * sA + bl * umb_bil(umb.tx, UMB_T, ub, vb) * sB;
            float ngy = (1 - bl) * umb_bil(umb.ty, UMB_T, ua, va) * sA + bl * umb_bil(umb.ty, UMB_T, ub, vb) * sB;
            float v = umb_bil(umb.h, UMB_N, X, Y) + (nn - 0.5f) * 0.1f * gr;
            float nx = -(umb_bil(umb.gx, UMB_N, X, Y) * depth + ngx * gd), ny = -(umb_bil(umb.gy, UMB_N, X, Y) * depth + ngy * gd);
            float inv = 1.0f / sqrtf(nx * nx + ny * ny + 1), dif = (nx * lx + ny * ly + lz) * inv, sp = (nx * hx + ny * hy + hz) * inv;
            float sh, r, g, b, d;
            uint32_t pi, R, G, B;
            if (dif < 0)
                dif = 0;
            if (sp < 0)
                sp = 0;
            sp *= sp; sp *= sp; sp *= sp; sp *= sp; sp *= sp;
            pi = (uint32_t)((0.17f + umb_smooth(0.36f, 0.64f, v) * 0.5f) * 255.0f) * 3u;
            sh = (0.3f + dif * 0.9f) * vg;
            sp *= vg;
            d = (float)bayer[((py & 3) << 2) | (px & 3)] / 16.0f - 0.47f;
            r = umb.lut[pi] * sh + umb.lut[765] * sp + d * 8;
            g = umb.lut[pi + 1] * sh + umb.lut[766] * sp + d * 4;
            b = umb.lut[pi + 2] * sh + umb.lut[767] * sp + d * 8;
            R = r < 0 ? 0u : r > 255 ? 255u : (uint32_t)r;
            G = g < 0 ? 0u : g > 255 ? 255u : (uint32_t)g;
            B = b < 0 ? 0u : b > 255 ? 255u : (uint32_t)b;
            row[cx] = swap16(((R & 0xF8u) << 8) | ((G & 0xFCu) << 3) | (B >> 3));
        }
    }
}
#else
#define UMB_ON 0
static void umb_backdrop(int32_t x0, int32_t y0) { (void)x0; (void)y0; }
#endif
