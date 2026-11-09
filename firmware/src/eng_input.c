/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.54) INPUT: a track that plays the audio input instead of a synth (engine 15).
 * The input is the USB audio one: an interface on the USB host port (usb_host.c, V10.53) or the computer's USB IN+OUT
 * (usb.c uac_in_fill), a block of stereo Q15 in uin_l / uin_r (uin_on: this block holds it).
 *
 * SOURCE picks what the track takes: IN 1 (left), IN 2 (right), 1+2 (their sum, mono) or STEREO (both, the image kept
 * as the IMAGE of DRUM-X and the poly synths keep theirs: the part's mono sum through its chain, each side what it gives
 * up, eng_drum.c dx_sl / dx_sr). Two INPUT tracks can take IN 1 and IN 2 each. Then GAIN, GATE (a noise gate: closed
 * below its level), BITS and RATE (fewer bits, a lower rate: held samples), and the track's own chain as any part's:
 * FILTER, DIST, LEVEL, PAN, the sends, the mutes, the matrix. PLAY: THRU sounds it all the time; KEYS only while a
 * note plays, at the note's envelope (VOICE: ATK DEC SUS REL; the sequencer's steps gate it: a trance gate), RING then
 * multiplying it by a sine at the note's pitch. WIDTH: STEREO's width.
 *
 * While a track is INPUT the input goes to it alone (fx.c mix_part: the last live track's USB IN+OUT mix steps aside).
 * The FOLLOW source of the matrix (mod.c MS_FOLLOW) is the input's envelope, whatever track takes it. */
static int32_t uin_l[CTL], uin_r[CTL];                  /* (0.7.5) the input's block (usb.c uac_in_fill; was in fx.c) */
static volatile uint8_t uin_on;
static int32_t sw_l[CTL], sw_r[CTL];                    /* (SWARM) its own block, beside the input's (both can play) */
static uint8_t sw_on;
enum { AIN_IN1, AIN_IN2, AIN_SUM, AIN_ST, AIN_SW, AIN_N };   /* (SWARM: the drone of the surroundings) */
static const char *const N_AIN_SRC[] = {"IN 1", "IN 2", "1+2", "STEREO", "SWARM"};
static const char *const N_AIN_PLAY[] = {"THRU", "KEYS"};
static const int16_t SW_FULL[P_COUNT] = {[P_E4] = 127};   /* (SWARM's fill: WIDTH whole, DIST none) */
static int16_t ain_gg[NTRK];                             /* GATE's gain now, Q15 */
static uint8_t ain_upd[NTRK];                            /* this block's GATE not yet stepped (ain_block) */
static uint8_t ain_any;                                  /* a track is INPUT (mod.c macro_master, once a block) */

/* the source of track t, block of n, into out (mono; STEREO: the mid, its sides into dx_sl / dx_sr at WIDTH when
 * side): GAIN, GATE (stepped once a block), BITS, RATE */
static __attribute__((noinline)) void ain_fill(const track_t *t, int32_t *out, uint32_t n, int side)
{
    const int16_t *p = t->p;
    uint32_t ti = (uint32_t)(t - trk) % NTRK, src = (uint32_t)clamp(p[P_E0], 0, AIN_N - 1), i;
    int32_t g = clamp(p[P_E1], 0, 127), thr = clamp(p[P_E2], 0, 127), sh = clamp(p[P_E6], 0, 127) * 14 / 127;
    uint32_t hold = (1u << (clamp(p[P_E7], 0, 127) * 5 / 127)) - 1u;   /* RATE: 1 .. 32 samples held */
    int32_t g0 = ain_gg[ti], g1 = g0, pk = 0, w = 0;
    g = g * g;                                           /* GAIN: Q12, 64 = 1.0, 127 = +12 dB */
    const int32_t *bl = uin_l, *br = uin_r;
    if (src == AIN_SW) {                                 /* (SWARM: its own block, stereo, whole; its knobs are the
                                                          * swarm's) */
        bl = sw_l, br = sw_r;
        src = AIN_ST;
        g = 4096;
        thr = sh = 0;
        hold = 0;
        p = SW_FULL;
    }
    if (side && src == AIN_ST) {
        int32_t d = clamp(p[P_DIST], 0, 64);
        w = clamp(p[P_E4], 0, 127) * (d ? 256 - d * 3 : 256) >> 8;   /* (DIST shapes the sum only: narrower) */
        if (w)
            dx_xon |= 1u;
    }
    for (i = 0; i < n; i++) {
        uint32_t j = i & ~hold;
        int32_t l = bl[j], r = br[j], x = src == AIN_IN1 ? l : src == AIN_IN2 ? r : src == AIN_SUM ? l + r : (l + r) >> 1;
        x = (x * g) >> 12;
        if (src == AIN_ST && w)
            dx_sl[i] = -(dx_sr[i] = ((((l - r) * g) >> 13) * w) >> 7);   /* L = sum - sl, R = sum - sr */
        out[i] = x;
        x = x < 0 ? -x : x;
        if (x > pk)
            pk = x;
    }
    if (thr) {                                           /* GATE: open over its level, closed under 3/4 of it */
        if (ain_upd[ti]) {
            int32_t lv = thr * thr * 2;
            g1 = pk > lv ? 32767 : pk < lv * 3 / 4 ? g0 - (g0 >> 4) - 1 : g0;
            g1 = clamp(g1, 0, 32767);
            ain_gg[ti] = (int16_t)g1;
            ain_upd[ti] = 0;
        }
        for (i = 0; i < n; i++) {
            int32_t k = g0 + (((g1 - g0) * (int32_t)i) >> CTL_LOG2);
            out[i] = mulq15(out[i], k);
            if (src == AIN_ST && w) {
                dx_sl[i] = mulq15(dx_sl[i], k);
                dx_sr[i] = -dx_sl[i];
            }
        }
    } else
        ain_gg[ti] = 32767;
    if (sh)
        for (i = 0; i < n; i++)
            out[i] &= ~((1 << sh) - 1);                  /* BITS */
}

/* (SWARM) SOURCE SWARM: a drone that makes itself from the surroundings (the user's ask: the antenna and the clocks,
 * their relations). With MENU > SYSTEM > BLUETOOTH AIR scanning, every device heard around (phones, watches,
 * headphones, laptops, TVs: their advertisements, bt_seg.c BLE_HW_ADV_TAP) is a voice, eight at most; with none, the
 * FM-1's own clocks (the audio interrupt's jitter against TIMER4) are one, so it always plays. A voice is a sine: its
 * note its address's (in the track's ROOT / SCALE), its level its RSSI (the range seen), each advertisement a hit (its
 * clock its rhythm: the swarm's polyrhythm; V10.6z6: played on the grid's next step, the LEAD's below), its period
 * against the loudest's a detune (two devices beat at their clocks' relation), its jitter a vibrato. The knobs
 * (input_desc): PLUCK (0 a sustained drone .. a pluck on each hit), FM (the device's own bytes as the modulator, an
 * envelope on each hit), PLAY (KEYS: the keys transpose it and gate it
 * at the note's envelope), IMAGE (0 each at its channel's side .. each hit thrown across the field), DRIFT (each hit
 * bends the pitch, gliding back), OCTAVE (V10.6v: its first half each hit of a device an octave up, over 1..4 octaves,
 * round; its second half a walk of the scale: each hit the next degree of ROOT / SCALE, over 2..8 degrees, round),
 * CHAOS (V10.6v, SELF's place; V10.6z6: the ceiling, the closeness and the devices give how much of it plays: CHAOS c
 * below as CHAOS x that depth: SELF's cross-modulation of before, twice as fast (CHAOS c = SELF 2c); past 24 each voice
 * phase-modulated by the one before (a chain, never a loop: no noise), past 48 the FM ratio jumping on hits, past 64 the
 * FM deeper and up to 100 a wavefold; past 100 each voice ring-modulated by the one before: another timbre). (V10.6v)
 * Each device its own timbre from its bytes (its family by its advertisement's manufacturer or appearance: SW_FAM): the
 * sine driven towards a square, an FM ratio of
 * SW_RT, an FM depth, a tone (its RSSI moving opens it); a new device glides the knobs PLUCK FM IMAGE DRIFT away from
 * the knob's values (sw_ko: ~1.5 s) to another place; it swells in, a device gone fades out long, a ninth takes the voice
 * heard longest ago without a click. The FM-1's own voice: today's timbre (CHAOS 0: V10.6u's, until V10.6z6's grid).
 * Into its own block (sw_l / sw_r) once a block (mod.c), the first SWARM track's knobs: SWARM and a USB input (IN 1
 * IN 2 1+2 STEREO on another track) play together */
#define SW_N 8u
#define SW_V (SW_N + 1u)                                 /* (V10.6z6) the voices: the devices', then the lead's (sw[SW_N]) */
#define SW_TPS 24000000u                                 /* TIMER4 ticks a second */
#define SW_SELF 0x4E4763CBu                              /* (SELF_ID's id as sw_heard makes it) */
static struct sw_dev {
    uint32_t id, last, per, ph, pm, inc;                 /* address hash (0 free); last packet; period; phases; step */
    int32_t acc, amp, jit, fme, pan, dr, lp;             /* hit; level; jitter (Q8); FM envelope; pan (Q15, L 0); drift;
                                                          * (V10.6v) its tone's state */
    uint16_t lv;                                         /* last RSSI level (0..126) */
    uint8_t hit, ch, note, oct;
    uint8_t nt, sh, rt, fi, tn, mv, in, rj;              /* (V10.6v) the note sounding; shape; FM ratio (SW_RT); FM depth
                                                          * (Q7); tone (255 open); RSSI's movement; its swell; CHAOS's
                                                          * ratio (0xFF its own) */
    int8_t w[8];                                         /* its bytes: the FM modulator */
} sw[SW_V] __attribute__((section(".bss.swarm")));      /* (V10.6v: its own section, out of the merged statics the
                                                          * USB's interrupt code reads, as fx.c's djf) */
static int32_t sw_lmin = 127, sw_lmax, sw_prev;
static uint32_t sw_tick, sw_rnd = 0x9E3779B9u;
static int8_t sw_key;                                    /* KEYS: the transpose */
static uint8_t ain_tr, ain_sw;                           /* the first SWARM (else INPUT) track; a track is SWARM */
static int16_t sw_ko[4] __attribute__((section(".bss.swarm"))), sw_kt[4] __attribute__((section(".bss.swarm")));   /* .. PLUCK
                                                          * FM IMAGE DRIFT's offsets (Q8), where they go */
static const uint8_t SW_RT[6][2] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}, {0, 1}, {1, 1}};   /* x1 x2 x3 x4 x0.5 x1.5 */
static BTAPP_RO const uint8_t SW_FAM[4][4] = {                  /* a family's shape, FM ratios (two of SW_RT), depth, tone */
    {0, 0, 0, 0},                                        /* (any: its bytes alone) */
    {0, 0x21, 120, 170},                                 /* Apple: a sine, x2 / x3, deep: glass, bells */
    {190, 0x30, 30, 90},                                 /* Microsoft: a square, x1 / x4, shallow: reeds */
    {100, 0x54, 80, 50},                                 /* Google, Samsung, a watch: a triangle, x0.5 / x1.5: wood */
};
static int32_t scale_snap(const track_t *t, int32_t n);  /* seq.c */
static int32_t scale_steps(const track_t *t, int32_t n, int32_t d);
#ifndef fm1_ticks
static uint32_t fm1_ticks(void);                         /* (hal/fm1_time.h; the host tests' own) */
#endif
static uint32_t sw_rand(void) { sw_rnd ^= sw_rnd << 13; sw_rnd ^= sw_rnd >> 17; sw_rnd ^= sw_rnd << 5; return sw_rnd; }
/* (V10.6z6, the owner: "still mostly a pedal note with an irregular rhythm"; melodies from the proximity's changes) the
 * LEAD, a ninth voice (sw[SW_N]: the devices' oscillator, shape, FM, fold, tone, CHAOS's chain) playing a melody on the
 * tempo's grid:
 *   closeness  PROX (bt.c bt_sw_prox: a central's link, ~100 packets a second, as MS_PROX maps it; not calibrated: its
 *              own range, larger nearer) when a central is connected, else AIR's nearest device (its level through
 *              PROX's FAR / NEAR points, or how far over the weakest heard: 32 levels full), else (none) a slow walk
 *              mostly far; one pole (PROX ~12 ms, the others ~0.4 s): sm.c 0 FAR .. 32767 NEAR, the morph (sm.mq
 *              0..128)
 *   FAR / NEAR two states as DRUM-X's patches A / B (EDIT > SWARM, SWARM 2: SIDE then the side's values), each RATE (the
 *              grid: 1/1 .. 1/32; the notes' decay half a step), MOVE (under 24 a pedal re-struck softly, 24..63 a
 *              neighbour and back, over 40 rests, from 64 an arpeggio of the chord's tones, over 80 octave jumps), TONE
 *              (sine .. square, dark .. open, past 90 a fold, the FM index), NOISE (into the lead before its tone: its
 *              colour TONE's), MOD (vibrato, the FM's depth, CHAOS's depth); every value morphed by the closeness
 *   devices    each one heard adds 12 to MOD's depth (1 mild .. 8 extreme; PROX alone 20): CHAOS the ceiling (the CHAOS
 *              that plays: CHAOS x the depth); the nearest's FM ratio and depth the lead's; the bed a little lower near
 *   the melody each step of the grid (in time with the transport, its own clock stopped; the devices' hits wait for it
 *              too: no irregular rhythm) the closeness's change since the last step moves the degree: rising up
 *              the scale, falling down, by two when fast; flat: MOVE's; from C3 over 2 octaves (OCTAVE's second half
 *              3), reflected at the ends; in the track's ROOT / SCALE; CHR: the blues (1 b3 3 4 b5 5 b7), b3 3 b5 5 b7
 *              scooped from a semitone below (a grace) half the time
 * The two states in slots no EXT sound reads (DRUM-X's lane levels 2..7, WHEEL's ROTOR, DRUM's IMAGE, LOFI's RTRG), each
 * stored ^ its mask: what every project and preset before holds there (127, or 0) reads as the factory pair */
enum { SWS_RATE, SWS_MOVE, SWS_TONE, SWS_NOISE, SWS_MOD, SWS_N };
static const uint8_t SW_SLOT[2][SWS_N] = {{P_LN1, P_LN2, P_LN3, P_LN4, P_LN5}, {P_LN6, P_WDUST, P_WRDEP, P_DXIMG, P_LRTRG}};
static const uint8_t SW_XM[2][SWS_N] = {{127 ^ 30, 127 ^ 12, 127 ^ 24, 127 ^ 30, 127 ^ 14},   /* FAR: 1/2, a pedal, dark, a breath */
                                        {127 ^ 96, 127 ^ 100, 127 ^ 100, 12, 88}};             /* NEAR: 1/16, an arpeggio, open */
static int32_t sw_sv(const track_t *t, uint32_t s, uint32_t k) { return (t->p[SW_SLOT[s & 1u][k]] & 127) ^ SW_XM[s & 1u][k]; }
static void sw_sv_put(track_t *t, uint32_t s, uint32_t k, int32_t v) { t->p[SW_SLOT[s & 1u][k]] = (int16_t)(clamp(v, 0, 127) ^ SW_XM[s & 1u][k]); }
static struct {
    int32_t c, cs;                                       /* the closeness (Q15, smoothed); at the last step */
    uint32_t free, nr, lph;                              /* samples while stopped; the noise; the vibrato's phase */
    int16_t wk, sus;                                     /* (no source) the walk; the lead's sustain */
    int8_t anc, nb;                                      /* the melody's degree; a neighbour to come back from */
    uint8_t arp, src, nd, mq, ak, ash, md, ri;           /* the arpeggio's place; 0 none 1 AIR 2 PROX; the devices; the
                                                          * morph (0 FAR .. 128 NEAR); the lead's decay, attack (shifts);
                                                          * the depth; RATE's place (0 1/1 .. 5 1/32) */
} sm __attribute__((section(".bss.swarm")));
static volatile int16_t sw_pxv, sw_pxf, sw_pxn;          /* (bt.c bt_sw_prox) PROX's closeness; its FAR / NEAR points (Q8) */
static volatile uint8_t sw_pxon;                         /* .. a central's link gives it */
static uint32_t ms_clock;                                /* (mod.c: the transport's samples) */
static uint32_t beat_samples(void);                      /* fx.c */
static uint32_t scale_mask(const track_t *t);            /* seq.c */
/* a step of the grid: the lead's next note */
static __attribute__((noinline)) void sw_step(const track_t *t, const int32_t *mp, uint32_t len, uint32_t nv)
{
    static const int8_t SINW[8] = {0, 90, 127, 90, 0, -90, -127, -90};
    struct sw_dev *L = &sw[SW_N];
    uint32_t r = sw_rand(), mask = scale_mask(t) & 0xFFFu, k = 0, i, mv = (uint32_t)mp[SWS_MOVE], bl = mask == 0xFFFu || !mask;
    int32_t d = sm.c - sm.cs, st = 0, span, deg, nt, lv, rt = t->p[P_ROOT];   /* (ROOT 0..11: param_fit) */
    sm.cs = sm.c;
    if (bl)
        mask = 0x4F9u;                                   /* (CHR: the blues, 1 b3 3 4 b5 5 b7) */
    for (i = 0; i < 12u; i++)
        k += mask >> i & 1u;
    span = (int32_t)k * (2 + (t->p[P_E6] >= 64));
    if (!sm.src)                                         /* (none: a walk, mostly far) */
        sm.wk = (int16_t)clamp(sm.wk + (int32_t)(r >> 20 & 4095u) - 2048, 0, 14000);
    sm.ak = (uint8_t)clamp(30 - __builtin_clz((len >> CTL_LOG2) | 1u), 2, 10);   /* (the decay: half the step) */
    sm.ash = (uint8_t)(6u - (sm.ri > 4u ? 4u : sm.ri));
    L->mv = (uint8_t)clamp((d < 0 ? -d : d) >> 5, 0, 255);   /* (moving: its tone opens) */
    L->rj = 0xFFu;
    if (mv > 40u && (r & 255u) < (mv - 40u) / 2u) {     /* a rest */
        sm.sus = 0;
        L->acc = 0;
        return;
    }
    if (d > 300 || d < -300) {                           /* the curve moves (~1 %: not its noise): so does the melody */
        st = (d > 0 ? 1 : -1) * (d > 2400 || d < -2400 ? 2 : 1);
        sm.nb = 0;
    } else if (mv >= 24u && mv < 64u && (r >> 9 & 1u)) {   /* flat: a neighbour and back */
        st = sm.nb ? -sm.nb : (r >> 10 & 1u) ? 1 : -1;
        sm.nb = (int8_t)(sm.nb ? 0 : st);
    }
    deg = sm.anc + st;
    deg = deg > span ? 2 * span - deg : deg < 0 ? -deg : deg;   /* (reflected at the ends) */
    sm.anc = (int8_t)deg;
    if (mv >= 64u) {                                     /* an arpeggio of the chord's tones on it: 1 3 5 8 5 3 */
        i = sm.arp++ % 6u;
        deg += i == 3u ? (int32_t)k : 2 * (int32_t)(i < 3u ? i : 6u - i);
    }
    for (nt = 48 + rt; deg > 0; deg--)                   /* (the degree up the scale from ROOT, C3 ..) */
        do nt++; while (!(mask >> ((uint32_t)(nt - rt) % 12u) & 1u));
    i = (uint32_t)(nt - rt) % 12u;
    if (mv > 80u && (int32_t)(r >> 16 & 127u) < ((int32_t)mv - 80) / 2)
        nt += 12;                                        /* an octave jump */
    if (bl && (0x4D8u >> i & 1u) && (r >> 24 & 1u))
        L->dr = -16;                                     /* (the blues: b3 3 b5 5 b7 scooped from a semitone below) */
    lv = 7000 + sm.mq * 40;
    sm.sus = (int16_t)(lv * (100 - clamp((int32_t)sm.ri * 14 + (int32_t)mv / 4, 0, 85)) / 100);
    L->acc = lv - sm.sus;                                /* (FAR: a pedal, re-struck softly) */
    L->fme = 32767;
    L->pan = 16384 + (((int32_t)(r & 0xFFFFu) - 32768) * (t->p[P_E4] & 127) >> 8);   /* (IMAGE) */
    L->nt = (uint8_t)(nt > 108 ? 108 : nt);
    if (nv < SW_N && sw[nv].tn != 255u)                 /* (the nearest device's FM ratio and depth) */
        L->rt = sw[nv].rt, L->fi = sw[nv].fi;
    else
        L->rt = 1u, L->fi = 64u;
    memcpy(L->w, SINW, 8);                               /* (its modulator: a sine) */
}

/* (V10.6v) a device's family from its advertisement (p: the header, the address, then its AD structures, n bytes in all):
 * the manufacturer's id (AD 0xFF), else its appearance (AD 0x19); 0 none known. (Only ever from the radio's interrupt,
 * bt_air_tap, the segment's own code: in the Bluetooth segment) */
static BTAPP_FN uint32_t sw_family(const uint8_t *p, uint32_t n)
{
    uint32_t i = 8, l, ty, v;
    while (i + 3u < n && (l = p[i]) != 0u && i + 1u + l <= n) {
        ty = p[i + 1u];
        v = (uint32_t)p[i + 2u] | (l > 2u ? (uint32_t)p[i + 3u] << 8 : 0u);
        if (ty == 0xFFu && l > 2u)
            return v == 0x004Cu ? 1u : v == 0x0006u ? 2u : v == 0x00E0u || v == 0x0075u ? 3u : 0u;
        if (ty == 0x19u && l > 2u)                       /* (a phone, a computer; a watch, a tag) */
            return (v >> 6) == 1u ? 1u : (v >> 6) == 2u ? 2u : (v >> 6) == 3u || (v >> 6) == 31u ? 3u : 0u;
        i += 1u + l;
    }
    return 0;
}
/* (V10.6v) a new device's timbre from its bytes (d->w) and its family (sw_family): shape, FM ratio, depth, tone; the knobs
 * PLUCK FM IMAGE DRIFT to glide somewhere else (sw_kt: ain_swarm). (The radio's interrupt alone: in the Bluetooth segment,
 * as sw_family) */
static BTAPP_FN void sw_timbre(struct sw_dev *d, const uint8_t *p, uint32_t n)
{
    static BTAPP_RO const uint8_t KR[4] = {48, 40, 48, 32};   /* (how far: PLUCK FM IMAGE DRIFT) */
    uint32_t i, h = d->id * 2654435761u, f = sw_family(p, n), r = sw_rnd ^ h;
    const uint8_t *fa = SW_FAM[f];
    for (i = 0; i < 4u; i++, r = r * 1664525u + 1013904223u)
        sw_kt[i] = (int16_t)(((int32_t)((r >> 16) % (2u * KR[i] + 1u)) - KR[i]) << 8);
    for (i = 0; i < 8u; i++)
        h = (h ^ (uint8_t)d->w[i]) * 16777619u;
    h ^= h >> 15;
    d->sh = (uint8_t)(f ? clamp(fa[0] + (int32_t)(h & 63u) - 32, 0, 255) : (int32_t)(h & 255u));
    d->rt = (uint8_t)(f ? (h >> 8 & 1u ? fa[1] >> 4 : fa[1] & 15u) : (h >> 8) % 6u);
    d->fi = (uint8_t)(f ? fa[2] + (h >> 10 & 63u) : 40u + (h >> 10) % 160u);
    d->tn = (uint8_t)(f ? fa[3] + (h >> 16 & 63u) : 40u + (h >> 16) % 200u);
    d->in = 0;
}
static BTAPP_FN void sw_moved(struct sw_dev *d, uint32_t lv)
{
    uint32_t dl = (lv > d->lv ? lv - d->lv : d->lv - lv) * 24u + d->mv;
    d->mv = (uint8_t)(dl > 255u ? 255u : dl);
}
static void sw_heard(const uint8_t *p, uint32_t n, uint32_t lv, uint32_t ch, uint32_t now)
{
    uint32_t i, k = SW_N, o = 0, j = 0, id;
    struct sw_dev *d;
    id = ((uint32_t)p[2] | (uint32_t)p[3] << 8 | (uint32_t)p[4] << 16 | (uint32_t)p[5] << 24) ^ ((uint32_t)p[6] << 5 | (uint32_t)p[7] << 13) ^ (p[0] & 15u) << 28;
    id |= 1u;
    for (i = 0; i < SW_N; i++) {
        if (sw[i].id == id) { k = i; break; }
        if (!sw[i].id ? o != 0xFFFFFFFFu : now - sw[i].last > o) { o = sw[i].id ? now - sw[i].last : 0xFFFFFFFFu; j = i; }   /* (a free
                                                         * one first, else the longest unheard) */
    }
    if (k == SW_N) {                                     /* a new device: the oldest's voice (or a free one; its level kept:
                                                          * no click, V10.6v) */
        const uint8_t *q = n > 8u ? p + 8 : p + 2;      /* (no divide: the data bytes, or the address with none) */
        uint32_t m = n > 8u ? n - 8u : 6u, c = 0;
        d = &sw[j];
        d->id = id;
        d->per = 0;
        d->jit = d->oct = d->note = d->nt = d->mv = d->sh = 0;
        for (i = 0; i < 8u; i++) {
            d->w[i] = (int8_t)q[c];
            if (++c >= m)
                c = 0;
        }
        d->rj = d->tn = d->in = 0xFFu;                   /* (the FM-1's own: the timbre of before) */
        d->rt = (uint8_t)((id >> 5) % 3u);
        d->fi = 128;
        if (id != SW_SELF)
            sw_timbre(d, p, n);
    } else {
        uint32_t dt = now - (d = &sw[k])->last;
        if (dt < 4u * SW_TPS) {
            int32_t e = d->per ? (int32_t)dt - (int32_t)d->per : 0;
            d->per = d->per ? d->per + (uint32_t)((int32_t)(dt - d->per) / 8) : dt;
            e = e < 0 ? -e : e;
            d->jit += (((int32_t)((uint64_t)(uint32_t)e * 256u / (d->per | 1u))) - d->jit) / 4;
        }
        if (d->tn != 255u)                               /* (V10.6v) its RSSI moving: brighter (a device's: the segment) */
            sw_moved(d, lv);
    }
    d->last = now;
    d->lv = (uint16_t)lv;
    d->ch = (uint8_t)ch;
    d->hit = 1;
}
void bt_air_tap(const uint8_t *p, uint32_t n, uint32_t w, uint32_t ch)   /* the BLE RX interrupt */
{
    if (n >= 8u)
        sw_heard(p, n, (w & 63u) + (w >> 8 & 63u), ch, fm1_ticks());
}
/* (V10.6z6) a block's values of every voice, for ain_swarm's samples (sw_prep: once a block, built for size) */
struct sw_blk {
    int32_t gl[SW_V], gr[SW_V], a0[SW_V], da[SW_V], fe[SW_V], kq[SW_V], fv[SW_V], nz[SW_V], slf, xm, rm;
    uint32_t pmi[SW_V];
};
static __attribute__((noinline)) uint32_t sw_prep(struct sw_blk *b, uint32_t n)
{
    static const uint8_t SELF_ID[8] = {0, 0, 0x4A, 0x49, 0x41, 0x4E, 0x54, 0x31};   /* (the FM-1's own voice: "JIANT1") */
    const track_t *t = &trk[ain_tr % NTRK];
    const int16_t *p = t->p;
    uint32_t i, v, now = fm1_ticks(), ref = 0, rlv = 0, any = 0, ext = 0, nd = 0, nv = SW_N, len, pos, hit;
    int32_t pl, fm, img, dft, ocv = clamp(p[P_E6], 0, 127), ocs, ch, near = -1, mp[SWS_N], md;
    int32_t fg;
    sw_rnd += now;                                       /* (the interrupt's own jitter: entropy) */
    for (i = 0; i < 4u; i++)                             /* (V10.6v) the knobs where a new device sent them, ~1.5 s */
        sw_ko[i] = (int16_t)(sw_ko[i] + ((sw_kt[i] - sw_ko[i]) >> 9));
    pl = clamp(p[P_E1] + (sw_ko[0] >> 8), 0, 127);
    fm = clamp(p[P_E2] + (sw_ko[1] >> 8), 0, 127) * 6;
    img = clamp(p[P_E4] + (sw_ko[2] >> 8), 0, 127);
    dft = clamp(p[P_E5] + (sw_ko[3] >> 8), 0, 127);
    ocs = ocv < 64 ? ocv / 16 : 2 + (ocv - 64) * 6 / 63;   /* (V10.6v: octaves 0..3 more; degrees 2..8) */
    for (v = 0; v < SW_N; v++) {                         /* the reference clock: the loudest device heard */
        if (sw[v].id && now - sw[v].last < 2u * SW_TPS && sw[v].id != SW_SELF) {   /* (V10.6h: the FM-1's own voice is not a
                                                         * device; the old test never matched, its clock beat every ~2 s) */
            ext = 1;
            if (sw[v].per && sw[v].lv >= rlv)
                rlv = sw[v].lv, ref = sw[v].per;
        }
        if (sw[v].id && sw[v].id != SW_SELF && now - sw[v].last < 6u * SW_TPS && (nd++, (int32_t)sw[v].lv > near))
            near = sw[v].lv, nv = v;                     /* (V10.6z6) the devices around, the nearest */
    }
    {   /* (V10.6z6) the closeness: PROX, else the nearest device, else the walk; the morph, the states, the depth */
        int32_t s = sm.wk, f = sw_pxf, q = sw_pxn;
        sm.src = 0;
        if (sw_pxon)
            s = sw_pxv, sm.src = 2;
        else if (nd) {
            s = f != q ? ((near << 8) - f) * 32767 / (q - f) : (near - sw_lmin) << 10;   /* (32 levels over the weakest) */
            sm.src = 1;
        }
        sm.c += (clamp(s, 0, 32767) - sm.c) >> (sm.src == 2u ? 3 : 8);
        sm.mq = (uint8_t)((sm.c + 128) >> 8);            /* (0..128) */
        for (i = 0; i < SWS_N; i++) {
            int32_t a = sw_sv(t, 0, i);
            mp[i] = a + ((sw_sv(t, 1, i) - a) * sm.mq >> 7);
        }
        sm.nd = (uint8_t)nd;
        md = clamp(mp[SWS_MOD] + (int32_t)nd * 12 + (sm.src == 2u && !nd ? 20 : 0), 0, 127);
        sm.md = (uint8_t)md;
        sm.ri = (uint8_t)((uint32_t)mp[SWS_RATE] * 6u >> 7);
        len = beat_samples();
        len = sm.ri <= 2u ? len << (2u - sm.ri) : len >> (sm.ri - 2u);
        pos = song.playing ? ms_clock : sm.free;
        sm.free += n;
        hit = len >= n && pos % len < n;                 /* (a step of the grid in this block) */
        if (hit)
            sw_step(t, mp, len, nv);
    }
    ch = clamp(p[P_E7], 0, 127) * md >> 7;               /* (V10.6z6: CHAOS the ceiling, the depth moves it) */
    b->slf = ch * 6;
    b->xm = ch > 24 ? (ch - 24) * 12 : 0, fg = ch > 64 && ch <= 100 ? 4096 + (ch - 64) * 64 : 0;   /* (CHAOS: */
    b->rm = ch > 100 ? (ch - 100) * 8 / 9 : 0;             /* the chain's depth, the fold's 1..1.6, the ring's 0..3/4) */
    if (!ext && !(sw_rand() & 511u))                    /* (none around: the FM-1's own clocks, ~0.7 s apart) */
        sw_heard(SELF_ID, 8, 40u + (sw_rnd >> 27), 38u, now);
    for (v = 0; v < SW_V; v++) {
        struct sw_dev *d = &sw[v];
        uint32_t age = now - d->last, sh, dk = pl > 64 ? 3u : 5u;
        int32_t lv, tgt, det = 0, nt, fx = ch > 64 ? (ch - 64) * 12 : 0, fmv = fm;
        b->a0[v] = b->da[v] = b->nz[v] = 0;
        b->fv[v] = fg;
        if (v == SW_N) {                                 /* (V10.6z6) the lead: sw_step's accent and sustain, a vibrato,
                                                          * TONE NOISE MOD as the morph has them */
            lv = mp[SWS_TONE];
            sm.lph += 6233u * (uint32_t)(48 + md) << 4;  /* (~0.8 .. 2.8 Hz, up to a semitone) */
            det = (osc_sine(sm.lph) * (md >> 3)) >> 15;
            tgt = sm.sus;
            dk = sm.ak;
            sh = tgt + d->acc > d->amp ? sm.ash : sm.ak > 4u ? sm.ak - 2u : 2u;
            d->sh = (uint8_t)(lv > 32 ? (lv - 32) * 8 / 3 : 0);
            d->tn = (uint8_t)(40 + lv * 3 / 2);
            b->fv[v] = lv > 90 ? 4096 + (lv - 90) * 60 : fg;
            b->nz[v] = mp[SWS_NOISE];
            fmv += lv * 3;
            fx = md * 4;
            nt = d->nt;
        } else {
            if (!d->id)
                continue;
            if (age > 20u * SW_TPS && d->amp < 8) { d->id = 0; continue; }
            lv = d->lv;
            sw_lmin = lv < sw_lmin ? lv : sw_lmin;
            sw_lmax = lv > sw_lmax ? lv : sw_lmax;
            if (!d->note)                                /* its note: its address, in the track's scale (C1 .. C5) */
                d->note = (uint8_t)clamp(scale_snap(t, 36 + (int32_t)(d->id >> 9) % 48), 24, 96);
            tgt = age > 6u * SW_TPS ? 0 : 1200 + 9000 * clamp(lv - sw_lmin, 0, 127) / (int32_t)(((uint32_t)(sw_lmax - sw_lmin) & 127u) | 1u);
            tgt = tgt * (192 - sm.mq / 2) >> 8;          /* (V10.6z6: a bed under the lead, lower near) */
            if (d->hit && hit) {                         /* a hit (V10.6z6: on the grid): its accent, FM, image, drift, octave */
                uint32_t r = sw_rand();
                d->hit = 0;
                d->acc = tgt * (1 + pl / 32);
                d->fme = 32767;
                d->pan = img ? 16384 + ((int32_t)(r & 0xFFFFu) - 32768) * img / 254 : d->ch == 37u ? 6000 : d->ch == 39u ? 26767 : 16384;
                d->dr = dft ? (int32_t)((r >> 16) % 65u - 32u) * dft / 64 : 0;
                d->oct = ocs ? (uint8_t)((d->oct + 1u) % (uint32_t)(ocs + (ocv < 64))) : 0;   /* (V10.6v: a walk of ocs places) */
                d->rj = ch > 48 && (int32_t)((r >> 9) & 127u) < (ch - 48) * 2 ? (uint8_t)((r >> 3) % 6u) : 0xFFu;   /* (CHAOS) */
            }
            if (ref && d->per)                           /* its clock against the reference's: a detune */
                det = (int32_t)((((uint64_t)d->per << 8) / (ref | 1u)) & 255u) - 128;
            det = det / 32 + (int32_t)((d->jit * (int32_t)((now >> 18) & 7u)) >> 9);   /* (+ jitter) */
            nt = ocv < 64 ? (int32_t)d->note + 12 * d->oct : scale_steps(t, d->note, d->oct % ocs);   /* (V10.6v: the scale's) */
            d->nt = (uint8_t)clamp(nt, 0, 127);
            sh = d->in < 255u ? 7u : age > 6u * SW_TPS && d->id != SW_SELF ? 10u : 3u;   /* (V10.6v: a swell in, a long fade out) */
            d->in = (uint8_t)(d->in + (d->in < 255u && (sw_tick & 1u)));
            tgt = tgt * (127 - pl) / 127;
        }
        det += d->dr;                                    /* (+ drift; the lead's grace) */
        d->dr -= d->dr / 16 + (d->dr > 0) - (d->dr < 0);   /* (the drift glides back) */
        nt += sw_key;
        d->inc = pitch_inc((uint32_t)clamp(nt * 16 + det, 0, 2047));
        d->acc -= (d->acc >> dk) + (d->acc > 0);         /* (down to 0: a pluck fast, the drone slow) */
        d->fme -= d->fme >> 5;
        if (d->mv) d->mv--;
        b->a0[v] = d->amp;
        d->amp += (tgt + d->acc - d->amp) >> sh;
        b->da[v] = (d->amp - b->a0[v]) >> CTL_LOG2;           /* (n is CTL) */
        b->gl[v] = 32767 - d->pan / 2;
        b->gr[v] = 16384 + d->pan / 2;
        b->fe[v] = (((fmv * d->fi) >> 7) * d->fme >> 15) + fx;   /* (its FM depth now) */
        i = d->rj < 6u ? d->rj : d->rt % 6u;            /* (its modulator's ratio, CHAOS's) */
        b->pmi[v] = d->inc * SW_RT[i][0] + (SW_RT[i][1] ? d->inc >> 1 : 0u);
        b->kq[v] = d->tn == 255u ? 0 : (int32_t)((d->tn + d->mv > 255u ? 256u : d->tn + d->mv + 1u) << 6);   /* (its tone) */
        any |= (uint32_t)(d->amp > 8);
    }
    if (!(++sw_tick & 1023u) && sw_lmax - sw_lmin > 2)   /* (the range relaxes, ~1.5 s a step) */
        sw_lmin++, sw_lmax--;
    return any;
}
static __attribute__((noinline)) void ain_swarm(uint32_t n)
{
    struct sw_blk b;
    uint32_t i, v, nr = sm.nr;
    if (!(sw_on = (uint8_t)(sw_prep(&b, n) != 0u)))
        return;
    for (i = 0; i < n; i++) {
        int32_t l = 0, r = 0, so = (sw_prev * b.slf) >> 6, yp = 0, ns;   /* (CHAOS: a chain, each voice by the one before;
                                                         * the first by none: no loop, never noise) */
        nr = nr * 1664525u + 1013904223u;
        ns = (int32_t)nr >> 16;
        for (v = 0; v < SW_V; v++) {
            struct sw_dev *d = &sw[v];
            uint32_t k = d->pm >> 29, f = (d->pm >> 13) & 0xFFFFu, ph;
            int32_t x, mo;
            if (!b.a0[v] && !b.da[v])
                continue;
            mo = (d->w[k] * (int32_t)(65536u - f) + d->w[(k + 1u) & 7u] * (int32_t)f) >> 8;   /* (Q15: its bytes) */
            ph = d->ph + (uint32_t)((mo * b.fe[v]) << 4) + (uint32_t)(so << 6) + ((uint32_t)(yp * b.xm) << 4);
            x = osc_sine(ph);
            if (d->sh)                                   /* (V10.6v) its shape: the sine driven, .. a square */
                x = clamp((x * (32 + d->sh / 2)) >> 5, -32767, 32767);
            if (b.fv[v]) {                               /* (CHAOS; V10.6z6 the lead's TONE) a wavefold */
                uint32_t z = ((uint32_t)((x * b.fv[v]) >> 12) + 32768u) & 0x1FFFFu;
                x = z < 65536u ? (int32_t)z - 32768 : 98303 - (int32_t)z;
            }
            mo = yp;
            yp = x;                                      /* (CHAOS's ring: the voice before's, as its oscillator gives it) */
            if (b.rm)                                    /* (CHAOS past 100) a ring by its neighbour, up to 3/4 */
                x += ((((x * mo) >> 15) - x) * b.rm) >> 5;
            x += (ns * b.nz[v]) >> 7;                    /* (V10.6z6: the lead's NOISE, its tone's colour) */
            if (b.kq[v]) {                               /* (its tone; its RSSI moving opens it) */
                d->lp += ((x - d->lp) * b.kq[v]) >> 14;
                x = d->lp;
            }
            x = (x * (b.a0[v] + b.da[v] * (int32_t)i)) >> 15;
            l += (x * b.gl[v]) >> 15;
            r += (x * b.gr[v]) >> 15;
            d->ph += d->inc;
            d->pm += b.pmi[v];
        }
        sw_l[i] = l;
        sw_r[i] = r;
        sw_prev = (l + r) >> 1;
    }
    sm.nr = nr;
}
static void (*const volatile AIN_SWARM)(uint32_t) = ain_swarm;   /* (mod.c macro_master: RAM code) */
/* SWARM's knobs on the INPUT pages (the stored values the same slots, 0..127; PLAY as it is) */
static const param_desc_t SW_DESC[8] = {
    {"SOURCE", F_ENUM, 0, AIN_N - 1, AIN_IN1, N_AIN_SRC, 0}, {"PLUCK", F_PCT, 0, 127, 0, 0, 0}, {"FM", F_PCT, 0, 127, 0, 0, 0},
    {"PLAY", F_ENUM, 0, 1, 0, N_AIN_PLAY, 0}, {"IMAGE", F_PCT, 0, 127, 0, 0, 0}, {"DRIFT", F_PCT, 0, 127, 0, 0, 0},
    {"OCTAVE", F_PCT, 0, 127, 0, 0, 0}, {"CHAOS", F_PCT, 0, 127, 0, 0, 0}};   /* (V10.6v: CHAOS in SELF's place) */
static const param_desc_t *input_desc(const track_t *t, uint32_t k) { return t->p[P_E0] == AIN_SW && k < 8u ? &SW_DESC[k] : 0; }

static void ain_block(struct track *t) { ain_upd[(uint32_t)(t - trk) % NTRK] = 1; }

/* THRU (voice.c track_render, after the voices): the input into the part, 1 = it sounds */
static __attribute__((noinline)) uint32_t ain_thru(track_t *t, int32_t *out, uint32_t n)
{
    if (t->p[P_E3] || !(t->p[P_E0] == AIN_SW ? sw_on : uin_on) || n > CTL)
        return 0;
    ain_fill(t, out, n, 1);
    return 1;
}

/* (called through a pointer: track_render is inlined in RAM code, out of a direct call's reach of the image's end) */
static uint32_t (*const volatile AIN_THRU)(track_t *, int32_t *, uint32_t) = ain_thru;

static void ain_note_on(struct track *t, voice_t *v)
{
    v->ph[0] = 0;
    if (t->p[P_E0] == AIN_SW)                            /* (SWARM, KEYS: the key transposes the swarm, C4 as it is) */
        sw_key = (int8_t)clamp((int32_t)v->note - 60, -36, 36);
}
/* KEYS: a note's voice plays the input at its envelope, RING at its pitch */
static void ain_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    int32_t b[CTL], ring = t->p[P_E0] == AIN_SW ? 0 : clamp(t->p[P_E5], 0, 127) * 258;
    uint32_t i, ph = v->ph[0];
    if (!t->p[P_E3] || !(t->p[P_E0] == AIN_SW ? sw_on : uin_on) || n > CTL)
        return;
    ain_fill(t, b, n, 0);
    for (i = 0; i < n; i++) {
        int32_t x = b[i];
        if (ring) {
            x += mulq15(mulq15(x, osc_sine(ph)) - x, ring);
            ph += m->inc;
        }
        out[i] += mulq15(x, amp_at(m, i));
    }
    v->ph[0] = ph;
}

static const preset_t INPUT_PRESETS[] = {
    {"IN 1", {AIN_IN1, 64, 0, 0, 127, 0, 0, 0}, {0, 64, 127, 20}, 0, TAG(FX) | 1, FX(0, 0, 0, 0)},
    {"IN 2", {AIN_IN2, 64, 0, 0, 127, 0, 0, 0}, {0, 64, 127, 20}, 0, TAG(FX) | 1, FX(0, 0, 0, 0)},
    {"IN 1+2", {AIN_SUM, 64, 0, 0, 127, 0, 0, 0}, {0, 64, 127, 20}, 0, TAG(FX) | 1, FX(0, 0, 0, 0)},
    {"STEREO IN", {AIN_ST, 64, 0, 0, 127, 0, 0, 0}, {0, 64, 127, 20}, 0, TAG(FX) | 1, FX(0, 0, 0, 0)},
    {"MIC GATE", {AIN_IN1, 80, 24, 0, 127, 0, 0, 0}, {0, 64, 127, 20}, 0, TAG(VOX) | 1, FX(0, 0, 15, 25)},
    {"SPACE IN", {AIN_ST, 64, 0, 0, 127, 0, 0, 0}, {0, 64, 127, 20}, 0, TAG(PAD) | 1, FX(0, 30, 35, 60)},
    {"TRANCE GATE", {AIN_ST, 64, 0, 1, 127, 0, 0, 0}, {0, 40, 0, 10}, 0, TAG(FX) | 1, FX(0, 0, 20, 20)},
    {"RING IN", {AIN_IN1, 64, 0, 1, 127, 90, 0, 0}, {0, 64, 127, 30}, 0, TAG(FX) | 1, FX(0, 0, 20, 30)},
    {"CRUSH IN", {AIN_SUM, 72, 0, 0, 127, 0, 90, 60}, {0, 64, 127, 20}, 0, TAG(FX) | 1, FX(10, 0, 0, 15)},
    /* (SWARM) SOURCE PLUCK FM PLAY IMAGE DRIFT OCTAVE CHAOS (V10.6v: OCTAVE's octaves in its first half, CHAOS SELF's x2;
     * V10.6z6: CHAOS the ceiling of the depth the proximity and the devices give, FAR / NEAR the factory pair) */
    {"SWARM", {AIN_SW, 0, 20, 0, 40, 20, 0, 100}, {0, 64, 127, 20}, 0, TAG(PAD) | 1, FX(0, 30, 35, 60)},
    {"SWARM PLUCK", {AIN_SW, 110, 50, 0, 90, 40, 20, 70}, {0, 64, 127, 20}, 0, TAG(PLUCK) | 1, FX(0, 20, 40, 50)},
    {"SWARM CHOIR", {AIN_SW, 20, 0, 0, 60, 60, 15, 90}, {0, 64, 127, 20}, 0, TAG(PAD) | 1, FX(0, 50, 25, 80)},
};

static const engine_t ENG_INPUT = {
    .name = "EXT",                                   /* (V10.6d: EXT, external signals: the audio input and SWARM) */
    .page_title = {"EXT", "SHAPE"},
    .edit = {
        {"SOURCE", F_ENUM, 0, AIN_N - 1, AIN_IN1, N_AIN_SRC, 0},
        {"GAIN", F_INT, 0, 127, 64, 0, 0},
        {"GATE", F_PCT, 0, 127, 0, 0, 0},
        {"PLAY", F_ENUM, 0, 1, 0, N_AIN_PLAY, 0},
        {"WIDTH", F_PCT, 0, 127, 127, 0, 0},
        {"RING", F_PCT, 0, 127, 0, 0, 0},
        {"BITS", F_PCT, 0, 127, 0, 0, 0},
        {"RATE", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = INPUT_PRESETS,
    .npresets = NELEM(INPUT_PRESETS),
    .poly = 1,
    .note_on = ain_note_on,
    .render = ain_render,
    .block = ain_block,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .desc = input_desc,
};
