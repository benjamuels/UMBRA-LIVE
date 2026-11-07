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
 * (umb's union): a plate chosen starts afresh. HOME > ECO's fourth page, TOTALITY, is the plate alone over the whole screen. */
#define UMB_ON 1
#define UMB_N 60                                        /* a field's side: 60 x 60 cells of 4 x 4 px */
#define UMB_S (UMB_N * UMB_N)
#define UMB_T 32                                        /* the noise table: 32 x 32, tiling */
#define UMP_NPL 6u                                      /* the plates */
/* HOME > ECO > PLATE (ui_lens.c CE_HOME, the group's third page): KNOB 1 the plate, KNOB 2 and 3 its own two settings,
 * KNOB 4 LIGHT (one for all), kept with the settings in engine 15's spare favourite bytes (ui.c; settings_persist.c): the
 * plate [31] (bit 7: these bytes laid out so), the plates' two [4 + 2 x plate ..] (4 .. 15: bytes 4 .. 12 the hints' before
 * 0.9.5, 13 .. 16 free), LIGHT [16], each as the value ^ its default (0, never set: the default) */
#define umb_plate (favorites.factory[15][31])
static const char *const UMV_PL[UMP_NPL] = {"CINDER", "SAROS", "FILAMNT", "GRANULE", "CORONA", "TARN"};   /* (a knob card's
                                                         * value: 49 px; FILAMENT 50) */
static const char *const UMV_N[UMP_NPL][2] = {{"RISE", "COOL"}, {"SPIN", "FADE"}, {"SWING", "RING"}, {"BOIL", "FADE"},
                                              {"REACH", "FADE"}, {"DRAG", "BEND"}};
static const uint8_t UMV_D[UMP_NPL][2] = {{45, 50}, {30, 45}, {50, 50}, {35, 45}, {50, 45}, {55, 50}};
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
static uint32_t umv(uint32_t pl, uint32_t j)            /* plate pl's setting j (2: LIGHT), 0..99 */
{
    uint32_t v = j > 1u ? favorites.factory[15][16] ^ UMV_LD : favorites.factory[15][4u + pl * 2u + j] ^ UMV_D[pl][j];
    return v > 99u ? 99u : v;
}
static void umv_put(uint32_t pl, uint32_t j, uint32_t v)
{
    v = v > 99u ? 99u : v;
    if (j > 1u)
        favorites.factory[15][16] = (uint8_t)(v ^ UMV_LD);
    else
        favorites.factory[15][4u + pl * 2u + j] = (uint8_t)(v ^ UMV_D[pl][j]);
    umv_dirty = 1;
    umv_ms = fm1_ms;
}
static int32_t umv_light(uint32_t pl) { return ((int32_t)umv(pl, 2u) * 256 + 49) / 99; }   /* LIGHT, Q8 (99: 256) */
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
    if (pl == 1u || pl == 2u) {                         /* SAROS, FILAMENT */
        ump_on = 0xFFu;
        umm_backdrop(pl, x0, y0);
    } else {
        umm_on = 0;
        ump_backdrop(pl, x0, y0);
    }
}
/* TOTALITY (HOME > ECO's fourth page, ui_lens.c CE_HOME): the visual alone, over the whole screen: no header, tabs, knobs,
 * words or footer (ui_draw.c ui_draw_body draws nothing else); two strips, sent where they changed (gfx.c cv_blit_cells);
 * under LOAD LITE / ECO every 50 / 70 ms, as HUD's HOME. The keys, knobs and buttons work as on ECO */
static int umb_bare(void) { return ui.home && !gen_on() && !ui.hlv && ui.hset[0] == 3u; }
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
#include "ui_umbra_plate.c"                           /* HOME > ECO > PLATE: the visuals' page */
