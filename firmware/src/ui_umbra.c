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
 * colour, TUNE the size, TONE the height, DECAY how slowly it spreads; a closed hat's ring small). Between notes the
 * field relaxes toward a rest state drawn from the noise layers (dark, always drifting): a wave fades in DECAY's time
 * (default about 1 s). Over it a surface: the noise table sampled fine (a few pixels a feature), lit by the relief, and
 * a blue-noise grain: the dark lava's detail. HOME > ECO's third page, TOTALITY, is KILN alone over the whole screen.
 *
 * Fixed point throughout, sized for what the firmware has left (V10.6x: ~18 KB of RAM, ~9 KB of pool, ~16 KB of flash):
 *   the field 60 x 60 cells (a cell 4 x 4 px), a byte a chemical: 10.8 KB; no second copy: a step reads a rolling window
 *   of five rows of Moore averages (1.8 KB) and writes the field in place; the noise table 32 x 32 (1 KB, in flash);
 *   lit once a cell, at its centre (3,844 points a frame, tables for the light: no square root, no division), the shade
 *   and the light blended to a 2 x 2 px block; but each pixel's tone worked out from the field blended there (sharpened
 *   first), so the lava's edges are a pixel sharp, and the dark base (most of the screen) a pair at once; the surface
 *   (two table reads a pixel) and the grain each pixel, the surface left out when lite; the rings' glow
 *   (a fine line) and the flashes' (a dome, its gloss lit there) drawn a pixel pair, only over their bands; the events'
 *   constants worked out once a frame, so a point near a ring costs only multiplications (on the FM-1 the first version,
 *   lit a 2 x 2 px block with a square root and divisions, and the rings with nine divisions a point, drew a frame far
 *   too slowly). The chemistry steps 20 times a second (fewer coarsen it: each step's averaging is what smooths the
 *   field, and the falloff would pull harder a step); under LOAD LITE / ECO, or when the audio governor lightens the
 *   load, 10 (each step as two). */
#define UMB_ON 1
/* PLATE (MENU > SCREEN 2, menu_items.c umb_plate): 0 KILN; 1 SAROS, 2 FILAMENT, the melody plates (this file's end) */
#define UMB_N 60                                        /* the field: 60 x 60 cells */
#define UMB_S (UMB_N * UMB_N)
#define UMB_T 32                                        /* the noise table: 32 x 32, tiling (64 x 64 until the melody
                                                         * plates needed its room: the same features, the tile half) */
#define UMB_NE 16u                                      /* events (notes' waves) drawn at once */
#define UMB_FLOOR 3                                     /* the least of each chemical a cell keeps (of 255) */
#define UMB_SHARP 64                                    /* the field sharpened before it is blended (Q8 of its Laplacian) */
#define UMB_DOME 24                                     /* the flash's glow a disc, out to 1.5 R0 (x 16) .. */
#define UMB_DOMEK 205                                   /* .. this bright (of 256) */
#define UMB_HATCL 3u                                     /* the closed hat's drum lane (params.c LANE 3, HATCL) .. */
#define UMB_HATK 154                                    /* .. its spark and flash 0.6 the size (Q8) .. */
#define UMB_HATS 102                                    /* .. its ring spreading 0.4 as fast: a small ring */
#define UMB_C 4                                         /* a lit centre: the shade, the light added (RGB) */
#ifndef UMB_SH0
#define UMB_SH0 40                                      /* the shade: a face turned away (of 256) .. */
#define UMB_SH1 300                                     /* .. and how much the light adds (was 77, 230: flatter) */
#endif
#ifndef UMB_GRAIN
#define UMB_GRAIN 16                                    /* the dither's reach (levels, red and blue; green half): a grain */
#endif
#ifndef UMB_TEX
#define UMB_TEX 40                                      /* the surface's depth (+ up to 48 with GRAIN): its fine detail */
#endif
#define UMB_TX(x) if (kS) { int32_t t_ = (tx[x] * kS) >> 8; R += t_ - (t_ >> 2); G += t_ - (t_ >> 3); B += t_; }   /* (cold:
                                                         * blue most) */
enum { UK_RATE, UK_FLOW, UK_SWIRL, UK_RELIEF, UK_COARSE, UK_FINE, UK_BLEND, UK_GRAIN, UK_DECAY, UK_COUNT };
static uint8_t umb_p[UK_COUNT] = {45, 50, 35, 60, 35, 55, 40, 35, 40};   /* 0..99 (the emulator's sliders: web_umb) */
#include "ui_umbra_noise.h"                            /* umb_tn: the noise table; the light's, the lava's colour (umb_tl:
                                                         * its edge UMB_T0), the profiles: in flash (tools/gen_umbra_noise.py) */
static struct {                                         /* the knobs as the step and the frame use them (umb_consts) */
    int32_t sA, sB, bl, gr, depth, gd, oA, oB, life;    /* layers' scales (texels a cell, Q8), BLEND, GRAIN (Q8), relief
                                                         * and grain depth (x 16), the layers' drift, a wave's life */
} uf;
static struct {                                         /* the events, worked out once a frame (umb_events): per point
                                                         * only multiplications, no square root, no division */
    int32_t on[UMB_NE], fe[UMB_NE], fc[UMB_NE], R[UMB_NE], R2[UMB_NE], o2[UMB_NE], c2[UMB_NE], t2[UMB_NE];
    int32_t iu[UMB_NE], kw[UMB_NE], iR[UMB_NE], kc[UMB_NE], kg[UMB_NE], kt[UMB_NE], w4[UMB_NE];
    int32_t ro2[UMB_NE], ri2[UMB_NE];                   /* the ring's glow drawn a pixel: its band, R +- a width (px^2) */
    int32_t cb2[UMB_NE], kb[UMB_NE];                    /* .. the flash's: a dome cut at its shoulder (px^2), its profile */
} ue;
/* (SAROS and FILAMENT, the melody plates at this file's end: their state, kept in KILN's field: one plate draws at a time) */
#define UMM_TH 48                                       /* SAROS: threads at most */
#define UMM_ND 12                                       /* .. the notes' points glowing */
#define UMM_NP 81                                       /* FILAMENT: a string's points (3 px apart) */
#define UMM_NH 4                                        /* .. the notes a string holds */
typedef struct {
    uint8_t ta[UMM_TH], tb[UMM_TH], tc[UMM_TH], tf[UMM_TH];   /* SAROS: a thread's notes (a < b), track, the note before */
    uint16_t te[UMM_TH];                                /* .. its energy (Q8: burned up to 2.5) */
    uint32_t tt[UMM_TH];                                /* .. when it was last played (ms) */
    int32_t lx[UMM_TH], ly[UMM_TH], lsx[UMM_TH], lsy[UMM_TH];   /* .. as this frame draws it: from, a sample's step (px, Q8) */
    int16_t ln[UMM_TH];                                 /* .. its samples */
    uint8_t lc[UMM_TH][3];                              /* .. its light */
    uint8_t nn[UMM_ND], nc[UMM_ND], nv[UMM_ND];         /* the notes' points: note, track, velocity .. */
    uint32_t nt[UMM_ND];                                /* .. when */
    int16_t ux[12], uy[12];                             /* the twelve places on the circle of fifths, turned (Q15) */
    uint8_t nth, nnd, last[NTRK];                       /* threads in use, the next point; each track's last note + 1 */
    uint32_t th, tms;                                   /* the turn (2^32 a turn) and its clock */
    int16_t u[NTRK][UMM_NP], v[NTRK][UMM_NP];           /* FILAMENT: each string's height and speed (px, Q8) */
    uint8_t c2[NTRK], hn[NTRK][UMM_NH], hx[NTRK][UMM_NH];   /* .. its tension (Q8); the notes it holds (+ 1), where */
    int16_t ha[NTRK][UMM_NH];                           /* .. how hard they bow it (the side its sign) */
    uint32_t hf[NTRK][UMM_NH], h0[NTRK][UMM_NH];        /* .. how fast (phase a ms), since when */
    int8_t dir[NTRK];                                   /* .. the side a pluck goes */
    uint8_t mv[NTRK];                                   /* .. moving (still: nothing to work out) */
    uint32_t sub;                                       /* the substeps' clock (ms) */
} umm_t;
static struct {
    union {
        struct { uint8_t a[UMB_S], b[UMB_S], c[UMB_S]; };   /* the chemicals, 0..255 */
        umm_t m;                                        /* (SAROS, FILAMENT: KILN starts afresh when it comes back) */
    };
    uint16_t mr[5][UMB_N * 3];                          /* the step's window: five rows of Moore averages, x 16 */
    uint8_t nr[3][UMB_N];                               /* .. and three rows of the noise blend */
    uint8_t sr[2][(UMB_N + 2) * UMB_C];                 /* the frame: two rows of lit cell centres (umb_sample), -1 .. 60 */
    int16_t srj[2], srlo[2], srhi[2];                   /* .. which rows they are (by parity), which centres are lit */
    uint8_t vr[2][UMB_N + 2];                           /* .. and two rows of the field sharpened, -1 .. 60 (umb_vrow) */
    int16_t vrj[2];
    int16_t ex[UMB_NE], ey[UMB_NE];                     /* the events: where (px) */
    uint32_t et[UMB_NE];                                /* .. when it was played (fm1_ms): rings timed by the clock */
    uint16_t espd[UMB_NE], eamp[UMB_NE];                /* .. speed (px a 50 ms, Q8), height (Q8) */
    uint8_t ev[UMB_NE], er[UMB_NE], eg[UMB_NE], eb[UMB_NE], er0[UMB_NE], ew[UMB_NE];   /* strength, colour, size, width */
    uint8_t en[NTRK];                                   /* each track's level, 0..255 */
    uint16_t bx[NTRK], by[NTRK];                        /* .. and its body (cells, Q8) */
    uint16_t pulse;                                     /* the flow's push, Q8 */
    uint32_t f0, ms, rs, qr, steps, en_n, lastf, fav;   /* .. fav: the frames' interval, averaged (ms x 16) */
    uint8_t init, lite;                                 /* lite: frames slow (a full song): lighter until they recover */
    uint8_t bare;                                       /* TOTALITY being drawn: the whole screen KILN's, nothing darkened */
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
    int32_t x0 = (X >> 8) & (UMB_T - 1), y0 = (Y >> 8) & (UMB_T - 1), fx = X & 255, fy = Y & 255, x1 = (x0 + 1) & (UMB_T - 1),
            y1 = (y0 + 1) & (UMB_T - 1);
    int32_t a = umb_tn[y0 * UMB_T + x0], b = umb_tn[y0 * UMB_T + x1], c = umb_tn[y1 * UMB_T + x0], d = umb_tn[y1 * UMB_T + x1];
    return (a << 8) + (b - a) * fx + (c - a) * fy + (((a - b - c + d) * fx * fy) >> 8);
}
static int32_t umb_exp(int32_t i) { return i < 0 ? 255 : i > 255 ? 0 : umb_E[i]; }   /* 256 x exp(-i / 32) */
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
        umb.et[i] = fm1_ms - 600000u;
    umb.qr = umb_q.w;
    umb.ms = fm1_ms;
    umb.srj[0] = umb.srj[1] = umb.vrj[0] = umb.vrj[1] = -99;
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
/* a spark: a fresh reaction at a new spot (cells: across the field, between the knob row and the footer), a core of one chemical in a
 * ring of the next, its size the velocity v (0..256); for a played note (c a track) its event too, and a push to the
 * flow: a drum's large and bright, a melodic note's smaller and quieter */
static void umb_spark(uint32_t c, uint32_t lane, int32_t v)
{
    uint8_t *f3[3] = {umb.a, umb.b, umb.c};
    int32_t cx = (int32_t)(umb_rnd() % (UMB_N * 256u)), cy = umb.bare ? (int32_t)(UMB_N * 20 + umb_rnd() % (UMB_N * 216u)) :
             (int32_t)(UMB_N * 80 + umb_rnd() % (UMB_N * 150u));   /* (75 .. 215 px: clear of the knob row and the footer;
                                                         * TOTALITY: anywhere, 19 .. 221) */
    int32_t k1 = 500, k2 = 500, k3 = 500, k4 = 500, r, r2, x, y;
    uint32_t k = c % 3u, drum = c < NTRK && trk[c].engine == ENGI_DRUM, hat = drum && lane % NLANE == UMB_HATCL;
    if (c < NTRK) {
        k1 = umb_knob(c, 0);
        k2 = umb_knob(c, 1);
        k3 = umb_knob(c, 2);
        k4 = umb_knob(c, 3);
    }
    r = (drum ? 480 + v * 960 / 256 : 288 + v * 480 / 256) * (500 + k2) / 1000;   /* (cells, Q8; knob 2 its size) */
    if (hat)                                            /* (a closed hat: a small ring, UMB_HATK) */
        r = (r * UMB_HATK) >> 8;
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
        uint32_t q = umb.en_n++ % (umb.lite ? 6u : UMB_NE), p0;   /* (the oldest goes first; 6 when lite) */
        int32_t pos = (int32_t)(drum ? UMB_LANE_POS[lane % NLANE] : (c * 3u) % 8u) * 256 + (k1 - 500) * 2048 / 1000, f;
        pos &= 2047;                                    /* (eight places, Q8, round) */
        p0 = (uint32_t)pos >> 8;
        f = pos & 255;
        umb.er[q] = (uint8_t)(UMB_EVC[p0][0] + (((UMB_EVC[(p0 + 1u) & 7u][0] - UMB_EVC[p0][0]) * f) >> 8));
        umb.eg[q] = (uint8_t)(UMB_EVC[p0][1] + (((UMB_EVC[(p0 + 1u) & 7u][1] - UMB_EVC[p0][1]) * f) >> 8));
        umb.eb[q] = (uint8_t)(UMB_EVC[p0][2] + (((UMB_EVC[(p0 + 1u) & 7u][2] - UMB_EVC[p0][2]) * f) >> 8));
        umb.ex[q] = (int16_t)(cx * 4 / 256 + 2);
        umb.ey[q] = (int16_t)(cy * 4 / 256 + 2);
        umb.et[q] = fm1_ms;
        umb.ev[q] = (uint8_t)(drum ? 102 + v * 153 / 256 : 38 + v * 77 / 256);
        umb.er0[q] = (uint8_t)umb_clamp((((drum ? 7 : 3) * (400 + 16 * k2 / 10) * (154 + v * 102 / 256) / 256000) * (hat ? UMB_HATK : 256)) >> 8,
                                        2, 60);
        umb.ew[q] = (uint8_t)(drum ? (hat ? 2 + 4 * k2 / 1000 : 3 + 7 * k2 / 1000) : 0);   /* (a melodic note: no ring, a small
                                                         * flash where it lands; a closed hat's ring finer) */
        umb.eamp[q] = (uint16_t)((77 + 691 * k3 / 1000) * (drum ? 256 : 154) / 256);
        umb.espd[q] = (uint16_t)(drum ? ((1920 - 1632 * k4 / 1000) * (hat ? UMB_HATS : 256)) >> 8 : 0);   /* (a closed hat's
                                                         * slower: it fades small) */
        umb.bx[c] = (uint16_t)cx;
        umb.by[c] = (uint16_t)cy;
        v = drum ? 256 : 51 + v * 77 / 256;             /* a played note pushes the flow (a drum hardest) */
        umb.pulse = (uint16_t)(umb.pulse > v ? umb.pulse : v);
    }
}
/* the window's rows: row r's Moore averages (x 16), up and down clamped to the field (each column's three cells summed
 * once, then three columns at a time: no division) */
static void umb_mrow(int32_t r)
{
    const uint8_t *f3[3] = {umb.a, umb.b, umb.c};
    int32_t rm = (r > 0 ? r - 1 : 0) * UMB_N, r0 = r * UMB_N, rp = (r < UMB_N - 1 ? r + 1 : UMB_N - 1) * UMB_N, x, ch;
    uint16_t *m = umb.mr[r - (r / 5) * 5];
    for (ch = 0; ch < 3; ch++) {
        const uint8_t *f = f3[ch];
        int32_t col[UMB_N];
        for (x = 0; x < UMB_N; x++)
            col[x] = f[rm + x] + f[r0 + x] + f[rp + x];
        for (x = 0; x < UMB_N; x++) {
            int32_t s = col[x ? x - 1 : UMB_N - 1] + col[x] + col[x < UMB_N - 1 ? x + 1 : 0];
            m[x * 3 + ch] = (uint16_t)((s * 455) >> 8);   /* (x 16 / 9) */
        }
    }
}
/* the three chemicals' Moore averages at X, Y (cells, Q8) from the window rows w (y - 2 .. y + 2 as Y's row y), 0..255
 * into o */
static void umb_mbil3(int32_t X, int32_t Y, int32_t y, const uint16_t *const *w, int32_t *o)
{
    int32_t xi = X >> 8, yi = (Y >> 8) - y + 2, fx = X & 255, fy = Y & 255, ch;
    int32_t x0 = (xi < 0 ? xi + UMB_N : xi >= UMB_N ? xi - UMB_N : xi) * 3, x1 = x0 + 3 == UMB_N * 3 ? 0 : x0 + 3;
    const uint16_t *m0 = w[yi], *m1 = w[yi + 1];
    for (ch = 0; ch < 3; ch++) {
        int32_t a = m0[x0 + ch], b = m0[x1 + ch], c = m1[x0 + ch], d = m1[x1 + ch];
        o[ch] = (a + (((b - a) * fx) >> 8) + (((c - a) * fy) >> 8) + (((a - b - c + d) * fx * fy) >> 16)) >> 4;
    }
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
    for (y = 0; y < UMB_N; y++) {
        int32_t yb = y < UMB_N - 1 ? y + 1 : UMB_N - 1, ya = y > 0 ? y - 1 : 0, k5;
        const uint16_t *w[5], *m, *mb, *ma;
        const uint8_t *n0, *nb, *na_;
        while (top <= y + 2 && top < UMB_N)             /* the window: Moore rows up to y + 2 (from rows not yet written) */
            umb_mrow(top++);
        while (rtop <= y + 1 && rtop < UMB_N) {         /* .. and the noise rows up to y + 1 */
            uint8_t *n = umb.nr[rtop - (rtop / 3) * 3];
            for (x = 0; x < UMB_N; x++)
                n[x] = (uint8_t)umb_noise(x, rtop);
            rtop++;
        }
        for (k5 = 0; k5 < 5; k5++) {                    /* (the window's rows y - 2 .. y + 2, clamped: once a row) */
            int32_t r = umb_clamp(y - 2 + k5, 0, UMB_N - 1);
            w[k5] = umb.mr[r - (r / 5) * 5];
        }
        m = w[2];
        mb = umb.mr[yb - (yb / 5) * 5];
        ma = umb.mr[ya - (ya / 5) * 5];
        n0 = umb.nr[y - (y / 3) * 3];
        nb = umb.nr[yb - (yb / 3) * 3];
        na_ = umb.nr[ya - (ya / 3) * 3];
        for (x = 0; x < UMB_N; x++) {
            int32_t xm = x ? x - 1 : UMB_N - 1, xp = x < UMB_N - 1 ? x + 1 : 0, j = y * UMB_N + x;
            int32_t ax = m[xp * 3] - m[xm * 3], ay = mb[x * 3] - ma[x * 3];
            int32_t nf = n0[x], nfx = (n0[xp] - n0[xm]) >> 1, nfy = (nb[x] - na_[x]) >> 1;
            int32_t dx = (((((ax * ct - ay * st) >> 8) * k) >> 4) * 257 >> 16) - ((nfy * gr) >> 5);   /* its own slope, */
            int32_t dy = (((((ax * st + ay * ct) >> 8) * k) >> 4) * 257 >> 16) + ((nfx * gr) >> 5);   /* turned; the
                                                         * noise's curl (Q8 cells; x 257 >> 20: / 4080) */
            int32_t A, B, C, na, nb, nc, ra, rb, abc[3];
            dx = umb_clamp(dx, -511, 511);              /* (under 2 cells: the window holds it) */
            dy = umb_clamp(dy, -511, 511);
            umb_mbil3(x * 256 - dx, y * 256 - dy, y, w, abc);
            A = abc[0];
            B = abc[1];
            C = abc[2];
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
/* the events' constants for this frame: strength with its fade (fe) and the flash's (fc), the ring's radius (R, px) and
 * its square, the reach (o2), the flash's and the tint's reach (c2, t2), and the factors (Q12 / Q16) the profiles use */
static void umb_events(void)
{
    uint32_t q;
    for (q = 0; q < UMB_NE; q++) {
        int32_t age = (int32_t)(fm1_ms - umb.et[q]), life = uf.life * 50, R0 = umb.er0[q], w = umb.ew[q], R, Rt;   /* (ms) */
        ue.on[q] = age >= 0 && age < life && (!umb.lite || (w && q < 6u));   /* (lite: the drums' rings only) */
        if (!ue.on[q])
            continue;
        if (!w) {                                       /* a melodic note's flash: no ring, a short glow, its tint */
            ue.fe[q] = age * 2 < life ? (umb.ev[q] * (256 - age * 512 / life)) >> 8 : 0;
            ue.fc[q] = ue.fe[q];
            ue.on[q] = ue.fe[q] > 4;
            ue.R[q] = ue.R2[q] = ue.iu[q] = ue.kw[q] = ue.iR[q] = ue.w4[q] = ue.ro2[q] = ue.ri2[q] = 0;
            ue.o2[q] = ue.c2[q] = 9 * R0 * R0;
            ue.t2[q] = 8 * R0 * R0;
            ue.kc[q] = 2097152 / (R0 * R0);
            ue.cb2[q] = (R0 * R0 * UMB_DOME * UMB_DOME) >> 8;
            ue.kb[q] = 2097152 / (ue.cb2[q] + 1);
            ue.kg[q] = ((8192 * (int32_t)umb.eamp[q]) >> 8) / (R0 * R0);
            ue.kt[q] = 2097152 / (R0 * R0);
            continue;
        }
        ue.fe[q] = (umb.ev[q] * (256 - age * 256 / life)) >> 8;
        ue.fc[q] = age * 20 < life * 7 ? (umb.ev[q] * (256 - age * 5120 / (life * 7 + 1))) >> 8 : 0;
        R = R0 + ((age * umb.espd[q] * 41) >> 19);      /* (x 41 >> 19: / 50 ms / 256) */
        if (R > 120 || ue.fe[q] < 6) {                  /* (spread past half the screen, or nearly gone: done) */
            ue.on[q] = 0;
            continue;
        }
        Rt = R + w;
        ue.R[q] = R;
        ue.R2[q] = R * R;
        ue.o2[q] = (R + 3 * w + R0) * (R + 3 * w + R0);
        ue.c2[q] = 9 * R0 * R0;
        ue.t2[q] = 8 * Rt * Rt;
        ue.iu[q] = 32768 / (R > 2 ? R : 2);             /* u (px, Q4) = (d2 - R2) x iu >> 12: no square root */
        ue.kw[q] = 8192 / (w * w);                      /* the ring's profile and its slope */
        ue.iR[q] = 4096 / (R > 2 ? R : 2);              /* (its direction: dx / R near the ring) */
        ue.kc[q] = 2097152 / (R0 * R0);                 /* the flash's profile (x 32 / R0^2, Q16) */
        ue.cb2[q] = (R0 * R0 * UMB_DOME * UMB_DOME) >> 8;   /* .. its dome: out to UMB_DOME R0, then nothing */
        ue.kb[q] = 2097152 / (ue.cb2[q] + 1);
        ue.kg[q] = ((8192 * (int32_t)umb.eamp[q]) >> 8) / (R0 * R0);   /* .. its slope (Q12) */
        ue.kt[q] = 2097152 / (Rt * Rt);                 /* the tint's */
        ue.w4[q] = 64 * w;                              /* (beyond 4 widths the ring is nothing) */
        ue.ro2[q] = (R + w + 1) * (R + w + 1);          /* (its glow, half as wide: beyond a width under 2 %) */
        ue.ri2[q] = R > w + 1 ? (R - w - 1) * (R - w - 1) : 0;
    }
}
/* once a UI frame: the levels, the notes played since (one spark a track and drum sound: a chord, its loudest note),
 * then the steps owed (20 a second; 10, each as two, when the load is lightened), never more than four; the rings
 * are timed by the clock, so they spread smoothly at whatever rate the screen draws */
static void umb_tick(void)
{
    uint32_t n = 0, c, l, dt = (sys_load || gov_lite || umb.lite) ? 100u : 50u;   /* (20 steps a second; 10 when lite) */
    uint8_t vm[NTRK][NLANE];
    if (!umb.init)
        umb_start();
    if (umb.f0 == ui.frame)
        return;
    umb.f0 = ui.frame;
    {                                                   /* the governor: the frames' interval, averaged; over 55 ms
                                                         * (a full song taking the core) lite, under 38 ms full again */
        uint32_t dtf = fm1_ms - umb.lastf;
        umb.lastf = fm1_ms;
        if (dtf > 500u)
            dtf = 500u;
        umb.fav = umb.fav - (umb.fav >> 3) + dtf * 2u;
        if (umb.fav > 55u * 16u)
            umb.lite = 1;
        else if (umb.fav < 38u * 16u)
            umb.lite = 0;
    }
    umb.srj[0] = umb.srj[1] = umb.vrj[0] = umb.vrj[1] = -99;
    umb_consts();
    umb_events();
    umb_levels();
    if (umb_q.w - umb.qr > UMB_NQ)                      /* (a burst: the newest only) */
        umb.qr = umb_q.w - UMB_NQ;
    for (c = 0; c < NTRK; c++)
        for (l = 0; l < NLANE; l++)
            vm[c][l] = 0;
    for (; umb.qr != umb_q.w; umb.qr++) {
        uint32_t q = umb.qr % UMB_NQ, v = umb_q.vel[q] * 2u + 1u;
        c = umb_q.c[q];
        if (c >= NTRK || !umb_q.vel[q])                 /* (velocity 0: a note let go, FILAMENT's) */
            continue;
        l = trk[c].engine == ENGI_DRUM ? drum_lane(umb_q.note[q]) % NLANE : 0u;
        if (v > vm[c][l])
            vm[c][l] = (uint8_t)(v > 255u ? 255u : v);
    }
    for (c = 0; c < NTRK; c++)
        for (l = 0; l < NLANE; l++)
            if (vm[c][l] && !eco_off(c))
                umb_spark(c, l, vm[c][l]);
    while ((uint32_t)(fm1_ms - umb.ms) >= dt && n < 4u) {   /* (up to four: slow frames keep real time) */
        umb.ms += dt;
        umb_step(dt / 50u);
        n++;
    }
    if ((uint32_t)(fm1_ms - umb.ms) >= dt)
        umb.ms = fm1_ms;                                /* (behind: drop the steps, never catch up in a burst) */
}
/* the field sharpened (its Laplacian, UMB_SHARP) at cell x, y (in the field) */
static int32_t umb_vs(int32_t x, int32_t y)
{
    int32_t xm = x ? x - 1 : UMB_N - 1, xp = x < UMB_N - 1 ? x + 1 : 0, ym = y ? y - 1 : 0, yp = y < UMB_N - 1 ? y + 1 : y;
    const uint8_t *ar = umb.a + y * UMB_N;
    int32_t v = ar[x];
    return umb_clamp(v + (((4 * v - ar[xm] - ar[xp] - umb.a[ym * UMB_N + x] - umb.a[yp * UMB_N + x]) * UMB_SHARP) >> 8), 0, 255);
}
/* row j of the field sharpened (-1 .. 60; up and down it stops at the edge), two rows kept (by parity): each pixel's
 * tone is worked out from them (always the field's own cells, 4 px apart: lite lights fewer, never coarsens the lava) */
static const uint8_t *umb_vrow(int32_t j)
{
    uint32_t s = (uint32_t)j & 1u;
    if (umb.vrj[s] != j) {
        int32_t y = umb_clamp(j, 0, UMB_N - 1), i;
        umb.vrj[s] = (int16_t)j;
        umb.vr[s][0] = (uint8_t)umb_vs(UMB_N - 1, y);
        umb.vr[s][UMB_N + 1] = (uint8_t)umb_vs(0, y);
        for (i = 0; i < UMB_N; i++)
            umb.vr[s][i + 1] = (uint8_t)umb_vs(i, y);
    }
    return umb.vr[s];
}
/* a cell centre lit (cell i, j: across it wraps, up and down it stops at the edge; px, py its place on the screen,
 * unwrapped, for the events) into o: the shade (Q7) and the light added (RGB: the gloss, the tint the events give the
 * lava); vg darkens it under the knob row and the footer. The events' glow and the flashes' gloss are drawn a pixel pair
 * (umb_glow): here only their relief */
static void umb_sample(int32_t i, int32_t j, int32_t vg, uint8_t *o)
{
    static const int32_t LX = -128, LY = -159, LZ = 154, HX = -72, HY = -89, HZ = 229;   /* the light; half vector (Q8) */
    int32_t x = i < 0 ? i + UMB_N : i >= UMB_N ? i - UMB_N : i, y = umb_clamp(j, 0, UMB_N - 1), px = i * 4 + 2, py = j * 4 + 2;
    int32_t xm = x ? x - 1 : UMB_N - 1, xp = x < UMB_N - 1 ? x + 1 : 0, ym = y ? y - 1 : 0, yp = y < UMB_N - 1 ? y + 1 : y;
    const uint8_t *a = umb.a, *ar = a + y * UMB_N;
    int32_t n2, rs, nx, ny, fx = 0, fy = 0, dif, sp, sh, tw = 0, tr = 0, tg = 0, tb = 0, Ar = 0, Ag = 0, Ab = 0;
    uint32_t q;
    nx = -(((ar[xp] - ar[xm]) * uf.depth) >> 5);        /* the relief's slope: the normal (Q8) */
    ny = -(((a[yp * UMB_N + x] - a[ym * UMB_N + x]) * uf.depth) >> 5);
    for (q = 0; q < UMB_NE; q++) {                      /* the events: a flash at the heart, then a ring that rises out
                                                         * of the relief and spreads; the lava tinted */
        int32_t dx, dy, d2, u4, ring, core, dh, e;
        if (!ue.on[q])
            continue;
        dx = px - umb.ex[q];
        dy = py - umb.ey[q];
        d2 = dx * dx + dy * dy;
        if (d2 > ue.o2[q])
            continue;
        u4 = ((d2 - ue.R2[q]) * ue.iu[q]) >> 12;      /* (px from the ring, Q4: (d2 - R^2) / 2R, near the ring) */
        if (u4 > -ue.w4[q] && u4 < ue.w4[q]) {
            ring = (ue.fe[q] * umb_exp((u4 * u4 * ue.kw[q]) >> 16)) >> 8;
            dh = umb_clamp((((-ring * u4 * ue.kw[q]) >> 16) * umb.eamp[q]) >> 8, -2000, 2000);
            nx -= (dh * dx * ue.iR[q]) >> 12;
            ny -= (dh * dy * ue.iR[q]) >> 12;
        }
        if (ue.fc[q] && d2 < ue.c2[q]) {               /* the flash: exp(-d2 / R0^2), its slope -2 dx / R0^2 */
            core = (ue.fc[q] * umb_exp((d2 * ue.kc[q]) >> 16)) >> 8;
            fx += (core * dx * ue.kg[q]) >> 12;         /* (its gloss is drawn a pixel pair: umb_glow) */
            fy += (core * dy * ue.kg[q]) >> 12;
        }
        if (!umb.lite && d2 < ue.t2[q]) {
            e = (ue.fe[q] * umb_exp((d2 * ue.kt[q]) >> 16)) >> 8;
            tw += e;
            tr += umb.er[q] * e;
            tg += umb.eg[q] * e;
            tb += umb.eb[q] * e;
        }
    }
    nx = umb_clamp(nx, -16000, 16000);
    ny = umb_clamp(ny, -16000, 16000);
    n2 = nx * nx + ny * ny;                             /* 1 / |normal| from the tables (Q8) */
    rs = n2 < 262144 ? umb_rsa[n2 >> 10] : n2 < 16777216 ? umb_rsb[n2 >> 16] : umb_rsb[255];
    sp = umb_clamp((((nx * HX + ny * HY + HZ * 256) >> 8) * rs) >> 8, 0, 256);   /* (the gloss: the lava's, the rings') */
    if (fx | fy) {                                      /* (the shade: the flashes' too) */
        nx = umb_clamp(nx + fx, -16000, 16000);
        ny = umb_clamp(ny + fy, -16000, 16000);
        n2 = nx * nx + ny * ny;
        rs = n2 < 262144 ? umb_rsa[n2 >> 10] : n2 < 16777216 ? umb_rsb[n2 >> 16] : umb_rsb[255];
    }
    dif = umb_clamp((((nx * LX + ny * LY + LZ * 256) >> 8) * rs) >> 8, 0, 256);
    sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8;
    sp = (sp * vg) >> 8;
    sh = ((UMB_SH0 + ((dif * UMB_SH1) >> 8)) * vg) >> 9;   /* (Q7: 0 .. 170; the relief's contrast: dark faces 40, lit 340) */
    if (tw > 2) {                                       /* the lava under an event takes its colour: the shade lent to it */
        const uint8_t *P = umb_tl + umb_vs(x, y) * 3;
        int32_t m = tw > 192 ? 192 : tw, iw = 65536 / tw;
        int32_t l = ((((P[0] + P[1] + P[2]) * sh) >> 7) * 582) >> 10;   /* (its light: x 256 / 450) */
        int32_t k = (l * m) >> 8;
        Ar = (((tr * iw) >> 16) * k) >> 8;
        Ag = (((tg * iw) >> 16) * k) >> 8;
        Ab = (((tb * iw) >> 16) * k) >> 8;
        sh = (sh * (256 - m)) >> 8;
    }
    Ar += (236 * sp) >> 8;                              /* (the gloss: white-teal) */
    Ag += (250 * sp) >> 8;
    Ab += (246 * sp) >> 8;
    o[0] = (uint8_t)sh;
    o[1] = (uint8_t)(Ar > 255 ? 255 : Ar);
    o[2] = (uint8_t)(Ag > 255 ? 255 : Ag);
    o[3] = (uint8_t)(Ab > 255 ? 255 : Ab);
}
/* row j of cell centres (-1 .. 60), centres lo .. hi of it lit, kept two rows at a time (a slot by parity: rows j and
 * j + 1 never share one) and only what a strip needs: strips side by side (the knob row's) light each centre once */
static const uint8_t *umb_row(int32_t j, int32_t lo, int32_t hi, int32_t m)
{
    uint32_t s = (uint32_t)j & 1u;
    int32_t yc = j * m * 4 + 2, vg = umb.bare ? 256 : yc < 56 ? 108 : yc < 76 ? 108 + (yc - 56) * 7 : yc >= 226 ? 128 : 256, i;
    if (umb.srj[s] != j) {
        umb.srj[s] = (int16_t)j;
        umb.srlo[s] = (int16_t)(hi + 1);                /* (none lit) */
        umb.srhi[s] = (int16_t)hi;
    }
    for (i = lo; i < umb.srlo[s]; i++)                  /* (m: 1 a cell; 2 every other cell, when lite) */
        umb_sample(i * m, j * m, vg, umb.sr[s] + (i + 1) * UMB_C);
    for (i = umb.srhi[s] + 1; i <= hi; i++)
        umb_sample(i * m, j * m, vg, umb.sr[s] + (i + 1) * UMB_C);
    if (lo < umb.srlo[s])
        umb.srlo[s] = (int16_t)lo;
    if (hi > umb.srhi[s])
        umb.srhi[s] = (int16_t)hi;
    return umb.sr[s];
}
/* the events' glow on pixel row py (vg its darkening), a pixel pair a slot from x0 (np pairs) into gl (RGB): each ring a
 * fine line (half its relief's width) over its band (R +- 1 width: beyond, under 2 %), and each flash, worked out there,
 * so a ring is as fine as the screen; 0: nothing on this row (gl untouched) */
static int32_t umb_glow(int32_t py, int32_t x0, int32_t np, int32_t vg, int16_t *gl)
{
    uint32_t q;
    int32_t any = 0, k;
    for (q = 0; q < UMB_NE; q++) {
        int32_t dy, dy2, cr, cg, cb, sa[3], sb[3], sk[3], ns = 0, n, ex = umb.ex[q];
        if (!ue.on[q])
            continue;
        dy = py - umb.ey[q];
        dy2 = dy * dy;
        if (dy2 >= ue.o2[q])
            continue;
        if (ue.ro2[q] > dy2) {                          /* the ring: its band left and right of its hole (or across) */
            int32_t xo = (int32_t)umb_isqrt((uint32_t)(ue.ro2[q] - dy2));
            int32_t xi = ue.ri2[q] > dy2 ? (int32_t)umb_isqrt((uint32_t)(ue.ri2[q] - dy2)) : -1;
            sk[ns] = 0;
            sa[ns] = ex - xo;
            sb[ns++] = xi < 0 ? ex + xo : ex - xi;
            if (xi >= 0) {
                sk[ns] = 0;
                sa[ns] = ex + xi;
                sb[ns++] = ex + xo;
            }
        }
        if (ue.fc[q] && dy2 < ue.cb2[q]) {              /* the flash */
            int32_t xc = (int32_t)umb_isqrt((uint32_t)(ue.cb2[q] - dy2));
            sk[ns] = 1;
            sa[ns] = ex - xc;
            sb[ns++] = ex + xc;
        }
        if (!ns)
            continue;
        if (!any) {
            for (k = 0; k < np * 3; k++)
                gl[k] = 0;
            any = 1;
        }
        cr = (umb.er[q] * vg) >> 8;
        cg = (umb.eg[q] * vg) >> 8;
        cb = (umb.eb[q] * vg) >> 8;
        for (n = 0; n < ns; n++) {
            int32_t pa = (sa[n] - x0) >> 1, pb = (sb[n] - x0) >> 1, p, dx2, d2q, f, kk, r2 = 4 * ue.R2[q], iu = ue.iu[q];
            int16_t *o;
            if (pa < 0)
                pa = 0;
            if (pb > np - 1)
                pb = np - 1;
            dx2 = 2 * (x0 + 2 * pa) + 1 - 2 * ex;       /* (the pair's centre, x 2: d2 x 4; a pair on, + 4) */
            d2q = dx2 * dx2 + 4 * dy2;
            f = sk[n] ? (ue.fc[q] * UMB_DOMEK) >> 8 : (ue.fe[q] * 5) >> 2;   /* (the ring: half as wide (x 4), a little
                                                         * brighter, finer) */
            kk = sk[n] ? ue.kb[q] : ue.kw[q];
            for (p = pa, o = gl + pa * 3; p <= pb; p++, o += 3, d2q += 8 * dx2 + 16, dx2 += 4) {
                int32_t i, g;
                if (sk[n]) {
                    int32_t c, hx, hy, h2, hr, sp;
                    i = (d2q * kk) >> 18;               /* (d2 / its edge^2, of 32: the dome's upper half, cut there:
                                                         * a disc brightest at its heart, its edge sharp) */
                    if (i > 32)
                        continue;
                    g = (f * umb_D[i]) >> 8;
                    c = (ue.fc[q] * umb_exp((d2q * ue.kc[q]) >> 18)) >> 8;   /* its gloss: the bump's slope here, lit */
                    hx = (((c * dx2) >> 1) * ue.kg[q]) >> 12;
                    hy = ((c * dy) * ue.kg[q]) >> 12;
                    hx = umb_clamp(hx, -16000, 16000);
                    hy = umb_clamp(hy, -16000, 16000);
                    h2 = hx * hx + hy * hy;
                    hr = h2 < 262144 ? umb_rsa[h2 >> 10] : h2 < 16777216 ? umb_rsb[h2 >> 16] : umb_rsb[255];
                    sp = umb_clamp((((hx * -72 + hy * -89 + 229 * 256) >> 8) * hr) >> 8, 0, 256);
                    sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8; sp = (sp * sp) >> 8;
                    sp = (sp * ((ue.fc[q] * vg) >> 8)) >> 8;
                    o[0] = (int16_t)(o[0] + ((236 * sp) >> 8));
                    o[1] = (int16_t)(o[1] + ((250 * sp) >> 8));
                    o[2] = (int16_t)(o[2] + ((246 * sp) >> 8));
                } else {
                    int32_t u4 = ((d2q - r2) * iu) >> 14;
                    i = (u4 * u4 * kk) >> 14;
                    if (i > 255)
                        continue;
                    g = (f * umb_E[i]) >> 8;
                }
                o[0] = (int16_t)(o[0] + ((cr * g) >> 9));
                o[1] = (int16_t)(o[1] + ((cg * g) >> 9));
                o[2] = (int16_t)(o[2] + ((cb * g) >> 9));
            }
        }
    }
    return any;
}
#include "ui_umbra_melody.c"                          /* SAROS and FILAMENT, the melody plates (MENU > SCREEN 2 > PLATE) */
/* the canvas as the screen's x0, y0 .. : KILN. Each pixel's tone from the field blended there from the four cells round
 * it (so the lava's edges are a pixel sharp); the shade and the light added blended a 2 x 2 px block from the lit centres
 * (soft: they vary over cells; 4 px apart, 8 when lite); the events' glow a pixel pair; RGB565 with a grain (a 16 x 16
 * blue-noise dither, UMB_GRAIN) */
static void umb_backdrop(int32_t x0, int32_t y0)
{
    uint16_t vv[UMB_N + 3], sv[(UMB_N + 3) * UMB_C];   /* a pixel row's cells blended up and down (x 256): the field ..
                                                         * .. and (every other row) the lit centres' shade and light */
    uint8_t sb[121 * UMB_C], xi[240], xf[240], pf[121], dk[UMB_N + 3];
    uint16_t pi[121];   /* a block's shade and light; each
                                                         * pixel's cell and blend, each pair's centre; the lava's base */
    int16_t gl[121 * 3], tx[240];                       /* a pair's glow; a pixel's surface */
    int32_t np = ((int32_t)cv_w + 1) >> 1, m, sh, lo, hi, n, lv, nv, x, p, kt;
    uint32_t cx, cy;
    if (umb_plate % 3u) {                               /* SAROS, FILAMENT */
        umm_backdrop(umb_plate % 3u, x0, y0);
        return;
    }
    umm_on = 0;
    umb_tick();
    kt = umb.lite ? 0 : UMB_TEX + ((uf.gr * 48) >> 8);  /* (the surface's depth: GRAIN's; lite: none) */
    m = umb.lite ? 2 : 1;                               /* (the lit centres 4 px apart; 8 when lite) */
    sh = umb.lite ? 4 : 5;
    lo = ((2 * x0 - 3) << sh) >> 8;                     /* (the lit centres the strip needs: lo .. hi) */
    hi = (((2 * (x0 + (int32_t)cv_w - 1) - 3) << sh) >> 8) + 1;
    n = hi - lo + 1;
    lv = ((2 * x0 - 3) << 5) >> 8;                      /* (.. and the cells: lv .. lv + nv - 1) */
    nv = ((((2 * (x0 + (int32_t)cv_w - 1) - 3) << 5) >> 8) + 1) - lv + 1;
    for (x = 0; x < (int32_t)cv_w; x++) {
        int32_t t = (2 * (x0 + x) - 3) << 5;
        xi[x] = (uint8_t)((t >> 8) - lv);
        xf[x] = (uint8_t)(t & 255);
    }
    for (p = 0; p < np; p++) {
        int32_t t = (2 * (x0 + 2 * p) - 2) << sh;
        pi[p] = (uint16_t)(((t >> 8) - lo) * UMB_C);
        pf[p] = (uint8_t)(t & 255);
    }
    for (cy = 0; cy < cv_h; cy++) {
        int32_t py = y0 + (int32_t)cy, ty = (2 * py - 3) << sh, j0 = ty >> 8, fy = ty & 255, tv = (2 * py - 3) << 5, k, any;
        int32_t vg = umb.bare ? 256 : py < 56 ? 108 : py < 76 ? 108 + (py - 56) * 7 : py >= 226 ? 128 : 256;
        const uint8_t *v0 = umb_vrow(tv >> 8) + lv + 1, *v1 = umb_vrow((tv >> 8) + 1) + lv + 1;
        uint16_t *row = cv_px + cy * cv_w;
        const uint8_t *bq = umb_bn + ((py & 15) << 4);
        int8_t dr[16], dg[16];                          /* (this row's grain: the blue noise at its reach) */
        for (k = 0; k < 16; k++) {
            dr[k] = (int8_t)(((bq[k] - 128) * UMB_GRAIN) >> 8);
            dg[k] = (int8_t)(dr[k] >> 1);
        }
        if (kt) {                                       /* this row's surface: the noise table sampled fine, two ways
                                                         * turned (2.4 and 1.6 texels a pixel), drifting with the layers */
            uint32_t u1 = (uint32_t)(x0 * 125829 + py * 94372) + ((uint32_t)uf.oB << 8), v1 = (uint32_t)(py * 125829 - x0 * 94372);
            uint32_t u2 = (uint32_t)(py * 100663 - x0 * 29360) - ((uint32_t)uf.oA << 8);
            uint32_t v2 = (uint32_t)(-x0 * 100663 - py * 29360) + 1234567u;
            for (x = 0; x < (int32_t)cv_w; x++, u1 += 125829u, v1 -= 94372u, u2 -= 29360u, v2 -= 100663u)
                tx[x] = (int16_t)(umb_tn[((v1 >> 16) & (UMB_T - 1u)) * UMB_T | ((u1 >> 16) & (UMB_T - 1u))] +
                                  umb_tn[((v2 >> 16) & (UMB_T - 1u)) * UMB_T | ((u2 >> 16) & (UMB_T - 1u))] -
                                  256);
        }
        for (k = 0, fy = tv & 255; k < nv; k++)
            vv[k] = (uint16_t)((v0[k] << 8) + (v1[k] - v0[k]) * fy);
        for (k = 0; k + 1 < nv; k++)                    /* (between cells k and k + 1 the lava's base: one tone) */
            dk[k] = (uint8_t)(vv[k] < UMB_T0 * 256 && vv[k + 1] < UMB_T0 * 256);
        if (!cy || !(py & 1)) {                         /* the block's (rows py, py + 1: their centre, the same centres) */
            const uint8_t *r0 = umb_row(j0, lo, hi, m) + (lo + 1) * UMB_C, *r1 = umb_row(j0 + 1, lo, hi, m) + (lo + 1) * UMB_C;
            int32_t fb = py & 1 ? ty & 255 : (ty & 255) + (1 << sh), c;
            for (k = 0; k < n * UMB_C; k++)
                sv[k] = (uint16_t)((r0[k] << 8) + (r1[k] - r0[k]) * fb);
            for (p = 0; p < np; p++) {
                const uint16_t *s0 = sv + pi[p];
                for (c = 0; c < UMB_C; c++)
                    sb[p * UMB_C + c] = (uint8_t)((s0[c] + (((s0[c + UMB_C] - s0[c]) * pf[p]) >> 8)) >> 8);
            }
        }
        any = umb_glow(py, x0, np, vg, gl);
        for (cx = 0, p = 0; cx < cv_w; cx += 2, p++) {
            const uint8_t *b = sb + p * UMB_C;
            int32_t S = b[0], Ar = b[1], Ag = b[2], Ab = b[3], k2, kS = (S * kt) >> 7;   /* (kS: the surface lit) */
            if (any) {
                Ar += gl[p * 3];
                Ag += gl[p * 3 + 1];
                Ab += gl[p * 3 + 2];
            }
            if (dk[xi[cx]] && (cx + 1u >= cv_w || dk[xi[cx + 1u]])) {   /* (the base, gently graded: a pair one tone, but
                                                         * its dither) */
                int32_t kq = xi[cx], v = (vv[kq] + (((vv[kq + 1] - vv[kq]) * (xf[cx] + 32)) >> 8)) >> 8;
                const uint8_t *P = umb_tl + v * 3;
                Ar += (P[0] * S) >> 7;
                Ag += (P[1] * S) >> 7;
                Ab += (P[2] * S) >> 7;
                for (k2 = 0; k2 < 2 && cx + (uint32_t)k2 < cv_w; k2++) {
                    int32_t d = (x0 + (int32_t)cx + k2) & 15, R = Ar + dr[d], G = Ag + dg[d], B = Ab + dr[d];
                    UMB_TX(cx + (uint32_t)k2)
                    R = umb_clamp(R, 0, 255);
                    G = umb_clamp(G, 0, 255);
                    B = umb_clamp(B, 0, 255);
                    row[cx + (uint32_t)k2] = swap16((uint32_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
                }
                continue;
            }
            for (k2 = 0; k2 < 2 && cx + (uint32_t)k2 < cv_w; k2++) {   /* (a pixel: its tone, its own dither) */
                uint32_t xq = cx + (uint32_t)k2;
                int32_t kq = xi[xq], v = (vv[kq] + (((vv[kq + 1] - vv[kq]) * xf[xq]) >> 8)) >> 8;
                const uint8_t *P = umb_tl + v * 3;
                int32_t d = (x0 + (int32_t)xq) & 15;
                int32_t R = ((P[0] * S) >> 7) + Ar + dr[d], G = ((P[1] * S) >> 7) + Ag + dg[d], B = ((P[2] * S) >> 7) + Ab + dr[d];
                UMB_TX(xq)
                R = umb_clamp(R, 0, 255);
                G = umb_clamp(G, 0, 255);
                B = umb_clamp(B, 0, 255);
                row[xq] = swap16((uint32_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
            }
        }
    }
}
/* TOTALITY (HOME > ECO's third page, ui_lens.c CE_HOME): the visual alone, over the whole screen: no header, tabs, knobs,
 * words or footer (ui_draw.c ui_draw_body draws nothing else); two strips, sent where they changed (gfx.c cv_blit_cells);
 * under LOAD LITE / ECO every 50 / 70 ms, as HUD's HOME. The keys, knobs and buttons work as on ECO */
static int umb_bare(void) { return ui.home && !gen_on() && !ui.hlv && ui.hset[0] == 2u; }
static void umb_full(void)
{
    static uint32_t ms;
    uint32_t y;
    if (sys_load && !ui.force && fm1_ms - ms < (sys_load == LOAD_ECO ? 70u : 50u))
        return;
    ms = fm1_ms;
    dc_frame(ui.force);
    umb.bare = 1;
    for (y = 0; y < 240u; y += 120u) {
        cv_begin(240, 120, T_BG);
        umb_backdrop(0, (int32_t)y);
        cv_blit_cells(0, y);
    }
    umb.bare = 0;
}
