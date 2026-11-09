/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.9.9 ctl64) where the sequencer's notes land in the rendered audio, against where the sequencer puts them when it
 * runs a sample at a time (seq_tick(t, 1): micro_test's reference): a MICRO-shifted pattern (steps off the grid by
 * 64ths, as a live recording leaves them) on an ANALOG track with an instant attack, rendered through mix_block in
 * blocks of CTL samples. Each note's onset (the master's first sample above the floor after a rest) minus the
 * sequencer's sample for it. With blocks of 32 (before) a note started at its block's first sample: 0 .. 31 samples
 * early or late; now (any CTL) at its own sample: within 2 (the ramp's first sample rounds to 0; the sequencer's
 * sample-at-a-time run is itself a sample behind). Also: the same with the
 * track MONO (each note cuts the one before: the voice restarted in place at the sample), and a DRUM kit's hits.
 *   build/host/ctl64_timing_test [OUT.wav]      (OUT.wav: the POLY render, for listening / plotting) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("ctl64: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define NN 6u
static const int8_t MIC[NN] = {0, 13, -9, 27, -30, 5};     /* each step's MICRO (64ths) */

static void pattern(uint32_t engine, uint32_t mode)
{
    track_t *t = &trk[0];
    uint32_t i;
    ui_power_on();
    song.master_q12 = 4096;
    for (i = 1; i < NTRK; i++) trk[i].p[P_MUTE] = 1;
    set_engine_of(t, engine);
    t->engine = t->eng_req;
    apply_preset_to(t, 0);
    t->p[P_MUTE] = 0; t->p[P_SLEN] = 2 * NN; t->p[P_SDIV] = 2; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
    t->p[P_VOICE] = (int16_t)mode; t->p[P_SGATE] = 40; t->p[P_ATK] = 0; t->p[P_REL] = 10;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0; t->p[P_DIST] = 0;
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    for (i = 0; i < NN; i++) {                          /* a note, then a rest (the onset after silence) */
        step_t *s = &t->step[2 * i];
        s->time = ST_NOTE; s->vel = 100;
        if (engine == ENGI_DRUM) s->hit = (uint8_t)(1u << DV_HATC);   /* (the closed hat: its tail is short) */
        else { s->n = 1; s->note[0] = (uint8_t)(48 + 3 * i); }
        ST_MICRO_SET(s, MIC[i]);
    }
    #ifdef TIMING_FIT
    t->seq_fit = 128u | 8u;  /* 12 steps inside 8 reference steps */
    #endif
    song.g[G_BPM] = 128;
    for (i = 0; i < 200u; i++) { int32_t o[2 * CTL]; mix_block(o, CTL); }   /* (the sound load settled) */
}
/* the sequencer alone, a sample at a time: the sample each of the NN notes starts on */
static void reference(uint32_t engine, uint32_t mode, int32_t *at)
{
    track_t *t = &trk[0];
    uint32_t now, k, v0, n = 0;
    pattern(engine, mode);
    seq_start();
    for (k = 0; k < NN; k++) at[k] = -1;
    for (now = 0; now < 2u * NN * 5168u + 2000u && n < NN; now++) {   /* (a step at 128 BPM 1/16: 5168 samples) */
        v0 = vage;
        seq_tick(t, 1);
        if (vage != v0) at[n++] = (int32_t)now;
    }
    seq_stop();
}
/* rendered in blocks: each note's onset in the track's own render (events_block, then track_render as mix_part does:
 * the dry part, exactly 0 while nothing sounds; the master's leveler and DC blocker would never settle to 0) */
static void rendered(uint32_t engine, uint32_t mode, int32_t *on, const char *wav)
{
    track_t *t = &trk[0];
    static int32_t o[CTL];
    uint32_t b, i, n = 0, zeros = 1000;
    FILE *f = wav ? fopen(wav, "wb") : 0;
    uint32_t frames = 2u * NN * 5168u + 4000u;
    pattern(engine, mode);
    transport_req = 1; ui_input();
    for (i = 0; i < NN; i++) on[i] = -1;
    if (f) wav_hdr(f, (frames / CTL) * CTL);
    for (b = 0; b < frames / CTL; b++) {
        events_block(CTL);
        track_render(t, o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t x = o[i], a = x < 0 ? -x : x;
            if (f) wav_put(f, x >> 2, x >> 2);
            if (a >= 4) {                                   /* (a kit's tail leaves a few LSBs) */
                if (zeros >= 200u && n < NN) on[n++] = (int32_t)(b * CTL + i);   /* (the first sample after a silence) */
                zeros = 0;
            } else
                zeros++;
        }
    }
    if (f) fclose(f);
    transport_req = 2; ui_input();
}
static int32_t worst(const int32_t *ref, const int32_t *on, int32_t *mn, int32_t *mx)
{
    uint32_t k;
    int32_t w = 0;
    *mn = 1 << 30; *mx = -(1 << 30);
    for (k = 0; k < NN; k++) {
        int32_t d = on[k] - ref[k], a = d < 0 ? -d : d;
        if (ref[k] < 0 || on[k] < 0) return 1 << 20;
        if (a > w) w = a;
        if (d < *mn) *mn = d;
        if (d > *mx) *mx = d;
    }
    return w;
}
int main(int argc, char **argv)
{
    static const struct { uint32_t engine, mode; const char *name; int32_t tol; } C[3] = {
        {ENGI_ANALOG, V_POLY, "ANALOG POLY", 2}, {ENGI_ANALOG, V_MONO, "ANALOG MONO (each note cuts the last)", 2},
        {ENGI_DRUM, V_POLY, "DRUM kit hits", 2}};
    int32_t ref[NN], on[NN], mn, mx, w;
    uint32_t c, k;
    char what[200];
    for (c = 0; c < 3u; c++) {
        reference(C[c].engine, C[c].mode, ref);
        rendered(C[c].engine, C[c].mode, on, c == 0u && argc > 1 ? argv[1] : 0);
        for (k = 0; k < NN; k++)                        /* the grid of a run in blocks: the transport's position is 0 at
                                                         * the end of the block PLAY lands in (seq_start), so it sits
                                                         * CTL - 1 samples after the sample-at-a-time run's */
            if (ref[k] >= 0 && k) ref[k] += (int32_t)CTL - 1;   /* (not the first: PLAY's step starts the block) */
        w = worst(ref, on, &mn, &mx);
        printf("ctl64: %s: sequencer at", C[c].name);
        for (k = 0; k < NN; k++) printf(" %d", ref[k]);
        printf("; rendered at");
        for (k = 0; k < NN; k++) printf(" %d", on[k]);
        printf(" (onset - sequencer: %d .. %d samples)\n", mn, mx);
        snprintf(what, sizeof what, "%s: every note's onset within %d sample(s) of the sequencer's (worst %d; blocks of %u)",
                 C[c].name, C[c].tol, w, (unsigned)CTL);
        ck(what, w <= C[c].tol);
    }
    printf(bad ? "ctl64 timing: %d FAILED\n" : "ctl64 timing: all passed\n", bad);
    return bad != 0;
}

