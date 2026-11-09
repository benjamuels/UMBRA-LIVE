/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.6d) MATH (was LOFI): the formulas alone, BYTE (bytebeat) and FLOAT (floatbeat). The chip waves (PLS TRI SAW
 * NOIS WRAM, the STEP envelope) were redundant beside ANALOG (the user's call): a sound of theirs that arrives -- a
 * project, a user preset -- plays as ANALOG's nearest (core.h analog_from_chip). The engine's number (3) and its stored
 * values stay: WAVE 5 BYTE, 6 FLOAT; CHIP 0..3 the bits (STEP, 4, read as 4BIT: the same sound on BYTE and FLOAT) */
static const char *const N_CHIP[] = {"4BIT", "4B/2", "8BIT", "1BIT"};
static const char *const N_RWAVE[] = {"-", "-", "-", "-", "-", "BYTE", "FLOAT"};   /* (WAVE 5, 6: the stored numbers) */
enum { CHIP_4BIT, CHIP_4B2, CHIP_8BIT, CHIP_1BIT };
enum { RW_BYTE = 5, RW_FLOAT = 6 };

/* (JIANT 0.4) WAVE = BYTE: bytebeat, an 8-bit formula of the time t played as the wave (after viznut's 2011
 * one-liners: the music is in the formula). DUTY picks one of 32 (ALGO, DUTY / 4), CRSH is the formulas' variable
 * a (VAR: 1 .. 16); t runs with the note played (C4: ~8 kHz, the classic rate; an octave up twice as fast), restarts
 * at each note-on; the hold (4B/2), the bits (CHIP), TONE and the rest as for the other waves. Our own picks and
 * variations; each a few operations a sample */
static const char *const N_BYTE[] = {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10", "B11", "B12", "B13", "B14", "B15", "B16", "B17", "B18", "B19", "B20", "B21", "B22", "B23", "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31", "B32", "B33", "B34", "B35", "B36", "B37", "B38", "B39", "B40", "B41", "B42", "B43", "B44", "B45", "B46", "B47", "B48", 0};   /* (p6: 48) */
/* (JIANT 0.5) BYTE's own: RES the filter's resonance (VIB's slot), LOOP (MASK's slot) t held in a window of 2^17 ..
 * 2^5 steps: the formula's long evolution (0: free) folded into a short cycle repeating at the note: a pitched tone */
/* (JIANT 0.5) E4 (CUT) left: the filter is the FILTER page's (TYPE, CUT, RES: P_FTYPE P_FCUT P_FRES), on every engine */
static const param_desc_t LOFI_NONE = {"-", F_INT, 0, 0, 0, 0, 0};
/* (JIANT 0.5.1) each formula's start, in 512 steps of t: where it is already moving (some sit silent for up to
 * ~2 s from t = 0); a note starts there, LOOP's window too. The first 17 were rewritten in 0.5.1, away from the
 * well-worn one-liners (t * (42 & t >> 10), t & t >> 8 ..): rhythms and arpeggios after Tejeez, xpansive's
 * "lost in space", a melody of t >> 9 modulo 13, textures of t % 24 and t % 19 */
static const uint8_t BYTE_T0[32] = {0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 0, 0, 4, 2, 0, 0, 0, 16, 0, 7, 16, 0, 1, 8, 15, 8, 0, 15, 23, 8, 0};
static __attribute__((noinline)) uint32_t lofi_byte(uint32_t f, uint32_t t, uint32_t a)
{
    switch (f & 63u) {
    case 0: return (~t >> 2) * ((127u & t * (7u & t >> 10)) < (245u & t * (2u + (5u & t >> 14))));   /* (Tejeez) */
    case 1: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (5u + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (Tejeez) */
    case 2: return ((-t & 4095u) * (255u & t * (t & t >> 13)) >> 12) + (127u & t * (234u & t >> 8 & t >> 3) >> (3u & t >> 14));   /* (xpansive) */
    case 3: return t * (((t >> 9) ^ ((t >> 9) - 1u) ^ 1u) % 13u);
    case 4: return ((t >> 13 | t % 24u) & (t >> 7 | t % 19u)) << 3;
    case 5: return t * (t ^ (t + (t >> 15 | 1u)) ^ (((t - 1280u) ^ t) >> 10));
    case 6: return (t * (0xCA98u >> (t >> 9 & 14u) & 15u)) | t >> 8;
    case 7: return (t * (t >> 8 | t >> 9) & 46u & t >> 8) ^ ((t & t >> 13) | t >> 6);
    case 8: return (t >> 4) * (13u & 0x8898A989u >> (t >> 11 & 30u));
    case 9: return t * (t >> ((8u * ((t >> 15) | (t >> 8))) & 31u) & (20u | (((t >> 19) * 5u) >> (t & 31u)) | t >> 3));
    case 10: return (t & t % 255u) - (t * 3u & t >> 13 & t >> 6);
    case 11: return (t * (t >> 9 | t >> 13) & (t >> 6 | t >> 11) & 127u) + ((t >> 4) * (3u + (t >> 12 & 3u)) & 64u);
    case 12: return t + (t & (t ^ t >> 6)) - t * ((t >> 9) & (t % 16u ? 2u : 6u) & t >> 9);
    case 13: return t * (t >> 11 & t >> 8 & 123u & t >> 3);
    case 14: return (t * 9u & t >> 4) | (t * 5u & t >> 7) | (t * 3u & t >> 10);
    case 15: return (t * ((t & 4096u ? (t % 65536u < 59392u ? 7u : t & 7u) : 16u) + (1u & t >> 14)) >> (3u & -t >> (t & 2048u ? 2 : 10))) | t >> (t & 16384u ? (t & 4096u ? 10 : 3) : 2);   /* (Tejeez) */
    case 16: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (a + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (a: VAR) */
    case 17: return t * ((t >> (a & 15u) | t >> 8) & 63u & t >> 4);
    case 18: return (t * a & t >> 7) | (t * 3u & t >> 10);
    case 19: return (t >> a | t) * (t >> 9 & 7u);
    case 20: return t * (t >> 8 & a) ^ t >> 4;
    case 21: return (t * a & t >> 8) * (t >> 12 & 3u);
    case 22: return t * (0x9AD5u >> (t >> (8u + (a & 3u)) & 14u) & 15u);
    case 23: return (t ^ t >> a) * (t >> 10 & 5u);
    case 24: return t >> 2 & (t * a >> 6 | t >> 9);
    case 25: return (t | t >> a | t >> 7) * (t >> 13 & 7u);
    case 26: return t * ((t >> 11 & 3u) + a) & t >> 6;
    case 27: return (t * (a | 1u) % (257u - (t >> 10 & 127u))) & 255u;
    case 28: return t * (t >> 10 & 7u & a) | t >> 5;
    case 29: return (t >> 5 & t >> 7) * a + (t >> 9 & 32u);
    case 30: return t * (t >> 12 & a ? 3u : 2u) & t >> 6;
    /* (p6, the owner: more of them) ALGO 128 .. 191: sixteen more, the classics' families and our own */
    case 32: return (t * (t >> 5 | t >> 8)) >> (t >> 16 & 7u);
    case 33: return t * ((t >> 12 | t >> 8) & 63u & t >> 4);
    case 34: return (t >> 6 | t | t >> (t >> 16 & 7u)) * 10u + (t >> 11 & 7u);
    case 35: return (t | (t >> 9 | t >> 7)) * t & (t >> 11 | t >> 9);
    case 36: return (t * 5u & t >> 7) | (t * 3u & t * 4u >> 10);
    case 37: return (t >> 7 | t | t >> 6) * 10u + 4u * ((t & t >> 13) | t >> 6);
    case 38: return t & 4096u ? (t * (t ^ t % 255u) | t >> 4) >> 1 : (t >> 3) | (t & 8192u ? t << 2 : t);
    case 39: return (t * (t >> 9 | t >> 13) & 16u) * 8u + (t >> 6 & t >> 8 & 31u);
    case 40: return ((t >> 4) * (t >> 5 & 0x2Au)) | t >> 3;
    case 41: return t * ((t >> 3 | t >> 9) & 82u & t >> 9);
    case 42: return (t * a & t >> 4) | (t >> 5 & t * (a + 1u) >> 7);
    case 43: return t * (t >> ((t >> 11 & 3u) + 4u) & a * 3u);
    case 44: return (t >> 2) * ((t >> (8u + (a & 3u)) & 7u) | (t >> 12 & 3u));
    case 45: return (t * (0x5A3Cu >> (t >> 11 & 12u) & 15u)) >> (t >> 14 & 1u);
    case 46: return (t * (t >> 13 & 1u ? 3u : 4u) & t >> 5) | (t * (a + 2u) >> 9 & t >> 7);
    case 47: return (t ^ t >> a) * ((t >> 9) % 5u + 1u);
    default: return (t * a >> 4 | t >> 3) ^ (t >> 7 & t >> 11);
    }
}
/* (JIANT 0.5.1; 0.5.6 a floatbeat generator) WAVE = FLOAT: floatbeat, bytebeat's sibling in floats. Each ALGO is a
 * formula of t, as BYTE's, but made of sines: sin(t x (t >> 11 ^ t >> 13 & 7)), FM whose index walks with t's bits,
 * melodies read from nibble tables, kicks and noise hats inside the formula, folds and powers of sines .. rhythm,
 * melody and timbre at once. t runs 32 steps a cycle of the note (the formula in tune with the key, its rhythm faster
 * up the keyboard, as bytebeat's), with 8 bits of fraction for the sines (x = t in Q8; S(x) = sin(2 pi x / 32)).
 * The same t as BYTE's: MOTN runs its slow bits at their own speed, VAR mutates it (lofi_mut), BEND folds it, LOOP
 * holds it in a window; FBK (E5, 0.6.4) feeds the output back into t, from a shimmer to chaos; CHIP the bits, RES the
 * filter's */
static const char *const N_FLOAT[] = {"F01", "F02", "F03", "F04", "F05", "F06", "F07", "F08", "F09", "F10", "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "F25", "F26", "F27", "F28", "F29", "F30", "F31", "F32", "F33", "F34", "F35", "F36", "F37", "F38", "F39", "F40", "F41", "F42", "F43", "F44", "F45", "F46", "F47", "F48", 0};   /* (p6: 48) */
static const param_desc_t LOFI_ALGO = {"ALGO", F_INT, 0, 191, 64, N_BYTE, 0};    /* (p6: 48; the knob's: edit[2] keeps
                                                         * 0..127, FUN7's byte) */
static const param_desc_t LOFI_MOTN = {"MOTION", F_INT, 0, 255, 0, 0, 0};   /* (p6: 128 .. 255 SYNC; V10.6d: 0) */
static const param_desc_t LOFI_FALGO = {"ALGO", F_INT, 0, 191, 64, N_FLOAT, 0};   /* = edit[2] (FLOAT) */
/* .. its CHIP the resolution: 4 bits, 4 bits held over two samples, FULL (smooth, the default), 1 bit, STEP (4 bits and
 * the stepped envelope) */
static const char *const N_FCHIP[] = {"4BIT", "4B/2", "FULL", "1BIT"};
static const param_desc_t LOFI_FCHIP = {"CHIP", F_ENUM, 0, 3, 2, N_FCHIP, 0};    /* = edit[0] (FLOAT; 0.5.5: no STEP) */
/* (0.5.4) BYTE's and FLOAT's own second page: MOTN (E4, where CUT was: the time's speed, 64 as written), GRIT (BYTE,
 * E5: the samples held longer, a rate crusher), FBK (FLOAT, E5: the output back into the formula's time) */
static const param_desc_t LOFI_FDCY = {"FEEDBK", F_PCT, 0, 127, 0, 0, 0};            /* = edit[5] (FLOAT, 0.6.4: was DRV, a
                                                         * fold that silenced it and doubled the FILTER page's DIST) */
/* MOTN 0..127 -> the time's speed, Q8: x1/16 .. x1 (64) .. x15.5, exponential */
/* (p6, the owner: "the notes only transpose, not the speed") MOTION 128 .. 255: SYNC, the formula's slow bits (its
 * rhythm, its melody) at the song's position, a 1/16 every 2048 t (the speeds as 0 .. 127's), the note only its pitch
 * (the fast bits): every note the same sequence transposed, in time with the bar. The bar's clock is seq.c's (the same
 * variables, declared again here: engines.c comes first). Returns the slow t at the block's start, *rate its step a
 * sample (Q16) */
static volatile uint32_t beat_pos, beat_n;
static volatile uint32_t bar_n;
static __attribute__((noinline)) uint32_t lofi_sync(uint32_t *rate)
{
    uint32_t bl = (uint32_t)FS * 60u / (uint32_t)clamp(song.g[G_BPM], 20, 300);
    *rate = (8192u << 16) / bl;
    return (bar_n * 4u + beat_n) * 8192u + beat_pos * 8192u / bl;
}
static uint32_t lofi_motion(int32_t m)
{
    uint32_t e = (uint32_t)clamp(m >= 128 ? m - 128 : m, 0, 127) + 64u;
    return ((16u + (e & 15u)) << (e >> 4)) >> 4;
}
/* (0.5.4) VAR as a mutation of the time (BYTE's t, FLOAT's step count): v >> 4 picks the way, v & 15 how much. 0..15
 * the time as written; then stairs that transpose as it goes, xor and or with itself shifted, a bit dropped (holes in
 * the rhythm), the low bits reversed (the phrase backwards), multiplied (faster, higher), scrambled */
static uint32_t lofi_mut(uint32_t t, uint32_t v)
{
    uint32_t k = v & 15u, m;
    switch ((v >> 4) & 7u) {
    case 0: return t;
    case 1: return t + (t >> (9u - k / 2u)) * (k + 1u);
    case 2: return t ^ (t >> (2u + k / 2u));
    case 3: return t | (t >> (1u + k / 2u));
    case 4: return t & ~(1u << (2u + k / 2u));
    case 5: m = (1u << (3u + k / 2u)) - 1u; return (t & ~m) | (~t & m);
    case 6: return t * (3u + k);
    default: return t ^ ((t >> 3) * ((k + 1u) * 0x9E3779B1u >> 24));
    }
}
#define S(x) sine_i((uint32_t)(x) << 19)                 /* sin of x (t in Q8): a turn every 32 t */
#define PM(y, k) ((uint32_t)(((int32_t)(y) * (int32_t)(k)) >> 6))   /* a sine (Q15) times k: k / 16 of a turn */
/* the floatbeat formula f at t (its integer) and x (t in Q8); aux: a formula's own state (a kick's phase) */
static __attribute__((noinline)) int32_t lofi_float(uint32_t f, uint32_t t, uint32_t x, int32_t *aux)
{
    static const uint8_t R[8] = {16, 18, 20, 21, 24, 27, 30, 32};   /* just ratios, Q4: 1 9/8 5/4 4/3 3/2 5/3 15/8 2 */
    uint32_t st = t >> 11, fr = t & 2047u, h;
    int32_t E = 32767 - (int32_t)(fr << 4), a, b;          /* (a step's envelope: 1 .. 0) */
    switch (f & 63u) {
    case 0: return S(x * (((t >> 11) ^ (t >> 13)) & 7u) + x) * E >> 15;                       /* sin(t (t>>11 ^ t>>13 & 7)) */
    case 1: return S(x + PM(S(x * 3u), (int32_t)((t >> 10) & 15u) * 3));                      /* FM, its index walking */
    case 2: return S((x >> 4) * R[(0x36364689u >> ((st & 7u) * 4u)) & 7u]) * E >> 15;         /* a melody, a nibble table */
    case 3: return S(x * (1u + ((t >> 12) & 3u))) * S(x * (((t >> 9) & 7u) + 1u)) >> 15;      /* a ring, rhythmic */
    case 4: {                                                                                  /* a kick, a bass */
        uint32_t k = fr < 1024u ? (1024u - fr) * (1024u - fr) >> 6 : 0u;
        *aux += (int32_t)k + 40;
        a = (st & 1u) ? 0 : sine_i((uint32_t)*aux << 13) * (int32_t)(1024u - (fr < 1024u ? fr : 1024u)) >> 10;
        return (a + (S(x >> 1) >> 1)) >> 1;
    }
    case 5: return S(x * ((((t >> 9) & (t >> 11)) & 7u) | 1u)) * E >> 15;                     /* Sierpinski partials */
    case 6: return softclip(S(x * (1u + (st & 1u))) * (1 + (int32_t)((t >> 11) & 7u) * 3) >> 1);   /* drive, rhythmic */
    case 7: return S((x >> 4) * R[(st * 3u + (st >> 2)) & 7u] * (1u + (st & 1u)));            /* fifths and octaves */
    case 8: {                                                                                  /* two arpeggios */
        static const uint8_t H[8] = {2, 3, 4, 5, 6, 8, 5, 3};
        return (S(x * H[(t >> 9) & 7u] >> 1) + S(x * H[(t >> 11) & 7u] >> 2)) >> 1;
    }
    case 9: h = (t * 2654435761u) >> 17;                                                       /* a snare, a tone */
        a = (st & 1u) ? (int32_t)h - 16384 : 0;
        return (mulq15(a, E) + S(x)) >> 1;
    case 10: return S(x + PM(S(x * (((t >> 12) & 7u) + 1u)), E >> 10));                       /* a pluck of FM */
    case 11: return S(x + PM(S(t << 2), 6));                                                  /* a vibrato, deep (4 Hz) */
    case 12: return S((uint32_t)(S(x) * (2 + (int32_t)((t >> 11) & 7u))) >> 2);               /* folded, rhythmic */
    case 13: a = S(x * (1u + (st & 1u))); b = S((x >> 4) * (24u + (st & 6u) * 4u));           /* rectified, leaping */
             return (a < 0 ? -a : a) - (b < 0 ? -b : b);
    case 14: return S(x & ~((1u << (4u + (st & 7u))) - 1u));                                  /* a staircase sine */
    case 15: return S(x * (((t >> 8) % 7u) + 1u)) * E >> 15;                                  /* partials mod 7 */
    case 16: {                                                                                 /* three against four */
        int32_t e3 = 32767 - (int32_t)((t % 3072u) * 10u), e4 = E;
        return (mulq15(S(x * (1u + (st & 1u))), e4) + mulq15(S((x >> 4) * (24u + (st & 2u) * 8u)), e3 > 0 ? e3 : 0)) >> 1;
    }
    case 17: return S((x >> 6) * (64u + ((t >> 6) & 255u))) * E >> 15;                        /* a riser each bar */
    case 18: return mulq15(S(x * (1u + st % 3u)) * ((t >> 10) & 1u ? 1 : 0) + S(x + (t << 2)), 16384);   /* beating, gated */
    case 19: {                                                                                 /* odd partials, built up */
        uint32_t k, n = 1u + ((t >> 11) & 3u);
        for (a = 0, k = 0; k < n; k++)
            a += S(x * (2u * k + 1u)) / (int32_t)(2u * k + 1u);
        return a * 3 >> 2;
    }
    case 20: return S(x * (2u + ((st * 5u) & 7u)) >> 1);                                      /* leaps */
    case 21: {                                                                                 /* a drum loop */
        uint32_t k = fr < 768u ? (768u - fr) * (768u - fr) >> 6 : 0u;
        *aux += (int32_t)k + 30;
        a = (st & 3u) == 0u ? sine_i((uint32_t)*aux << 13) * (int32_t)(768u - (fr < 768u ? fr : 768u)) / 768 : 0;
        b = (t >> (8u + (st & 3u))) & 1u ? (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(1024u - (t & 1023u)) >> 12 : 0;
        return (a + b + (S(x >> 1) >> 2)) >> 1;
    }
    case 22: return S(x * (1u + (((t >> 9) & (t >> 7)) & 7u)) + (((t >> 9) & (t >> 7) & 255u) << 9));   /* bytebeat in it */
    case 23: return S(x * 2u + PM(S(x * 5u), (int32_t)((t >> 11) & 3u) * 8));                 /* FM 5:2, stepped */
    case 24: a = S(x * (1u + (st & 3u))); return mul64_sh15(a, a < 0 ? -a : a) * E >> 15;   /* squared */
    case 25: return S(x & ~(1u << (8u + (st & 7u)))) * E >> 15;                              /* a bit of t dropped: jumps */
    case 26: return S((x >> 4) * (34u - ((t >> 11) & 15u)) >> 1) * E >> 15;                   /* a descending line */
    case 27: return (S(x) + mulq15(S(x * (((t >> 13) & 3u) + 2u)), E)) >> 1;                   /* a partial, struck */
    case 28: return S(x + PM(S(x * 2u), S(t) >> 10));                                         /* FM, its index a 1 Hz sine */
    case 29: return S((x >> 4) * (16u + ((t >> 7) & 63u)));                                   /* a glide up partials */
    case 30: h = ((st + 1u) * 2654435761u) >> 29; return S((x >> 4) * R[h]) * E >> 15;        /* a random line */
    /* (p6) ALGO 128 .. 191: sixteen more */
    case 32: return S(x * (st % 5u + 1u)) * E >> 15;                                          /* partials mod 5, stepped */
    case 33: return S(x + PM(S(x * 7u), E >> 11));                                            /* FM 7:1, plucked */
    case 34: return (S(x) + S(x * 3u >> 1) + S((x >> 4) * R[st & 7u])) / 3;                    /* a fifth over a line */
    case 35: return (st & 3u) != 3u ? S((x >> 4) * R[(st * 5u) & 7u] * 2u) * E >> 15 : 0;     /* a melody with rests */
    case 36: return S(x + PM(S(x >> 1), (int32_t)((t >> 9) & 15u) * 2));                      /* a sub's FM, growling */
    case 37: a = S(x); return a > 0 ? a : a / 4;                                              /* half rectified */
    case 38: return S(x * (1u + ((t >> 10) & 1u) * 2u)) * S(t << 1) >> 15;                    /* tremolo, octave leaps */
    case 39: return S((x >> 3) * (8u + ((t >> 8) & 7u))) * E >> 15;                           /* a stair arpeggio */
    case 40: return (mulq15(S(x * 2u), E) + mulq15(S(x * 3u), 32767 - E)) >> 1;               /* the 2nd into the 3rd */
    case 41: return S(x + PM(S(x * 5u), (int32_t)(st & 7u) * 4));                             /* FM 5:1, its index stepped */
    case 42: return fr < 1024u ? S((x >> 4) * R[(t >> 13) & 7u]) * E >> 15 : 0;               /* a gated pad */
    case 43: return softclip((S(x) * 3 + S(x * 2u) * 2) >> 1);                                /* driven, two partials */
    case 44: return S(x * ((((t >> 9) ^ (t >> 12)) & 7u) | 1u));                              /* xor partials */
    case 45: return S((x >> 5) * (32u + ((t >> 5) & 31u))) * E >> 15;                         /* chirps */
    case 46: return (S(x) * S(x * (1u + (st & 3u))) >> 15) + (S(x >> 1) >> 2);               /* a ring over a sub */
    case 47: return S(x + PM(S(x * 3u), S(t << 1) >> 11));                                    /* FM 3:1, its index a slow sine */
    default: a = ((st * 5u) & 7u) < 5u ? E : 0;                                               /* euclid hats, a chord */
        b = (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(512u - (t & 511u)) >> 11;
        h = (x >> 4) * R[(st >> 1) & 7u] >> 4;                                                 /* (the chord's root walks) */
        return (S(h * 16u) + S(h * 20u) + S(h * 24u)) / 4 + (mulq15(b, a) >> 1);
    }
}
#undef S
#undef PM

/* (0.9.11) LOFI's level and its edges. Through the master's leveler (fx.c lev_block: peaks to ~-9 dBFS, 2:1) LOFI measured
 * as loud as the others in LUFS (-12.4 against the melodic engines' -12.3, the factory presets' medians, C4 at 100), but
 * it was heard far louder and piercing: its waves are full-scale and dense (square-ish bytebeat, naive pulses and saws:
 * a low crest factor, energy spread from 60 Hz to 16 kHz over every critical band), so a Zwicker loudness put it at
 * +6 phon over the melodic engines (BYTEBEAT over the loudest ANALOG preset), its sharpness twice theirs, ~35 dB more
 * of its energy above 6 kHz; after a quiet part the leveler, lifted, let its onset through +5 LU over the sound before.
 * LOFI_GAIN, the voice's level by WAVE (Q15, on the block's amplitude ramp): -4.5 dB PLS SAW NOIS, -1.5 TRI, -3 WRAM,
 * -6 BYTE FLOAT. LOFI_SOFT, a fixed one-pole (-3 dB at ~7.5 kHz, -6.8 at the top) on the chip waves and FLOAT, ahead
 * of the FILTER page's CUT (which keeps its scale): the aliasing pulses' and saws' top filed, the 4 .. 8 kHz bite of
 * FLOAT's FM a little; BYTE's own two poles lower (lofi_byte_render). Its state in v->ph[2] (kept on a retrigger).
 * Three or four operations a sample; user presets and old projects play quieter and softer too */
#define LOFI_GAIN 16384                                 /* (V10.6d: BYTE's and FLOAT's, -6 dB; the chips' gone) */
#define LOFI_SOFT(x, y) ((y) + ((((x) - (y)) * 5) >> 3))   /* y + 5/8 (x - y): -3 dB at ~7.5 kHz, -1.2 at 4 */

/* BYTE's samples (lofi_render's set-up done: inc the note's, the hold, the bits, the filter): t in v->s[2], its
 * fraction (Q24) in v->ph[0] */
static __attribute__((noinline)) void lofi_byte_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                      const vmod_t *m, uint32_t inc, int32_t hold, int32_t bits,
                                                      const tsvf_t *flt)
{
    const int16_t *p = t->p;
    uint32_t vr = (uint32_t)clamp(p[P_E3] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /* (0.6.81) LFO / ENV ->
                                                         * SHAPE move VAR (SHAPE did nothing on BYTE) */
    uint32_t f = (uint32_t)p[P_E2] >> 2, a = 1u + (vr >> 3), tt = (uint32_t)v->s[2], tf = v->ph[0], i;
    uint32_t ti = (inc >> 8) * 31u;                     /* t a sample, Q24: C4 (inc ~2.55e7) ~0.18 = 8 kHz */
    uint32_t ft = ft_voice(p);            /* (JIANT 0.5) the FILTER's type (COMB: open, voice.c combs) */
    int32_t kd = 8192 - (p[P_FRES] > 50 ? p[P_FRES] : 50) * 7600 / 127;   /* (0.5.4: the FILTER page's RES alone) */
    uint32_t bs = p[P_E6] ? 13u - (uint32_t)p[P_E6] / 11u : 0u;   /* BEND: the fold's shift, 13 .. 2 (0: none) */
    int32_t held = v->s[0], cnt = v->s[1], ic1 = v->s[4], ic2 = v->s[5], l1 = v->s[3], l2 = (int32_t)v->ph[2];
    uint32_t lm = p[P_E7] ? (1u << (17u - (uint32_t)p[P_E7] * 12u / 127u)) - 1u : 0xFFFFFFFFu;   /* LOOP: t's window */
    uint32_t t0 = (uint32_t)(f < 32u ? BYTE_T0[f] : 0u) << 9;    /* (0.5.1: the formula's start) */
    uint32_t mul = lofi_motion(p[P_E4]), ts = v->ph[1], var = vr;   /* (0.5.4) MOTN: the slow time, Q8 */
    uint32_t sy = p[P_E4] >= 128, sr = 0, sb = sy ? lofi_sync(&sr) : 0u;   /* (p6) SYNC */
    hold *= 1 + (p[P_E5] >> 3);                         /* (0.5.4) GRIT: each sample held up to 16 x longer */
    for (i = 0; i < n; i++) {
        if (--cnt <= 0) {
            uint32_t te;
            if (sy)                                      /* (SYNC: the slow bits at the bar's position) */
                ts = (sb + ((i * sr) >> 16)) * mul;
            te = mul == 256u && !sy ? tt : (tt & 255u) | ((ts >> 8) & ~255u);   /* (MOTN: the slow bits at their own
                                                         * speed; 64: t as written) */
            uint32_t tl = lofi_mut((te & lm) + t0, var), tw = bs ? tl ^ (tl >> bs) : tl;   /* (VAR: mutated) */
            int32_t s = (int32_t)(lofi_byte(f, tw, a) & 255u) * 256 - 32640;
            cnt = hold;
            if (bits < 8)
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            held = s;
        }
        tf += ti;
        tt += tf >> 24;
        tf &= 0xFFFFFFu;
        ts += ((ti >> 8) * mul) >> 16;
        l1 += ((held >> 1) - l1) >> 1;                  /* (JIANT 0.5) the 8-bit steps rounded: two one-poles before
                                                         * the filter (BYTE was knife-sharp); 0.9.11: at ~4.9 kHz, were
                                                         * ~6.3 (x 0.6): their images softer, the steps' edges filed */
        l2 += (l1 - l2) >> 1;
        out[i] += voice_amp(soft_knee(svf_mode(flt, l2, &ic1, &ic2, ft, kd), 16000) << 1, m, i);
    }
    v->s[3] = l1;
    v->ph[2] = (uint32_t)l2;
    v->s[0] = held;
    v->s[1] = cnt;
    v->ph[1] = ts;
    v->s[2] = (int32_t)tt;
    v->s[4] = ic1;
    v->s[5] = ic2;
    v->ph[0] = tf;
}

/* FLOAT's samples (lofi_render's set-up done: inc the note's, the filter): t in v->s[2], its fraction (Q24) in v->ph[0],
 * MOTN's slow t (Q8) in v->ph[1], a formula's own state in v->s[3], the last output in v->s[0] (0.5.6) */
static __attribute__((noinline)) void lofi_float_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                       const vmod_t *m, uint32_t inc, const tsvf_t *flt)
{
    const int16_t *p = t->p;
    uint32_t f = (uint32_t)p[P_E2] >> 2, tt = (uint32_t)v->s[2], tf = v->ph[0], ts = v->ph[1], i;
    uint32_t ti = inc >> 3;                             /* (t a sample, Q24: 32 a cycle of the note) */
    uint32_t sy = p[P_E4] >= 128, sr = 0, sb = sy ? lofi_sync(&sr) : 0u;   /* (p6) SYNC */
    uint32_t mul = lofi_motion(p[P_E4]), var = (uint32_t)clamp(p[P_E3] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /*
                                                         * (0.6.81: SHAPE moves VAR, as BYTE's) */
    uint32_t bs = p[P_E6] ? 13u - (uint32_t)p[P_E6] / 11u : 0u;   /* BEND: t ^ t >> 13 .. 2 */
    uint32_t lm = p[P_E7] ? (1u << (17u - (uint32_t)p[P_E7] * 12u / 127u)) - 1u : 0xFFFFFFFFu;   /* LOOP: t's window */
    uint32_t ft = ft_voice(p);
    int32_t rs = p[P_FRES] > 50 ? p[P_FRES] : 50, kd = 8192 - rs * 7600 / 127, y1 = v->s[4], y2 = v->s[5];
    int32_t aux = v->s[3], fb = v->s[0], fbk = p[P_E5], sm = (int32_t)v->ph[2];   /* (sm: LOFI_SOFT's state) */
    uint32_t chip = (uint32_t)p[P_E0], qs = chip == CHIP_8BIT ? 0u : chip == CHIP_1BIT ? 15u : 12u;   /* (CHIP: the bits) */
    for (i = 0; i < n; i++) {
        uint32_t te, tl, tw;
        int32_t s;
        if (sy)                                          /* (p6 SYNC: the slow bits at the bar's position) */
            ts = (sb + ((i * sr) >> 16)) * mul;
        te = mul == 256u && !sy ? tt : (tt & 255u) | ((ts >> 8) & ~255u);
        tl = lofi_mut(te & lm, var);
        tw = bs ? tl ^ (tl >> bs) : tl;
        if (fbk) {                                        /* FBK: the last output moves t, up to +-16 t (half a turn of
                                                         * the sines, Q8): the formula modulates itself, FM-like */
            int32_t d = (fb * fbk) >> 10;
            s = lofi_float(f, tw + (uint32_t)(d >> 8), (tw << 8 | tf >> 16) + (uint32_t)d, &aux);
        } else {
            s = lofi_float(f, tw, tw << 8 | tf >> 16, &aux);
        }
        if (qs)
            s = ((s + (1 << (qs - 1u))) >> qs) << qs;
        if (chip == CHIP_4B2 && (tt & 1u))                /* (4B/2: held over two t) */
            s = fb;
        fb = s;
        tf += ti;
        tt += tf >> 24;
        tf &= 0xFFFFFFu;
        ts += ((ti >> 8) * mul) >> 16;
        sm = LOFI_SOFT(s >> 1, sm);
        out[i] += voice_amp(soft_knee(svf_mode(flt, sm, &y1, &y2, ft, kd), 16000) << 1, m, i);
    }
    v->ph[2] = (uint32_t)sm;
    v->s[0] = fb;
    v->s[2] = (int32_t)tt;
    v->s[3] = aux;
    v->s[4] = y1;
    v->s[5] = y2;
    v->ph[0] = tf;
    v->ph[1] = ts;
}

static const param_desc_t *lofi_desc(const track_t *t, uint32_t k)
{
    if (k == 4u)                                        /* (the knobs' ranges past edit[]'s, as before) */
        return &LOFI_MOTN;
    if (t->p[P_E1] != RW_FLOAT)
        return k == 2u ? &LOFI_ALGO : 0;
    return k == 0u ? &LOFI_FCHIP : k == 2u ? &LOFI_FALGO : k == 5u ? &LOFI_FDCY : 0;
}

static void lofi_note_on(track_t *t, voice_t *v)
{
    v->s[0] = 0;                 /* held sample */
    v->s[1] = 0;                 /* hold counter */
    v->s[2] = 0;                 /* the time t, from 0 */
    v->ph[1] = 0u;               /* (BYTE: its slow time; FLOAT: its phase, a step at once) */
    v->s[3] = 0;                 /* (BYTE: its smoothing's first pole) */
    v->ph[2] = 0;                /* (.. its second; FLOAT: its patch's own state) */
    v->s[4] = v->s[5] = 0;       /* (JIANT 0.4) the filter's two states */
    if (t->p[P_E1] == RW_FLOAT)                         /* (0.5.6: its t from 0, its fraction too) */
        v->ph[0] = 0;
}

/* the set-up both formulas share (the level, the pitch, the filter), then BYTE's or FLOAT's samples */
static void lofi_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t chip = (uint32_t)p[P_E0], inc;
    int32_t rs = p[P_FRES] > 50 ? p[P_FRES] : 50;       /* (JIANT 0.5) its RES the FILTER page's, 50 at least */
    tsvf_t flt;
    vmod_t mg = *m;                                     /* (0.9.11) the engine's level: on the block's amplitude ramp */
    mg.amp0 = mulq15(m->amp0, LOFI_GAIN);
    mg.amp1 = mulq15(m->amp1, LOFI_GAIN);
    inc = pitch_inc(clamp(mg.pitch16, 0, 2047));       /* (the track's LFO in m->pitch16 already) */
    tsvf_coef(&flt, (p[P_FCUT] << 8) + mg.cutoff, rs);   /* (JIANT 0.5: the FILTER page's CUT) */
    if (p[P_E1] == RW_FLOAT)                            /* (JIANT 0.5.1) floatbeat */
        lofi_float_render(t, v, out, n, &mg, inc, &flt);
    else                                                /* (JIANT 0.4) bytebeat */
        lofi_byte_render(t, v, out, n, &mg, inc, 1 + (chip == CHIP_4B2 ? 1 : 0),
                         chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4, &flt);
}

/* (V10.6d) every sound starts still (MOTION 0: the formula's slow bits at 1/16, no evolution to startle) and with RTRG on
 * (ui.c apply_preset_to: each note the formula from its start, a voice that plays in tune); BYTE first, then FLOAT
 * (their stored numbers kept: 0..2, 8 BYTE; 9..11 FLOAT), 3..7 (where the chip presets were) FLOAT voices at the note:
 * keys, a bass, a pad, a lead, a bell */
static const preset_t LOFI_PRESETS[] = {
    /* name, {CHIP, WAVE, ALGO, VAR, MOTION, GRIT / FEEDBK, BEND, LOOP}. (V10.6i, reworked in place: the stored numbers
     * kept, LOFI_PC and GEN's roles too: 4 6 11) MOTION 0 on all (the time still); the voices to play filtered, the
     * filter opened by velocity and its envelope; the BYTE ones and FLOAT ARP the experiments, moved by the matrix (the
     * track's SHP moves VAR, CUT the FILTER page's CUT); INIT FLOAT a bare sine (LOOP 127: one cycle of F01) */
    /* BYTE: 8 bits, ALGO B02 (4 / 4 = 1); B17 (64 / 4 = 16) with VAR 40 (a = 6); B05 folded (BEND), resonant */
    {"BYTEBEAT", {2, 5, 4, 0, 0, 0, 0, 0}, {0, 80, 110, 40}, 0, 1 | TAG(FX), FX(0, 0, 25, 20), PAT(4),
     MODS(MMOD(1, PS_MODW, PD_E(3), 40), MMOD(2, PS_AT, PD_E(6), 40))},      /* MODW mutates the time, AT folds it */
    {"BYTE VAR", {2, 5, 64, 40, 0, 0, 0, 0}, {0, 80, 110, 50}, 0, 1 | TAG(FX), FX(0, 20, 30, 25), PAT(3),
     MODS(P_LWAVE, LW_SH, P_LRATE, -8, MMOD(1, PS_LFO, PD_E(3), 24), MMOD(2, PS_RAND, PD_PAN, 30))},   /* a new VAR each 1/8 */
    {"BYTE BENT", {2, 5, 16, 0, 0, 24, 70, 0}, {0, 80, 110, 45}, 0, 1 | TAG(FX), FX(10, 10, 30, 25), PAT(4),   /* (0.5.4: GRIT 24) */
     MODS(MENV2(0, 50, 0, 30), MMOD(1, PS_ENV2, PD_E(6), -40), MMOD(2, PS_RAND, PD_REV, 30))},   /* unfolds as it sounds */
    /* FLOAT at the note: F28 a partial struck; F20 odd partials (a hollow square); F12 a deep vibrato (slow, MOTION 0);
     * F24 FM 5:2; F11 a pluck of FM */
    {"MATH KEYS", {2, 6, 108, 0, 0, 0, 0, 0}, {0, 75, 40, 45}, 20, TAG(KEYS), FX(0, 20, 30, 35), PAT(6),
     MODS(P_FCUT, 84, MMOD(1, PS_VEL, PD_CUT, 20))},
    {"MATH BASS", {2, 6, 76, 0, 0, 0, 0, 0}, {0, 60, 90, 22}, 24, 1 | TAG(BASS), FX(0, 0, 5, 0), PAT(2),
     MODS(P_FCUT, 70, P_GLIDE, 10)},
    {"MATH PAD", {2, 6, 44, 0, 0, 0, 0, 0}, {75, 90, 78, 95}, 0, TAG(PAD), FX(0, 50, 30, 60), PAT(5),
     MODS(P_FCUT, 96, P_LRATE, 20, P_LD_FLT, 10)},
    {"MATH LEAD", {2, 6, 92, 0, 0, 0, 0, 0}, {4, 70, 100, 42}, 0, 1 | TAG(LEAD), FX(0, 0, 35, 25), PAT(4),
     MODS(P_LRATE, 88, P_LFADE, 72, MMOD(1, PS_MODW, PD_VIB, 2), P_GLIDE, 10)},
    {"MATH BELL", {2, 6, 40, 0, 0, 0, 0, 0}, {0, 88, 0, 70}, 12, TAG(BELL), FX(0, 20, 35, 55), PAT(7),
     MODS(P_FCUT, 100, MMOD(1, PS_VEL, PD_CUT, 22))},
    /* (JIANT 0.5, appended) BYTE B11 looped (LOOP 80: a short cycle, a tone at the note), resonant; its LOOP leaps on
     * the 1/8 (V10.6i), MODW the VAR */
    {"BYTE TONE", {2, 5, 40, 0, 0, 0, 0, 80}, {0, 70, 100, 40}, 0, 1 | TAG(LEAD), FX(0, 0, 30, 20), PAT(4),
     MODS(P_LWAVE, LW_SQR, P_LRATE, -8, MMOD(1, PS_LFO, PD_E(7), 6), MMOD(2, PS_MODW, PD_E(3), 30))},
    /* (V10.6i) a bare sine (was FLOAT FM) */
    {"INIT FLOAT", {2, 6, 0, 0, 0, 0, 0, 127}, {0, 64, 80, 20}, 0, TAG(KEYS), FX(0, 0, 0, 0)},
    /* (JIANT 0.5.1, appended; 0.5.6 floatbeat) FLOAT: F03 a nibble-table melody (S&H 1/16 on its filter, each note its
     * place); F29 FM breathing at 1 Hz, slow (V10.6i: the filter drifting) */
    {"FLOAT ARP", {2, 6, 8, 0, 0, 0, 0, 0}, {0, 50, 80, 50}, 0, 1 | TAG(PLUCK), FX(0, 25, 40, 30), PAT(3),
     MODS(P_FCUT, 100, P_LWAVE, LW_SH, P_LRATE, -10, P_LD_FLT, 16, MMOD(1, PS_RAND, PD_PAN, 40))},
    {"FLOAT PAD", {2, 6, 112, 0, 0, 0, 0, 0}, {80, 90, 69, 100}, 0, 1 | TAG(PAD), FX(30, 10, 50, 40), PAT(3), BREATHE(14, 0, 0),
     MODS(P_FCUT, 90)},
};
/* (V10.6i) browsed: the voices to play, the experiments, INIT (ui.c: in LOFI_ORDER's place; LOFI_PC stays) */
static const uint8_t MATH_ORDER[] = {3, 4, 5, 6, 7, 11,   /* to play */
                                     0, 1, 2, 8, 10,      /* experiments */
                                     9};                  /* INIT */
_Static_assert(sizeof MATH_ORDER == NELEM(LOFI_PRESETS), "MATH's browsing order: every preset once");

static const engine_t ENG_LOFI = {
    .name = "MATH",
    .page_title = {"MATH", "TIME"},
    .edit = {
        {"CHIP", F_ENUM, 0, 3, 2, N_CHIP, 0},           /* (FLOAT: 8BIT reads FULL, lofi_desc) */
        {"WAVE", F_ENUM, RW_BYTE, RW_FLOAT, RW_BYTE, N_RWAVE, 0},
        {"ALGO", F_INT, 0, 127, 64, 0, 0},              /* (the knob 0..191 and the names: lofi_desc) */
        {"VAR", F_PCT, 0, 127, 0, 0, 0},
        {"MOTION", F_INT, 0, 127, 0, 0, 0},             /* (V10.6d: 0 the default; the knob to 255 SYNC: lofi_desc) */
        {"GRIT", F_PCT, 0, 127, 0, 0, 0},               /* (FLOAT: FEEDBK) */
        {"BEND", F_PCT, 0, 127, 0, 0, 0},
        {"LOOP", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = LOFI_PRESETS,
    .npresets = NELEM(LOFI_PRESETS),
    .note_on = lofi_note_on,
    .render = lofi_render,
    .knob = {P_E1, P_E2, P_E3, P_REL},
    .keep = 0x31,                /* the held sample and the filter */
    .desc = lofi_desc,
};
