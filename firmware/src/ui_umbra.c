/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* UMBRA OBSCURA: HOME's plates in place of the ecosystem. Included by ui_lzhome.c before eco_draw.
 *
 * Six plates (SELECT in TOTALITY), one drawn at a time, all from the same ring of notes (voice.c trk_note_on's
 * umb_q: keys, sequencer, arp, MIDI; a note let go: velocity 0):
 *   0 CINDER   cold embers: a heat field that rises and cools under a crust of cracks      (ui_umbra_field.c)
 *   1 SAROS    the melody as string art on the circle of fifths                             (ui_umbra_melody.c)
 *   2 FILAMENT a taut string a track, plucked and bowed                                     (ui_umbra_melody.c)
 *   3 GRANULE  the sun's surface: a honeycomb of granules, a note lights its own            (ui_umbra_field.c)
 *   4 CORONA   the eclipse: the moon's disc, the corona's streamers fed by the notes        (ui_umbra_field.c)
 *   5 TARN     the eclipse reflected in dark water, a note a drop                           (ui_umbra_field.c)
 * KILN (after paketa12's "lamp is lava": a BZ reaction, lit as a relief) was the first plate, in CINDER's place: it changed
 * the whole screen every frame (HOME's changed-cells blit sent it all, ~35 ms of SPI a frame) and took ~10 KB of the slot.
 * These are built to the FM-1's limits: a byte field or none, a table read and a palette read a pixel, no lighting pass, fades
 * in steps, still between notes, so the blit (gfx.c cv_blit_cells) sends little. Their state shares one block of the pool
 * (umb's union): a plate chosen starts afresh. TOTALITY (ui_umbra_tot.c), a HOME tap on HOME, is the plate alone over the
 * whole screen, KNOB 1..4 the macros M1..M4 (MACROS - AUDIO and MACROS - VISUALS send them on: ui_umbra_mac.c), FX its
 * visual effects. */
#define UMB_ON 1
#define UMB_N 60                                        /* a field's side: 60 x 60 cells of 4 x 4 px */
#define UMB_S (UMB_N * UMB_N)
#define UMB_T 32                                        /* the noise table: 32 x 32, tiling */
#define UMP_NPL 6u                                      /* the plates */
/* (set on HOME > ECO > PLATE until 2026-11-09; TOTALITY turns them) the plate's own three
 * settings and LIGHT (one for all), kept with the settings in spare favourite bytes (ui.c; settings_persist.c): the
 * plate engine 15's [31] (bit 7: these bytes laid out so), the plates' first two [15][4 + 2 x plate ..] (4 .. 15: bytes
 * 4 .. 12 the hints' before 0.9.5, 13 .. 16 free), their third TRIO's retired row's [6][8 + plate] (bytes 8 .. 18 always
 * 0, bt.c), LIGHT [15][16], each as the value ^ its default (0, never set: the default). The third: CINDER's SWAY, SAROS's,
 * CORONA's and TARN's SIZE (the eclipse's: a quarter .. twice), FILAMENT's SPREAD, GRANULE's GLOW (umv_sc) */
#define umb_plate (favorites.factory[15][31])
static const char *const UMV_PL[UMP_NPL] = {"CINDER", "SAROS", "FILAMENT", "GRANULE", "CORONA", "TARN"};
static const char *const UMV_N[UMP_NPL][3] = {{"RISE", "COOL", "SWAY"}, {"SPIN", "FADE", "SIZE"}, {"SWING", "RING", "SPREAD"},
                                              {"BOIL", "FADE", "GLOW"}, {"REACH", "FADE", "SIZE"}, {"DRAG", "BEND", "SIZE"}};
static const uint8_t UMV_D[UMP_NPL][2] = {{45, 50}, {30, 45}, {50, 50}, {35, 45}, {50, 45}, {55, 50}};   /* (the third: 50) */
#define UMV_LD 99u                                      /* LIGHT's default */
static uint8_t umv_dirty;                               /* turned: saved once the knobs rest (umv_poll) */
static uint32_t umv_ms;
/* the plate (0 .. 5); the first time with these settings, the bytes as KILN, SAROS and FILAMENT kept them (three each)
 * cleared: the defaults (KILN's place CINDER's) */
static uint32_t umb_pl(void)
{
    uint8_t *f = favorites.factory[15];
    if (!(f[31] & 0x80u)) {
        memset(f + 4, 0, 13);
        f[31] = (uint8_t)(f[31] % 3u | 0x80u);
    }
    return (f[31] & 7u) < UMP_NPL ? f[31] & 7u : 0u;
}
static uint8_t *umv_at(uint32_t pl, uint32_t j, uint32_t *d)   /* plate pl's setting j's byte (3: LIGHT), its default */
{
    *d = j > 2u ? UMV_LD : j > 1u ? 50u : UMV_D[pl][j];
    return j > 2u ? &favorites.factory[15][16] : j > 1u ? &favorites.factory[6][8u + pl] : &favorites.factory[15][4u + pl * 2u + j];
}
/* (MACROS - VISUALS, ui_umbra_mac.c) each target's value where a route reaches it, + 1 (0: none), worked out once a frame
 * (umq_tick); UMT_* below */
static uint8_t umq_o[14] __attribute__((section(".pool")));
static uint32_t umv_raw(uint32_t pl, uint32_t j)        /* plate pl's setting j (2: its third, 3: LIGHT), 0..99, as set */
{
    uint32_t d, v = *umv_at(pl, j, &d) ^ d;
    return v > 99u ? 99u : v;
}
static __attribute__((noinline)) uint32_t umv(uint32_t pl, uint32_t j) { return umq_o[j] ? umq_o[j] - 1u : umv_raw(pl, j); }   /* .. as routed */
static void umv_put(uint32_t pl, uint32_t j, uint32_t v)
{
    uint32_t d;
    uint8_t *b = umv_at(pl, j, &d);
    *b = (uint8_t)((v > 99u ? 99u : v) ^ d);
    umv_dirty = 1;
    umv_ms = fm1_ms;
}
static int32_t umv_light(uint32_t pl) { return ((int32_t)umv(pl, 3u) * 256 + 49) / 99; }   /* LIGHT, Q8 (99: 256) */
/* plate pl's third as a size (Q8): 0 a quarter, 50 as it was, 99 twice */
static int32_t umv_sc(uint32_t pl)
{
    int32_t v = (int32_t)umv(pl, 2u);
    return v <= 50 ? 64 + v * 192 / 50 : 256 + (v - 50) * 256 / 49;
}
/* TOTALITY's own settings (ui_umbra_tot.c), in SAMPLE's retired favourites row (engine 4: never selectable, nothing reads its
 * stars; its presets were < 64, so its bytes 8.. were always 0; TRIO's row, bt.c, the same way), each as the value ^ its
 * default: [8 + i] i 0 DISSOLVE (1..3: once the routing, one for all plates: MACROS - VISUALS' now), 4..7 the visual effects
 * (FX), 8 SPEED, 9 REACT, 10 MOVEMENT, 11 TRAVEL. [20 ..] held each plate's KNOB 1..4 (gone: TOTALITY's knobs are the
 * macros M1..M4, MACROS - VISUALS sends them on; its routes in those bytes, ui_umbra_mac.c) */
enum { UMT_A, UMT_B, UMT_C, UMT_LIGHT, UMT_SPEED, UMT_REACT, UMT_MOVE, UMT_TRAVEL, UMT_FADE, UMT_MIR, UMT_MOS, UMT_GLI, UMT_SOL,
       UMT_BEING, UMT_NT };                             /* MACROS - VISUALS' targets: the plate's own three, LIGHT .. */
_Static_assert(UMT_NT == sizeof umq_o, "umq_o: a byte a target");
#define UMT_NS 12u
static const uint8_t UMT_D[UMT_NS] = {50, 0, 0, 0, 0, 0, 0, 0, 50, 50, 1, 50};
static const uint8_t UMT_MAX[UMT_NS] = {99, 0, 0, 0, 3, 99, 99, 99, 99, 99, 3, 99};
static const uint8_t UMT_Q[UMT_NS] = {UMT_FADE, UMT_NT, UMT_NT, UMT_NT, UMT_MIR, UMT_MOS, UMT_GLI, UMT_SOL, UMT_SPEED,
                                      UMT_REACT, UMT_MOVE, UMT_TRAVEL};   /* setting i's target */
static uint32_t umt_raw(uint32_t i)                     /* setting i as set */
{
    uint32_t v = favorites.factory[4][8u + i] ^ UMT_D[i];
    return v > UMT_MAX[i] ? UMT_D[i] : v;
}
static __attribute__((noinline)) uint32_t umt_get(uint32_t i)   /* .. as routed (a call: the plates read it often) */
{
    uint32_t t = UMT_Q[i];
    return t < UMT_NT && umq_o[t] ? umq_o[t] - 1u : umt_raw(i);
}
static void umt_put(uint32_t i, int32_t v)
{
    favorites.factory[4][8u + i] = (uint8_t)((v < 0 ? 0 : v > UMT_MAX[i] ? UMT_MAX[i] : v) ^ UMT_D[i]);
    umv_dirty = 1;
    umv_ms = fm1_ms;
}
/* NOTE MOVEMENT (set on HOME > ECO > NOTES until 2026-11-09, MOVEMENT [10]): where a track's notes land on a plate moves along a path of the track's
 * own (a Lissajous figure, its ratios and its start the track's), an offset each plate adds to the note's own place (umn_at;
 * across the plate either way, folded back at its edges: a track's notes keep their shape among themselves).
 *   STATIC  none: each note where its pitch puts it, as ever
 *   MOVE    a step along the path a note (TRAVEL [11]: 50 a 64th of the way round, 99 a 16th, 0 none)
 *   DRIFT   a step as long as the time since the track's last note (TRAVEL's a half second; 4 s at most): notes close in
 *           time land close, after a rest far off
 *   RANDOM  anywhere */
static const char *const UMN_MV[4] = {"STATIC", "MOVE", "DRIFT", "RANDOM"};
static const uint8_t UMN_L[NTRK][2] = {{1, 2}, {2, 1}, {2, 3}, {3, 2}, {1, 3}, {3, 1}};   /* (each track's figure) */
static struct {
    int16_t o[NTRK][2];                                 /* each track's offset now, across and down (Q8 of a plate's span:
                                                         * -256 .. 256) */
    uint32_t p[NTRK], t[NTRK];                          /* .. its place on its path (2^32 a turn), its last note's time */
} umn __attribute__((section(".pool")));
/* SPEED: the plates' own clock (umb_now, ms), moved once a frame at its pace: 50 as the wall's, 0 still (a plate frozen
 * as it is), 99 four times as fast. Every time a plate keeps (a note's age, a fade, a step) is on it */
static uint32_t umb_now, umb_cr, umb_cq, umb_cf;
static void umq_tick(void);                             /* (ui_umbra_mac.c) */
static void umb_clock(void)
{
    uint32_t v, dt = fm1_ms - umb_cr, q;
    if (umb_cf == ui.frame + 1u)
        return;
    umb_cf = ui.frame + 1u;
    umq_tick();                                         /* (the routes first: SPEED may be routed) */
    v = umt_get(8u);
    q = v <= 50u ? v * 256u / 50u : 256u + (v - 50u) * 768u / 49u;
    umb_cr = fm1_ms;
    umb_cq += (dt > 250u ? 250u : dt) * q;
    umb_now += umb_cq >> 8;
    umb_cq &= 255u;
    if (!umt_get(10u))                                  /* (STATIC: no offset, the plates' notes where they were) */
        memset(umn.o, 0, sizeof umn.o);
}
/* REACT: how hard a note (its velocity, 1..127) reaches the plate: 50 as played, 0 not at all (0: a note let go stays one) */
static uint32_t umb_vel(uint32_t v)
{
    uint32_t r = umt_get(9u);
    return !v ? 0u : (v = v * (r <= 50u ? r * 256u / 50u : 256u + (r - 50u) * 768u / 49u) >> 8) > 127u ? 127u : v;
}
#include "ui_umbra_noise.h"                            /* umb_at: in flash
                                                         * (tools/gen_umbra_noise.py) */
/* the noise: 32 x 32 bytes of value noise that tiles, two octaves (lattices of 4 and 8, features 8 and 4 texels apart; seed
 * 719, its spread, range and slopes closest to KILN's 64 x 64 table's); the fades' profile, 255 exp(-i / 32) (umb_exp); the
 * log, 512 ln(r / 16), r 16 .. 175 px (CORONA's map, TARN's reflection). Made once before a plate first needs them
 * (umb_tabs: umb_backdrop, umb_welcome), in RAM. (In flash, made by tools/gen_umbra_noise.py, until the welcome wanted
 * 1.6 KB of the slot: these are the same values) */
static uint8_t umb_tn[32 * 32];
static uint8_t umb_E[256] __attribute__((section(".pool")));
static uint16_t umb_ln[160] __attribute__((section(".pool")));
static __attribute__((noinline)) uint32_t umb_h8(uint32_t x, uint32_t y, uint32_t s)   /* a lattice point's value, 0..255 */
{
    uint32_t h = x * 374761393u + y * 668265263u + s * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (h ^ (h >> 16)) >> 24;
}
/* an octave at texel x, y: the lattice of per (4 or 8; it wraps) smoothstepped between its points */
static int32_t umb_oct(uint32_t x, uint32_t y, uint32_t per, uint32_t s)
{
    uint32_t gx = x * per, gy = y * per, xi = gx >> 5, yi = gy >> 5, x1 = (xi + 1u) & (per - 1u), y1 = (yi + 1u) & (per - 1u);
    int32_t xf = (int32_t)(gx & 31u) << 3, yf = (int32_t)(gy & 31u) << 3, u = (xf * xf * (768 - 2 * xf)) >> 16,
            v = (yf * yf * (768 - 2 * yf)) >> 16, a = (int32_t)umb_h8(xi, yi, s), b = (int32_t)umb_h8(x1, yi, s),
            c = (int32_t)umb_h8(xi, y1, s), d = (int32_t)umb_h8(x1, y1, s);
    return a + (((b - a) * u) >> 8) + (((c - a) * v) >> 8) + (((a - b - c + d) * u * v) >> 16);
}
static void umb_tabs(void)
{
    uint32_t k, e;
    if (umb_tn[0])                                      /* (made: its first byte is 94) */
        return;
    for (k = 0, e = 255u << 16; k < 256u; k++, e = ((e >> 8) * 63519u) >> 8)
        umb_E[k] = (uint8_t)(e >> 16);
    for (k = 0, e = 0; k < 160u; k++) {                 /* (ln(r / 16) in Q24: 2 atanh(1 / (2j - 1)), j 17 .. r, summed) */
        uint32_t q = 2u * k + 33u;
        umb_ln[k] = (uint16_t)((e + (1u << 14)) >> 15);
        e += (1u << 25) / q + (1u << 25) / (3u * q * q * q);
    }
    for (k = 0; k < 32u * 32u; k++)
        umb_tn[k] = (uint8_t)((umb_oct(k & 31u, k >> 5, 4u, 719u) * 159 + umb_oct(k & 31u, k >> 5, 8u, 720u) * 97) >> 8);
}
/* SAROS and FILAMENT (ui_umbra_melody.c) */
#define UMM_TH 48                                       /* SAROS: threads at most */
#define UMM_ND 12                                       /* .. the notes' points glowing */
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
} umm_t;
/* CINDER, GRANULE, CORONA, TARN (ui_umbra_field.c) */
typedef struct {
    uint8_t h[UMB_S];                                   /* the heat, a byte a cell */
    uint8_t rp[UMB_N], rc[UMB_N];                       /* the step's rows as they were: the one above, this one */
    uint8_t pw[256], sm[256], c1[256], c2[256];         /* curves: the heat's glow (^0.75), the hot glow, the crust's layers */
    uint8_t hx[NTRK], hy[NTRK], hr[NTRK], hv[NTRK], hn[NTRK];   /* each track's held note: where (cells), how wide (Q4),
                                                         * how hard; the note + 1 */
    uint32_t T;                                         /* the steps */
    uint8_t t[UMB_S], tr[UMB_N];                        /* the heat's tint, a byte a cell (ump_tint: tracks 3 and 4's warm
                                                         * share, carried with it); the step's row as it was */
} ump_ci_t;
#define UMG_G 12u                                       /* GRANULE: 12 x 12 granules .. */
#define UMG_N (UMG_G * UMG_G)
#define UMG_C 20                                        /* .. 20 px apart */
#define UMG_NB 12u                                      /* .. bright points at most */
typedef struct {
    uint8_t lab[UMB_S], prof[UMB_S];                    /* each 4 x 4 px block's granule, and its light's profile there */
    uint8_t sx[UMG_N], sy[UMG_N], amb[UMG_N], col[UMG_N][3];   /* a granule: its heart (px), its glow at rest, its colour */
    uint16_t E[UMG_N];                                  /* .. its light (Q8, up to 1.6) */
    uint32_t born[UMG_N];                               /* .. when it was born (it fades in) */
    uint8_t gb[UMG_N];                                  /* .. this frame's: the glow, */
    uint16_t gl[UMG_N];                                 /* .. the light (in 16 steps) */
    uint8_t bx[UMG_NB], by[UMG_NB], bv[UMG_NB], bc[UMG_NB];   /* the bright points (the hats): where, how hard, the lane */
    uint32_t bt[UMG_NB];                                /* .. when */
    uint8_t nb;
    uint32_t tb;                                        /* the boil's clock */
} ump_gr_t;
#define UMR_NA 128u                                     /* CORONA: streamers round */
typedef struct {
    uint8_t pa[UMB_S], pr[UMB_S];                       /* a quarter of the screen in 2 x 2 px blocks: the angle (256 a
                                                         * quarter turn), the radius (px); mirrored for the rest */
    uint8_t base[UMR_NA];                               /* the corona at rest (Q8) */
    uint16_t E[UMR_NA];                                 /* the notes' light on each streamer (Q8, up to 1.6) */
    uint16_t S[UMR_NA + 1];                             /* this frame's: each streamer's light (in 16 steps) .. */
    int16_t K[UMR_NA + 1];                              /* .. and how fast it falls off (Q8) */
    uint16_t P;                                         /* the kick's pulse (Q8) */
    uint16_t W[UMR_NA];                                 /* the notes' light from tracks 3 and 4 (of E) .. */
    uint8_t Wc[UMR_NA];                                 /* .. which (ump_tint: the last to bring as much as there was) */
    uint8_t T[UMR_NA + 1], Pt, pc[4], bc[10];           /* this frame's tint a streamer (ump_tint), the kick's, the
                                                         * prominences', the beads' */
    uint16_t pa_[4], ba[10];                            /* the prominences (the snare) and the beads (the hats): where (1024
                                                         * a turn) .. */
    uint8_t ph[4], bv[10], np, nbd;                     /* .. how high, how hard */
    uint32_t pt[4], bt[10];                             /* .. when */
} ump_co_t;
#define UMT_N 48                                        /* TARN: the water 48 x 48 cells of 5 x 5 px */
typedef struct {
    int16_t h[2][UMT_N * UMT_N];                        /* its height, now and a step before (swapped) */
    uint8_t rl[1024];                                   /* the reflection by r^2 / 32 */
    int16_t gr[2][UMT_N * 2];                           /* two rows of slopes (by parity), x and y */
    int16_t grj[2];
    uint8_t cur;                                        /* which of h is now */
    uint8_t tx[8], ty[8], tv[8], nt;                    /* stars glinting (the hats) */
    uint32_t tt[8];
} ump_ta_t;
typedef struct {
    union {                                             /* (one plate draws at a time: a plate chosen starts afresh) */
        umm_t m;
        ump_ci_t ci;
        ump_gr_t gr;
        ump_co_t co;
        ump_ta_t ta;
    };
    uint32_t f0, qr, rs, ms, last;                      /* the frame worked out, the notes read, the dice, the steps' clock,
                                                         * the last frame's time */
    uint8_t init;                                       /* (SAROS, FILAMENT: 0 when they start afresh) */
    uint8_t bare;                                       /* TOTALITY being drawn: the whole screen the plate's, nothing darkened */
} umb_t;
static umb_t umb __attribute__((section(".pool")));    /* (V10: in the pool, zeroed at boot; RAM's full) */

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
/* each note's offset as NOTE MOVEMENT gave it (umb_q's entry, across and down): a layer reading the notes after another
 * (the plate API's overlay) places each note where it was, not where the track's last note left the offset */
static int16_t umb_qo[UMB_NQ][2] __attribute__((section(".pool")));
/* a note on track c, umb_q's entry q (once a note, before a plate places it): its offset moved (NOTE MOVEMENT), kept */
static void umn_note(uint32_t c, uint32_t q)
{
    uint32_t m = umt_get(10u), t = umt_get(11u), st = t * t * 26843u, dt = umb_now - umn.t[c], ax;
    umn.t[c] = umb_now;
    if (m) {
        umn.p[c] += m == 1u ? st : m == 2u ? st / 500u * (dt > 4000u ? 4000u : dt) : 0u;
        for (ax = 0; ax < 2u; ax++)                     /* (the path; RANDOM: a span either way, folded: even) */
            umn.o[c][ax] = (int16_t)(m == 3u ? (int32_t)(umb_rnd() % 513u) - 256
                                             : SINE[(umn.p[c] * UMN_L[c][ax] + (ax ? 0u : c * 715827883u)) >> 22] >> 7);
    }
    umb_qo[q][0] = umn.o[c][0];
    umb_qo[q][1] = umn.o[c][1];
}
/* v (0 .. span) moved by offset o (Q8 of the span), folded back into 0 .. span */
static int32_t umn_fold(int32_t o, int32_t v, int32_t span)
{
    int32_t p = 2 * span;
    if (!o)
        return v;
    v = (v + ((o * span) >> 8)) % p;
    v += v < 0 ? p : 0;
    return v > span ? p - v : v;
}
/* v (0 .. span along axis ax: 0 across, 1 down) where track c's offset moves it */
static int32_t umn_at(uint32_t c, uint32_t ax, int32_t v, int32_t span) { return umn_fold(umn.o[c][ax], v, span); }
/* the noise table at X, Y (texels, Q8; it wraps): its value x 256 */
static int32_t umb_tbil(int32_t X, int32_t Y)
{
    int32_t x0 = (X >> 8) & (UMB_T - 1), y0 = (Y >> 8) & (UMB_T - 1), fx = X & 255, fy = Y & 255, x1 = (x0 + 1) & (UMB_T - 1),
            y1 = (y0 + 1) & (UMB_T - 1);
    int32_t a = umb_tn[y0 * UMB_T + x0], b = umb_tn[y0 * UMB_T + x1], c = umb_tn[y1 * UMB_T + x0], d = umb_tn[y1 * UMB_T + x1];
    return (a << 8) + (b - a) * fx + (c - a) * fy + (((a - b - c + d) * fx * fy) >> 8);
}
static int32_t umb_exp(int32_t i) { return i < 0 ? 255 : i > 255 ? 0 : umb_E[i]; }   /* 256 x exp(-i / 32) */
/* the notes' colours: eight round a cycle (mint, teal, aqua, ice, indigo, violet, rose, amber); each drum sound has its
 * own place on it, a melodic track c (c x 3) % 8 */
static const uint8_t UMB_EVC[8][3] = {{140, 236, 176}, {120, 222, 196}, {96, 214, 232}, {104, 160, 255},
                                      {118, 108, 232}, {168, 130, 255}, {232, 124, 196}, {242, 172, 96}};
static const uint8_t UMB_LANE_POS[NLANE] = {3, 5, 6, 2, 1, 4, 7, 0};   /* KICK ice, SNARE violet, CLAP rose, HATCL aqua,
                                                                        * HATOP teal, TOM indigo, RIM amber, BELL mint */
#include "ui_umbra_melody.c"                          /* SAROS and FILAMENT, the melody plates */
#include "ui_umbra_field.c"                           /* CINDER, GRANULE, CORONA and TARN, the field plates */
/* ----------------------------------------------------------------------------------------- the plate modules --- */
/* A plate built as a module (umb_plate.h) runs on a layer: its block, the notes it has read, its frame. Layer 0 is the
 * ground, its block the plates' shared one (umb's union: one plate draws there at a time); layer 1 (the emulator's test
 * of two at once, UMB_LAYERS 2) an overlay with its own. The firmware reads the notes and hands each layer its own; NOTE
 * MOVEMENT steps on the ground's only. FILAMENT is the first module (resident: built into the image). */
#include "umb_plate.h"
#include "plates/filament.c"                          /* FILAMENT, through the plate API */
_Static_assert(NTRK == UMB_NTRK, "the plate API's tracks");
#ifndef UMB_LAYERS
#define UMB_LAYERS 1
#endif
#define UMB_VGN 150u                                    /* a strip's rows at most (gfx.c CV_MAX / 240) */
static int32_t umb_api_move(const umb_note_t *n, uint32_t ax, int32_t v, int32_t span) { return umn_fold(n->mo[ax & 1u], v, span); }
static const umb_api_t umb_api = {UMB_API, umb_api_move, SINE, UMB_EVC};
#if FELUCCA_FLASH && !FELUCCA_WEB
static const umb_plate_t *ups_plate(void);             /* umb_slot.c: the plate file in flash, if one is to run */
#else
#define ups_plate() ((const umb_plate_t *)0)
#endif
typedef struct {
    const umb_plate_t *p;                               /* the plate it draws (its module's header): resident .. */
    const umb_plate_t *pp;                              /* .. or the slot's in its place (umb_slot.c), the one last drawn */
    void *st;                                           /* its block .. */
    uint32_t cap;                                       /* .. of these bytes */
    uint32_t qr, f0, last;                              /* the notes read, the frame worked out, its time */
    uint8_t on;                                         /* the plate it last drew + 1 (0: none: it starts afresh) */
    uint8_t mov;                                        /* its notes step NOTE MOVEMENT (the ground's) */
} umb_layer_t;
#if UMB_LAYERS > 1
static uint8_t umb_ovl;                                 /* (the emulator: web_umb(97, 1)) FILAMENT over the plate in TOTALITY */
static uint32_t umb_ost[(sizeof(plf_t) + 3u) / 4u] __attribute__((section(".pool")));
#endif
static umb_layer_t umb_lay[UMB_LAYERS] = {
    {&umb_plate_filament, 0, &umb, __builtin_offsetof(umb_t, f0), 0, 0, 0, 0, 1},
#if UMB_LAYERS > 1
    {&umb_plate_filament, 0, umb_ost, sizeof umb_ost, 0, 0, 0, 0, 0},
#endif
};
static int16_t umb_vg[UMB_VGN] __attribute__((section(".pool")));   /* the strip's rows' light (umb_strip_t.vg) */
/* layer li's plate (as plate pl: its settings) over the canvas as the screen's x0, y0 .., in draw mode mode */
static void umb_lay_draw(uint32_t li, uint32_t pl, int32_t x0, int32_t y0, uint32_t mode)
{
    umb_layer_t *L = &umb_lay[li];
    const umb_plate_t *p = ups_plate();
    umb_lctx_t c;
    umb_strip_t s;
    uint32_t j, h = cv_h < UMB_VGN ? cv_h : UMB_VGN;
    if (!p || p->state > L->cap || !(p->modes & UMB_MODE(mode)))
        p = L->p;                                       /* (none in the slot, or not one for this layer: the resident) */
    if (p->magic != UMB_PLATE_MAGIC || p->api > UMB_API || p->state > L->cap || !(p->modes & UMB_MODE(mode)))
        return;                                         /* (not a plate, a newer one, too big, or not this way: nothing) */
    if (p != L->pp) {                                   /* (another plate in its place: it starts afresh) */
        L->pp = p;
        L->on = 0;
    }
    for (j = 0; j < UMB_NCTL; j++)                      /* (the routes, MACROS - VISUALS, move the ground's plate only: */
        c.k[j] = (uint8_t)(j < 3u ? li ? umv_raw(pl, j) : umv(pl, j) : 0u);   /* the overlay its own, as stored) */
    c.light = li ? ((int32_t)umv_raw(pl, 3u) * 256 + 49) / 99 : umv_light(pl);
    c.now = umb_now;
    c.dt = 0;
    c.bare = umb.bare;
    c.tracks = (uint8_t)trk_live;
    c.layer = (uint8_t)li;
    c.rsv = 0;
    if (L->on != pl + 1u) {                             /* chosen: its block zeroed, the notes from now */
        memset(L->st, 0, p->state);
        L->on = (uint8_t)(pl + 1u);
        L->qr = umb_q.w;
        L->last = umb_now;
        p->init(L->st, &c, &umb_api);
    }
    if (L->f0 != ui.frame) {                            /* once a frame: the notes since (a burst: the newest), its frame */
        L->f0 = ui.frame;
        c.dt = umb_now - L->last > 250u ? 250u : umb_now - L->last;
        L->last = umb_now;
        if (umb_q.w - L->qr > UMB_NQ)
            L->qr = umb_q.w - UMB_NQ;
        for (; L->qr != umb_q.w; L->qr++) {
            uint32_t q = L->qr % UMB_NQ, t = umb_q.c[q], v = umb_vel(umb_q.vel[q]);   /* (REACT) */
            umb_note_t n;
            if (t >= NTRK || (umb_q.vel[q] && (eco_off(t) || !v)))
                continue;
            if (v && L->mov)
                umn_note(t, q);                         /* (NOTE MOVEMENT) */
            n.track = (uint8_t)t;
            n.note = umb_q.note[q];
            n.vel = (uint8_t)v;
            n.drum = trk[t].engine == ENGI_DRUM;
            n.lane = (uint8_t)(n.drum ? drum_lane(n.note) % NLANE : 0u);
            n.rsv = 0;
            n.mo[0] = umb_qo[q][0];                     /* (where NOTE MOVEMENT put it: the ground's call, this frame's) */
            n.mo[1] = umb_qo[q][1];
            p->note(L->st, &c, &umb_api, &n);
        }
        p->frame(L->st, &c, &umb_api);
    }
    for (j = 0; j < h; j++)
        umb_vg[j] = (int16_t)umb_rowlight(y0 + (int32_t)j, c.light);
    s.px = cv_px;
    s.w = (int32_t)cv_w;
    s.h = (int32_t)h;
    s.x0 = x0;
    s.y0 = y0;
    s.vg = umb_vg;
    s.mode = mode;
    p->draw(L->st, &c, &umb_api, &s);
}
/* the canvas as the screen's x0, y0 .. : the plate */
static void umb_backdrop(int32_t x0, int32_t y0)
{
    uint32_t pl = umb_pl();
    umb_tabs();                                         /* (the noise, once) */
    umb_clock();
    if (pl == 2u) {                                     /* FILAMENT: a module (plates/filament.c) */
        ump_on = 0xFFu;
        umm_on = 0;
        umb_lay_draw(0u, pl, x0, y0, UMB_DRAW_REPLACE);
    } else if (pl == 1u) {                              /* SAROS */
        ump_on = 0xFFu;
        umb_lay[0].on = 0;
        umm_backdrop(pl, x0, y0);
    } else {
        umm_on = 0;
        umb_lay[0].on = 0;
        ump_backdrop(pl, x0, y0);
    }
}
#include "ui_umbra_tot.c"                             /* TOTALITY: the plate alone, the visual effects */
#include "ui_umbra_mac.c"                             /* HOME > UMBRA's MACROS - AUDIO and MACROS - VISUALS */
#include "ui_umbra_plate.c"                           /* the visuals' settings saved */
