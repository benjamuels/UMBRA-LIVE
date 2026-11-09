/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Parameter descriptors, formatting and the page table. */
static const char *const N_LWAVE[] = {"SIN", "TRI", "SAW", "SQR", "S&H"};
static const char *const N_LWAVE6[] = {"SIN", "TRI", "SAW", "SQR", "S&H", "DRIFT"};   /* (V10.36) the LFO's own: AUTO PAN keeps five */
/* (JIANT 0.6.51) the LFO's RATE below 0: in time with the tempo, -1 the slowest (next to the slowest free rate) */
static const char *const N_LSYNC[14] = {"4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8D", "1/4T", "1/8", "1/8T", "1/16",
                                        "1/16T", "1/32", "8BAR", "16BAR"};   /* (V10.27: -13 8BAR, -14 16BAR, a tester's) */
static const char *const N_AMODE[] = {"OFF", "UP", "DN", "UPDN", "RND", "ORD", "REPEAT",   /* (append-only: seq.c AM_*) */
                                       "DNUP", "UP+8", "CONV", "DIVG", "PINKY", "THUMB", "WALK", "CHORD",
                                       "TRNS"};                         /* (JIANT: seq.c AM_TRNS) */
static const char *const N_DIV[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR"};
/* (JIANT 0.4) the delay's TIME: N_DIV's ten, then free times past the last division (5 ms .. 1.48 s, exponential;
 * fx.c delay_samples, DTIME_MS); (0.8.4) appended up to the longest the line holds at a quarter rate (the values
 * before them mean what they did) */
#define DT_FREE 10
/* (0.8.4) the delay's line (fx.c dly_buf): DLY_LEN samples of int16, the one constant of its size. 32768 (64 KB): the full
 * rate to 0.74 s, half to 1.49 s, a quarter to 2.97 s; 65536 (128 KB): 1.49 / 2.97 / 5.94 s */
#ifndef DLY_LEN
#define DLY_LEN 32768u
#endif
static const char *const N_DRTIM[] = {"=L", "x1/2", "x2/3", "x3/4", "x4/3", "x3/2", "x2"};   /* (V10.49) R TIME */
static const char *const N_DTIME[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR", "5ms", "6ms", "7ms", "8ms", "9ms", "10ms", "11ms", "12ms", "13ms", "15ms", "17ms", "19ms", "21ms", "24ms", "27ms", "31ms", "35ms", "39ms", "44ms", "50ms", "56ms", "64ms", "72ms", "81ms", "91ms", "103ms", "116ms", "131ms", "148ms", "167ms", "189ms", "213ms", "241ms", "272ms", "307ms", "346ms", "391ms", "441ms", "498ms", "562ms", "634ms", "716ms", "808ms", "912ms", "1.03s", "1.16s", "1.31s", "1.48s",
                                      /* (0.8.4) past the line at the full rate (fx.c dly_lo): half rate to 2.97 s, then a quarter */
                                      "1.67s", "1.89s", "2.13s", "2.40s", "2.71s"
#if DLY_LEN >= 65536u
                                      , "3.06s", "3.45s", "3.90s", "4.40s", "4.97s", "5.61s", "5.90s"
#else
                                      , "2.95s"
#endif
                                      , "1/8D", "1/16D", "1/4D"   /* (V10.8) dotted, appended (the values before mean what
                                                         * they did); DTIME_ORDER puts them by their divisions */
                                      };
#define DT_DOT (DLY_LEN >= 65536u ? 70u : 64u)          /* the first dotted one */
typedef char dt_dot_ok[NELEM(N_DTIME) == DT_DOT + 3u ? 1 : -1];
static const uint16_t DTIME_MS[NELEM(N_DTIME) - DT_FREE] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 17, 19, 21, 24, 27, 31, 35, 39, 44, 50, 56, 64, 72, 81, 91, 103, 116, 131, 148, 167, 189, 213, 241, 272, 307, 346, 391, 441, 498, 562, 634, 716, 808, 912, 1029, 1162, 1311, 1480,
                                      1670, 1885, 2128, 2402, 2711
#if DLY_LEN >= 65536u
                                      , 3060, 3454, 3899, 4400, 4967, 5606, 5900
#else
                                      , 2950
#endif
                                      };
static const char *const N_SCALE[] = {"CHR", "MAJ", "MIN", "DOR", "MIX", "PEN", "MPEN", "HARM",
                                    "PHRY", "LYD", "LOC", "MEL", "BLUES", "WHOLE", "DIMHW", "DIMWH"};
static const char *const N_ONOFF[] = {"OFF", "ON"};
static const char *const N_DLYTYPE[] = {"ANALOG", "DIGITAL", "REVERS", 0};   /* (0.6.90) DLY 3 > TYPE; (V10.6g) REVERSE */
static const char *const N_PFXRES[] = {"1/8", "1/16", "1/32"};     /* (0.6.99) FX > PUNCH: the lane's step */
/* seq.c kb_map; 1 = SNAP (stored projects: the former ON); 3 = SEQ: SNAP, and the sequencer's notes snap too as
 * they play (seq.c seq_step; the steps keep what was written). Append-only: older projects hold 0..2 */
static const char *const N_QUANT[] = {"OFF", "SNAP", "WHITE", "SEQ"};
static const char *const N_DTYPE[] = {"SOFT", "HARD", "FOLD", "CRUSH", "RECT", "AMP"};   /* (JIANT) fx.c DT_*; 0.7.8 AMP */
static const char *const N_FTYPE[] = {"LP", "HP", "BP", "COMB", "COMB-", "MS20", "LP24"};   /* (JIANT 0.5) P_FTYPE: eng_analog.c FT_*
                                                         * (0.9.9-snd1: COMB- the negative comb, MS20) */
static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */
static const char *const N_GLMODE[] = {"RATE", "TIME"};
static const char *const N_PRIO[] = {"LAST", "LOW", "HIGH"};
static const char *const N_ALLOC[] = {"ROT", "REUSE"};
static const char *const N_ORDER[] = {"NOTE", "PLAY"};
static const char *const N_CLOCK[] = {"INT", "USB+BT", "TRS"};   /* (V10.37) USB follows Bluetooth's clock too */
static const char *const N_MIDI_INPUT[] = {"USB", "TRS"};
static const char *const N_ROUTE[] = {"CH1-6", "SEL"};   /* MIDI IN (seq.c): 1..6 -> parts, 7..16 ignored / all -> selected */
static const char *const N_NOTE[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const N_DASH[] = {"--"};
static const char *const N_RTYPE[] = {"ROOM", "SPRING", "SHIMR", "RESO", "CLOUD"};   /* (JIANT 0.5: + 3) */   /* G_RTYPE: the reverb bus's model (fx.c) */
static const char *const N_GO[] = {"--", "GO"};
static const char *const N_SLCR[] = {"OFF", "GATE", "STUT"};             /* SL_OFF .. SL_STUT (slicer.c) */
static const char *const N_SLDIV[] = {"1/8", "1/16", "1/32", "8T", "16T", "32T"};   /* SL_DEN */
static const char *const N_SLPQ[] = {"CHR", "SCL"};                      /* (JIANT 0.6.3) the SLICER's pitch steps */
static const char *const N_APSYNC[] = {"FREE", "BEAT"};                  /* (JIANT 0.6.9) AUTO PAN's RATE: Hz / the tempo */
static const char *const N_APDIV[] = {"4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};   /* .. BEAT (fx.c AP_SIXT) */
/* modulation matrix (mod.c): sources, destinations (E1..E8 = P_E0..P_E7: shown with the engine's labels) */
static const char *const N_MSRC[] = {"OFF", "LFO", "ENV", "VEL", "KEY", "RAND", "MODW", "AT", "EXPR",
                                     "M1", "M2", "M3", "M4", "STEP", "NOISE", "PROX", "ENV2", "IN ENV", "T1 ENV", "T2 ENV", "T3 ENV", "T4 ENV", "T5 ENV", "T6 ENV"};   /* (V10.22) ENV2 */   /* (0.7.8) NOISE; (V10.14) PROX */   /* (JIANT: the macros, LFO held; the modulation sequence) */
static const char *const N_MDST[] = {"OFF", "PITCH", "CUT", "SHP", "AMP", "PAN", "DIST", "CHO", "DLY", "REV", "L.RATE",
                                     "L.PIT", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8",
                                     "CLIP", "PNCH",   /* (JIANT: the master, FX > MASTER) */
                                     "D.TIM", "D.FB", "D.TON", "D.MIX", "R.SIZ", "R.DMP", "C.RAT", "C.DEP", "F.CUT", "LEVEL"};   /* (0.7.0) the FX buses'
                                                         * globals (DELAY TIME FDBK TONE MIX, REVERB SIZE DAMP, CHORUS RATE
                                                         * DEPTH): mod.c macro_master, fx.c GFX() */
static const char *const N_ENGNAME[] = {"ANALOG", FELUCCA_FM4 ? "DIGITAL" : "-", "PHASE", "MATH", "-", "VOICE", "-", "WHEEL", "STRING", "-",
                                             "DRUM", "NOISE", "FM6", "ADD", "ANIMAL", "EXT"};   /* (JIANT: SAMPLE 4, GRAIN 8, SLICE 13 retired; 0.8.3 ANIMAL;
                                                         * 0.9.9-snd5 STRING ADD) */

#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}

static const param_desc_t TP[P_COUNT];
/* (V10.6i) a factory preset's movement (preset_t.mod): each pair a common parameter (< P_E0) and its value, within its
 * range (ui.c apply_preset_to, tests/hostsim.c) */
_Static_assert(P_E0 <= 127, "preset_t.mod names a parameter in an int8_t");
static void preset_mods(track_t *t, const preset_t *pr)
{
    const int8_t *m = pr->mod;
    for (; m && m[0] >= 0; m += 2)
        if (m[0] < P_E0)
            t->p[m[0]] = (int16_t)clamp(m[1], TP[m[0]].min, TP[m[0]].max);
}
static const param_desc_t TP[P_COUNT] = {
    [P_LEVEL] = PD("LEVEL", F_DB, 0, 127, 104),
    [P_ATK] = PD("ATK", F_TIME, 0, 127, 10),
    [P_DEC] = PD("DEC", F_TIME, 0, 127, 70),
    [P_SUS] = PD("SUS", F_PCT, 0, 127, 90),
    [P_REL] = PD("REL", F_TIME, 0, 127, 60),
    [P_ED_FLT] = PD("FILTER", F_BIPCT, -64, 63, 0),
    [P_ED_PIT] = PD("PITCH", F_BIPCT, -64, 63, 0),
    [P_ED_SHP] = PD("SHAPE", F_BIPCT, -64, 63, 0),
    [P_ED_FX] = PE("LOOP", N_ONOFF, 0),          /* (JIANT 0.4, P_ELOOP) ON: held, the ENV starts again at the sustain */
    [P_LRATE] = PD("RATE", F_LFOHZ, -14, 127, 60),  /* (JIANT 0.6.51) below 0 in time: 4BAR .. 1/32 (N_LSYNC) */
    [P_LWAVE] = PE("WAVE", N_LWAVE6, 0),
    [P_LPHASE] = PD("PHASE", F_INT, -1, 127, 0),   /* (JIANT 0.6.51) -1: FREE, never restarted by a note (0.9.18: nor its
                                                     * FADE); the pages show it as MODE TRIG / FREE and PHASE 0..127 */
    [P_LFADE] = PD("FADE", F_TIME, 0, 127, 0),
    [P_LD_PIT] = PD("PITCH", F_BIPCT, -64, 63, 0),
    [P_LD_FLT] = PD("FILTER", F_BIPCT, -64, 63, 0),
    [P_LD_SHP] = PD("SHAPE", F_BIPCT, -64, 63, 0),
    [P_LD_AMP] = PD("AMP", F_PCT, 0, 127, 0),
    [P_AMODE] = PE("MODE", N_AMODE, 0),
    [P_ARATE] = PE("RATE", N_DIV, 2),
    [P_AOCT] = PD("OCT", F_INT, 1, 4, 1),
    [P_AGATE] = PD("GATE", F_PCT, 1, 127, 64),
    [P_ASWING] = PD("SWING", F_PCT, 0, 100, 0),
    [P_APROB] = PD("PROB", F_PCT, 0, 127, 127),
    [P_AHOLD] = PE("HOLD", N_ONOFF, 0),
    [P_AORDER] = PE("ORDER", N_ORDER, 0),
    [P_ROOT] = PD("ROOT", F_NOTE, 0, 11, 0),
    [P_SCALE] = PE("SCALE", N_SCALE, 0),
    [P_QUANT] = PE("QUANT", N_QUANT, 0),
    [P_TRANS] = PD("TRANS", F_SEMI, -24, 24, 0),
    [P_SLEN] = PD("LEN", F_STEPS, 1, NSTEP, 16),
    [P_SDIV] = PE("DIV", N_DIV, 2),
    [P_SSWING] = PD("SWING", F_PCT, 0, 100, 0),
    [P_SGATE] = PD("GATE", F_PCT, 1, 127, 127),   /* (V10.6z4, the owner: 100 %, was 50 %; stored as before: param_store_def) */
    [P_DIST] = PD("DIST", F_PCT, 0, 127, 0),
    [P_CHOR] = PD("CHORUS", F_PCT, 0, 127, 0),
    [P_DLY] = PD("DLY", F_PCT, 0, 127, 0),
    [P_REV] = PD("REV", F_PCT, 0, 127, 0),
    [P_VOICE] = PE("VOICE", N_VOICE, 0),
    [P_GLIDE] = PD("GLIDE", F_TIME, 0, 127, 0),
    [P_GLMODE] = PE("MODE", N_GLMODE, 0),
    [P_PRIO] = PE("PRIO", N_PRIO, 0),
    [P_ALLOC] = PE("ALLOC", N_ALLOC, 0),
    [P_DETUNE] = PD("DETUNE", F_INT, 0, 127, 40),
    [P_PAN] = PD("PAN", F_BIPCT, -64, 63, 0),
    [P_MUTE] = PE("MUTE", N_ONOFF, 0),
    [P_APDEP] = PD("DEPTH", F_PCT, 0, 127, 0),       /* (JIANT 0.6.9) AUTO PAN (core.h: the old SLICER's slots) */
    [P_APRATE] = PD("RATE", F_LFOHZ, 0, 127, 2),    /* (BEAT: 0..7, page_desc AP_BEAT) */
    [P_APWAVE] = PE("WAVE", N_LWAVE, 1),
    [P_APSYNC] = PE("SYNC", N_APSYNC, 1),
#define MSLOT(k) [P_M##k##SRC] = PE("SRC" #k, N_MSRC, 0), [P_M##k##DST] = PE("DEST" #k, N_MDST, 0), \
                 [P_M##k##AMT] = PD("AMT" #k, F_BIPCT, -64, 63, 0)
    MSLOT(1), MSLOT(2), MSLOT(3), MSLOT(4),
#undef MSLOT
/* the OP ENV / OP LEVEL values of DIGITAL (ids 61..80): with FELUCCA_FM4 its operator envelopes; without it
 * inert, on no page and in no editor layout, read only when a DIGITAL sound converts (fm4_convert.c). Their labels
 * stay: the editor's library files key parameters by label ("ATK#2" ..), so an old file's values still find them */
#define FMOP(k) [P_FM##k##_ATK] = PD("ATK", F_TIME, 0, 127, 0), \
                [P_FM##k##_DEC] = PD("DEC", F_TIME, 0, 127, 0), \
                [P_FM##k##_SUS] = PD("SUS", F_PCT, 0, 127, 127), \
                [P_FM##k##_REL] = PD("REL", F_TIME, 0, 127, 0), \
                [P_FM##k##_LEVEL] = PD("LEVEL", F_PCT, 0, 127, 127)
    FMOP(1), FMOP(2), FMOP(3), FMOP(4),
#undef FMOP
    [P_SOFS] = PD("ROTATE", F_INT, -32, 32, 0),    /* the sequence played OFS steps later (seq.c seq_src; 0.6.92: was OFFSET) */
    [P_POFS] = PD("PITCH", F_SEMI, -24, 24, 0),   /* its notes PIT semitones up (not on a kit) */
/* the DRUM engine's lane levels (eng_drum.c, #97): 100 % = the kit as designed (the sound before 1.1) */
#define LANE(k, l) [P_LN0 + (k)] = PD(l, F_PCT, 0, 127, 127)
    LANE(0, "KICK"), LANE(1, "SNARE"), LANE(2, "CLAP"), LANE(3, "HATCL"),
    LANE(4, "HATOP"), LANE(5, "TOM"), LANE(6, "RIM"), LANE(7, "BELL"),
#undef LANE
    [P_DTYPE] = PE("TYPE", N_DTYPE, 0),         /* (JIANT) DIST: SOFT (as before) HARD FOLD CRUSH RECT AMP */
    [P_DTONE] = PD("TONE", F_BIPCT, -64, 63, 0),   /* .. its tone: darker / brighter */
    [P_MSLEN] = PD("LEN", F_STEPS, 1, 16, 16),  /* (JIANT) the modulation sequence (mod.c MS_STEP) */
    [P_MSDIV] = PE("DIV", N_DIV, 2),
    [P_MSSLW] = PD("SLEW", F_PCT, 0, 127, 0),
#define MSTEP(k) [P_MS0 + (k)] = PD("LEVEL", F_PCT, 0, 127, 0)
    MSTEP(0), MSTEP(1), MSTEP(2), MSTEP(3), MSTEP(4), MSTEP(5), MSTEP(6), MSTEP(7),
    MSTEP(8), MSTEP(9), MSTEP(10), MSTEP(11), MSTEP(12), MSTEP(13), MSTEP(14), MSTEP(15),
#undef MSTEP
    [P_FTYPE] = PE("TYPE", N_FTYPE, 0),         /* (JIANT 0.5) the filter: LP (as before) HP BP COMB */
    [P_FCUT] = PD("CUT", F_CUTOFF, 0, 127, 127),  /* .. the other engines' (on the part): open, */
    [P_FRES] = PD("RES", F_PCT, 0, 127, 0),       /* no resonance: out of the chain (its drive: DIST, the part's) */
};

static const param_desc_t GP[G_COUNT] = {
    [G_BPM] = PD("BPM", F_BPM, 40, 240, 120),
    [G_SWING] = PD("SWING", F_PCT, 0, 100, 0),
    [G_CLOCK] = PE("CLOCK", N_CLOCK, 0),
    [G_TUNE] = PD("TUNE", F_INT, -50, 50, 0),
    [G_DTIME] = PE("TIME", N_DTIME, 1),                 /* (JIANT 0.4: past the divisions, free times) */
    [G_DFDBK] = PD("FEEDBK", F_PCT, 0, 120, 60),
    [G_DCOLOR] = PD("TONE", F_PCT, 0, 127, 64),        /* (JIANT 0.4, was COLR + HPF) the repeats: below 64 darker, above
                                                        * thinner (64 open) */
    [G_DMIX] = PD("MIX", F_PCT, 0, 127, 90),
    [G_RSIZE] = PD("SIZE", F_PCT, 0, 127, 90),
    [G_RDAMP] = PD("DAMP", F_PCT, 0, 127, 60),
    [G_CRATE] = PD("RATE", F_LFOHZ, 0, 127, 40),
    [G_CDEPTH] = PD("DEPTH", F_PCT, 0, 127, 60),
    [G_MIDI] = PE("MIDI", N_MIDI_INPUT, 0),
    [G_DREL] = PD("REL", F_PCT, 0, 100, 30),     /* DUCK's release: 40 .. 600 ms (fx.c duck_block) */
    [G_ROUTE] = PE("ROUTE", N_ROUTE, 0),          /* (was "--": stored 0 = CH1-4, as MIDI IN always was) */
    [G_INFO] = PD("CPU", F_INT, 0, 0, 0),
    [G_SLOT] = PD("SLOT", F_INT, 1, 4, 1),
    [G_DJF] = PD("DJ FILT", F_BIPCT, -64, 63, 0),   /* (0.9.1) the master DJ FILTER: 0 off, - low-pass, + high-pass (fx.c) */
    [G_LOAD] = PE("LOAD", N_GO, 0),
    [G_SAVE] = PE("SAVE", N_GO, 0),
    [G_ENGSEL] = PE("ENG", N_ENGNAME, 0),
    [G_CLIP] = PD("CLIP", F_PCT, 0, 100, 0),     /* the master clipper: drive x1 .. x4 into the soft clip */
    [G_CLRSEQ] = PE("CLRSQ", N_GO, 0),
    [G_INITSND] = PE("INIT", N_GO, 0),
    /* the reverb's model on the REVERB page: the id of the old GM drum channel (G_DRCH, inert since 1.0) */
    [G_RTYPE] = PE("TYPE", N_RTYPE, 0),
    /* (JIANT) the drum bus and the kick's ducking (fx.c, eng_drum.c); the ids of the GM drum part's level and
     * reverb send, inert since 1.0 */
    [G_PUNCH] = PD("PUNCH", F_PCT, 0, 100, 0),    /* DRUM fattened: up to +5 dB the first 4 ms, the body +3 dB (0.5) */
    [G_DUCK] = PD("DUCK", F_PCT, 0, 100, 0),     /* the kick ducks the other parts: up to -18 dB */
    /* (JIANT, FUNB) the buses: not in a project's header (project.c: after the punch-in lane) */
    [G_CSTER] = PD("STEREO", F_BIPCT, -64, 63, 0), /* (0.6.9) the CHORUS's stereo: 0 as before, -64 mono, 63 ~2x (was G_DHPF) */
    [G_WIDTH] = PD("WIDE", F_PCT, 0, 127, 0),    /* the delay's right echo up to 1/32 later, the chorus's taps apart */
    [G_DTYPE] = {"TYPE", F_ENUM, 0, 2, 0, N_DLYTYPE, 0},   /* (0.6.90) ANALOG (saturating, resonant, past 100 % grows) / DIGITAL */
    [G_DPING] = {"PING", F_ENUM, 0, 1, 0, N_ONOFF, 0},   /* (0.6.90) PING-PONG: the repeats left, right, left .. */
    [G_PFXLEN] = PD("LEN", F_INT, 1, 64, 64),    /* (0.6.99) the punch-in lane: its steps and their size */
    [G_PFXRES] = {"RES", F_ENUM, 0, 2, 1, N_PFXRES, 0},
    [G_DRTIM] = PE("R TIME", N_DRTIM, 0),           /* (V10.49) the right echo's time, a ratio of TIME (fx.c dly_rate) */
    [G_DDRV] = PD("DRIVE", F_PCT, 0, 100, 0),    /* (snd2) the delay's loop driven: 0 clean as before, 100 x4 into the clip */
    [G_DMOD] = PD("MOD", F_PCT, 0, 127, 0),      /* (0.6.89: 0, the repeats exact copies, by default) */     /* (0.6.86) the delay's drift: WOW + FLUTTER (fx.c dly_wow_adv) */
    [G_RMOD] = PD("MOD", F_PCT, 0, 127, 0),      /* the reverb's modulation depth (ROOM: the combs, SPRING: its wobble) */
    [G_RRATE] = PD("RATE", F_LFOHZ, 0, 127, 30), /* .. its rate */
    [G_RPRE] = PD("PRE", F_INT, -24, 100, 0),    /* the reverb's pre-delay, ms (0.6.12: SHIMMER / CLOUD's AIR, -24..24, below 0) */
    [G_RFILT] = PD("FILT", F_BIPCT, -64, 63, 0), /* (JIANT 0.3) the reverb's input: darker (low-pass) / brighter (low cut) */
    [G_RWIDE] = PD("WIDE", F_PCT, 0, 127, 0),    /* .. its width: its combs (ROOM) / taps (SPRING) apart, left and right */
    [G_DPIT] = PD("PITCH", F_SEMI, -12, 12, 0),  /* the GRAIN delay: each repeat through two grains this far up / down */
    [G_DSPRY] = PD("SPRAY", F_PCT, 0, 127, 0),    /* .. their spray: each grain from a random place (0: plain delay) */
    [G_STRN] = PD("ALL TR", F_INT, -14, 14, 0),  /* (JIANT 0.4) every sequence this many steps up / down its scale (0.6.45:
                                                  * named ALL TR, was STRN: nobody knew what moved their notes) */
    [G_SLMODE] = PE("MODE", N_SLCR, 0),         /* (JIANT 0.6.3) the SLICER bus (slicer.c) */
    [G_SLPAT] = PD("PAT", F_INT, 1, 16, 1),
    [G_SLRATE] = PE("RATE", N_SLDIV, 1),
    [G_SLDEP] = PD("DEPTH", F_PCT, 0, 127, 127),
    [G_SLT1] = PE("T1", N_ONOFF, 1), [G_SLT1 + 1] = PE("T2", N_ONOFF, 1), [G_SLT1 + 2] = PE("T3", N_ONOFF, 1),
    [G_SLT1 + 3] = PE("T4", N_ONOFF, 1), [G_SLT1 + 4] = PE("T5", N_ONOFF, 1), [G_SLT6] = PE("T6", N_ONOFF, 1),
    [G_SLFX] = PE("FX", N_ONOFF, 1),            /* the delay, reverb and chorus returns: on, the tails cut too */
    [G_SLPLEN] = PD("LEN", F_INT, 1, 16, 16),
    [G_SLPQ] = PE("QUANT", N_SLPQ, 1),            /* the pitch steps: every semitone / the scale's */
    [G_SLP0] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 1] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 2] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 3] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 4] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 5] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 6] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 7] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 8] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 9] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 10] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 11] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 12] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 13] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 14] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP15] = PD("PITCH", F_SEMI, -24, 24, 0),
};

static const param_desc_t LOFI_RTRG = {"RTRG", F_ONOFF, 0, 1, 1, 0, 0};   /* (0.6.12) P_LRTRG on a MATH track (V10.6d: ON its default) */
static const param_desc_t TRK_CHANCE = {"CHANCE", F_PCTINV, 0, 127, 0, 0, 0};   /* (0.6.37) P_TCHAN: 0 = 100 % */
static const param_desc_t MORPH_DESC = {"MORPH", F_PCT, 0, 127, 0, 0, 0};   /* (0.6.45) P_MORPH */
static const char *const N_CLEN[] = {"OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15",
                                     "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
                                     "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43",
                                     "44", "45", "46", "47", "48", "49", "50", "51", "52", "53", "54", "55", "56", "57",
                                     "58", "59", "60", "61", "62", "63", "64", 0};
static const param_desc_t CLEN_DESC = {"C.LEN", F_ENUM, 0, 64, 0, N_CLEN, 0};   /* (0.6.47) P_CLEN */
static const param_desc_t DXIMG_DESC = {"IMAGE", F_INT, 0, 127, 0, 0, 0};       /* (0.6.48) P_DXIMG */
/* (V10.6) SEQ > EUCLID / MODES' knobs (ui_euclid.c: ids ID_EU0 + EK_*, the views of the selected track's settings) */
#define ID_EU0 0xF0u
#define ID_SC0 0xE8u                                    /* (V10.25) SCL > SCENE's four (ui_scene.c sc_desc) */
#define ID_MU0 0xE4u                                    /* (V10.16) SEQ > MUTATE's four (EK_U0 + 0..3) */
static const param_desc_t *eu_desc(uint32_t k, int16_t **valp);
static const param_desc_t *sc_desc(uint32_t k, int16_t **valp);   /* (V10.25) ui_scene.c */
static const char *const N_CHRD[] = {"OFF", "TRIAD", "7TH", "ADD9", "SUS2", "SUS4", "MAJ", "MIN", "7", "MAJ7", "MIN7",
                                     "POWER", 0};
static const char *const N_CVOI[] = {"CLOSE", "LEAD", "OPEN", "+BASS", 0};
static const param_desc_t CHORD_DESC[3] = {              /* (0.6.81) SCL > CHORD: P_CHRD P_CVOI P_CSTR */
    {"CHORD", F_ENUM, 0, 11, 0, N_CHRD, 0}, {"VOICING", F_ENUM, 0, 3, 0, N_CVOI, 0}, {"STRUM", F_PCT, 0, 127, 0, 0, 0}};
static const param_desc_t ENV2_DESC[4] = {             /* (V10.22) ENV > ENV 2 (core.h P_E2*): as ENV's */
    {"ATK", F_TIME, 0, 127, 0, 0, 0}, {"DEC", F_TIME, 0, 127, 0, 0, 0}, {"SUS", F_PCT, 0, 127, 0, 0, 0},
    {"REL", F_TIME, 0, 127, 0, 0, 0}};
static const param_desc_t HUM_DESC[3] = {                /* (0.6.87) SEQ > HUMAN: P_HVEL P_HTIM P_HGAT (127 = 0 %) */
    {"HUMAN", F_PCTINV, 0, 127, 127, 0, 0}, {"TIME", F_PCTINV, 0, 127, 127, 0, 0}, {"GATE", F_PCTINV, 0, 127, 127, 0, 0}};   /* (0.6.92:
                                                         * one amount, P_HVEL; TIME and GATE stay stored, 127) */
static const param_desc_t RECQ_DESC = {"REC Q", F_PCT, 0, 127, 127, 0, 0};   /* (0.6.88) P_HRQ: 100 % on the grid */
static const param_desc_t DRIFT_DESC = {"DRIFT", F_PCTINV, 0, 127, 127, 0, 0};   /* (0.7.8) P_DRIFT: 127 = 0 % */
static const param_desc_t TVEL_DESC = {"VEL", F_PCT, 0, 127, 127, 0, 0};   /* (after V10.7.2) P_TVEL: 127 = 100 % (core.h) */
static const param_desc_t WROT_DESC[2] = {               /* (0.7.8) WHEEL's ROTOR: P_WRDEP (127 = 100 %, as before), P_WDUST */
    {"DEPTH", F_PCT, 0, 127, 127, 0, 0}, {"DUST", F_PCTINV, 0, 127, 127, 0, 0}};
static const param_desc_t VLANG_DESC = {"LANG", F_ENUM, 0, LG_N - 1, 0, N_LANG, 0};   /* (0.8.5) VOICE's words: */
static const char *const N_DKEYMODE[] = {"CLASSIC", "EXTENDED"};
static const param_desc_t DKEYMODE_DESC = {"KEY MAP",F_ENUM,0,1,1,N_DKEYMODE,0};   /* (2..9: EXTENDED, eng_drum.c drum_chrom) */
static const param_desc_t SIMG_DESC = {"IMAGE", F_PCTINV, 0, 127, 127, 0, 0};   /* (V10.49) P_SIMG: 127 = 0 % */
static const param_desc_t VLYR_DESC = {"LYRIC", F_ENUM, 0, LYR_N - 1, 0, N_LYRIC, 0};  /* P_VLANG, P_VLYR (sing.c) */
static const param_desc_t VCONS_DESC = {"CONS", F_INT, 0, 127, 127, 0, 0};   /* (V10.6y) P_VCONS: 127 = 100 % (core.h) */
static const param_desc_t *track_desc(const track_t *t, uint32_t id)
{
    if (id == MOT_DJF)                                /* (0.9.1) a DJ FILTER record (motion.c): the global's */
        return &GP[G_DJF];
    if (id == P_SIMG && ENGINES[eng_idx(t->eng_req)] != &ENG_DRUM)   /* (V10.49) a synth's STEREO IMAGE */
        return &SIMG_DESC;
    if (id == P_DKEYMODE && ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM) return &DKEYMODE_DESC;
    if ((id == P_VLANG || id == P_VLYR || id == P_VCONS) && ENGINES[eng_idx(t->eng_req)] == &ENG_FORMANT)
        return id == P_VLANG ? &VLANG_DESC : id == P_VLYR ? &VLYR_DESC : &VCONS_DESC;
    if (id >= P_E0 && id <= P_E7) {                   /* the engine asked for (t->engine follows after a fade) */
        const engine_t *e = ENGINES[eng_idx(t->eng_req)];
        const param_desc_t *d = e->desc ? e->desc(t, id - P_E0) : 0;   /* a mode-dependent label / names */
        return d ? d : &e->edit[id - P_E0];
    }
    if (id == P_LRTRG && ENGINES[eng_idx(t->eng_req)] == &ENG_LOFI)   /* (0.6.12) LOFI's RTRG in DIGITAL's slot */
        return &LOFI_RTRG;
    if (id == P_TCHAN && !(FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL))   /* (0.6.37) SHIFT's CHANCE */
        return &TRK_CHANCE;
    if (!(FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL))   /* (0.6.43) MUTATE's four */
        switch (id) {
        case P_MORPH: return &MORPH_DESC;
        case P_CLEN: return &CLEN_DESC;
        case P_DXIMG: return &DXIMG_DESC;
        case P_CHRD: return &CHORD_DESC[0];
        case P_CVOI: return &CHORD_DESC[1];
        case P_CSTR: return &CHORD_DESC[2];
        case P_HVEL: return &HUM_DESC[0];
        case P_E2ATK: return &ENV2_DESC[0];
        case P_E2DEC: return &ENV2_DESC[1];
        case P_E2SUS: return &ENV2_DESC[2];
        case P_E2REL: return &ENV2_DESC[3];
        case P_HTIM: return &HUM_DESC[1];
        case P_HGAT: return &HUM_DESC[2];
        case P_HRQ: return &RECQ_DESC;
        case P_DRIFT: return &DRIFT_DESC;
        case P_TVEL: return &TVEL_DESC;
        case P_WRDEP: return &WROT_DESC[0];
        case P_WDUST: return &WROT_DESC[1];
        default: break;
        }
    return &TP[id];
}

/* the descriptor of parameter id for engine e without the engine's desc hook (the device display
 * only; range and default are the same): what the editor protocol, user presets and projects use */
static const param_desc_t *param_desc_of(uint32_t e, uint32_t id)
{
    return id >= P_E0 ? &ENGINES[e]->edit[id - P_E0] : &TP[id];
}
/* (V10.6z4) the default a store (projects, user presets) leaves out or fills in: GATE's 50 % as every store before
 * (its value then not written), so one saved at the old default loads 50 %, a new track's 100 % is written */
#define GATE_STORE_DEF 64
static int16_t param_store_def(uint32_t e, uint32_t id)
{
    return id == P_SGATE ? GATE_STORE_DEF : param_desc_of(e, id)->def;
}

/* a retired F_ENUM value kept as an alias (the sample engines' SET / SRC aliases left with them in JIANT): none now.
 * -> the value v stands for */
static int32_t enum_orig(const param_desc_t *d, int32_t v)
{
    (void)d;
    return v;
}
static uint32_t as_mix(uint32_t h, const void *p, uint32_t n)   /* FNV-1a, continued (project.c autosave_sig; song_play.c snd_sig) */
{
    const uint8_t *b = p;
    while (n--) {
        h ^= *b++;
        h *= 16777619u;
    }
    return h;
}
/* the track's settings, not the sound's: what a sound load (factory or user preset, an engine jump,
 * TOOLS INIT) leaves alone. The mix (LEVEL, PAN, MUTE: the TRACKS faders), the arpeggiator (ARP, ARP 2),
 * the scale and key map (SCL), the pattern parameters (LEN, DIV, SWING, GATE) and the SLICER insert,
 * which chops whatever the track plays in time with its sequencer */
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_SOFS || i == P_POFS ||
           (!FELUCCA_FM4 && (i == P_HVEL || i == P_HTIM || i == P_HGAT || i == P_HRQ)) ||   /* (0.6.87) HUMAN: the sequencer's, a sound
                                                         * keeps it (FELUCCA_FM4: DIGITAL's OP SUS / LEVEL there) */
           (!FELUCCA_FM4 && (i == P_CHRD || i == P_CVOI || i == P_CSTR || i == P_CLEN || i == P_TCHAN));   /* (0.7.0)
                                                         * SCL > CHORD, C.LEN, CHANCE: the sequencer's too (a community
                                                         * report: browsing presets switched CHORD off); (V10.6i) MUTATE's
                                                         * old slots are ENV 2's now, (VEL) its P_MCFG the sound's VEL: a
                                                         * sound's, set by its preset */
}

/* a stored value as the parameter takes it (projects, user presets, motion): inside d's range. FM6's SLOT is never
 * stored as a bank slot (JIANT 0.6.11, from Felucca 1.4.1): 9..40 (B1..B32) is 8, OWN, and the patch comes with what is
 * loaded */
static int32_t param_fit(const param_desc_t *d, int32_t v)
{
    v = clamp(v, d->min, d->max);
    return d->names == N_FM6_PATCH && v > (int32_t)FM6_OWN ? (int32_t)FM6_OWN : v;
}

/* v is a value a knob passes over: an alias, or one of FM6's bank slots without a voice */
static int enum_skip(const param_desc_t *d, int32_t v)
{
    return enum_orig(d, v) != v || (d->names == N_FM6_PATCH && !fm6_slot_ok(v));
}
/* a knob moved an F_ENUM from `from` to v: past any value it skips in that direction; with none left to the end, the
 * last one it does not skip on the way from `from` (else `from`) */
static int32_t enum_step(const param_desc_t *d, int32_t from, int32_t v)
{
    int32_t dir = v > from ? 1 : -1, w = v;
    while (w != from && enum_skip(d, w))
        w = w + dir > d->max || w + dir < d->min ? from : w + dir;
    while (w == from && v != from && enum_skip(d, v))
        v -= dir;
    return w == from ? v : w;
}

/* #48: the note divisions in the order of their length, longest first (triplets between their neighbours), on the
 * knobs and the gauges; the stored values (N_DIV, N_SLDIV indices: projects, presets, the editor protocol) stay.
 * -> the shown order of d's values (index: position, entry: value), 0 = the values' own order */
static const uint8_t DIV_ORDER[10] = {9, 8, 7, 6, 0, 1, 4, 2, 5, 3};   /* 4BAR 2BAR 1/1 1/2 1/4 1/8 8T 1/16 16T 1/32 */
static const uint8_t SLDIV_ORDER[6] = {0, 3, 1, 4, 2, 5};              /* 1/8 8T 1/16 16T 1/32 32T */
static const uint8_t DTIME_ORDER[NELEM(N_DTIME)] = {9, 8, 7, 6, DT_DOT + 2u, 0, DT_DOT, 1, DT_DOT + 1u, 4, 2, 5, 3, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
                                                 58, 59, 60, 61, 62
#if DLY_LEN >= 65536u
                                                 , 63, 64, 65, 66, 67, 68, 69
#else
                                                 , 63
#endif
                                                 };   /* the divisions as DIV_ORDER,
                                                         * then the free times, short to long */
static const uint8_t *enum_order(const param_desc_t *d)
{
    return d->names == N_DIV ? DIV_ORDER : d->names == N_SLDIV ? SLDIV_ORDER : d->names == N_DTIME ? DTIME_ORDER :
           0;
}
static int32_t enum_rank(const param_desc_t *d, int32_t v)   /* v's place in the shown order (+ min): the gauges */
{
    const uint8_t *o = enum_order(d);
    int32_t r;
    if (d->fmt == F_PCTINV) return d->max + d->min - v;   /* (the gauge full at 0) */
    if (d == &VCONS_DESC) return (int32_t)vcons_rank(v);   /* (V10.6y: its place, core.h) */
    for (r = 0; o && r < d->max - d->min; r++)
        if (o[r] == v - d->min)
            break;
    return o ? r + d->min : v;
}
/* a knob turned `steps` (signed) from v: clamped, over the shown order, past aliases (enum_step) */
/* (p1, main 0.9.16) a value with names (F_INT, its range split evenly over them: engine desc hooks): its name's index,
 * *k the names */
static uint32_t name_of(const param_desc_t *d, int32_t v, uint32_t *k)
{
    uint32_t n = 0;
    while (d->names[n])
        n++;
    *k = n;
    return (uint32_t)(clamp(v, d->min, d->max) - d->min) * n / (uint32_t)(d->max - d->min + 1);
}
/* (V10.27) the LFO's RATE turned: Hz down to its slowest, then 16BAR 8BAR (stored -14 -13: appended), 4BAR .. 1/32 */
static int32_t lrate_rank(int32_t v) { return v >= 0 ? v + 14 : v >= -12 ? v + 12 : v == -13 ? 12 : 13; }
static int32_t lrate_at(int32_t r) { return r >= 14 ? r - 14 : r <= 11 ? r - 12 : r == 12 ? -13 : -14; }
static int32_t param_turn(const param_desc_t *d, int32_t v, int32_t steps)
{
    if (d == &TP[P_LRATE])
        return lrate_at(clamp(lrate_rank(v) + steps, 0, 141));
    const uint8_t *o = enum_order(d);
    uint32_t k, i, w = (uint32_t)(d->max - d->min + 1);
    if (d->fmt == F_PCTINV) steps = -steps;         /* (right: more chance, less skipped) */
    if (d == &VCONS_DESC)                           /* (V10.6y) CONS: over its places, 127 (100 %) at 32 */
        return (clamp((int32_t)vcons_rank(v) + steps, 0, 127) + 95) & 127;
    if (o)
        return o[clamp(enum_rank(d, v) - d->min + steps, 0, d->max - d->min)] + d->min;
    if (d->fmt == F_INT && d->names && (i = name_of(d, v, &k), k < w)) {   /* (p1, main 0.9.16) a name a detent (VOICE's
                                                         * THEME was 16 detents a name), onto its middle */
        i = (uint32_t)clamp((int32_t)i + steps, 0, (int32_t)k - 1);
        return d->min + (int32_t)((2u * i + 1u) * w / (2u * k));
    }
    return enum_step(d, v, clamp(v + steps, d->min, d->max));
}

static uint8_t fmt_named;        /* the last param_format was a name (F_ENUM, even "4"): ui_draw.c's digits do not roll it */

/* value string (<= 5 chars) and unit for a parameter value */
/* (V10.38, the user: a random INT out of tune spoils the song) an interval chosen at random (GEN, the sound dice):
 * unison, fourth, fifth or octave, up or down, within the parameter's range; semi_snap: the nearest of them */
static const int8_t SEMI_OK[9] = {0, 7, 12, -12, 5, -5, -7, 0, 7};
static int32_t semi_pick(uint32_t r, int32_t lo, int32_t hi)
{
    int32_t v = SEMI_OK[r % 9u];
    return v < lo || v > hi ? 0 : v;
}
static int32_t semi_snap(int32_t v)
{
    static const int8_t C[11] = {0, 5, -5, 7, -7, 12, -12, 19, -19, 24, -24};   /* (19: GLASS KEYS' octave and fifth) */
    uint32_t i;
    int32_t best = 0;
    for (i = 1; i < 11u; i++)
        if ((C[i] - v) * (C[i] - v) < (best - v) * (best - v))
            best = C[i];
    return best;
}
static void param_format(const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    *unit = "";
    fmt_named = d->fmt == F_ENUM;
    if (d == &FMT_WDESC[0] && v <= 0) {                 /* (0.8.5) VOICE's words: POS 0, each note the next */
        str_cpy(val, "AUTO", 6);
        fmt_named = 1;
        return;
    }
    if (d == &GP[G_CLOCK] && v == 1) {                  /* (V10.37) CLOCK USB+BT on a card: USB, +BT as its unit */
        str_cpy(val, "USB", 6);
        *unit = "+BT";
        fmt_named = 1;
        return;
    }
    if (d == &TP[P_LPHASE] && v < 0) {                  /* (0.6.51) the LFO's PHASE FREE */
        str_cpy(val, "FREE", 6);
        fmt_named = 1;
        return;
    }
    if (d->unit == U_REG64) {                           /* (V10.6d) ANIMAL's SIZE: semitones from the key */
        static const param_desc_t SEMI = {0, F_SEMI, 0, 0, 0, 0, 0};
        d = &SEMI;
        v = 64 - v;
    }
    if (d == &VCONS_DESC) {                             /* (V10.6y) VOICE's CONS: 50 .. 400 % */
        fmt_int(val, (int32_t)vcons_q6(v) * 100 / 64);
        *unit = "%";
        return;
    }
    switch (d->fmt) {
    case F_PCT:                                     /* a 0..100 range (SWG) is its value, others a share of 127 */
        fmt_int(val, d->max == 100 || d->max == 120 ? v : (v * 100 + 63) / 127);   /* (0.6.86: DLY FEEDBK 0..120 %, 100 the
                                                                              * loop's gain 1) */
        *unit = "%";
        break;
    case F_PCTINV:
        fmt_int(val, ((d->max - v) * 100 + 63) / 127);
        *unit = "%";
        break;
    case F_BIPCT:
        fmt_int(val, v * 100 / 64);
        if (v > 0) {
            char t[8];
            fmt_int(t, v * 100 / 64);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "%";
        break;
    case F_TIME: {
        uint32_t ms10 = TIME_MS_X10_AT(v);
        if (ms10 < 100u) {
            fmt_fix(val, (int32_t)ms10, 1);
            *unit = "ms";
        } else if (ms10 < 10000u) {
            fmt_int(val, (int32_t)((ms10 + 5u) / 10u));
            *unit = "ms";
        } else {
            fmt_fix(val, (int32_t)(ms10 / 100u), 2);
            if (ms10 >= 100000u)
                fmt_fix(val, (int32_t)(ms10 / 1000u), 1);
            *unit = "s";
        }
        break;
    }
    case F_LFOHZ: {
        uint32_t h = LFO_HZ_X100[v & 127];
        if (v < 0) {                                    /* (0.6.51) the LFO in time */
            str_cpy(val, N_LSYNC[(uint32_t)(-v - 1) % 14u], 6);
            break;
        }
        if (h < 1000u)
            fmt_fix(val, (int32_t)h, 2);
        else
            fmt_fix(val, (int32_t)(h / 10u), 1);
        *unit = "Hz";
        break;
    }
    case F_CUTOFF: {
        uint32_t h = CUTOFF_HZ[v & 127];
        if (h < 1000u) {
            fmt_int(val, (int32_t)h);
            *unit = "Hz";
        } else {
            fmt_fix(val, (int32_t)(h / 100u), 1);
            *unit = "kHz";
        }
        break;
    }
    case F_DB:
        if (v <= 0) {
            str_cpy(val, "OFF", 6);
        } else {
            fmt_fix(val, LEVEL_DB_X10_AT(v), 1);
            *unit = "dB";
        }
        break;
    case F_SEMI:
        fmt_int(val, v);
        if (v > 0) {
            char t[8];
            fmt_int(t, v);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "st";
        break;
    case F_ENUM:
        str_cpy(val, d->names[v < d->min ? d->min : v > d->max ? d->max : v], 6);
        if (d->unit)
            *unit = d->unit;
        break;
    case F_BPM:
        fmt_int(val, v);
        *unit = "BPM";
        break;
    case F_NOTE:
        str_cpy(val, N_NOTE[v % 12], 6);
        break;
    case F_ONOFF:
        str_cpy(val, N_ONOFF[v ? 1 : 0], 6);
        break;
    case F_STEPS:
        fmt_int(val, v);
        *unit = "STEP";
        break;
    default:
        if (d->names) {                               /* F_INT with a 0-terminated name list: the range */
            uint32_t k;                               /* split evenly over the names (engine desc hooks) */
            str_cpy(val, d->names[name_of(d, v, &k)], 6);
        } else {
            fmt_int(val, v);
        }
        if (d->unit)
            *unit = d->unit;
        break;
    }
}

/* ------------------------------------------------------------ pages --- */
enum { FAM_HOME, FAM_ENV, FAM_LFO, FAM_FX, FAM_SCL, FAM_EDIT, FAM_GLO, FAM_SAVE, FAM_ARP, FAM_SEQ, FAM_TRK,
       FAM_COUNT };
enum { SC_TRACK, SC_GLOBAL, SC_ENGINE, SC_STEP, SC_TRK };   /* SC_TRK: the TRACKS page (ui_input.c tracks_edit) */
enum { GR_NONE, GR_ADSR, GR_LFO, GR_STEPS, GR_ARP, GR_SCALE, GR_FX, GR_ROLL, GR_BROWSE, GR_SLOTS, GR_USER, GR_TRK,
       GR_SLCR, GR_MOD, GR_PATS, GR_SONG, GR_TOOLS, GR_CHANCE, GR_MOTION, GR_EVENTS, GR_DXSND, GR_MSEQ, GR_SLPIT,
       GR_FMOP, GR_FMOP2, GR_FMENV,
       GR_COMP, GR_EUCLID, GR_MORPH, GR_DXIMG, GR_FUGUE, GR_SHAPE, GR_PUNCH, GR_BUSES, GR_FOCUS, GR_STAGE,
       GR_LYRIC, GR_GCOMP, GR_LIVE, GR_SCENE, GR_SONGSAVE, GR_SPLIT, GR_SPLIT2, GR_LYRSTEP, GR_TRACKGEN, GR_SWARM };   /* append only */   /* (0.8.5:
                                                         * LENTE's EDIT > FOCUS, ENV > STAGE) */   /* (0.6.99) FX > PUNCH: the punch-in lane */   /* (JIANT 0.6.39) SEQ > COMP: the step components (ui_input.c comp_*, ui_graph.c graph_comp) */   /* (JIANT 0.6.10, from Felucca 1.5: FM6's operator pages, ui_fm6op.c) */

typedef struct {
    const char *title;
    uint8_t fam, scope, graph;
    uint8_t id[4];               /* param ids; 0xFF = empty slot */
} page_t;

/* (0.6.18) GLO > SYSTEM's TRKS: how many tracks play, the setting of MENU > AUDIO > TRACKS (ui.c ui_rec_prefs bit 7,
 * kept with the settings, not in the song). An id past the globals (no song.g of its own): trks_ui mirrors the setting
 * (ui.c rp_apply), a turn writes it back (ui_input.c knob_turn) */
#define G_TRKS 0xFEu
static const char *const N_TRKS[] = {"4", "6"};
static const param_desc_t TRKS_DESC = PE("TRACKS", N_TRKS, 0);
static int16_t trks_ui;
/* (0.6.30) .. and LOAD, next to it (ui.c ui_load, voice.c sys_load): the heavy parts lightened or gone while they
 * sound, ECO the lightest. load_ui mirrors it as trks_ui does */
#define G_LOADP 0xFDu
/* (0.7.6) a track page's slot holding the global ALL TR (G_STRN): SCL's KNOB 4, where TRANS was (page_desc) */
#define ID_ALLTR 0xFCu
static const char *const N_LOAD[] = {"ECO", "LITE", "FULL"};
static const param_desc_t LOAD_DESC = PE("LOAD", N_LOAD, 2);
static int16_t load_ui;
/* (0.9.18) the LFO's MODE TRIG / FREE (LFO KNOB 3) and its PHASE (LFO 2 KNOB 1): views of P_LPHASE, whose -1 has been FREE
 * since 0.6.51 (stored as ever: every project and preset opens as it was). page_desc reads them into lmode_ui / lphase_ui,
 * a turn writes P_LPHASE back (ui_input.c lfo_view_put). FREE keeps no phase: the one TRIG had stays in lfo_ph_mem
 * (RAM, a track each) for TRIG again, and PHASE turned in FREE moves only that (shown dimmed) */
#define ID_LMODE 0xFBu
#define ID_LPHS 0xFAu
static const char *const N_LMODE[] = {"TRIG", "FREE"};
static const param_desc_t LFO_MODE_DESC = PE("MODE", N_LMODE, 0);
static const param_desc_t LFO_PHASE_DESC = PD("PHASE", F_INT, 0, 127, 0);
static int16_t lmode_ui, lphase_ui;
/* (0.9.9-snd8) the master's TAPE (FX > TAPE): its DRIVE and WOW, 0..100 (fx.c tape_block). Not song globals: song_t's size
 * is the sound's (its fields' offsets in the recorded code); a project keeps them (project.c 'T' 'P') */
#define ID_TAPE 0xF8u
#define ID_TWOW 0xF9u
static const param_desc_t TAPE_DESC __attribute__((section(".rodata.tape"))) = PD("TAPE", F_PCT, 0, 100, 0);
static const param_desc_t TWOW_DESC __attribute__((section(".rodata.tape"))) = PD("WOW", F_PCT, 0, 100, 0);   /* (their own
                                                         * section: out of the merged constants, whose places are recorded) */
static int16_t tape_v[2] __attribute__((section(".bss.tape")));
/* (p9, the owner: "a light 4-band EQ on the master, to refine the song") FX > EQ: LOW (under ~200 Hz), LO MID (200 Hz ..
 * 1 kHz), HI MID (1 .. 4 kHz), HIGH (over 4 kHz), -12 .. +12 dB each (fx.c eq_block; all 0: not run). Kept with the
 * project ('E' 'Q', project.c proj_eq) */
#define ID_EQ0 0xE0u
static const param_desc_t EQ_DESC[4] __attribute__((section(".rodata.tape"))) = {
    PD("LOW", F_INT, -12, 12, 0), PD("LO MID", F_INT, -12, 12, 0), PD("HI MID", F_INT, -12, 12, 0), PD("HIGH", F_INT, -12, 12, 0)};
static int16_t eq_v[4] __attribute__((section(".bss.tape")));   /* (out of the merged statics, as fx.c's djf) */
static uint8_t lfo_ph_mem[NTRK];
static int lfo_mode_of(const track_t *t) { return t->p[P_LPHASE] < 0; }   /* 1 FREE */
static const page_t PAGES[] = {
    {"ENV", FAM_ENV, SC_TRACK, GR_ADSR, {P_ATK, P_DEC, P_SUS, P_REL}},
    {"ENV DEST", FAM_ENV, SC_TRACK, GR_NONE, {P_ED_FLT, P_ED_PIT, P_ED_SHP, P_ELOOP}},   /* JIANT 0.4: LOOP */
    {"LFO", FAM_LFO, SC_TRACK, GR_LFO, {P_LRATE, P_LWAVE, ID_LMODE, P_LFADE}},   /* (0.9.18: MODE where PHASE was) */
    {"LFO DEST", FAM_LFO, SC_TRACK, GR_NONE, {P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP}},
    {"MOD", FAM_LFO, SC_TRACK, GR_MOD, {0xFF, P_M1SRC, P_M1DST, P_M1AMT}},   /* KNOB 1: the slot (mod_ui_slot) */
    {"MSEQ", FAM_LFO, SC_TRACK, GR_MSEQ, {0xFF, P_MS0, P_MSLEN, P_MSSLW}},   /* JIANT: KNOB 1 the step (ms_ui_step) */
    {"MSEQ 2", FAM_LFO, SC_TRACK, GR_NONE, {P_MSDIV, 0xFF, 0xFF, 0xFF}},
    {"FX", FAM_FX, SC_TRACK, GR_FX, {P_DIST, P_CHOR, P_DLY, P_REV}},
    {"MASTER", FAM_FX, SC_GLOBAL, GR_NONE, {G_CLIP, G_PUNCH, G_DUCK, G_DREL}},   /* JIANT: clipper, drum bus, ducking;
                                                         * (0.7.6) FX's second page, at hand (was after SL PITCH) */
    {"TAPE", FAM_FX, SC_GLOBAL, GR_NONE, {ID_TAPE, ID_TWOW, 0xFF, 0xFF}},
    {"EQ", FAM_FX, SC_GLOBAL, GR_NONE, {ID_EQ0, ID_EQ0 + 1u, ID_EQ0 + 2u, ID_EQ0 + 3u}},   /* (p9) the master's EQ, last */   /* (0.9.9-snd8) the master's tape, after MASTER */
    {"DIST", FAM_FX, SC_TRACK, GR_NONE, {P_DIST, P_DTYPE, P_DTONE, 0xFF}},   /* JIANT: the drive's type and tone */
    {"AUTO PAN", FAM_FX, SC_TRACK, GR_NONE, {P_APDEP, P_APRATE, P_APSYNC, P_APWAVE}},   /* (JIANT 0.6.9) the track's pan swung */
    {"BUSES", FAM_FX, SC_GLOBAL, GR_BUSES, {G_DMIX, G_RSIZE, G_CDEPTH, G_SLDEP}},   /* (0.8.5) FX's lens: the buses' main values */
    {"DLY", FAM_FX, SC_GLOBAL, GR_NONE, {G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX}},
    {"DLY 2", FAM_FX, SC_GLOBAL, GR_NONE, {G_WIDTH, G_DPIT, G_DSPRY, G_DMOD}},
    {"DLY 3", FAM_FX, SC_GLOBAL, GR_NONE, {G_DTYPE, G_DPING, G_DDRV, G_DRTIM}},   /* (0.6.90) ANALOG / DIGITAL, PING-PONG */   /* JIANT: width, the grains (0.4: the low cut
                                                                                 * in DLY's TONE; G_DHPF stored, unused) */
    {"REVERB", FAM_FX, SC_GLOBAL, GR_NONE, {G_RTYPE, G_RSIZE, G_RDAMP, G_RPRE}},   /* TYPE: ROOM / SPRING */
    {"REVERB 2", FAM_FX, SC_GLOBAL, GR_NONE, {G_RMOD, G_RRATE, G_RFILT, G_RWIDE}},   /* JIANT: modulation, tone, width */
    {"CHORUS", FAM_FX, SC_GLOBAL, GR_NONE, {G_CRATE, G_CDEPTH, G_CSTER, 0xFF}},   /* (0.6.9: STEREO) */
    /* (0.6.81) the SLICER after the buses, before MASTER: DELAY, REVERB and CHORUS nearer at hand */
    {"SLICER", FAM_FX, SC_GLOBAL, GR_SLCR, {G_SLMODE, G_SLPAT, G_SLRATE, G_SLDEP}},   /* (0.6.3) one, on a bus */
    {"SL BUS", FAM_FX, SC_GLOBAL, GR_SLCR, {G_SLT1, G_SLT1 + 1, G_SLT1 + 2, G_SLT1 + 3}},   /* what goes in */
    {"SL BUS 2", FAM_FX, SC_GLOBAL, GR_SLCR, {G_SLT1 + 4, G_SLT6, G_SLFX, 0xFF}},
    {"SL PITCH", FAM_FX, SC_GLOBAL, GR_SLPIT, {0xFF, G_SLP0, G_SLPLEN, G_SLPQ}},   /* KNOB 1 the step (sl_ui_step) */
    {"PUNCH", FAM_FX, SC_GLOBAL, GR_PUNCH, {G_PFXLEN, G_PFXRES, 0xFF, 0xFF}},   /* (0.6.99) the punch-in lane: LEN RES, REC, CLEAR */
    {"SCL", FAM_SCL, SC_TRACK, GR_SCALE, {P_ROOT, P_SCALE, P_QUANT, ID_ALLTR}},   /* (JIANT: ROOT, SCALE every melodic track's;
                                                         * 0.7.6: ALL TR, the global, where TRANS was: SCL 2 gone) */
    {"CHORD", FAM_SCL, SC_TRACK, GR_NONE, {P_CHRD, P_CVOI, P_CSTR, 0xFF}},   /* (0.6.81) a key plays a chord (seq.c) */
    {"SCENE", FAM_SCL, SC_TRACK, GR_SCENE, {ID_SC0, ID_SC0 + 1u, ID_SC0 + 2u, ID_SC0 + 3u}},   /* (V10.25) the multitimbral
                                                         * mode: SCENE, TRACK, ROLE, CHORD (ui_scene.c) */
    {"EDIT 1", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E0, P_E1, P_E2, P_E3}},
    {"EDIT 2", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E4, P_E5, P_E6, P_E7}},
    {"WORDS", FAM_EDIT, SC_TRACK, GR_LYRIC, {P_VLANG, P_VLYR, P_VCONS, 0xFF}},   /* (0.8.5) VOICE: LANG LYRIC, (V10.6y) CONS
                                                         * where POS was (POS: EDIT 1's KNOB 1, LANG on), OCT+ TYPE
                                                         * (the lyric on the NAME screen, ui_name.c) */
    {"RETRIG", FAM_EDIT, SC_TRACK, GR_NONE, {P_LRTRG, 0xFF, 0xFF, 0xFF}},
    {"MORPH", FAM_EDIT, SC_TRACK, GR_MORPH, {P_MORPH, P_DRIFT, 0xFF, 0xFF}},   /* (0.6.45) ANALOG PHASE LOFI: the wave morphs;
                                                         * (0.7.8) DRIFT: the voices wander */
    {"ROTOR", FAM_EDIT, SC_TRACK, GR_NONE, {P_WRDEP, P_WDUST, 0xFF, 0xFF}},   /* (0.7.8) WHEEL: the rotor's depth, DUST */
    /* (JIANT 0.6.54) DRUM: the whole kit first (KIT = EDIT 1, KIT 2 = EDIT 2, KIT 3, X-MOD, FILTER), then a sound
     * (SOUND 1..4, KNOB 1 the sound on each), (0.6.58) then the volumes (LEVELS, LEVELS 2); (0.7.8) STEREO after X-MOD */
    {"KIT 3", FAM_EDIT, SC_TRACK, GR_DXIMG, {0xFF, 0xFF, 0xFF, 0xFF}},     /* TONE's BP and FEEDBK (the kit's) */
    {"KIT 4", FAM_EDIT, SC_TRACK, GR_DXIMG, {0xFF, 0xFF, 0xFF, 0xFF}}, /* KEY MAP (KNOB 2; V10.6o: ACOUST gone) */
    {"X-MOD", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},     /* (JIANT 0.5.1) the kit's LFO on MORPH, RAND */
    {"STEREO", FAM_EDIT, SC_TRACK, GR_DXIMG, {P_DXIMG, 0xFF, 0xFF, 0xFF}}, /* (0.6.58) IMAGE: the kit's stereo placings; (0.7.8)
                                                         * with the kit's pages, before the sounds (the volumes last) */
    {"STEREO", FAM_EDIT, SC_TRACK, GR_DXIMG, {P_SIMG, 0xFF, 0xFF, 0xFF}},   /* (V10.49) a synth's: IMAGE, the voices across (POLY) */
    {"OPERATOR", FAM_EDIT, SC_TRACK, GR_FMOP, {0xFF, 0xFF, 0xFF, 0xFF}},    /* FM6 only: KNOB 1 the operator, then its */
    {"OP ENV", FAM_EDIT, SC_TRACK, GR_FMENV, {0xFF, 0xFF, 0xFF, 0xFF}},     /* bytes of the track's patch (ui_fm6op.c): */
    {"OPERATOR 2", FAM_EDIT, SC_TRACK, GR_FMOP2, {0xFF, 0xFF, 0xFF, 0xFF}}, /* CRS FINE LEVEL / STG RATE LEVEL / MODE DTUN VEL */
    {"FILTER", FAM_EDIT, SC_TRACK, GR_NONE, {P_FTYPE, P_E4, P_E5, P_E6}},   /* (JIANT 0.5) ANALOG: TYPE CUT RES DRV */
    {"FILTER", FAM_EDIT, SC_TRACK, GR_NONE, {P_FTYPE, P_FCUT, P_FRES, P_DIST}},   /* .. every other engine's (on the part;
                                                         * DRV: the part's DIST, after it) */
    {"SOUND", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},   /* DRUM: a sound of the kit (ui_dx.c) */
    {"SOUND 2", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"SOUND 3", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (JIANT 0.4) MODE, the pitch modulation, DRIVE */
    {"SOUND 4", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (JIANT 0.6.54) LAYER, FEEDBK, SEND */
    {"LEVELS", FAM_EDIT, SC_TRACK, GR_NONE, {P_LN0, P_LN1, P_LN2, P_LN3}},   /* DRUM only: the sounds' levels (was LANES) */
    {"LEVELS 2", FAM_EDIT, SC_TRACK, GR_NONE, {P_LN4, P_LN5, P_LN6, P_LN7}},
    {"OP1 ENV", FAM_EDIT, SC_TRACK, GR_ADSR, {P_FM1_ATK, P_FM1_DEC, P_FM1_SUS, P_FM1_REL}},
    {"OP2 ENV", FAM_EDIT, SC_TRACK, GR_ADSR, {P_FM2_ATK, P_FM2_DEC, P_FM2_SUS, P_FM2_REL}},
    {"OP3 ENV", FAM_EDIT, SC_TRACK, GR_ADSR, {P_FM3_ATK, P_FM3_DEC, P_FM3_SUS, P_FM3_REL}},
    {"OP4 ENV", FAM_EDIT, SC_TRACK, GR_ADSR, {P_FM4_ATK, P_FM4_DEC, P_FM4_SUS, P_FM4_REL}},
    {"OP LEVEL", FAM_EDIT, SC_TRACK, GR_NONE, {P_FM1_LEVEL, P_FM2_LEVEL, P_FM3_LEVEL, P_FM4_LEVEL}},
    {"VOICE", FAM_EDIT, SC_TRACK, GR_NONE, {P_VOICE, P_GLIDE, P_GLMODE, P_PRIO}},
    {"GLOBAL", FAM_GLO, SC_GLOBAL, GR_NONE, {G_BPM, G_SWING, G_CLOCK, G_TUNE}},
    {"SYSTEM", FAM_GLO, SC_GLOBAL, GR_NONE, {G_TRKS, G_LOADP, G_ROUTE, G_INFO}},   /* (0.6.18) TRACKS 4 / 6; (0.6.30)
                                                         * LOAD ECO / LITE / FULL (the USB card: the header's icon) */
    {"SYSTEM 2", FAM_GLO, SC_GLOBAL, GR_NONE, {G_MIDI, G_MIDI, G_MIDI, 0xFF}},   /* (V10.37) KNOB 3 Bluetooth's state */   /* (0.9.9-snd8) MIDI IN at a glance (ui_draw.c):
                                                         * KNOB 1 USB's state OFF WAIT BUS ENUM MIDI, KNOB 2 the TRS messages come in */
    {"KEY SPLIT", FAM_GLO, SC_GLOBAL, GR_SPLIT, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"KEY SPLIT 2", FAM_GLO, SC_GLOBAL, GR_SPLIT2, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"LYRIC STEP", FAM_SEQ, SC_TRACK, GR_LYRSTEP, {0xFF,0xFF,0xFF,0xFF}},
    {"GENERATE", FAM_SEQ, SC_TRACK, GR_TRACKGEN, {0xFF,0xFF,0xFF,0xFF}},
    {"COMPS", FAM_GLO, SC_GLOBAL, GR_GCOMP, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.9.9-seq2f) the global lane's step: ACTION
                                                         * LENGTH CONDITION VALUE (ui_input.c gco_knob) */
    {"COMPS 2", FAM_GLO, SC_GLOBAL, GR_GCOMP, {0xFF, 0xFF, 0xFF, 0xFF}},   /* .. its TRACKS, MONO / POLY */
    {"PRESETS", FAM_SAVE, SC_GLOBAL, GR_BROWSE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* browser: PRESETS knob / KNOB 1 */
    {"USER", FAM_SAVE, SC_GLOBAL, GR_USER, {0xFF, 0xFF, 0xFF, 0xFF}},       /* user presets: SLOT LOAD ERASE SAVE */
    {"PROJECT", FAM_SAVE, SC_GLOBAL, GR_SLOTS, {0xFF, G_SLOT, G_LOAD, G_SAVE}},   /* (0.6.12) PROJECT 1-6 (V10.6p; 1-8 before), SECT A-D */
    {"ARRANGE", FAM_SAVE, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.12) the project's song (SEQ > SONG) */
    {"TOOLS", FAM_SAVE, SC_GLOBAL, GR_TOOLS, {G_CLRSEQ, G_INITSND, 0xFF, 0xFF}},
    {"ARP", FAM_ARP, SC_TRACK, GR_ARP, {P_AMODE, P_ARATE, P_AOCT, P_AGATE}},
    {"ARP 2", FAM_ARP, SC_TRACK, GR_NONE, {P_ASWING, P_APROB, P_AHOLD, P_AORDER}},
    /* (0.6.90) SEQ: STEP (the piano roll) and the pages that edit it, one after another: NOTE, TIMING, STRETCH, COMP,
     * COMP 2, WHEN; then the track's: PATTERN, SHIFT, HUMAN, PLAYHEADS, AUTOMATION, AUTO LIST, MUTATE; PHRASES, SONG and
     * CHANCE out of the cycle (ui.c page_visible) */
    /* (0.6.92) eleven pages, was fifteen: the piano roll, the pattern, the roll's edits (NOTE: FOCUS PITCH MOVE MICRO,
     * TIMING folded in), STRETCH (on the turn), the step's two components (EVERY on them: WHEN and TRIG's loops in
     * one), TRACK (SHIFT's PITCH and CHANCE, HUMAN as one amount, REC Q; C.LEN retired from the pages), then the rest */
    {"STEP", FAM_SEQ, SC_STEP, GR_ROLL, {0, 1, 2, 3}},
    {"PATTERN", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},
    {"NOTE", FAM_SEQ, SC_STEP, GR_ROLL, {4, 5, 6, 9}},   /* (0.6.81) the piano roll's edits: FOCUS PITCH MOVE; (0.6.92) MICRO */
    {"STRETCH", FAM_SEQ, SC_TRACK, GR_SHAPE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.85) STRETCH REPEAT NUDGE (0.6.92: on the turn) ROTATE */
    {"COMP", FAM_SEQ, SC_STEP, GR_COMP, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.39) STEP TYPE and its two values */
    {"COMP 2", FAM_SEQ, SC_STEP, GR_COMP, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.82) the step's second component, EVERY */
    {"PLAYHEADS", FAM_SEQ, SC_TRACK, GR_FUGUE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.52) three more playheads (fugue.c; was FUGUE) */
    /* (0.8.2) the pages above edit the piano roll and keep it in the panel (ui_graph.c roll_view); TRACK and the ones
     * after are the track's own, with their own graphs. (0.8.3) LENTE (ui.c lens_*) groups them by level, the table
     * as it was: PATTERN (PATTERN STRETCH PLAYHEADS TRACK MUTATE), BAR (STEP NOTE AUTOMATION AUTO LIST), STEP (NOTE
     * COMP COMP 2); SELECT zooms, a SEQ tap the level's next set */
    {"TRACK", FAM_SEQ, SC_TRACK, GR_STEPS, {P_POFS, P_TCHAN, P_HVEL, P_HRQ}},   /* (0.6.92) PITCH CHANCE HUMAN REC Q */
    {"PHRASES", FAM_SEQ, SC_GLOBAL, GR_PATS, {0xFF, 0xFF, 0xFF, 0xFF}},    /* pattern loader (ui.c pat_load); 0.6.37: hidden */
    {"MIXER", FAM_TRK, SC_TRK, GR_TRK, {0, 1, 2, 3}},   /* GLO button; LEVEL PAN REV, (0.9.1) the DJ FILTER (was MUTE) */
    {"SONG", FAM_SEQ, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"CHANCE", FAM_SEQ, SC_STEP, GR_CHANCE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* a step's CHANCE RATCH; 0.6.37: hidden */
    {"AUTOMATION", FAM_SEQ, SC_TRACK, GR_MOTION, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"AUTO LIST", FAM_SEQ, SC_TRACK, GR_EVENTS, {0xFF, 0xFF, 0xFF, 0xFF}},   /* 1.1.5: the locks and events, edited */
    {"EUCLID", FAM_SEQ, SC_TRACK, GR_EUCLID, {ID_EU0, ID_EU0 + 1u, ID_EU0 + 2u, ID_EU0 + 3u}},   /* (V10.6, MUTATE's place)
                                                         * HITS STEPS ROTATE, LANE / VARY (ui_euclid.c) */
    /* (0.8.5) LENTE everywhere (ui_lens.c): pages the mock-up added, appended (the table's order is no one's index) */
    {"VOICE 2", FAM_EDIT, SC_TRACK, GR_NONE, {P_ALLOC, P_DETUNE, P_TVEL, 0xFF}},   /* EDIT > TONE: ALLOC, UNISON's DETUNE (in
                                                         * TP since Felucca, on no page until now), (after V10.7.2) VEL */
    {"FOCUS", FAM_EDIT, SC_TRACK, GR_FOCUS, {0xFF, 0xFF, 0xFF, 0xFF}},   /* EDIT's detail on a synth: PARAM (E1..E8), its
                                                         * VALUE, the MOD slot aimed at it: SRC AMT (ui_input.c focus_knob) */
    {"STAGE", FAM_ENV, SC_TRACK, GR_STAGE, {0xFF, 0xFF, P_ELOOP, 0xFF}},   /* ENV's detail: STAGE (ATK DEC SUS REL), its
                                                         * value, LOOP (ui_input.c stage_knob) */
    {"SENDS", FAM_TRK, SC_TRACK, GR_TRK, {P_DIST, P_CHOR, P_DLY, P_REV}},   /* GLO > MIX: the track's sends (as FX > SENDS),
                                                         * the six tracks' as bars (ui_graph.c track_col6) */
    {"BAR", FAM_SEQ, SC_TRACK, GR_STEPS, {0xFF, P_SLEN, 0xFF, 0xFF}},   /* (0.9.2) SEQ > BAR: the pattern's bars, KNOB 1
                                                         * the bar (the cursor at its place in it; was PRESETS on PATTERN's
                                                         * level), KNOB 2 LEN (ui_input.c edit_param) */
    {"MODES", FAM_SEQ, SC_TRACK, GR_EUCLID, {ID_EU0 + 4u, ID_EU0 + 5u, ID_EU0 + 6u, ID_EU0 + 7u}},   /* (V10.6) the four
                                                         * modes' amounts */
    {"LFO 2", FAM_LFO, SC_TRACK, GR_LFO, {ID_LPHS, 0xFF, 0xFF, 0xFF}},   /* (0.9.18) PHASE: where TRIG starts (FREE: dimmed) */
    {"MANO", FAM_GLO, SC_GLOBAL, GR_LIVE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (p2, main 0.9.11) GLO > LIVE: the keys a board of
                                                         * live modifiers (ui_live.c, seq_live.c) */
    {"RIELES", FAM_GLO, SC_GLOBAL, GR_LIVE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* .. the six sequences as rails, four knobs */
    {"TIMELINE", FAM_SAVE, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (add15) the song as lanes (was ARRANGE 2): a row's tracks, the section each
                                                         * plays from (song_chain.c row_src; ui_input.c song_trk_knob) */
    {"MUTATE", FAM_SEQ, SC_TRACK, GR_EUCLID, {ID_MU0, ID_MU0 + 1u, ID_MU0 + 2u, ID_MU0 + 3u}},
    {"ENV 2", FAM_ENV, SC_TRACK, GR_ADSR, {P_E2ATK, P_E2DEC, P_E2SUS, P_E2REL}},   /* (V10.22) a second ADSR, a MOD
                                                         * source (SRC ENV2) */   /* (V10.16) EUCLID's
                                                         * mutations on the bar: EVERY AMOUNT MODE WHAT */
    {"SAVE SONG AS", FAM_SAVE, SC_GLOBAL, GR_SONGSAVE, {0xFF, 0xFF, 0xFF, G_SAVE}},
    {"RHYTHM", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, 0xFF, 0xFF, P_SDIV}},
    {"PARTS", FAM_SAVE, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (V10.6o) the song's four sections as cards: KNOB 1 one,
                                                         * OCT+ it loops, OCT- the song from its row (ui_lzsave.c graph_parts) */
    {"CLIPS", FAM_SAVE, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (V10.6s) tracks x parts: KNOB 1 the track, 2 the part,
                                                         * OCT+ the cell, OCT- the track stops, F3-B3 a part, REC the song
                                                         * (ui_lzsave.c graph_clips, song_main.c clip_launch) */
    {"SWARM", FAM_EDIT, SC_TRACK, GR_SWARM, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (V10.6z6) a SWARM track's FAR / NEAR: SIDE RATE MOVE
                                                         * TONE, (SWARM 2) SIDE NOISE MOD NOW (ui_dx.c sw_ui_*) */
    {"SWARM 2", FAM_EDIT, SC_TRACK, GR_SWARM, {0xFF, 0xFF, 0xFF, 0xFF}},
};
#define SPLIT_PAGE(pg) ((uint32_t)((pg)->graph - GR_SPLIT) < 2u)
#define FIT_PAGE(pg) ((pg)->fam == FAM_SEQ && (pg)->id[0] == P_SLEN && (pg)->id[1] == 0xFFu)
#define BAR_PAGE(pg) ((pg)->graph == GR_STEPS && (pg)->id[0] == 0xFFu)
#define NPAGES (sizeof(PAGES) / sizeof(PAGES[0]))
static uint8_t mod_ui_slot;      /* the MOD page: the matrix slot (0..3) KNOB 2..4 edit */
static uint8_t ms_ui_step;       /* (JIANT) the MSEQ page: the step (0..15) KNOB 2 edits */
static const char *const N_RFOLW[] = {"OFF", "T1", "T2", "T3", "T4", "T5", "T6"};
static const param_desc_t RESO_PITCH = PD("PITCH", F_INT, 0, 14, 0);   /* (0.6.5) RESO: the chord's steps up the scale (G_RPRE) */
static const param_desc_t RESO_FOLW = PE("FOLLOW", N_RFOLW, 0);          /* .. the track whose notes it follows (G_RDAMP) */
static const param_desc_t REV_PRE = PD("PRE", F_INT, 0, 100, 0);       /* ROOM / SPRING: the pre-delay, ms (G_RPRE) */
static const param_desc_t REV_AIR = PD("AIR", F_SEMI, -24, 24, 0);    /* (0.6.12) SHIMMER / CLOUD: pitch and tone, down dark,
                                                                       * up airy (G_RPRE; fx.c rev_air) */
static const param_desc_t AP_BEAT = PE("RATE", N_APDIV, 2);          /* (0.6.9) AUTO PAN's RATE on the tempo (SYNC BEAT) */
static uint8_t sl_ui_step;       /* (JIANT 0.6.3) the SL PITCH page: the step KNOB 2 edits */

/* page_desc's views (ids ID_TAPE .. ID_LMODE): (0.9.18) the LFO's MODE and PHASE, views of P_LPHASE; (0.9.9-snd8) the
 * master's TAPE (noinline: page_desc is recorded code) */
static __attribute__((noinline)) const param_desc_t *page_desc_v(uint32_t id, int16_t **valp)
{
    if (id >= ID_EQ0 && id < ID_EQ0 + 4u) {           /* (p9) the master's EQ */
        *valp = &eq_v[id - ID_EQ0];
        return &EQ_DESC[id - ID_EQ0];
    }
    if (id == ID_TAPE || id == ID_TWOW) {
        *valp = &tape_v[id - ID_TAPE];
        return id == ID_TAPE ? &TAPE_DESC : &TWOW_DESC;
    } else {
        int32_t ph = TSEL->p[P_LPHASE];
        lmode_ui = (int16_t)(ph < 0);
        lphase_ui = (int16_t)(ph < 0 ? lfo_ph_mem[song.sel % NTRK] : ph);
        *valp = id == ID_LMODE ? &lmode_ui : &lphase_ui;
        return id == ID_LMODE ? &LFO_MODE_DESC : &LFO_PHASE_DESC;
    }
}
static const param_desc_t *page_desc(const page_t *pg, uint32_t slot, int16_t **valp)
{
    uint32_t id = pg->id[slot];
    if (id == 0xFFu) {
        *valp = 0;
        return 0;
    }
    if (id == ID_ALLTR) {                             /* (0.7.6) SCL's KNOB 4: the global ALL TR on the track page */
        *valp = &song.g[G_STRN];
        return &GP[G_STRN];
    }
    if (id == G_TRKS) {                               /* (0.6.18) not a song global: the device's setting */
        *valp = &trks_ui;
        return &TRKS_DESC;
    }
    if ((id >= ID_TAPE && id <= ID_LMODE) || (id >= ID_EQ0 && id < ID_EQ0 + 4u))   /* (0.9.18) the LFO's MODE and PHASE;
                                                         * (snd8) the master's TAPE; (p9) its EQ */
        return page_desc_v(id, valp);
    if (id == G_LOADP) {                              /* (0.6.30) the same */
        *valp = &load_ui;
        return &LOAD_DESC;
    }
    if (pg->scope == SC_STEP || pg->scope == SC_TRK) {
        *valp = 0;
        return 0;
    }
    if (pg->scope == SC_GLOBAL) {
        if (pg->graph == GR_SLPIT && id == G_SLP0)    /* the pitch of the step shown */
            id += sl_ui_step & 15u;
        *valp = &song.g[id];
        if (song.g[G_RTYPE] == 3 && (id == G_RPRE || id == G_RDAMP))   /* (0.6.5) RESO: PITCH, FOLW in their slots */
            return id == G_RPRE ? &RESO_PITCH : &RESO_FOLW;
        if (id == G_RPRE)                             /* (0.6.12) SHIMMER / CLOUD: AIR; ROOM / SPRING: PRE */
            return song.g[G_RTYPE] == 2 || song.g[G_RTYPE] == 4 ? &REV_AIR : &REV_PRE;
        return &GP[id];
    }
    if (pg->graph == GR_MOD)                          /* SRC DST AMT of the slot shown */
        id += 3u * mod_ui_slot;
    if (pg->graph == GR_MSEQ && id == P_MS0)          /* the level of the step shown */
        id += ms_ui_step & 15u;
    if (id >= ID_EU0 && id < ID_EU0 + 8u)            /* (V10.6) EUCLID's and MODES' views (ui_euclid.c eu_put) */
        return eu_desc(id - ID_EU0, valp);
    if (id >= ID_MU0 && id < ID_MU0 + 4u)            /* (V10.16) MUTATE's */
        return eu_desc(id - ID_MU0 + 8u, valp);
    if (id >= ID_SC0 && id < ID_SC0 + 4u)            /* (V10.25) SCENE's */
        return sc_desc(id - ID_SC0, valp);
    *valp = &TSEL->p[id];
    if (id == P_APRATE && TSEL->p[P_APSYNC])         /* (0.6.9) AUTO PAN on the tempo: a division */
        return &AP_BEAT;
    return track_desc(TSEL, id);
}


