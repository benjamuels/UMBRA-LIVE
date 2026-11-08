/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* UMBRA OBSCURA: HOME's plates in place of the ecosystem. Included by ui_lzhome.c before eco_draw.
 *
 * Six plates (HOME > ECO > PLATE, KNOB 1), one drawn at a time, all from the same ring of notes (voice.c trk_note_on's
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
 * whole screen, KNOB 1..4 routed by HOME > ECO > ROUTING (ui_umbra_plate.c), FX its visual effects. */
#define UMB_ON 1
#define UMB_N 60                                        /* a field's side: 60 x 60 cells of 4 x 4 px */
#define UMB_S (UMB_N * UMB_N)
#define UMB_T 32                                        /* the noise table: 32 x 32, tiling */
#define UMP_NPL 6u                                      /* the plates */
/* HOME > ECO > PLATE (ui_lens.c CE_HOME, the group's third page; ALGORITHM the plate): KNOB 1..3 the plate's own three
 * settings, KNOB 4 LIGHT (one for all), kept with the settings in spare favourite bytes (ui.c; settings_persist.c): the
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
static uint32_t umv(uint32_t pl, uint32_t j)            /* plate pl's setting j (2: its third, 3: LIGHT), 0..99 */
{
    uint32_t d, v = *umv_at(pl, j, &d) ^ d;
    return v > 99u ? 99u : v;
}
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
 * default: [8 + i] i 0 DISSOLVE (1..3: the routing as it was, one for all plates), 4..7 the visual effects (FX), 8 SPEED,
 * 9 REACT, 10 MOVEMENT, 11 TRAVEL (umt_ix: a routed target's); [20 + 2 x plate ..] each plate's routing, a nibble a knob
 * (umr_get) */
enum { UMT_A, UMT_B, UMT_C, UMT_LIGHT, UMT_SPEED, UMT_REACT, UMT_MOVE, UMT_TRAVEL, UMT_FADE, UMT_NT };   /* a routed knob: the
                                                         * plate's own three, LIGHT .. */
#define UMT_NS 12u
static const uint8_t UMT_D[UMT_NS] = {50, 0, 0, 0, 0, 0, 0, 0, 50, 50, 1, 50};
static const uint8_t UMT_MAX[UMT_NS] = {99, 0, 0, 0, 3, 99, 99, 99, 99, 99, 3, 99};
static const uint8_t UMR_D[4] = {UMT_A, UMT_B, UMT_C, UMT_LIGHT};   /* a plate's KNOB 1..4 at first */
static uint32_t umr_get(uint32_t pl, uint32_t k)       /* plate pl's KNOB k + 1 in TOTALITY: its target (UMT_*) */
{
    uint32_t v = (favorites.factory[4][20u + pl * 2u + (k >> 1)] >> (k & 1u) * 4u & 15u) ^ UMR_D[k];
    return v < UMT_NT ? v : UMR_D[k];
}
static void umr_put(uint32_t pl, uint32_t k, int32_t v)
{
    uint8_t *b = &favorites.factory[4][20u + pl * 2u + (k >> 1)];
    uint32_t sh = (k & 1u) * 4u;
    v = v < 0 ? 0 : v > UMT_NT - 1 ? UMT_NT - 1 : v;
    *b = (uint8_t)((*b & ~(15u << sh)) | (((uint32_t)v ^ UMR_D[k]) & 15u) << sh);
    umv_dirty = 1;
    umv_ms = fm1_ms;
}
static uint32_t umt_get(uint32_t i)
{
    uint32_t v = favorites.factory[4][8u + i] ^ UMT_D[i];
    return v > UMT_MAX[i] ? UMT_D[i] : v;
}
static void umt_put(uint32_t i, int32_t v)
{
    favorites.factory[4][8u + i] = (uint8_t)((v < 0 ? 0 : v > UMT_MAX[i] ? UMT_MAX[i] : v) ^ UMT_D[i]);
    umv_dirty = 1;
    umv_ms = fm1_ms;
}
/* NOTE MOVEMENT (HOME > ECO > NOTES, MOVEMENT [10]): where a track's notes land on a plate moves along a path of the track's
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
static void umb_clock(void)
{
    uint32_t v = umt_get(8u), dt = fm1_ms - umb_cr, q = v <= 50u ? v * 256u / 50u : 256u + (v - 50u) * 768u / 49u;
    if (umb_cf == ui.frame + 1u)
        return;
    umb_cf = ui.frame + 1u;
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
#include "ui_umbra_noise.h"                            /* umb_tn, umb_pal, umb_E, umb_at, umb_ln: in flash
                                                         * (tools/gen_umbra_noise.py) */
/* SAROS and FILAMENT (ui_umbra_melody.c) */
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
/* CINDER, GRANULE, CORONA, TARN (ui_umbra_field.c) */
typedef struct {
    uint8_t h[UMB_S];                                   /* the heat, a byte a cell */
    uint8_t rp[UMB_N], rc[UMB_N];                       /* the step's rows as they were: the one above, this one */
    uint8_t pw[256], sm[256], c1[256], c2[256];         /* curves: the heat's glow (^0.75), the hot glow, the crust's layers */
    uint8_t hx[NTRK], hy[NTRK], hr[NTRK], hv[NTRK], hn[NTRK];   /* each track's held note: where (cells), how wide (Q4),
                                                         * how hard; the note + 1 */
    uint32_t T;                                         /* the steps */
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
/* a note on track c (once a note, before a plate places it): its offset moved (NOTE MOVEMENT) */
static void umn_note(uint32_t c)
{
    uint32_t m = umt_get(10u), t = umt_get(11u), st = t * t * 26843u, dt = umb_now - umn.t[c], ax;
    umn.t[c] = umb_now;
    if (!m)
        return;
    umn.p[c] += m == 1u ? st : m == 2u ? st / 500u * (dt > 4000u ? 4000u : dt) : 0u;
    for (ax = 0; ax < 2u; ax++)                         /* (the path; RANDOM: a span either way, folded: even) */
        umn.o[c][ax] = (int16_t)(m == 3u ? (int32_t)(umb_rnd() % 513u) - 256
                                         : SINE[(umn.p[c] * UMN_L[c][ax] + (ax ? 0u : c * 715827883u)) >> 22] >> 7);
}
/* v (0 .. span along axis ax: 0 across, 1 down) where track c's offset moves it, folded back into 0 .. span */
static int32_t umn_at(uint32_t c, uint32_t ax, int32_t v, int32_t span)
{
    int32_t o = umn.o[c][ax], p = 2 * span;
    if (!o)
        return v;
    v = (v + ((o * span) >> 8)) % p;
    v += v < 0 ? p : 0;
    return v > span ? p - v : v;
}
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
/* the canvas as the screen's x0, y0 .. : the plate */
static void umb_backdrop(int32_t x0, int32_t y0)
{
    uint32_t pl = umb_pl();
    umb_clock();
    if (pl == 1u || pl == 2u) {                         /* SAROS, FILAMENT */
        ump_on = 0xFFu;
        umm_backdrop(pl, x0, y0);
    } else {
        umm_on = 0;
        ump_backdrop(pl, x0, y0);
    }
}
#include "ui_umbra_tot.c"                             /* TOTALITY: the plate alone, the visual effects */
#include "ui_umbra_plate.c"                           /* HOME > ECO > PLATE: the visuals' page */
