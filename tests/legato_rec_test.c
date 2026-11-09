/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.6z4) the feel of live recording: a performance with real timing (staccato, held notes, legato overlaps, gaps,
 * early and late hits, velocities) played into REC on the real sources (ui_test.c's), then played back; what the steps
 * hold (place, MICRO, ties, LEN, SLIDE, velocity) and what the sequencer plays (MIDI OUT ALL: note-ons and -offs, a
 * sample at a time) against what was played. Two voice modes (LEGATO, POLY) at REC Q 0 % and 100 %.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/legato_rec_test.c -lm
 * Checks: every note into its own (nearest) step; its velocity as played; at REC Q 0 % its start within 1/64 of a step
 * of where it was played (100 %: on the grid); its length within 1/16 of a step (a note held into a legato next one:
 * up to that one); a legato (the next key down before this one is up) on LEGATO played back legato (the next note-on
 * before this note-off: one envelope, glide), on POLY touching (no gap); a detached note (a gap after it) played
 * back with a gap. And GATE: 100 % on new tracks, a project's 50 % kept. LEGATO_REC_VERBOSE=1 prints the table. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("legato: %-104s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
typedef struct { double at, len; uint8_t note, vel; } pl_t;
/* the performance, in steps of 1/16 at 120 BPM (5512.5 samples): note on at `at`, held `len` */
static const pl_t PL[] = {
    {0.00, 0.40, 60, 100},   /* staccato on the grid */
    {1.06, 1.38, 62, 80},    /* late, held over a step (to 2.44) */
    {2.30, 0.95, 64, 120},   /* legato: down before 62 is up (2.30 < 2.44), early for step 2 */
    {3.92, 0.70, 65, 60},    /* a gap before it (3.25 .. 3.92), early for step 4 */
    {5.03, 2.55, 67, 127},   /* long: tied over two steps (to 7.58) */
    {7.42, 0.50, 69, 90},    /* legato out of the long one (down at 7.42, 67 up at 7.58) */
    {9.10, 0.92, 71, 110},   /* late, held almost the step */
    {10.55, 1.80, 72, 45},   /* late past the middle: step 11, early; held to 12.35 */
    {12.25, 0.55, 74, 100},  /* legato again (12.25 < 12.35) */
    {13.60, 0.30, 76, 70},   /* early for step 14, short */
};
#define NPL NELEM(PL)
static uint32_t slen(const track_t *t, uint32_t i) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), i); }
static uint32_t gridp(const track_t *t, uint32_t i)
{
    uint32_t k, p = 0;
    for (k = 0; k < i; k++) p += slen(t, k);
    return p;
}
static uint32_t at_of(const track_t *t, double x)       /* the sample of place x (steps) */
{
    uint32_t i = (uint32_t)x;
    return gridp(t, i) + (uint32_t)((x - i) * slen(t, i) + 0.5);
}
static double place_of(const track_t *t, int32_t s)      /* .. and back */
{
    uint32_t i = 0;
    while (s >= (int32_t)slen(t, i) && i < 63u) { s -= (int32_t)slen(t, i); i++; }
    return i + (double)s / slen(t, i);
}
typedef struct { int32_t on, off, qon, qoff; uint8_t vel; } ev_t;   /* (q: the order they went out in) */
static int32_t evq;
static ev_t PB[128];                                      /* the playback's, by note */
static void drain(uint32_t now)
{
    while (mo_r != mo_w) {
        uint32_t pkt = midi_out_q[mo_r++ % MQ], st = (pkt >> 8) & 0xF0u, n = (pkt >> 16) & 127u, v = pkt >> 24;
        if (st == 0x90u && v) {
            if (PB[n].on < 0) { PB[n].on = (int32_t)now; PB[n].vel = (uint8_t)v; PB[n].qon = evq++; }
        } else if (st == 0x80u || st == 0x90u) {
            if (PB[n].on >= 0 && PB[n].off < 0) { PB[n].off = (int32_t)now; PB[n].qoff = evq++; }
        }
    }
}
static void tick1(track_t *t, uint32_t *now, uint32_t to)
{
    while (*now < to) { seq_tick(t, 1); (*now)++; drain(*now); }
}
static const char *const VM[4] = {"POLY", "MONO", "LEGATO", "UNISON"};
static void take(uint32_t vmode, int32_t hrq)
{
    static const char *const TN[3] = {"NOTE", "TIE", "REST"};
    track_t *t;
    uint32_t now = 0, i, k, ev;
    int verbose = getenv("LEGATO_REC_VERBOSE") != 0;
    int ok_step = 1, ok_vel = 1, ok_start = 1, ok_len = 1, ok_leg = 1, ok_gap = 1;
    double w_start = 0, w_len = 0;
    char what[160];
    uint8_t into[NPL];
    ui_power_on();
    usb.config = 1;
    mo_r = mo_w = 0;
    mo_lvl = MO_ALL;
    t = &trk[0];
    set_engine_of(t, ENGI_ANALOG); t->engine = t->eng_req; apply_preset_to(t, 0);
    track_defaults_steps(t);
    t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_SSWING] = 0; song.g[G_SWING] = 0; song.g[G_BPM] = 120;
    t->p[P_HRQ] = (int16_t)hrq; t->p[P_VOICE] = (int16_t)vmode; t->p[P_AMODE] = 0; t->p[P_GLIDE] = 0;
    for (i = 1; i < NTRK; i++) trk[i].p[P_MUTE] = 1;
    seq_start();
    song.rec = 1;
    for (ev = 0; ev < 2u * NPL; ev++) {                  /* the presses and the releases, in time order */
        uint32_t best = 0, bt = 0xFFFFFFFFu, on = 0;
        static uint8_t done[2 * 16];
        if (!ev) memset(done, 0, sizeof done);
        for (k = 0; k < NPL; k++) {
            uint32_t a = at_of(t, PL[k].at), b = at_of(t, PL[k].at + PL[k].len);
            if (!done[2 * k] && a < bt) { bt = a; best = k; on = 1; }
            if (!done[2 * k + 1] && done[2 * k] && b < bt) { bt = b; best = k; on = 0; }
        }
        tick1(t, &now, bt);
        done[2 * best + !on] = 1;
        if (on) input_on(t, PL[best].note, PL[best].vel);
        else input_off(t, PL[best].note);
    }
    tick1(t, &now, gridp(t, 16) - 1u);
    song.rec = 0;
    seq_stop();
    mo_r = mo_w;
    for (k = 0; k < NVOICE; k++) t->v[k].active = t->v[k].gate = 0;
    for (k = 0; k < 128u; k++) PB[k].on = PB[k].off = -1;
    now = 0;
    seq_start();
    tick1(t, &now, gridp(t, 16) - 1u);
    seq_stop();
    drain(now);
    if (verbose)
        printf("  %s, REC Q %d %%: note | played at / len / vel | step: kind MICRO LEN SLIDE vel (ties) | played back at / len / vel\n",
               VM[vmode], hrq * 100 / 127);
    for (k = 0; k < NPL; k++) {
        const pl_t *p = &PL[k];
        uint32_t want = (uint32_t)(p->at + 0.5), ties = 0, j;
        const step_t *s = &t->step[want % 16u];
        double pa = PB[p->note].on >= 0 ? place_of(t, PB[p->note].on) : -1;
        double pe = PB[p->note].off >= 0 ? place_of(t, PB[p->note].off) : -1;
        double played_end = p->at + p->len, want_end = played_end, want_at = hrq >= 127 ? (double)want : p->at;
        int legato_next = k + 1u < NPL && PL[k + 1u].at < played_end;
        int gap_next = k + 1u < NPL && PL[k + 1u].at > played_end + 0.1;
        into[k] = s->time == ST_NOTE && s->n >= 1u && s->note[0] == p->note;
        for (j = want + 1u; j < 16u && t->step[j].time == ST_TIE; j++) ties++;
        if (hrq >= 127) want_end = want + p->len;          /* (quantised: the length kept from the grid) */
        if (verbose)
            printf("  %3d | %5.2f %4.2f %3d | %2u: %-4s %+3d %2u %s %3u (%u) | %5.2f %4.2f %3d%s\n", p->note, p->at, p->len, p->vel,
                   want, TN[s->time % 3u], (int)ST_MICRO(s), (unsigned)CT_LEN(s), s->flags & SF_SLIDE ? "S" : "-", s->vel, ties,
                   pa, pe >= 0 && pa >= 0 ? pe - pa : -1, PB[p->note].vel,
                   legato_next ? (PB[PL[k + 1u].note].on >= 0 && PB[PL[k + 1u].note].qon < PB[p->note].qoff ? "  legato" :
                                  PB[PL[k + 1u].note].on >= 0 && PB[PL[k + 1u].note].on == PB[p->note].off ? "  touch" : "  RETRIG") : "");
        ok_step &= into[k];
        ok_vel &= PB[p->note].vel == p->vel;
        if (pa < 0 || pe < 0) { ok_start = ok_len = 0; continue; }
        if (fabs(pa - want_at) > w_start) w_start = fabs(pa - want_at);
        if (!legato_next && fabs(pe - want_end) > w_len) w_len = fabs(pe - want_end);
        if (legato_next) {                                 /* (held to the next one: its length is the legato's) */
            int32_t nx = PB[PL[k + 1u].note].on, me = PB[p->note].off;
            int32_t g = (int32_t)gridp(t, (uint32_t)(PL[k + 1u].at + 0.5));
            if (vmode == V_LEGATO) ok_leg &= nx >= 0 && PB[PL[k + 1u].note].qon < PB[p->note].qoff && nx >= me - 2;
            else {                                         /* (POLY: to its step's end; from an early start a whole
                                                         * step at most: LEN counts from the note's start) */
                int32_t full = PB[p->note].on + (int32_t)slen(t, 0);
                ok_leg &= nx >= 0 && me >= (g < full ? g : full) - (int32_t)slen(t, 0) / 32 && me <= nx + 2;
            }
        }
        if (gap_next) ok_gap &= PB[PL[k + 1u].note].on > PB[p->note].off + 64;
    }
    ok_start &= w_start <= (hrq >= 127 ? 0.002 : 1.0 / 64 + 0.002);
    ok_len &= w_len <= 1.0 / 16 + 1.0 / 64;
    snprintf(what, sizeof what, "%s REC Q %3d %%: each note into its own step", VM[vmode], hrq * 100 / 127);
    ck(what, ok_step);
    snprintf(what, sizeof what, "%s REC Q %3d %%: each played back at its velocity", VM[vmode], hrq * 100 / 127);
    ck(what, ok_vel);
    snprintf(what, sizeof what, "%s REC Q %3d %%: starts where played (worst %.3f of a step)", VM[vmode], hrq * 100 / 127, w_start);
    ck(what, ok_start);
    snprintf(what, sizeof what, "%s REC Q %3d %%: lengths as held (worst %.3f of a step)", VM[vmode], hrq * 100 / 127, w_len);
    ck(what, ok_len);
    snprintf(what, sizeof what, "%s REC Q %3d %%: legato played back %s", VM[vmode], hrq * 100 / 127,
             vmode == V_LEGATO ? "legato (one envelope)" : "touching");
    ck(what, ok_leg);
    snprintf(what, sizeof what, "%s REC Q %3d %%: a gap kept a gap", VM[vmode], hrq * 100 / 127);
    ck(what, ok_gap);
}

/* (V10.6z4) GATE 100 % for new tracks; a store's 50 % (the old default, left out of it) still loads 50 % */
static void gate_default(void)
{
    static project_store_t st;
    int ok;
    ui_power_on();
    ck("new tracks: GATE 100 % (127; was 64, 50 %)", trk[0].p[P_SGATE] == 127 && trk[3].p[P_SGATE] == 127 &&
       TP[P_SGATE].def == 127 && param_store_def(ENGI_ANALOG, P_SGATE) == 64);
    trk[0].p[P_SGATE] = 64;                               /* (a project saved at the old default: no GATE byte) */
    trk[1].p[P_SGATE] = 127;
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    trk[0].p[P_SGATE] = trk[1].p[P_SGATE] = 90;
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ck("a project: GATE 50 % loads 50 %, 100 % loads 100 % (the store's default stays 64)",
       ok && trk[0].p[P_SGATE] == 64 && trk[1].p[P_SGATE] == 127);
}

int main(void)
{
    gate_default();
    take(V_LEGATO, 0);
    take(V_LEGATO, 127);
    take(V_POLY, 0);
    take(V_POLY, 127);
    printf("legato: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
