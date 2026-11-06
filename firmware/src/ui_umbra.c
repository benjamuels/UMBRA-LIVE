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
 * keeps stirring that spot. Each note is also an event: a flash, then a ring rising out of the relief and spreading, in
 * its drum sound's colour (a melodic track's own), shaped by its track's four HOME knobs (DRUM-X: MORPH turns the
 * colour, TUNE the size, TONE the height, DECAY how slowly it spreads). Between notes the field relaxes toward a rest
 * state drawn from the noise layers (dark, always drifting): a wave fades in DECAY's time (default about 1 s).
 *
 * Fixed point throughout, sized for what the firmware has left (V10.6x: ~18 KB of RAM, ~9 KB of pool, ~16 KB of flash):
 *   the field 60 x 60 cells (a cell 4 x 4 px), a byte a chemical: 10.8 KB; no second copy: a step reads a rolling window
 *   of five rows of Moore averages (1.8 KB) and writes the field in place; the noise table 64 x 64 (4 KB, in flash);
 *   lit once a 2 x 2 px block (14,400 a frame), each block's colour dithered per pixel. Under LOAD LITE / ECO, or when
 *   the audio governor lightens the load, it steps at 10 Hz instead of 20 (the same speed, half the work). */
#define UMB_ON 1
#define UMB_N 60                                        /* the field: 60 x 60 cells */
#define UMB_S (UMB_N * UMB_N)
#define UMB_T 64                                        /* the noise table: 64 x 64, tiling */
#define UMB_NE 16u                                      /* events (notes' waves) drawn at once */
#define UMB_FLOOR 3                                     /* the least of each chemical a cell keeps (of 255) */
enum { UK_RATE, UK_FLOW, UK_SWIRL, UK_RELIEF, UK_COARSE, UK_FINE, UK_BLEND, UK_GRAIN, UK_DECAY, UK_COUNT };
static uint8_t umb_p[UK_COUNT] = {45, 50, 35, 60, 35, 55, 40, 35, 40};   /* 0..99 (the emulator's sliders: web_umb) */
#include "ui_umbra_noise.h"                            /* umb_tn: the noise table, in flash (tools/gen_umbra_noise.py) */
static struct {                                         /* the knobs as the step and the frame use them (umb_consts) */
    int32_t sA, sB, bl, gr, depth, gd, oA, oB, life;    /* layers' scales (texels a cell, Q8), BLEND, GRAIN (Q8), relief
                                                         * and grain depth (x 16), the layers' drift, a wave's life */
} uf;
static struct {
    uint8_t a[UMB_S], b[UMB_S], c[UMB_S];               /* the chemicals, 0..255 */
    uint16_t mr[5][UMB_N * 3];                          /* the step's window: five rows of Moore averages, x 16 */
    uint8_t nr[3][UMB_N];                               /* .. and three rows of the noise blend */
    uint8_t sr[120 * 3];                                /* the frame: one row of lit 2 x 2 blocks (RGB) */
    int16_t sry, srx0, srx1;
    int16_t ex[UMB_NE], ey[UMB_NE];                     /* the events: where (px) */
    uint16_t eage[UMB_NE], espd[UMB_NE], eamp[UMB_NE];   /* .. age (steps), speed (px a step, Q8), height (Q8) */
    uint8_t ev[UMB_NE], er[UMB_NE], eg[UMB_NE], eb[UMB_NE], er0[UMB_NE], ew[UMB_NE];   /* strength, colour, size, width */
    uint8_t en[NTRK];                                   /* each track's level, 0..255 */
    uint16_t bx[NTRK], by[NTRK];                        /* .. and its body (cells, Q8) */
    uint16_t pulse;                                     /* the flow's push, Q8 */
    uint32_t f0, ms, rs, qr, steps, en_n;
    uint8_t init;
} umb;

static uint32_t umb_rnd(void)                          /* mulberry32 */
{
    uint32_t t = umb.rs += 0x6D2B79F5u;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
static uint32_t umb_isqrt(uint32_t v)
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
static int32_t umb_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#define UMB_K(k) ((int32_t)umb_p[k] * 256 / 99)        /* a knob, Q8 (0..256) */

/* the table at X, Y (texels, Q8; it wraps): its value x 256 */
static int32_t umb_tbil(int32_t X, int32_t Y)
{
    int32_t x0 = (X >> 8) & 63, y0 = (Y >> 8) & 63, fx = X & 255, fy = Y & 255, x1 = (x0 + 1) & 63, y1 = (y0 + 1) & 63;
    int32_t a = umb_tn[y0 * UMB_T + x0], b = umb_tn[y0 * UMB_T + x1], c = umb_tn[y1 * UMB_T + x0], d = umb_tn[y1 * UMB_T + x1];
    return (a << 8) + (b - a) * fx + (c - a) * fy + (((a - b - c + d) * fx * fy) >> 8);
}
/* a chemical at X, Y (cells, Q8; across it wraps, up and down it stops at the edge): its value x 256 */
static int32_t umb_fbil(const uint8_t *f, int32_t X, int32_t Y)
{
    int32_t xi = X >> 8, yi = Y >> 8, fx = X & 255, fy = Y & 255;
    int32_t x0 = ((xi % UMB_N) + UMB_N) % UMB_N, x1 = x0 + 1 == UMB_N ? 0 : x0 + 1;
    int32_t y0 = umb_clamp(yi, 0, UMB_N - 1) * UMB_N, y1 = umb_clamp(yi + 1, 0, UMB_N - 1) * UMB_N;
    int32_t a = f[y0 + x0], b = f[y0 + x1], c = f[y1 + x0], d = f[y1 + x1];
    return (a << 8) + (b - a) * fx + (c - a) * fy + (((a - b - c + d) * fx * fy) >> 8);
}
/* the cold palette at v (0..255): near-black, deep blue, pale teal, white-teal */
static void umb_pal(int32_t v, int32_t *r, int32_t *g, int32_t *b)
{
    static const uint8_t P[4][4] = {{0, 5, 5, 8}, {97, 24, 44, 74}, {199, 143, 217, 201}, {255, 236, 250, 246}};
    static const int32_t INV[3] = {2702, 2570, 4681};   /* (1024 x 256 / each span) */
    uint32_t k = v > 199 ? 2u : v > 97 ? 1u : 0u;
    int32_t f = ((v - P[k][0]) * INV[k]) >> 10;
    *r = P[k][1] + (((P[k + 1][1] - P[k][1]) * f) >> 8);
    *g = P[k][2] + (((P[k + 1][2] - P[k][2]) * f) >> 8);
    *b = P[k][3] + (((P[k + 1][3] - P[k][3]) * f) >> 8);
}
/* 256 x exp(-i / 32), i 0..255: the waves' profiles */
static int32_t umb_exp(int32_t i)
{
    static uint8_t E[256], made;
    if (!made) {
        int32_t k;
        uint32_t v = 255u << 16;                        /* (exp(-1/32) = 0.96923: Q16 steps) */
        for (k = 0; k < 256; k++, v = ((v >> 8) * 63519u) >> 8)   /* (32-bit: 65280 x 63519 fits) */
            E[k] = (uint8_t)(v >> 16);
        made = 1;
    }
    return i < 0 ? 255 : i > 255 ? 0 : E[i];
}
/* a wave's life in steps, at the DECAY knob (0.25 .. 8 s: 5 .. 160 steps; 40: ~1 s) */
static int32_t umb_life(void)
{
    int32_t e = (int32_t)umb_p[UK_DECAY] * 1280 / 99, f = e & 255;   /* 2 ^ (5 m), Q8 */
    int32_t p = (256 + ((f * (168 + ((88 * f) >> 8))) >> 8)) << (e >> 8);
    return (5 * p) >> 8;
}
/* the knobs, worked out once a step and once a frame (no divisions in the loops) */
static void umb_consts(void)
{
    uf.sA = 41 + UMB_K(UK_COARSE) * 170 / 256;
    uf.sB = 238 + UMB_K(UK_FINE) * 887 / 256;
    uf.bl = UMB_K(UK_BLEND);
    uf.gr = UMB_K(UK_GRAIN);
    uf.depth = 36 + UMB_K(UK_RELIEF) * 264 / 256;      /* (x 3/4 of a 3 px cell's: the same slopes a pixel) */
    uf.gd = (uf.gr * 4) >> 4;                           /* (the grain a little under: no glints in rows) */
    uf.oA = (int32_t)umb.steps * 9 / 2;
    uf.oB = (int32_t)umb.steps * 41 / 4;
    uf.life = umb_life();
}
/* the noise blend (0..255) at cell x, y, as the step uses it */
static int32_t umb_noise(int32_t x, int32_t y)
{
    int32_t X = x * 256, Y = y * 256;
    int32_t A = umb_tbil(((((X * 211 - Y * 145) >> 8) * uf.sA) >> 8) + uf.oA, (((X * 145 + Y * 211) >> 8) * uf.sA) >> 8);
    int32_t B = umb_tbil(((((X * 116 + Y * 228) >> 8) * uf.sB) >> 8) - uf.oB, ((((Y * 116 - X * 228) >> 8) * uf.sB) >> 8) + 4429);
    return ((256 - uf.bl) * (A >> 8) + uf.bl * (B >> 8)) >> 8;
}
static void umb_start(void)
{
    uint32_t i;
    umb.rs = 218u + fm1_ms;
    umb_consts();
    for (i = 0; i < UMB_S; i++) {
        umb.a[i] = (uint8_t)(70 + umb_rnd() % 30u);    /* (near the rest state: the first notes stand out) */
        umb.b[i] = (uint8_t)(85 + umb_rnd() % 30u);
        umb.c[i] = (uint8_t)(85 + umb_rnd() % 30u);
    }
    for (i = 0; i < NTRK; i++) {                        /* the bodies start on a ring */
        umb.bx[i] = (uint16_t)((128 + ((sine_i(i * 715827883u + 0x40000000u + 357913941u) >> 7) * 69 >> 8)) * UMB_N);
        umb.by[i] = (uint16_t)((128 + ((sine_i(i * 715827883u + 357913941u) >> 7) * 69 >> 8)) * UMB_N);
    }
    for (i = 0; i < UMB_NE; i++)
        umb.eage[i] = 9999;
    umb.qr = umb_q.w;
    umb.ms = fm1_ms;
    umb.sry = -1;
    umb.init = 1;
}
/* add amt (0..255) to f round cx, cy (cells, Q8) within r (cells, Q8), falling off to the edge */
static void umb_stamp(uint8_t *f, int32_t cx, int32_t cy, int32_t r, int32_t amt)
{
    int32_t x, y, r2 = (r * r) >> 8;
    for (y = (cy - r) >> 8; y <= (cy + r + 255) >> 8; y++) {
        if (y < 0 || y >= UMB_N)
            continue;
        for (x = (cx - r) >> 8; x <= (cx + r + 255) >> 8; x++) {
            int32_t dx = x * 256 - cx, dy = y * 256 - cy, d2 = (dx * dx + dy * dy) >> 8, k, v;
            if (d2 > r2)
                continue;
            k = y * UMB_N + ((x % UMB_N) + UMB_N) % UMB_N;
            v = f[k] + amt * (r2 - d2) / (r2 + 1);
            f[k] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}
/* the events' colours: eight round a cycle (mint, teal, aqua, ice, indigo, violet, rose, amber); each drum sound has its
 * own place on it, a melodic track its own; the track's knob 1 turns the cycle (centred: these) */
static const uint8_t UMB_EVC[8][3] = {{140, 236, 176}, {120, 222, 196}, {96, 214, 232}, {104, 160, 255},
                                      {118, 108, 232}, {168, 130, 255}, {232, 124, 196}, {242, 172, 96}};
static const uint8_t UMB_LANE_POS[NLANE] = {3, 5, 6, 2, 1, 4, 7, 0};   /* KICK ice, SNARE violet, CLAP rose, HATCL aqua,
                                                                        * HATOP teal, TOM indigo, RIM amber, BELL mint */
/* a track's HOME knob k (0..3) as its gauge shows it, 0..1000 (DRUM-X: MORPH TUNE TONE DECAY) */
static int32_t umb_knob(uint32_t c, uint32_t k)
{
    track_t *t = &trk[c];
    uint32_t id = trk_engp(t)->knob[k & 3u];
    const param_desc_t *d = track_desc(t, id);
    return umb_clamp(RATIO(d, enum_rank(d, t->p[id])), 0, 1000);
}
/* a spark: a fresh reaction at a new spot (cells: across the field, clear of the top), a core of one chemical in a
 * ring of the next, its size the velocity v (0..256); for a played note (c a track) its event too, and a push to the
 * flow: a drum's large and bright, a melodic note's smaller and quieter */
static void umb_spark(uint32_t c, uint32_t lane, int32_t v)
{
    uint8_t *f3[3] = {umb.a, umb.b, umb.c};
    int32_t cx = (int32_t)(umb_rnd() % (UMB_N * 256u)), cy = (int32_t)((UMB_N * 56 + umb_rnd() % (UMB_N * 184u)));
    int32_t k1 = 500, k2 = 500, k3 = 500, k4 = 500, r, r2, x, y;
    uint32_t k = c % 3u, drum = c < NTRK && trk[c].engine == ENGI_DRUM;
    if (c < NTRK) {
        k1 = umb_knob(c, 0);
        k2 = umb_knob(c, 1);
        k3 = umb_knob(c, 2);
        k4 = umb_knob(c, 3);
    }
    r = (drum ? 480 + v * 960 / 256 : 288 + v * 480 / 256) * (500 + k2) / 1000;   /* (cells, Q8; knob 2 its size) */
    r2 = (r * r) >> 8;
    for (y = (cy - r) >> 8; y <= (cy + r + 255) >> 8; y++) {
        if (y < 0 || y >= UMB_N)
            continue;
        for (x = (cx - r) >> 8; x <= (cx + r + 255) >> 8; x++) {
            int32_t dx = x * 256 - cx, dy = y * 256 - cy, d2 = (dx * dx + dy * dy) >> 8, j;
            uint32_t m;
            if (d2 > r2)
                continue;
            j = y * UMB_N + ((x % UMB_N) + UMB_N) % UMB_N;
            m = d2 * 10 < r2 * 3 ? k : (k + 1u) % 3u;
            f3[m][j] = 255;
            f3[(m + 1u) % 3u][j] = 13;
            f3[(m + 2u) % 3u][j] = 13;
        }
    }
    if (c < NTRK) {
        uint32_t q = umb.en_n++ % UMB_NE, p0;
        int32_t pos = (int32_t)(drum ? UMB_LANE_POS[lane % NLANE] : (c * 3u) % 8u) * 256 + (k1 - 500) * 2048 / 1000, f;
        pos &= 2047;                                    /* (eight places, Q8, round) */
        p0 = (uint32_t)pos >> 8;
        f = pos & 255;
        umb.er[q] = (uint8_t)(UMB_EVC[p0][0] + (((UMB_EVC[(p0 + 1u) & 7u][0] - UMB_EVC[p0][0]) * f) >> 8));
        umb.eg[q] = (uint8_t)(UMB_EVC[p0][1] + (((UMB_EVC[(p0 + 1u) & 7u][1] - UMB_EVC[p0][1]) * f) >> 8));
        umb.eb[q] = (uint8_t)(UMB_EVC[p0][2] + (((UMB_EVC[(p0 + 1u) & 7u][2] - UMB_EVC[p0][2]) * f) >> 8));
        umb.ex[q] = (int16_t)(cx * 4 / 256 + 2);
        umb.ey[q] = (int16_t)(cy * 4 / 256 + 2);
        umb.eage[q] = 0;
        umb.ev[q] = (uint8_t)(drum ? 102 + v * 153 / 256 : 38 + v * 77 / 256);
        umb.er0[q] = (uint8_t)umb_clamp((drum ? 7 : 4) * (400 + 16 * k2 / 10) * (154 + v * 102 / 256) / 256000, 2, 60);
        umb.ew[q] = (uint8_t)(3 + 7 * k2 / 1000);
        umb.eamp[q] = (uint16_t)((77 + 691 * k3 / 1000) * (drum ? 256 : 154) / 256);
        umb.espd[q] = (uint16_t)(1920 - 1632 * k4 / 1000);
        umb.bx[c] = (uint16_t)cx;
        umb.by[c] = (uint16_t)cy;
        v = drum ? 256 : 51 + v * 77 / 256;             /* a played note pushes the flow (a drum hardest) */
        umb.pulse = (uint16_t)(umb.pulse > v ? umb.pulse : v);
    }
}
/* the window's rows: row r's Moore averages (x 16), up and down clamped to the field */
static void umb_mrow(int32_t r)
{
    const uint8_t *f3[3] = {umb.a, umb.b, umb.c};
    int32_t rm = (r > 0 ? r - 1 : 0) * UMB_N, r0 = r * UMB_N, rp = (r < UMB_N - 1 ? r + 1 : UMB_N - 1) * UMB_N, x, ch;
    uint16_t *m = umb.mr[r % 5];
    for (ch = 0; ch < 3; ch++) {
        const uint8_t *f = f3[ch];
        for (x = 0; x < UMB_N; x++) {
            int32_t xm = x ? x - 1 : UMB_N - 1, xp = x < UMB_N - 1 ? x + 1 : 0;
            int32_t s = f[rm + xm] + f[rm + x] + f[rm + xp] + f[r0 + xm] + f[r0 + x] + f[r0 + xp] + f[rp + xm] + f[rp + x] +
                        f[rp + xp];
            m[x * 3 + ch] = (uint16_t)((s * 455) >> 8);   /* (x 16 / 9) */
        }
    }
}
/* a chemical's Moore average (x 16) at X, Y (cells, Q8) from the window */
static int32_t umb_mbil(int32_t X, int32_t Y, int32_t ch)
{
    int32_t xi = X >> 8, yi = Y >> 8, fx = X & 255, fy = Y & 255;
    int32_t x0 = ((xi % UMB_N) + UMB_N) % UMB_N, x1 = x0 + 1 == UMB_N ? 0 : x0 + 1;
    const uint16_t *m0 = umb.mr[umb_clamp(yi, 0, UMB_N - 1) % 5], *m1 = umb.mr[umb_clamp(yi + 1, 0, UMB_N - 1) % 5];
    int32_t a = m0[x0 * 3 + ch], b = m0[x1 * 3 + ch], c = m1[x0 * 3 + ch], d = m1[x1 * 3 + ch];
    return a + (((b - a) * fx) >> 8) + (((c - a) * fy) >> 8) + (((a - b - c + d) * fx * fy) >> 16);
}
/* a step (inc: 1 at 20 Hz, 2 at 10 Hz): the bodies feed, the field reacts where its slope and the noise's curl push it,
 * and relaxes toward the rest state; written in place, row by row, from the window */
static void umb_step(uint32_t inc)
{
    uint8_t *f3[3] = {umb.a, umb.b, umb.c};
    int32_t life = umb_life() / (int32_t)inc, xk, kd, s, k, ct, st, gr, top = 0, x, y, rtop = 0;
    uint32_t ph = (uint32_t)umb_p[UK_SWIRL] * 13119610u, i;
    if (life < 2)
        life = 2;
    xk = 12288 / life;                                  /* the falloff a step, Q12: exp(-3 / life) */
    kd = 4096 - xk + ((xk * xk) >> 13);
    s = 154 + UMB_K(UK_RATE) * 205 / 256;               /* the reaction's strength, Q8: 0.6 .. 1.4 */
    k = (102 + UMB_K(UK_FLOW) * 819 / 256) * (256 + umb.pulse) >> 8;   /* the push along the slope, Q8 */
    ct = sine_i(ph + 0x40000000u) >> 7;                 /* SWIRL turns it (Q8) */
    st = sine_i(ph) >> 7;
    gr = UMB_K(UK_GRAIN);
    for (i = 0; i < NTRK; i++)                          /* a sounding track keeps stirring its spot */
        if (umb.en[i] >= 8)
            umb_stamp(f3[i % 3u], umb.bx[i], umb.by[i], 230 + umb.en[i] * 480 / 256, umb.en[i] * 15 / 256);
    umb.steps += inc;
    umb_consts();
    for (i = 0; i < inc; i++)
        umb.pulse = (uint16_t)((umb.pulse * 210) >> 8);
    for (i = 0; i < UMB_NE; i++)
        if (umb.eage[i] < 9999)
            umb.eage[i] = (uint16_t)(umb.eage[i] + inc);
    for (y = 0; y < UMB_N; y++) {
        int32_t yb = y < UMB_N - 1 ? y + 1 : UMB_N - 1, ya = y > 0 ? y - 1 : 0;
        while (top <= y + 2 && top < UMB_N)             /* the window: Moore rows up to y + 2 (from rows not yet written) */
            umb_mrow(top++);
        while (rtop <= y + 1 && rtop < UMB_N) {         /* .. and the noise rows up to y + 1 */
            uint8_t *n = umb.nr[rtop % 3];
            for (x = 0; x < UMB_N; x++)
                n[x] = (uint8_t)umb_noise(x, rtop);
            rtop++;
        }
        for (x = 0; x < UMB_N; x++) {
            const uint16_t *m = umb.mr[y % 5];
            int32_t xm = x ? x - 1 : UMB_N - 1, xp = x < UMB_N - 1 ? x + 1 : 0, j = y * UMB_N + x;
            int32_t ax = m[xp * 3] - m[xm * 3], ay = umb.mr[yb % 5][x * 3] - umb.mr[ya % 5][x * 3];
            int32_t nf = umb.nr[y % 3][x], nfx = (umb.nr[y % 3][xp] - umb.nr[y % 3][xm]) / 2;
            int32_t nfy = (umb.nr[yb % 3][x] - umb.nr[ya % 3][x]) / 2;
            int32_t dx = (((((ax * ct - ay * st) >> 8) * k) >> 4) * 257 >> 16) - ((nfy * gr) >> 5);   /* its own slope, */
            int32_t dy = (((((ax * st + ay * ct) >> 8) * k) >> 4) * 257 >> 16) + ((nfx * gr) >> 5);   /* turned; the
                                                         * noise's curl (Q8 cells; x 257 >> 20: / 4080) */
            int32_t A, B, C, na, nb, nc, ra, rb;
            dx = umb_clamp(dx, -511, 511);              /* (under 2 cells: the window holds it) */
            dy = umb_clamp(dy, -511, 511);
            A = umb_mbil(x * 256 - dx, y * 256 - dy, 0) >> 4;
            B = umb_mbil(x * 256 - dx, y * 256 - dy, 1) >> 4;
            C = umb_mbil(x * 256 - dx, y * 256 - dy, 2) >> 4;
            na = A + ((s * A * (((B * 307) >> 8) - C)) >> 16) + (((nf - 128) * gr * 10) >> 16);
            nb = B + ((s * B * (C - ((A * 307) >> 8))) >> 16);
            nc = C + ((s * C * (A - B)) >> 16);
            ra = 77 + (((nf - 128) * 77) >> 8);         /* the rest state: dark, drifting with the noise */
            rb = (255 - ra) >> 1;
            na = ra + (((umb_clamp(na, UMB_FLOOR, 255) - ra) * kd) >> 12);
            nb = rb + (((umb_clamp(nb, UMB_FLOOR, 255) - rb) * kd) >> 12);
            nc = rb + (((umb_clamp(nc, UMB_FLOOR, 255) - rb) * kd) >> 12);
            umb.a[j] = (uint8_t)na;
            umb.b[j] = (uint8_t)nb;
            umb.c[j] = (uint8_t)nc;
        }
    }
}
/* the tracks' levels (each part's peak, slewed as eco_step's: a hit at once, a quick fall) */
static void umb_levels(void)
{
    uint32_t c;
    for (c = 0; c < NTRK; c++) {
        int32_t v = trk[c].peak * 255 / 7000;
        if (v > 255)
            v = 255;
        trk[c].peak = 0;
        umb.en[c] = (uint8_t)(eco_off(c) ? 0 : v > umb.en[c] ? v : umb.en[c] + (v - umb.en[c]) / 4);
    }
}
/* once a UI frame: the levels, the notes played since (one spark a track and drum sound: a chord, its loudest note),
 * then the steps owed (20 Hz; 10 Hz when the load is lightened), never more than two */
static void umb_tick(void)
{
    uint32_t n = 0, c, l, dt = (sys_load || gov_lite) ? 100u : 50u;
    uint8_t vm[NTRK][NLANE];
    if (!umb.init)
        umb_start();
    if (umb.f0 == ui.frame)
        return;
    umb.f0 = ui.frame;
    umb.sry = -1;
    umb_consts();
    umb_levels();
    if (umb_q.w - umb.qr > UMB_NQ)                      /* (a burst: the newest only) */
        umb.qr = umb_q.w - UMB_NQ;
    for (c = 0; c < NTRK; c++)
        for (l = 0; l < NLANE; l++)
            vm[c][l] = 0;
    for (; umb.qr != umb_q.w; umb.qr++) {
        uint32_t q = umb.qr % UMB_NQ, v = umb_q.vel[q] * 2u + 1u;
        c = umb_q.c[q];
        if (c >= NTRK)
            continue;
        l = trk[c].engine == ENGI_DRUM ? drum_lane(umb_q.note[q]) % NLANE : 0u;
        if (v > vm[c][l])
            vm[c][l] = (uint8_t)(v > 255u ? 255u : v);
    }
    for (c = 0; c < NTRK; c++)
        for (l = 0; l < NLANE; l++)
            if (vm[c][l] && !eco_off(c))
                umb_spark(c, l, vm[c][l]);
    while ((uint32_t)(fm1_ms - umb.ms) >= dt && n < 2u) {
        umb.ms += dt;
        umb_step(dt / 50u);
        n++;
    }
    if ((uint32_t)(fm1_ms - umb.ms) >= dt)
        umb.ms = fm1_ms;                                /* (behind: drop the steps, never catch up in a burst) */
}
/* one 2 x 2 px block lit, its centre at px, py (pixel edges): RGB 0..255 into o */
static void umb_block(int32_t px, int32_t py, int32_t vg, uint8_t *o)
{
    static const int32_t LX = -128, LY = -159, LZ = 154, HX = -72, HY = -89, HZ = 229;   /* the light; half vector (Q8) */
    int32_t sA = uf.sA, sB = uf.sB, bl = uf.bl, gr = uf.gr, depth = uf.depth, gd = uf.gd, oA = uf.oA, oB = uf.oB;
    int32_t life = uf.life;
    int32_t X = px * 64 - 128, Y = py * 64 - 128;      /* (cells, Q8) */
    /* the two layers, each turned its own way (no lattice lined up with the light: no glints in rows) */
    int32_t XA = ((((X * 211 - Y * 145) >> 8) * sA) >> 8) + oA, YA = (((X * 145 + Y * 211) >> 8) * sA) >> 8;
    int32_t XB = ((((X * 116 + Y * 228) >> 8) * sB) >> 8) - oB, YB = ((((Y * 116 - X * 228) >> 8) * sB) >> 8) + 4429;
    int32_t tA = umb_tbil(XA, YA), tB = umb_tbil(XB, YB);
    int32_t nn = ((256 - bl) * (tA >> 8) + bl * (tB >> 8)) >> 8;
    int32_t axA = (umb_tbil(XA + 256, YA) - umb_tbil(XA - 256, YA)) >> 1, ayA = (umb_tbil(XA, YA + 256) - umb_tbil(XA, YA - 256)) >> 1;
    int32_t axB = (umb_tbil(XB + 256, YB) - umb_tbil(XB - 256, YB)) >> 1, ayB = (umb_tbil(XB, YB + 256) - umb_tbil(XB, YB - 256)) >> 1;
    int32_t ngx = ((256 - bl) * ((((axA * 211 + ayA * 145) >> 8) * sA) >> 8) + bl * ((((axB * 116 - ayB * 228) >> 8) * sB) >> 8)) >> 8;
    int32_t ngy = ((256 - bl) * ((((ayA * 211 - axA * 145) >> 8) * sA) >> 8) + bl * ((((axB * 228 + ayB * 116) >> 8) * sB) >> 8)) >> 8;
    int32_t gx = (umb_fbil(umb.a, X + 256, Y) - umb_fbil(umb.a, X - 256, Y)) >> 1;   /* the relief's slope (x 256 a cell) */
    int32_t gy = (umb_fbil(umb.a, X, Y + 256) - umb_fbil(umb.a, X, Y - 256)) >> 1;
    int32_t v = (umb_fbil(umb.a, X, Y) >> 8) + (((nn - 128) * gr * 26) >> 16), t, sm, nx, ny, len, dif, sp, sh, r, g, b;
    int32_t gr_ = 0, gg_ = 0, gb_ = 0, tw = 0, tr = 0, tg = 0, tb = 0;
    uint32_t q;
    nx = -((gx * depth) >> 12) - ((ngx * gd) >> 12);    /* (the normal, Q8) */
    ny = -((gy * depth) >> 12) - ((ngy * gd) >> 12);
    for (q = 0; q < UMB_NE; q++) {                      /* the events: a flash at the heart, then a ring that rises out
                                                         * of the relief and spreads, in its colour; the lava tinted */
        int32_t age = umb.eage[q], fade, dx, dy, d2, rad, w, o2, d4, u, ring, core = 0, dh, e, R0 = umb.er0[q];
        if (age >= life)
            continue;
        fade = 256 - age * 256 / life;
        dx = px - umb.ex[q];
        dy = py - umb.ey[q];
        d2 = dx * dx + dy * dy;
        rad = R0 + ((age * umb.espd[q]) >> 8);
        w = umb.ew[q];
        o2 = rad + 3 * w + R0;
        if (d2 > o2 * o2)
            continue;
        d4 = (int32_t)umb_isqrt((uint32_t)d2 << 8) + 1; /* (px, Q4) */
        u = d4 - rad * 16;
        ring = ((umb.ev[q] * fade) >> 8) * umb_exp(u * u / (w * w * 8)) >> 8;
        if (age * 20 < life * 7)
            core = ((umb.ev[q] * (256 - age * 5120 / (life * 7 + 1))) >> 8) * umb_exp(d2 * 32 / (R0 * R0)) >> 8;
        dh = ((ring * -2 * u) / (16 * w * w) + (core * -2 * d4) / (16 * R0 * R0)) * umb.eamp[q] >> 8;   /* (Q8 a px) */
        nx -= dh * dx * 16 / d4;
        ny -= dh * dy * 16 / d4;
        gr_ += umb.er[q] * (ring + core);
        gg_ += umb.eg[q] * (ring + core);
        gb_ += umb.eb[q] * (ring + core);
        e = ((umb.ev[q] * fade) >> 8) * umb_exp(d2 * 32 / ((rad + w) * (rad + w))) >> 8;
        tw += e;
        tr += umb.er[q] * e;
        tg += umb.eg[q] * e;
        tb += umb.eb[q] * e;
    }
    nx = umb_clamp(nx, -16000, 16000);
    ny = umb_clamp(ny, -16000, 16000);
    len = (int32_t)umb_isqrt((uint32_t)(nx * nx + ny * ny + 65536));
    dif = umb_clamp((nx * LX + ny * LY + LZ * 256) / len, 0, 256);
    sp = umb_clamp((nx * HX + ny * HY + HZ * 256) / len, 0, 256);
    sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8;
    t = umb_clamp(((v - 92) * 3692) >> 10, 0, 256);   /* (x 256 / 71) */         /* the tone: two of them, softly (0.17 .. 0.67 of the palette) */
    sm = (t * t * (768 - 2 * t)) >> 16;
    umb_pal(43 + ((sm * 128) >> 8), &r, &g, &b);
    sh = ((77 + ((dif * 230) >> 8)) * vg) >> 8;
    sp = (sp * vg) >> 8;
    r = ((r * sh) >> 8) + ((236 * sp) >> 8);
    g = ((g * sh) >> 8) + ((250 * sp) >> 8);
    b = ((b * sh) >> 8) + ((246 * sp) >> 8);
    if (tw > 2) {                                       /* the lava under an event takes its colour */
        int32_t m = tw > 192 ? 192 : tw, l = ((r + g + b) * 582) >> 10;   /* (x 256 / 450) */
        r = (r * (256 - m) + (tr / tw) * l / 256 * m) >> 8;
        g = (g * (256 - m) + (tg / tw) * l / 256 * m) >> 8;
        b = (b * (256 - m) + (tb / tw) * l / 256 * m) >> 8;
    }
    r += ((gr_ >> 9) * vg) >> 8;                        /* (the glow: its colour, added) */
    g += ((gg_ >> 9) * vg) >> 8;
    b += ((gb_ >> 9) * vg) >> 8;
    o[0] = (uint8_t)umb_clamp(r, 0, 255);
    o[1] = (uint8_t)umb_clamp(g, 0, 255);
    o[2] = (uint8_t)umb_clamp(b, 0, 255);
}
/* the canvas as the screen's x0, y0 .. : KILN, lit a 2 x 2 block at a time (a row of blocks kept while a strip runs
 * through its two pixel rows), RGB565 with a 4 x 4 ordered dither */
static void umb_backdrop(int32_t x0, int32_t y0)
{
    static const int8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    uint32_t cx, cy;
    umb_tick();
    for (cy = 0; cy < cv_h; cy++) {
        int32_t py = y0 + (int32_t)cy, sy = py >> 1, sx0 = x0 >> 1, sx1 = (x0 + (int32_t)cv_w - 1) >> 1, sx;
        uint16_t *row = cv_px + cy * cv_w;
        if (sy != umb.sry || sx0 < umb.srx0 || sx1 > umb.srx1) {
            int32_t yc = sy * 2 + 1;                    /* (darker under the knob row and the footer, so the words read) */
            int32_t vg = yc < 56 ? 108 : yc < 76 ? 108 + (yc - 56) * 7 : yc >= 226 ? 128 : 256;
            for (sx = sx0; sx <= sx1 && sx < 120; sx++)
                umb_block(sx * 2 + 1, yc, vg, umb.sr + sx * 3);
            umb.sry = (int16_t)sy;
            umb.srx0 = (int16_t)sx0;
            umb.srx1 = (int16_t)sx1;
        }
        for (cx = 0; cx < cv_w; cx++) {
            int32_t px = x0 + (int32_t)cx, d = bayer[((py & 3) << 2) | (px & 3)] - 8;
            const uint8_t *s = umb.sr + (px >> 1) * 3;
            uint32_t R = (uint32_t)umb_clamp(s[0] + d / 2, 0, 255), G = (uint32_t)umb_clamp(s[1] + d / 4, 0, 255);
            uint32_t B = (uint32_t)umb_clamp(s[2] + d / 2, 0, 255);
            row[cx] = swap16(((R & 0xF8u) << 8) | ((G & 0xFCu) << 3) | (B >> 3));
        }
    }
}
