/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* UMBRA OBSCURA: HOME's plates in place of the ecosystem. Included by ui_lzhome.c before eco_draw.
 *
 * KILN (after paketa12's "lamp is lava" in TouchDesigner): a Belousov-Zhabotinsky cellular automaton (three chemicals,
 * each step averaged over the Moore neighbourhood, a -> b -> c -> a) whose field is re-read every step at places pushed
 * by its own slope (turned by SWIRL: the recursive displacement) and by the curl of two blended noise layers (COARSE and
 * FINE set each layer's scale, BLEND mixes them, GRAIN how much they push and texture the surface), then lit as a relief
 * in the cold palette (near-black, deep blue, pale teal).
 * The six tracks are bodies: every note played (keys, sequencer, arp, MIDI: voice.c trk_note_on's ring) sparks a fresh
 * reaction at a new spot, its size the velocity, and moves its track's body there; while the track sounds, its level
 * keeps stirring that spot; each note gives the flow a push (the first track's the hardest). Between notes the field
 * relaxes, each step, toward a rest state drawn from the two noise layers (dark, a slow landscape always drifting): a
 * spark's waves fade back into it in DECAY's time (default about 1 s), so each hit reads as its own event.
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
#define UMB_FLOOR 0.012f                              /* the least of each chemical a cell keeps */
#define UMB_T 64                                        /* the noise table: 64 x 64, tiling */
enum { UK_RATE, UK_FLOW, UK_SWIRL, UK_RELIEF, UK_COARSE, UK_FINE, UK_BLEND, UK_GRAIN, UK_DECAY, UK_COUNT };
static uint8_t umb_p[UK_COUNT] = {45, 50, 35, 60, 35, 55, 40, 35, 40};   /* 0..99 (web_umb sets them) */
static struct {
    float a[UMB_S], b[UMB_S], c[UMB_S], sa[UMB_S], sb[UMB_S], sc[UMB_S];
    float h[UMB_S], gx[UMB_S], gy[UMB_S], nf[UMB_S], nfx[UMB_S], nfy[UMB_S];
    float tn[UMB_T * UMB_T], tx[UMB_T * UMB_T], ty[UMB_T * UMB_T];
    float lut[256 * 3];
    float gx0[8], gy0[8], gage[8], gv[8];            /* the sparks' glow: where, how old (s), how strong */
    uint32_t gn;
    float was[100];                                 /* a sample of the field a second ago (the stall check) */
    float en[NTRK], bx[NTRK], by[NTRK], pulse, t;  /* each track's level (0..1) and its body (cells) */
    uint8_t init;
    uint32_t f0, ms, rs, qr, steps, quiet;          /* frame, step clock, random state, notes read, steps, steps since
                                                     * the last spark */
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
    for (i = 0; i < NTRK; i++) {                    /* the bodies start on a ring */
        umb.bx[i] = (0.5f + cosf((float)i * 1.0471976f + 0.5f) * 0.27f) * UMB_N;
        umb.by[i] = (0.5f + sinf((float)i * 1.0471976f + 0.5f) * 0.27f) * UMB_N;
    }
    for (i = 0; i < 8u; i++)
        umb.gage[i] = 9;
    umb.rs = seed;
    umb.qr = umb_q.w;
    umb_palette();
    umb_moore(umb.a, umb.h, UMB_N);
    umb_grad(umb.h, umb.gx, umb.gy, UMB_N);
    umb.ms = fm1_ms;
    umb.init = 1;
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
        e = (float)v / 256.0f;
        umb.en[c] = e > umb.en[c] ? e : umb.en[c] + (e - umb.en[c]) * 0.25f;
        if (eco_off(c))
            umb.en[c] = 0;
    }
}
/* a spark: a fresh reaction at a new spot (the track's body moves there), a core of one chemical in a ring of the next,
 * its size the velocity (0..1), a glow ring and a push to the flow: a drum track's large and bright, a melodic one's
 * smaller and quieter */
static void umb_spark(uint32_t c, float v)
{
    float *f3[3] = {umb.a, umb.b, umb.c};
    float cx = umb_rand(&umb.rs) * UMB_N, cy = (0.22f + umb_rand(&umb.rs) * 0.72f) * UMB_N, r, r2;
    uint32_t k = c % 3u, drum = c < NTRK && trk[c].engine == ENGI_DRUM;
    r = drum ? 2.5f + v * 5.0f : 1.5f + v * 2.5f;   /* (a drum's the big one) */
    r2 = r * r;
    int32_t x, y;
    for (y = (int32_t)floorf(cy - r); y <= (int32_t)ceilf(cy + r); y++)
        for (x = (int32_t)floorf(cx - r); x <= (int32_t)ceilf(cx + r); x++) {
            float d2 = ((float)x - cx) * ((float)x - cx) + ((float)y - cy) * ((float)y - cy);
            int32_t j = (((y % UMB_N) + UMB_N) % UMB_N) * UMB_N + (((x % UMB_N) + UMB_N) % UMB_N);
            uint32_t m = d2 < r2 * 0.3f ? k : (k + 1u) % 3u;
            if (d2 > r2)
                continue;
            f3[m][j] = 1;
            f3[(m + 1u) % 3u][j] = 0.05f;
            f3[(m + 2u) % 3u][j] = 0.05f;
        }
    if (c < NTRK) {
        umb.bx[c] = cx;
        umb.by[c] = cy;
        umb.gx0[umb.gn % 8u] = cx * 3 + 1;              /* (a played note glows; the ember's own sparks don't) */
        umb.gy0[umb.gn % 8u] = cy * 3 + 1;
        umb.gage[umb.gn % 8u] = 0;
        umb.gv[umb.gn % 8u] = drum ? 0.35f + 0.65f * v : 0.12f + 0.25f * v;
        umb.gn++;
    }
    if (c < NTRK) {                                     /* a played note pushes the flow (the first track hardest) */
        v = drum ? 1.0f : 0.2f + 0.3f * v;
        umb.pulse = umb.pulse > v ? umb.pulse : v;
    }
    umb.quiet = 0;
}
static void umb_step(void)
{
    float *f3[3] = {umb.a, umb.b, umb.c};
    float sA = 0.12f + UMB_M(UK_COARSE) * 0.5f, sB = 0.7f + UMB_M(UK_FINE) * 2.6f, bl = UMB_M(UK_BLEND), gr = UMB_M(UK_GRAIN);
    float oA = umb.t * 0.35f, oB = umb.t * 0.8f, k, th, ct, st, s, gamp, kd;
    uint32_t i;
    int32_t x, y;
    for (i = 0; i < NTRK; i++)                          /* a sounding track keeps stirring its spot */
        if (umb.en[i] >= 0.03f)
            umb_stamp(f3[i % 3u], umb.bx[i], umb.by[i], 1.2f + umb.en[i] * 2.5f, umb.en[i] * 0.06f);
    umb.steps++;
    umb.pulse *= 0.82f;
    for (i = 0; i < 8u; i++)
        umb.gage[i] += 0.05f;
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
    s = 0.6f + UMB_M(UK_RATE) * 0.8f;
    {                                                   /* DECAY: a wave's life 0.25 .. 8 s (40: ~1 s), to 5 % of itself */
        float life = 0.25f * powf(2.0f, UMB_M(UK_DECAY) * 5.0f);
        kd = expf(-0.15f / life);
    }
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
            na = na < UMB_FLOOR ? UMB_FLOOR : na > 1 ? 1 : na;   /* (a trace of each everywhere: none dies out) */
            nb = nb < UMB_FLOOR ? UMB_FLOOR : nb > 1 ? 1 : nb;
            nc = nc < UMB_FLOOR ? UMB_FLOOR : nc > 1 ? 1 : nc;
            {                                           /* the falloff: back toward the drifting dark rest state */
                float ra = 0.3f + (umb.nf[j] - 0.5f) * 0.3f, rb = (1 - ra) * 0.5f;
                umb.a[j] = ra + (na - ra) * kd;
                umb.b[j] = rb + (nb - rb) * kd;
                umb.c[j] = rb + (nc - rb) * kd;
            }
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
    if (umb_q.w - umb.qr > UMB_NQ)                      /* (a burst: the newest only) */
        umb.qr = umb_q.w - UMB_NQ;
    {                                                   /* the notes played since: one spark a track (a chord's loudest) */
        float vm[NTRK] = {0};
        uint32_t c;
        for (; umb.qr != umb_q.w; umb.qr++) {
            uint32_t q = umb.qr % UMB_NQ;
            float v = (float)umb_q.vel[q] / 127.0f + 0.001f;
            if (umb_q.c[q] < NTRK && v > vm[umb_q.c[q]])
                vm[umb_q.c[q]] = v;
        }
        for (c = 0; c < NTRK; c++)
            if (vm[c] > 0 && !eco_off(c))
                umb_spark(c, vm[c] > 1 ? 1 : vm[c]);
    }
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
            {                                           /* the sparks' glow: a soft ring blooming out, ~1.2 s */
                uint32_t q;
                for (q = 0; q < 8u; q++) {
                    float age = umb.gage[q], dx, dy, rr, ring;
                    if (age > 1.2f)
                        continue;
                    dx = (float)px - umb.gx0[q];
                    dy = (float)py - umb.gy0[q];
                    if (dx < -60 || dx > 60 || dy < -60 || dy > 60)
                        continue;
                    rr = sqrtf(dx * dx + dy * dy);
                    ring = umb.gv[q] * (1 - age / 1.2f) * expf(-(rr - 6 - age * 40) * (rr - 6 - age * 40) * 0.012f);
                    if (age < 0.4f)                     /* (a flash at its heart first) */
                        ring += umb.gv[q] * (1 - age / 0.4f) * expf(-rr * rr * 0.01f);
                    sh += ring * 0.3f;
                    sp += ring * 0.6f;
                }
            }
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
