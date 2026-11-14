/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* UMBRA LIVE: HOME's plates in place of the ecosystem. Included by ui_lzhome.c before eco_draw.
 *
 * A plate is a picture played by the notes (voice.c trk_note_on's umb_q: keys, sequencer, arp, MIDI; a note let go:
 * velocity 0). One is the ground; up to three more lie over it as layers, their light added (umb_scene): the VEILS,
 * plates light enough to lie over another (umb_plate_t.kind UMB_KIND_OVERLAY), and BODIES, UMBRA's figures. A ground lies
 * only on MAIN, a veil only on a layer (the user, 2026-11-13). Seventeen: CORONA built in, sixteen with an install
 * (tools/build.py BANK_PLATES), eight grounds and eight veils:
 *   CINDER   cold embers: a heat field that rises and cools under a crust of cracks        (plates/cinder.c)
 *   GRANULE  the sun's surface: a honeycomb of granules, a note lights its own             (plates/granule.c)
 *   CORONA   the eclipse: the moon's disc, the corona's streamers fed by the notes         (ui_umbra_field.c: built in)
 *   TARN     the eclipse reflected in dark water, a note a drop                            (plates/tarn.c)
 *   SEPIA GYRE AURORA FATHOM RAPIDS   fluid plates: ink in water, a whirlpool, the northern lights, the sea from the
 *            side, a stream; stepped 12 .. 25 times a second and still between steps        (plates/fluid.inc)
 *   veils: BEACON ZODIAC SONAR MOTHS MARSH PACHINKO   line art from UMBRA OBSCURA, the game          (plates/ *.c)
 *          KOI MEDUSA   fish seen from above, jellyfish: made for SEPIA and FATHOM          (plates/fluid.inc)
 * (SAROS, FILAMENT, WAX, RIVULET, BLOT, BOATS, HELIX (fizz.c), BOKEH, RIDGE, QUILL and MURMUR are plate files no install brings
 * since then: the plates page can still send one.) Built to the FM-1's limits: a byte field or none, a table read and a palette
 * read a pixel, no lighting pass, fades in steps, still between notes, so the blit (gfx.c cv_blit_cells) sends little.
 * Every plate declares its elements (umb_plate.h): LIVE VIZ CTRLS (ui_umbra_mac.c) sets each one, chooses the tracks
 * whose notes drive it and links knobs, taps and the sound's own movement to it. TOTALITY (ui_umbra_tot.c), a HOME tap
 * on HOME, is the picture alone over the whole screen. */
#define UMB_ON 1
#include "umb_plate.h"
#define UMB_N 60                                        /* a field's side: 60 x 60 cells of 4 x 4 px */
#define UMB_S (UMB_N * UMB_N)
#define UMB_T 32                                        /* the noise table: 32 x 32, tiling */
#define UMP_NPL 6u                                      /* the plates known by name (their numbers 0 .. 5) */
#define UMB_NONE 0x3Fu                                  /* no ground: black under the layers */
/* The ground's plate is kept with the settings: engine 15's favourites byte [31] (bit 7: laid out so; its low six bits
 * the plate, UMB_NONE: none). Everything else the picture keeps (each plate's elements, their tracks and what moves
 * them, the layers) is LIVE VIZ CTRLS' store (umc, below). (Until 2026-11-12 a plate's three settings, LIGHT and
 * TOTALITY's own were bytes of retired favourites rows, [15][4..16], [4][8..31], [6][0..18]: left as they are, read by
 * nothing) */
#define umb_plate (favorites.factory[15][31])
static const char *const UMV_PL[UMP_NPL] = {"CINDER", "SAROS", "FILAMENT", "GRANULE", "CORONA", "TARN"};
static uint8_t umv_dirty, umc_dirty;                    /* turned: saved once the knobs rest (umv_poll): the settings, the store */
static uint32_t umv_ms;
static int umb_has(uint32_t pl);                        /* (below: the plate modules) */
static const umb_plate_t *umb_mod(uint32_t pl);
/* the ground's plate as chosen (0 .. 5 the known ones, 6 + i the bank's slot i, UMB_NONE: none), in [31]'s low six bits */
static uint32_t umb_pick(void)
{
    uint8_t *f = favorites.factory[15];
    if (!(f[31] & 0x80u))
        f[31] = 0x80u;                                  /* (the first time: CINDER, as ever; not loaded: CORONA, umb_pl) */
    return f[31] & 0x3Fu;
}
static uint32_t umb_pl(void)                            /* .. drawn: CORONA while it is not there (on the FM-1, a plate not */
{                                                       /* loaded: the choice is kept) */
    uint32_t pl = umb_pick();
    return pl == UMB_NONE || umb_has(pl) ? pl : 4u;
}
/* (LIVE VIZ CTRLS, ui_umbra_mac.c) the picture's own elements as moved now, worked out once a frame (umq_tick): SCREEN's
 * (LIGHT SPEED REACT MOVE TRAVEL DISSLV) at 0 .., EFFECTS' (MIRROR MOSAIC GLITCH SOLAR INVERT) at 8 .. */
enum { G_LIGHT, G_SPEED, G_REACT, G_MOVE, G_TRAVEL, G_FADE, G_MIR = 8, G_MOS, G_GLI, G_SOL, G_INV, G_N = 16 };
static uint8_t umq_o[G_N] __attribute__((section(".pool")));
/* what can move an element: KNOB 1..4 (TOTALITY's: JIANT's macros M1..M4), a track's hits, its LFO, its step, the level,
 * the beat, a tap of ENV, LFO, EDIT, SEL or ARP in TOTALITY (one set to TAP on LIVE VIZ CTRLS' BUTTONS page); each one's
 * value now, 0 .. 256 (umq_tick) */
enum { UMQ_OFF, UMQ_M1, UMQ_HIT = UMQ_M1 + 4, UMQ_LFO = UMQ_HIT + NTRK, UMQ_STEP = UMQ_LFO + NTRK, UMQ_LEVEL = UMQ_STEP + NTRK,
       UMQ_BEAT, UMQ_TAP, UMQ_NS = UMQ_TAP + 5 };
_Static_assert(UMQ_NS <= 32, "a link's source: five bits");
_Static_assert(UMQ_NS <= 32, "a link's source: 5 bits");
_Static_assert(UMQ_M1 == UMB_SRC_KNOB(0) && UMQ_HIT == UMB_SRC_HIT(0) && UMQ_LFO == UMB_SRC_LFO(0) && UMQ_STEP == UMB_SRC_STEP(0) &&
               UMQ_LEVEL == UMB_SRC_LEVEL && UMQ_BEAT == UMB_SRC_BEAT, "the plate API's sources");
static int16_t umq_s[UMQ_NS] __attribute__((section(".pool")));
/* (SAVE > SONG > TIMELINE's transitions, ui_umbra_mac.c umv_tick, once a frame while the song plays) each picture
 * track's light now (Q8: FADE; 0 MAIN, 1 OVER), DISSOLVE's gate (256: none), BLOCKS as MOSAIC's (0: none); and what
 * they come from: the transition (umc_cell_t.tr) of the cell in effect on each track, when its plate came in (0: long ago) */
static uint16_t umv_lt[2] = {256u, 256u}, umv_gt = 256u;
static uint8_t umv_mo;
static struct { uint8_t tr[2], row; uint32_t t0[2]; } umv_st;   /* (row: the song's row they were laid for) */
BTAPP_FN static void umv_tick(void);
static const char *const UMV_TN[2] = {"MAIN", "OVER"};   /* the picture tracks */
/* NOTE MOVEMENT (SCREEN's MOVE): where a track's notes land on a plate moves along a path of the track's
 * own (a Lissajous figure, its ratios and its start the track's), an offset each plate adds to the note's own place (umn_at;
 * across the plate either way, folded back at its edges: a track's notes keep their shape among themselves).
 *   STATIC  none: each note where its pitch puts it, as ever
 *   MOVE    a step along the path a note (TRAVEL: 50 a 64th of the way round, 99 a 16th, 0 none)
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
    if (bt_hw_seg())
        umv_tick();
    v = umq_o[G_SPEED];
    q = v <= 50u ? v * 256u / 50u : 256u + (v - 50u) * 768u / 49u;
    umb_cr = fm1_ms;
    umb_cq += (dt > 250u ? 250u : dt) * q;
    umb_now += umb_cq >> 8;
    umb_cq &= 255u;
    if (!umq_o[G_MOVE])                                 /* (STATIC: no offset, the plates' notes where they were) */
        memset(umn.o, 0, sizeof umn.o);
}
/* REACT: how hard a note (its velocity, 1..127) reaches the plate: 50 as played, 0 not at all (0: a note let go stays one) */
static uint32_t umb_vel(uint32_t v)
{
    uint32_t r = umq_o[G_REACT];
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
/* The plates' block (umb's union): the ground's state from its start (CORONA's own, ump_co_t; a module's), LAYER 1..3's
 * plates' from its end down (umb_scene), UMB_BLK bytes in all: TARN's water, the largest ground, and two light plates
 * over it; under a lighter ground, more */
#define UMB_BLK 11776u
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
    uint8_t pk[4], bk[10];                              /* .. their shape (a drum's lane's: UMR_PK, UMR_BK) */
    uint32_t pt[4], bt[10];                             /* .. when */
} ump_co_t;
typedef struct {
    union {
        ump_co_t co;
        uint32_t blk[UMB_BLK / 4u];
    };
    uint32_t rs;                                        /* the dice */
    uint8_t bare;                                       /* TOTALITY being drawn: the whole screen the plate's, nothing darkened */
} umb_t;
static umb_t umb __attribute__((section(".pool")));    /* (V10: in the pool, zeroed at boot; RAM's full) */
/* a layer: the plate it draws, its part of the block, the notes it has read, its frame. Layer 0 is the ground, 1 .. 3
 * LAYER 1..3 over it */
#define UMB_NLAY 4u
typedef struct {
    const umb_plate_t *pp;                              /* the plate it last drew (its header; 0: none: it starts afresh) */
    void *st;                                           /* .. where its state lay */
    uint32_t qr, f0, last;                              /* the notes read, the frame worked out, its time */
    uint8_t gen;                                        /* the bank as it was (ups_gen) */
} umb_layer_t;
static umb_layer_t umb_lay[UMB_NLAY];

static uint32_t umb_mb32(uint32_t *s)                 /* mulberry32 (TOTALITY's umt_rnd too, its own seed) */
{
    uint32_t t = *s += 0x6D2B79F5u;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
static uint32_t umb_rnd(void) { return umb_mb32(&umb.rs); }
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
    uint32_t m = umq_o[G_MOVE], t = umq_o[G_TRAVEL], st = t * t * 26843u, dt = umb_now - umn.t[c], ax;
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
/* Every plate is a module (umb_plate.h): a header and four entry points, run on a layer. CORONA's is built in (the
 * welcome uses its state: ui_umbra_field.c); the others come from the bank (umb_slot.c: the plate files an install
 * brings or the plates page loads, in projects 5..8's flash). In the emulator and the host tests (UMB_RESIDENT) they are
 * built in and stand in the bank's slots as an install leaves them (tools/build.py BANK_PLATES), so the same code finds
 * them. The plates are numbered so: 0 .. 5 the six known by name (CINDER, SAROS, FILAMENT, GRANULE, CORONA, TARN: the
 * first slot holding a plate of that name is that plate), 6 + i the bank's slot i holding another. What a plate keeps
 * (LIVE VIZ CTRLS' store, below) goes by its name, wherever it lies */
#ifndef UMB_RESIDENT
#define UMB_RESIDENT (FELUCCA_WEB || !FELUCCA_FLASH)
#endif
#ifndef UMB_NSLOT
#define UMB_NSLOT 16u                                   /* the bank's slots (umb_slot.c) */
#endif
static uint8_t ups_gen;                                 /* the bank changed (umb_slot.c: a slot written or erased) */
#if UMB_RESIDENT
#include "plates/cinder.c"
#include "plates/granule.c"
#include "plates/tarn.c"
#include "plates/beacon.c"
#include "plates/zodiac.c"
#include "plates/sonar.c"
#include "plates/moths.c"
#include "plates/marsh.c"
#include "plates/pachinko.c"
#include "plates/fluid.inc"                           /* the fluid grounds and the two veils made for them */
#if UMB_RES_ALL                                         /* (the plates page's player, web/emu/build.sh plates.wasm: every
                                                         * plate there is, the eleven no install brings after the sixteen) */
#include "plates/saros.c"
#include "plates/filament.c"
#include "plates/wax.c"
#include "plates/rivulet.c"
#include "plates/blot.c"
#include "plates/boats.c"
#include "plates/fizz.c"
#include "plates/bokeh.c"
#include "plates/ridge.c"
#include "plates/quill.c"
#include "plates/murmur.c"
#define UMB_RES_MORE , &umb_plate_saros, &umb_plate_filament, &umb_plate_wax, &umb_plate_rivulet, &umb_plate_blot, \
                     &umb_plate_boats, &umb_plate_fizz, &umb_plate_bokeh, &umb_plate_ridge, &umb_plate_quill, &umb_plate_murmur
#else
#define UMB_RES_MORE
#endif
static const umb_plate_t *const UMB_RES[] = {&umb_plate_cinder, &umb_plate_granule, &umb_plate_tarn, &umb_plate_sepia,
                                             &umb_plate_gyre, &umb_plate_aurora, &umb_plate_fathom, &umb_plate_rapids,
                                             &umb_plate_beacon, &umb_plate_zodiac, &umb_plate_sonar, &umb_plate_moths,
                                             &umb_plate_marsh, &umb_plate_pachinko, &umb_plate_koi, &umb_plate_medusa UMB_RES_MORE};
static const umb_plate_t *ups_plate(uint32_t i) { return i < sizeof UMB_RES / sizeof UMB_RES[0] ? UMB_RES[i] : 0; }
#else
static const umb_plate_t *ups_plate(uint32_t i);       /* umb_slot.c: slot i's plate file, if one is to run */
#endif
_Static_assert(sizeof(ump_co_t) <= UMB_BLK, "CORONA's block");
_Static_assert(NTRK == UMB_NTRK, "the plate API's tracks");
#define UMB_NPLT (UMP_NPL + UMB_NSLOT)                  /* the plates' numbers */
static int umb_named(const umb_plate_t *p, const char *n)   /* plate p is called n */
{
    uint32_t i;
    for (i = 0; i < 10u && p->name[i] == n[i] && n[i]; i++)
        ;
    return i < 10u && p->name[i] == n[i];
}
/* plate pl's module (0: none: a plate not loaded) */
static const umb_plate_t *umb_mod(uint32_t pl)
{
    const umb_plate_t *p;
    uint32_t i;
    if (pl >= UMP_NPL)
        return pl < UMB_NPLT ? ups_plate(pl - UMP_NPL) : 0;
    if (pl == 4u)
        return &umb_plate_corona;
    for (i = 0; i < UMB_NSLOT; i++)
        if ((p = ups_plate(i)) != 0 && umb_named(p, UMV_PL[pl]))
            return p;
    return 0;
}
/* plate pl there to draw: CORONA always; a known one loaded; a slot's other plate (a slot holding a known plate is that
 * plate's number, never twice) */
static int umb_has(uint32_t pl)
{
    const umb_plate_t *p;
    uint32_t k;
    if (pl >= UMB_NPLT || !(p = umb_mod(pl)))
        return 0;
    for (k = 0; pl >= UMP_NPL && k < UMP_NPL; k++)
        if (umb_named(p, UMV_PL[k]))
            return 0;
    return 1;
}
static const char *umb_pname(uint32_t pl)              /* plate pl's name, as the screen shows it */
{
    const umb_plate_t *p;
    return pl < UMP_NPL ? UMV_PL[pl] : (p = umb_mod(pl)) != 0 ? p->name : pl == UMB_NONE ? "NONE" : "";
}
/* (SAVE's library, ui_lzsave.c) the bank's slot i: its plate's name ("": none); *cur: it is the ground's plate now */
static const char *umb_bank(uint32_t i, int *cur)
{
    const umb_plate_t *p = ups_plate(i);
    *cur = p && umb_mod(umb_pl()) == p;
    return p ? p->name : "";
}

/* ---------------------------------------------------------------------------- LIVE VIZ CTRLS' store (umc) --- */
/* What LIVE VIZ CTRLS sets (ui_umbra_mac.c), kept in flash on its own (storage.c OBJ_UMBCTL, 0xDA000 / 0xDB000; saved once
 * the knobs rest, umv_poll): a record a page, found by its name's key, so a plate keeps its own wherever it lies in the
 * bank. A page is a plate, or one of four that are not: BODIES (UMBRA's figures, a veil: on a layer; JIANT's beings
 * are no page since 2026-11-13), SCREEN (LIGHT SPEED REACT MOVE TRAVEL DISSLV: every plate's) and EFFECTS (MIRROR MOSAIC GLITCH SOLAR
 * INVERT). A record holds each of the page's elements (a plate's controls, umb_plate_t.ctl): its value, the tracks
 * whose notes drive it (0: the plate's own choice), and UMC_NL links, each a source, the element it moves and how far
 * (-99 .. 99 of the element's range with the source at full). An element's value now is its own and its links' moves
 * added (umc_ev). The store also holds which plates lie on LAYER 1..3 */
#define UMC_NL 12u                                      /* a page's links */
#define UMC_NREC 36u                                    /* the records kept: a page's own (a full bank's 16, CORONA, the four
                                                         * that are no plate), the rest for presets as they are changed */
#define UMC_NPRE 4u                                     /* a page's presets: BASIC, then three (a plate's own: umb_pre_t) */
#define UMC_OFF 0xFFFFu                                 /* a cell: nothing there */
#define UMC_MAGIC 0x31434D55u                           /* "UMC1" */
typedef struct {
    uint16_t key;                                       /* the page's name's key (0: free) */
    uint8_t v[UMB_NCTL];                                /* each element's value + 1 (0: its default) */
    uint8_t t[UMB_NCTL];                                /* .. its tracks (bit t; 0: the plate's own choice) */
    uint8_t ls[UMC_NL];                                 /* a link: its source (UMQ_*; 0: none) | its element << 5 */
    int8_t ld[UMC_NL];                                  /* .. how far */
    uint8_t cur;                                        /* (the page's own record) the preset chosen, 0 .. UMC_NPRE - 1 */
    uint8_t pre;                                        /* the preset this record is (0: the page's own, BASIC) */
} umc_rec_t;
/* (2026-11-14, plate API 6: the user, "all the new elements and their attributes editable and controllable in LIVE VIZ
 * CTRLS") a record's more, in umc.x beside umc.rec (a store from before: all 0, every one its default): its plate's other
 * elements. A page's element j: 0 .. 7 its eight, 8 + e * 3 + k its other element e's parameter k */
#define UMC_NX (UMB_NXE * UMB_NXP)
typedef struct {
    uint8_t v[UMC_NX];                                  /* each one's value + 1 (0: its default) */
    uint8_t lh[UMC_NL / 4u];                            /* each link's element's high bits, 2 a link (umc_lj) */
} umc_x_t;
typedef struct {                                        /* a cell of SAVE > SONG > TIMELINE's two picture tracks */
    uint16_t key;                                       /* the page shown from this row on (its name's key; 0: as before it,
                                                         * UMC_OFF: none) */
    uint8_t pre, tr;                                    /* .. its preset; how it comes and goes (umv_tf: 0 comes in a
                                                         * DISSOLVE of 1 s, goes in a CUT) */
} umc_cell_t;
typedef struct {
    uint32_t magic;
    uint16_t lay[3];                                    /* LAYER 1..3: the page lying there (its key; 0: none) */
    uint16_t rsv;
    umc_rec_t rec[UMC_NREC];
    umc_cell_t cell[NSONG_USE][CHAIN_ROWS][2];          /* each song's rows: MAIN (the ground), OVER (LAYER 1) */
    umc_x_t x[UMC_NREC];                                /* (API 6) each record's more (last: an older store reads short) */
} umc_t;
static umc_t umc __attribute__((section(".pool")));
static umc_rec_t umc_tmp;                               /* (umc_pre) */
static umc_x_t umc_tmpx;
static umc_x_t *umc_x(const umc_rec_t *r) { return r == &umc_tmp ? &umc_tmpx : &umc.x[r - umc.rec]; }
static uint8_t *umc_vp(const umc_rec_t *r, uint32_t j) /* element j's value + 1 */
{
    return j < UMB_NCTL ? (uint8_t *)&r->v[j] : &umc_x(r)->v[j - UMB_NCTL];
}
static uint32_t umc_lj(const umc_rec_t *r, uint32_t i) /* link i's element */
{
    return (uint32_t)(r->ls[i] >> 5) | (umc_x(r)->lh[i >> 2] >> 2u * (i & 3u) & 3u) << 3;
}
static void umc_lset(umc_rec_t *r, uint32_t i, uint32_t s, uint32_t j)   /* link i: source s (0: none), element j */
{
    umc_x_t *x = umc_x(r);
    uint8_t *h = &x->lh[i >> 2];
    r->ls[i] = (uint8_t)(s ? s | (j & 7u) << 5 : 0u);
    *h = (uint8_t)((*h & ~(3u << 2u * (i & 3u))) | (j >> 3) << 2u * (i & 3u));
}
static int umc_flash(void *p, uint32_t n, int save);    /* (project.c, after storage.c) the store read (0) or written (1,
                                                         * only with the transport stopped): 0 done */
static uint8_t umc_gen;                                 /* the layers changed */
/* the pages that are no plate: their numbers past the plates', their names, their elements */
/* BUTTONS (the user, 2026-11-13): what SEL, ENV, ARP, LFO and EDIT do in TOTALITY, each one of UMX_BF: a tap shows or
 * hides that layer (MAIN the ground, OVER LAYER 1, LAYER 3), held with ALGORITHM turned the next plate there; TAP: a tap
 * fires its source for the links (SEL TAP .. EDIT TAP), held its layer as ever (ui_umbra_tot.c umt_btns) */
enum { UMX_BODY = UMB_NPLT, UMX_SCREEN, UMX_FX, UMX_BTN, UMX_N };
static const char *const UMX_PN[4] = {"BODIES", "SCREEN", "EFFECTS", "BUTTONS"};
static const umb_ctl_t UMX_C[4][6] = {{{"LIGHT", 99}, {"SIZE", 50}, {"TRACK", 0}},
                                      {{"LIGHT", 99}, {"SPEED", 50}, {"REACT", 50}, {"MOVE", 1}, {"TRAVEL", 50}, {"DISSLV", 50}},
                                      {{"MIRROR", 0}, {"MOSAIC", 0}, {"GLITCH", 0}, {"SOLAR", 0}, {"INVERT", 0}},
                                      {{"SEL", 2}, {"ENV", 1}, {"ARP", 0}, {"LFO", 3}, {"EDIT", 3}}};
static const uint8_t UMX_NC[4] = {3, 6, 5, 5};
static const char *const UMX_BF[4] = {"MAIN", "OVER", "LAYER 3", "TAP"};
static const uint8_t UMX_BEL[5] = {0x80u, 0x80u, 0x80u, 0x80u, 0x80u};   /* (BUTTONS: SET alone, no links: umx_row) */
typedef struct {
    const char *name;
    const umb_ctl_t *ctl;                               /* its elements .. */
    const uint8_t *el;                                  /* .. each one's UMB_EL_* (0: none has any) */
    uint32_t nctl;
    const umb_plate_t *p;                               /* (API 6) its plate (0: none, or older): its other elements .. */
    uint32_t nx;                                        /* .. their parameters, all told (LIVE VIZ CTRLS' rows past the eight) */
} umx_desc_t;
/* page pl: what it is. 0: not there (a plate not loaded) */
static int umx_desc(uint32_t pl, umx_desc_t *d)
{
    const umb_plate_t *p;
    uint32_t k;
    d->el = 0;
    d->p = 0;
    d->nx = 0;
    if (pl >= UMX_N)
        return 0;
    if (pl >= UMX_BODY) {
        k = pl - UMX_BODY;
        d->name = UMX_PN[pl - UMX_BODY];
        d->ctl = UMX_C[k];
        d->nctl = UMX_NC[k];
        if (pl == UMX_BTN)
            d->el = UMX_BEL;
        return 1;
    }
    if (!umb_has(pl))
        return 0;
    p = umb_mod(pl);
    d->name = p->name;
    d->ctl = p->ctl;
    d->nctl = p->nctl;
    if (p->api >= 3u)
        d->el = p->el;
    if (p->api >= 6u) {
        d->p = p;
        for (k = 0; k < p->nxe && k < UMB_NXE; k++)
            d->nx += p->xel[k].np > UMB_NXP ? UMB_NXP : p->xel[k].np;
    }
    return 1;
}
/* page d's element j (umc_x_t), 0: not there */
static const umb_ctl_t *umx_c(const umx_desc_t *d, uint32_t j)
{
    const umb_xel_t *x;
    if (j < UMB_NCTL)
        return j < d->nctl ? &d->ctl[j] : 0;
    j -= UMB_NCTL;
    if (!d->p || j / UMB_NXP >= d->p->nxe || j >= UMC_NX)
        return 0;
    x = &d->p->xel[j / UMB_NXP];
    return j % UMB_NXP < x->np ? &x->p[j % UMB_NXP] : 0;
}
static int umx_tog(const umx_desc_t *d, uint32_t j)    /* .. an on/off toggle */
{
    return j >= UMB_NCTL && d->p && d->p->xel[(j - UMB_NCTL) / UMB_NXP].tog >> (j - UMB_NCTL) % UMB_NXP & 1u;
}
static const char *umx_en(const umx_desc_t *d, uint32_t j)   /* .. its name (a toggle: its element's) */
{
    return umx_tog(d, j) ? d->p->xel[(j - UMB_NCTL) / UMB_NXP].name : umx_c(d, j)->name;
}
static uint32_t umx_jat(const umx_desc_t *d, uint32_t i)   /* LIVE VIZ CTRLS' row i (1 ..): its element */
{
    uint32_t j;
    if (i <= d->nctl)
        return i - 1u;
    for (j = UMB_NCTL, i -= d->nctl; j < UMB_NCTL + UMC_NX - 1u; j++)
        if (umx_c(d, j) && !--i)
            break;
    return j;
}
static uint32_t umx_max(uint32_t pl, uint32_t j)       /* page pl's element j's most (MOVE's and MIRROR's four, a track; a
                                                         * toggle 1) */
{
    umx_desc_t d;
    if (j >= UMB_NCTL)
        return umx_desc(pl, &d) && umx_tog(&d, j) ? 1u : 99u;
    return (pl == UMX_SCREEN && j == G_MOVE) || (pl == UMX_FX && !j) || pl == UMX_BTN ? 3u : pl == UMX_BODY && j == 2u ? NTRK : 99u;
}
static uint32_t umx_key(const char *n)                 /* a name's key (never 0) */
{
    uint32_t h = 5381u;
    while (*n)
        h = (h * 33u) ^ (uint8_t)*n++;
    h = (h ^ (h >> 16)) & 0xFFFFu;
    return h ? h : 1u;
}
static void umc_load(void)                             /* the store read, once (none yet, or another's: empty) */
{
    if (umc.magic == UMC_MAGIC)
        return;
    memset(&umc, 0, sizeof umc);                        /* (a shorter store, from before the presets and the cells: its
                                                         * records as they were, the rest empty) */
    if (umc_flash(&umc, sizeof umc, 0) || umc.magic != UMC_MAGIC)
        memset(&umc, 0, sizeof umc);
    umc.magic = UMC_MAGIC;
    umc.lay[1] = 0;                                     /* (LAYER 2: dropped for now) */
    if (umc.rsv != 1u) {                                /* (a store from before the BUTTONS page, 2026-11-13: ENV toggles OVER
                                                         * now and LFO took its tap, so what ENV TAP moved LFO TAP moves) */
        uint32_t i, k;
        for (i = 0; i < UMC_NREC * UMC_NL; i++) {
            uint8_t *l = &umc.rec[i / UMC_NL].ls[i % UMC_NL];
            if ((k = (*l & 31u) - UMQ_TAP) < 2u)
                *l = (uint8_t)(*l - k + (k ^ 1u));
        }
        umc.rsv = 1;
    }
    umc_gen++;
}
/* the record of the page called name (kind: 0 a plate, its first nctl elements KNOB 1..3's; 1 BODIES; 2 SCREEN;
 * 3 EFFECTS), made the first time it is asked for: its links at first (KNOB 1..3 a plate's first three elements, KNOB 4
 * LIGHT down to dark; LFO TAP INVERT, the strobe, ENV TAP GLITCH, EDIT TAP MOSAIC: the user, 2026-11-13) */
static umc_rec_t *umc_rec(const char *name, uint32_t kind, uint32_t nctl)
{
    static const uint8_t FX0[3] = {G_INV - G_MIR, G_GLI - G_MIR, G_MOS - G_MIR};
    uint32_t k = umx_key(name), i, f = UMC_NREC - 1u;
    umc_rec_t *r;
    umc_load();
    for (i = UMC_NREC; i--;) {
        if (umc.rec[i].key == k && !umc.rec[i].pre)
            return &umc.rec[i];
        if (!umc.rec[i].key)
            f = i;
    }
    r = &umc.rec[f];                                    /* (a free one; all taken: the last one's place) */
    memset(r, 0, sizeof *r);
    memset(&umc.x[f], 0, sizeof umc.x[f]);
    r->key = (uint16_t)k;
    for (i = 0; i < 3u; i++)
        if (kind == 3u || (!kind && i < nctl)) {
            r->ls[i] = (uint8_t)(kind ? (UMQ_TAP + (i < 2u ? i ^ 1u : i)) | (uint32_t)FX0[i] << 5 : (UMQ_M1 + i) | i << 5);
            r->ld[i] = 99;
        }
    if (kind == 2u) {
        r->ls[0] = UMQ_M1 + 3u;                         /* (.. G_LIGHT, 0) */
        r->ld[0] = -99;
    }
    return r;
}
/* the record of the preset a page has chosen: b its own (BASIC: that one), p its plate (0: no plate). A preset not
 * changed yet has no record: it is made up as it comes (a plate's own preset from its file; another: BASIC as it is now)
 * in one place (umc_tmp: good until the next asked for); w: to be written, so it takes a free record (none left: what is
 * written does not stay) */
static umc_rec_t *umc_pre(umc_rec_t *b, const umb_plate_t *p, int w)
{
    uint32_t pre = b->cur, i;
    umc_rec_t *r = &umc_tmp;
    if (!pre)
        return b;
    for (i = 0; i < UMC_NREC; i++)
        if (umc.rec[i].key == b->key && umc.rec[i].pre == pre)
            return &umc.rec[i];
    for (i = 0; w && i < UMC_NREC; i++)
        if (!umc.rec[i].key) {
            r = &umc.rec[i];
            break;
        }
    *r = *b;
    *umc_x(r) = *umc_x(b);
    r->pre = (uint8_t)pre;
    r->cur = 0;
    if (p && p->api >= 4u && pre <= p->npre && pre <= UMB_NPRE) {
        const umb_pre_t *f = &p->pre[pre - 1u];
        memset(r->t, 0, sizeof r->t + sizeof r->ls + sizeof r->ld);
        memcpy(r->v, f->v, sizeof r->v);
        memset(umc_x(r)->lh, 0, sizeof umc_x(r)->lh);  /* (its links the file's: the eight's) */
        for (i = 0; i < 4u; i++)
            if ((f->ls[i] & 31u) < UMQ_NS) {
                r->ls[i] = f->ls[i];
                r->ld[i] = f->ld[i];
            }
    }
    return r;
}
static umc_rec_t *umx_recf(uint32_t pl, int w)         /* page pl's record now, its preset's (the page there: umx_desc) */
{
    umx_desc_t d;
    (void)umx_desc(pl, &d);
    return umc_pre(umc_rec(d.name, pl < UMX_BODY ? 0u : pl - UMX_BODY + 1u, d.nctl),
                   pl < UMB_NPLT ? umb_mod(pl) : 0, w);
}
#define umx_rec(pl) umx_recf(pl, 0)
/* record r's element j as set (its default def) */
static uint32_t umc_raw(const umc_rec_t *r, uint32_t j, uint32_t def, uint32_t max)
{
    uint32_t v = *umc_vp(r, j) ? *umc_vp(r, j) - 1u : def;
    return v > max ? max : v;
}
/* .. now: moved by its links, each its depth of the range (0 .. max) with its source at full */
static __attribute__((noinline)) uint32_t umc_ev(const umc_rec_t *r, uint32_t j, uint32_t def, uint32_t max)
{
    int32_t a = (int32_t)umc_raw(r, j, def, max) * 25344;   /* (x 256 x 99) */
    uint32_t i;
    for (i = 0; i < UMC_NL; i++)
        if ((r->ls[i] & 31u) && umc_lj(r, i) == j)
            a += r->ld[i] * umq_s[r->ls[i] & 31u] * (int32_t)max;
    return (uint32_t)umb_clamp((a + 12672) / 25344, 0, (int32_t)max);
}
static uint32_t umx_raw(uint32_t pl, uint32_t j)       /* page pl's element j as set */
{
    umx_desc_t d;
    const umb_ctl_t *c;
    return umx_desc(pl, &d) && (c = umx_c(&d, j)) ? umc_raw(umx_rec(pl), j, c->def, umx_max(pl, j)) : 0u;
}
static void umc_touch(void)                            /* the store changed: saved once the knobs rest */
{
    umc_dirty = 1;
    umv_ms = fm1_ms;
}
static void umx_put(uint32_t pl, uint32_t j, int32_t v)
{
    umx_desc_t d;
    if (!umx_desc(pl, &d) || !umx_c(&d, j))
        return;
    *umc_vp(umx_recf(pl, 1), j) = (uint8_t)(1 + umb_clamp(v, 0, (int32_t)umx_max(pl, j)));
    umc_touch();
}
#define umg_raw(g) umx_raw(UMX_SCREEN + ((g) >> 3), (g) & 7u)   /* SCREEN's and EFFECTS' own (G_*), as set */
#define umg_put(g, v) umx_put(UMX_SCREEN + ((g) >> 3), (g) & 7u, v)
/* where page pl lies: 0 nowhere, 1 the ground, 2 .. 4 LAYER 1..3 */
static uint32_t umx_place(uint32_t pl)
{
    umx_desc_t d;
    uint32_t i, k;
    if (pl < UMB_NPLT && umb_pick() == pl)
        return 1;
    if (!umx_desc(pl, &d))
        return 0;
    umc_load();
    for (i = 0, k = umx_key(d.name); i < 3u; i++)
        if (umc.lay[i] == k)
            return 2u + i;
    return 0;
}
/* .. laid at w (what lay there: nowhere; the ground left: none, black) */
static void umx_place_set(uint32_t pl, uint32_t w)
{
    umx_desc_t d;
    uint32_t at = umx_place(pl);
    if (!umx_desc(pl, &d) || at == w)
        return;
    if (at == 1u)
        umb_plate = (uint8_t)(0x80u | UMB_NONE);
    else if (at)
        umc.lay[at - 2u] = 0;
    if (w == 1u)
        umb_plate = (uint8_t)(0x80u | pl);
    else if (w)
        umc.lay[w - 2u] = (uint16_t)umx_key(d.name);
    umv_dirty = 1;
    umc_gen++;
    umc_touch();
}
/* a page's presets (LIVE VIZ CTRLS' PRESETS; a TIMELINE cell's): the one chosen, 0 BASIC .. UMC_NPRE - 1 */
static uint32_t umx_pre(uint32_t pl)
{
    umx_desc_t d;
    return umx_desc(pl, &d) ? umc_rec(d.name, pl < UMX_BODY ? 0u : pl - UMX_BODY + 1u, d.nctl)->cur : 0u;
}
static void umx_pre_set(uint32_t pl, uint32_t v)
{
    umx_desc_t d;
    umc_rec_t *b;
    if (!umx_desc(pl, &d))
        return;
    b = umc_rec(d.name, pl < UMX_BODY ? 0u : pl - UMX_BODY + 1u, d.nctl);
    if (b->cur != v % UMC_NPRE) {
        b->cur = (uint8_t)(v % UMC_NPRE);
        umc_touch();
    }
}
static void umx_pre_name(uint32_t pl, uint32_t v, char *b)   /* page pl's preset v's name (12 bytes): "P2 FLARE" */
{
    const umb_plate_t *p = pl < UMB_NPLT ? umb_mod(pl) : 0;
    b[0] = 'P', b[1] = (char)('1' + v), b[2] = 0;
    if (!v)
        str_cpy(b + 2, " BASIC", 10);
    else if (p && p->api >= 4u && v <= p->npre && v <= UMB_NPRE) {
        b[2] = ' ';
        str_cpy(b + 3, p->pre[v - 1u].name, 8);
    }
}
static uint32_t umx_find(uint32_t key);
static int umx_can(uint32_t pl, uint32_t w);           /* (ui_umbra_mac.c) */
/* SAVE > SONG > TIMELINE's picture tracks: the current song's row r's cell on track tk (0 MAIN: the ground; 1 OVER: LAYER 1) */
static umc_cell_t *umv_cell(uint32_t r, uint32_t tk)
{
    umc_load();
    return &umc.cell[song_cur % NSONG_USE][r % CHAIN_ROWS][tk & 1u];
}
/* row r as the picture: each track's cell, or the last one before it that says something (none: left as it is), laid where
 * it belongs with its preset (a page not there now: left) */
static void umv_apply(uint32_t r)
{
    uint32_t tk, j, pl;
    for (tk = 0; tk < 2u; tk++) {
        const umc_cell_t *c = 0;
        for (j = r % CHAIN_ROWS + 1u; j-- > 0u && !(c = umv_cell(j, tk))->key;)
            c = 0;
        if (!c)
            continue;
        umv_st.tr[tk] = c->tr;
        if (c->key == UMC_OFF) {
            if (tk && umc.lay[0]) {
                umc.lay[0] = 0;
                umc_gen++;
                umc_touch();
            } else if (!tk && umb_pick() != UMB_NONE) {
                umb_plate = (uint8_t)(0x80u | UMB_NONE);
                umv_dirty = 1;
                umc_touch();
            }
        } else if ((pl = umx_find(c->key)) != UMB_NONE && umx_can(pl, 1u + tk)) {
            umx_place_set(pl, 1u + tk);
            umx_pre_set(pl, c->pre);
        }
    }
}
/* a cell turned s: as before it (--), OFF, then every page that can lie there (the ground: a plate that fills the screen;
 * the layer: a veil) */
static void umv_turn(umc_cell_t *c, uint32_t tk, int32_t s)
{
    umx_desc_t d;
    int32_t pl = !c->key ? -2 : c->key == UMC_OFF ? -1 : (int32_t)umx_find(c->key);
    if (pl > (int32_t)UMX_BODY)
        pl = -2;
    do
        pl += s > 0 ? 1 : -1;
    while (pl >= 0 && pl <= (int32_t)UMX_BODY && !(umx_desc((uint32_t)pl, &d) && umx_can((uint32_t)pl, 1u + tk)));
    if (pl < -2 || pl > (int32_t)UMX_BODY)
        return;
    c->key = (uint16_t)(pl == -2 ? 0u : pl == -1 ? UMC_OFF : umx_key(d.name));
    c->pre = 0;
    umc_touch();
}
static void umv_name(const umc_cell_t *c, char *b)     /* a cell's page as words (12 bytes) */
{
    uint32_t pl = umx_find(c->key);
    umx_desc_t d;
    str_cpy(b, !c->key ? "--" : c->key == UMC_OFF ? "OFF" : umx_desc(pl, &d) ? d.name : "?", 12);
}
/* (ui_input.c row_copy, project.c: a new song) the current song's row d's cells become row s's (s past the rows: none);
 * d past the rows: every row's none */
static void umv_rows(uint32_t d, uint32_t s)
{
    umc_cell_t *c = umv_cell(d, 0);
    if (d >= CHAIN_ROWS)
        memset(umv_cell(0, 0), 0, sizeof umc.cell[0]);
    else if (s < CHAIN_ROWS)
        memcpy(c, umv_cell(s, 0), sizeof umc.cell[0][0]);
    else
        memset(c, 0, sizeof umc.cell[0][0]);
    umc_touch();
}
/* LAYER 1..3's pages as numbers (UMB_NONE: none, or not there), found again when the bank or the layers change */
static uint8_t umx_lp[3];
static uint32_t umx_find(uint32_t key)                 /* the page whose name's key it is (UMB_NONE: none there) */
{
    umx_desc_t d;
    uint32_t pl;
    for (pl = 0; key && pl <= UMX_BODY; pl++)
        if (umx_desc(pl, &d) && umx_key(d.name) == key)
            return pl;
    return UMB_NONE;
}
static void umx_layers(void)
{
    static uint8_t gen = 0xFFu;
    uint32_t i;
    umc_load();
    if (gen == (uint8_t)(ups_gen * 16u + umc_gen))
        return;
    gen = (uint8_t)(ups_gen * 16u + umc_gen);
    for (i = 0; i < 3u; i++) {                          /* (a page that may not lie there, from an older store: none) */
        uint32_t pl = umx_find(umc.lay[i]);
        umx_lp[i] = (uint8_t)(umx_can(pl, 2u + i) ? pl : UMB_NONE);
    }
}
_Static_assert(UMX_BODY < UMB_NONE, "a layer's page: a byte, UMB_NONE none");

/* ------------------------------------------------------------------------------------------- the layers --- */
#define UMB_VGN 150u                                    /* a strip's rows at most (gfx.c CV_MAX / 240) */
static int32_t umb_api_move(const umb_note_t *n, uint32_t ax, int32_t v, int32_t span) { return umn_fold(n->mo[ax & 1u], v, span); }
static const umb_api_t umb_api = {UMB_API, umb_api_move, SINE, UMB_EVC, umb_tn, umb_E, umb_ln, ump_lut};
static int16_t umb_vg[UMB_VGN] __attribute__((section(".pool")));   /* the strip's rows' light (umb_strip_t.vg) */
static void umb_lay_tracks(umb_lctx_t *c)              /* the tracks now: NOTE MOVEMENT's offsets, the roots */
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        c->mo[t][0] = umn.o[t][0];
        c->mo[t][1] = umn.o[t][1];
        c->root[t] = (uint8_t)((uint32_t)trk[t].p[P_ROOT] % 12u);
    }
}
/* plate p can run in room bytes, drawn in mode */
static int umb_ok(const umb_plate_t *p, uint32_t mode, uint32_t room)
{
    return p && p->magic == UMB_PLATE_MAGIC && p->api && p->api <= UMB_API && p->state <= room && (p->modes & UMB_MODE(mode));
}
#define UMB_THS 54                                      /* a TIMELINE preview's side (umb_thumb) */
static uint16_t umb_tl[2 * UMB_THS] __attribute__((section(".pool")));   /* .. a row of the screen for it */
static uint8_t umb_th;                                  /* .. being drawn: the strip is that row, not the canvas */
/* layer li's plate p, its state at st, over the canvas as the screen's x0, y0 .., in draw mode mode, its light lq (Q8) */
static void umb_lay_draw(uint32_t li, const umb_plate_t *p, void *st, int32_t x0, int32_t y0, uint32_t mode, int32_t lq)
{
    umb_layer_t *L = &umb_lay[li];
    const umc_rec_t *r = umc_pre(umc_rec(p->name, 0u, p->nctl), p, 0);
    umb_lctx_t c;
    umb_strip_t s;
    uint32_t j, h = umb_th ? 1u : cv_h < UMB_VGN ? cv_h : UMB_VGN;
    for (j = 0; j < UMB_NCTL; j++) {                    /* its elements now, the tracks chosen for each */
        c.k[j] = (uint8_t)(j < p->nctl ? umc_ev(r, j, p->ctl[j].def, 99u) : 0u);
        c.et[j] = r->t[j];
    }
    c.light = lq;
    c.now = umb_now;
    c.dt = 0;
    c.bare = umb.bare;
    c.tracks = (uint8_t)trk_live;
    c.layer = (uint8_t)li;
    c.level = (uint8_t)(umq_s[UMQ_LEVEL] > 255 ? 255 : umq_s[UMQ_LEVEL]);
    c.beat = (uint8_t)(umq_s[UMQ_BEAT] > 255 ? 255 : umq_s[UMQ_BEAT]);
    c.rsv = c.rsv2[0] = c.rsv2[1] = c.rsv3[0] = c.rsv3[1] = c.rsv4[0] = c.rsv4[1] = c.rsv4[2] = 0;
    for (j = 0; j < UMC_NX; j++) {                      /* (API 6) its other elements, as LIVE VIZ CTRLS sets them */
        const umb_xel_t *x = &p->xel[j / UMB_NXP];
        c.xk[j / UMB_NXP][j % UMB_NXP] = (uint8_t)(p->api >= 6u && j / UMB_NXP < p->nxe && j % UMB_NXP < x->np ?
            umc_ev(r, UMB_NCTL + j, x->p[j % UMB_NXP].def, x->tog >> j % UMB_NXP & 1u ? 1u : 99u) : 0u);
    }
    umb_lay_tracks(&c);
    if (p != L->pp || st != L->st || L->gen != ups_gen) {   /* another plate in its place, its state moved, or the bank
                                                         * changed under it: it starts afresh, the notes from now */
        L->pp = p;
        L->st = st;
        L->gen = ups_gen;
        memset(st, 0, p->state);
        L->qr = umb_q.w;
        L->last = umb_now;
        p->init(st, &c, &umb_api);
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
            if (v && !li)
                umn_note(t, q);                         /* (NOTE MOVEMENT: the ground's notes step it) */
            n.track = (uint8_t)t;
            n.note = umb_q.note[q];
            n.vel = (uint8_t)v;
            n.drum = trk[t].engine == ENGI_DRUM;
            n.lane = (uint8_t)(n.drum ? drum_lane(n.note) % NLANE : 0u);
            n.rsv = 0;
            n.mo[0] = umb_qo[q][0];                     /* (where NOTE MOVEMENT put it: the ground's call, this frame's) */
            n.mo[1] = umb_qo[q][1];
            p->note(st, &c, &umb_api, &n);
        }
        umb_lay_tracks(&c);                             /* (after the notes: where they left the tracks) */
        p->frame(st, &c, &umb_api);
    }
    for (j = 0; j < h; j++)
        umb_vg[j] = (int16_t)umb_rowlight(y0 + (int32_t)j, lq);
    s.px = umb_th ? umb_tl : cv_px;
    s.w = umb_th ? 2 * UMB_THS : (int32_t)cv_w;
    s.h = (int32_t)h;
    s.x0 = x0;
    s.y0 = y0;
    s.vg = umb_vg;
    s.mode = mode;
    p->draw(st, &c, &umb_api, &s);
}
/* the canvas as the screen's x0, y0 .. : the ground's plate (none: black), then LAYER 1..3's plates' light added over it,
 * as much as la says (TOTALITY's SEL ENV ARP: hidden, they keep their state). A layer's state lies under the one before it from the
 * block's end; one that does not fit beside the ground's waits, and starts afresh when it does. (BODIES on a
 * layer is figures, not a plate: TOTALITY draws them, ui_umbra_tot.c) */
static int32_t umb_prep(void)                          /* before a frame's plates: the tables, the clock, the layers; LIGHT (Q8) */
{
    umb_tabs();                                         /* (the noise, once) */
    if (!ump_lut[3][255])                               /* (the palette as the panel takes it: once) */
        ump_lut_make();
    umb_clock();
    umx_layers();
    return ((int32_t)umq_o[G_LIGHT] * 256 + 49) / 99;   /* (99: 256) */
}
static void umb_scene(int32_t x0, int32_t y0, const uint16_t *la)   /* la: the ground's light, LAYER 1..3's (Q8) */
{
    const umb_plate_t *p = umb_mod(umb_pl());
    uint32_t i, top = UMB_BLK, g = 0;
    int32_t lq = umb_prep();
    if (umb_ok(p, UMB_DRAW_REPLACE, UMB_BLK)) {
        g = (p->state + 3u) & ~3u;
        umb_lay_draw(0u, p, &umb, x0, y0, UMB_DRAW_REPLACE, (((lq * umv_lt[0]) >> 8) * la[0]) >> 8);
    } else {
        memset(cv_px, 0, cv_w * cv_h * sizeof *cv_px);
        umb_lay[0].pp = 0;
    }
    for (i = 0; i < 3u; i++) {
        p = umx_lp[i] < UMB_NPLT ? umb_mod(umx_lp[i]) : 0;
        if (!umb_ok(p, UMB_DRAW_ADD, top - g)) {
            umb_lay[1u + i].pp = 0;
            continue;
        }
        top -= (p->state + 3u) & ~3u;
        if (la[1u + i])
            umb_lay_draw(1u + i, p, (uint8_t *)&umb + top, x0, y0, UMB_DRAW_ADD, (((lq * la[1u + i]) >> 8) * (i ? 256 : umv_lt[1])) >> 8);
    }
}
static const uint16_t UMB_LA1[4] = {256u, 256u, 256u, 256u};
static void umb_backdrop(int32_t x0, int32_t y0) { umb_scene(x0, y0, UMB_LA1); }   /* (HOME's pages: all of it) */
/* (SAVE > SONG > TIMELINE's previews) layer li alone (0 the ground, 1 LAYER 1's plate), as TOTALITY shows it: the screen's
 * middle 2 s x 2 s px at half size, on the canvas at x, y. Each lies where umb_scene keeps it, so the picture goes on as
 * on HOME. 0: no plate there to draw (none, a figure, no room) */
static int umb_thumb(uint32_t li, int32_t x, int32_t y)
{
    const umb_plate_t *p = umb_mod(umb_pl());
    uint32_t g = 0;
    int32_t lq = umb_prep(), i, j;
    void *st = &umb;
    if (umb_ok(p, UMB_DRAW_REPLACE, UMB_BLK))
        g = (p->state + 3u) & ~3u;
    else if (!li)
        return 0;
    if (li) {
        p = umx_lp[0] < UMB_NPLT ? umb_mod(umx_lp[0]) : 0;
        if (!umb_ok(p, UMB_DRAW_ADD, UMB_BLK - g))
            return 0;
        st = (uint8_t *)&umb + UMB_BLK - ((p->state + 3u) & ~3u);
    }
    umb.bare = umb_th = 1;
    for (j = 0; j < UMB_THS && (uint32_t)(y + j) < cv_h; j++) {
        memset(umb_tl, 0, sizeof umb_tl);
        umb_lay_draw(li, p, st, 120 - UMB_THS, 120 - UMB_THS + 2 * j, li ? UMB_DRAW_ADD : UMB_DRAW_REPLACE, lq);
        for (i = 0; i < UMB_THS; i++)
            cv_px[(uint32_t)(y + j) * cv_w + (uint32_t)(x + i)] = umb_tl[2 * i];
    }
    umb.bare = umb_th = 0;
    return 1;
}
#include "ui_umbra_tot.c"                             /* TOTALITY: the plate alone, the visual effects */
#include "ui_umbra_welcome.c"                         /* HOME > WELCOME's pages */
#include "ui_umbra_mac.c"                             /* HOME > UMBRA's LIVE AUDIO CTRLS and LIVE VIZ CTRLS */
#include "ui_umbra_plate.c"                           /* the visuals' settings saved */
