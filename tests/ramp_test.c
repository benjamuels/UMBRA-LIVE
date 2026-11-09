/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.6v) the step component RAMP (core.h CV_RAMP_*, seq.c ramp_run; JUMP's code): the step's LOCK glides there from the
 * knob's value (KNOB) or the other end (OPP) along its CURVE over its LENGTH (steps or bars), on the real sequencer,
 * motion, project and UI sources (the stubs of tests/ui_test.c):
 *   the target reached at the end, then the knob's value back (gliding, RAMP_BACK); no restart on the next passes when it
 *   is longer than the pattern, a restart each pass when shorter; a STOP puts the knob's value back; no lock: nothing;
 *   an engine's parameter (ANALOG's CUT); the curves (monotonic but ZIGZAG, 0 to 1); a project of before with JUMPs loads
 *   (a VALUE no RAMP can mean: none; legacy CP_JUMP / CP_REV: none; the global lane's: none); SEQ > COMP's knobs and cards.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/ramp_test.c -lm -o build/ramp
 * (its demo: tests/swarm_test.c SWARM_WAV) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int rbad;
static project_store_t st;
static void rcheck(const char *what, int ok)
{
    printf("ramp: %-86s %s\n", what, ok ? "ok" : "FAIL");
    rbad += !ok;
}
/* track 1: ANALOG, len steps of 1/16 (a note each), C major, no swing; CUT 40 (the knob); a LOCK of CUT `to` on step 0
 * with a RAMP of VALUE v, LENGTH l (clen) */
static track_t *rpat(uint32_t len, uint32_t v, uint32_t l, int16_t to, int lock)
{
    track_t *t;
    uint32_t i;
    ui_power_on();
    t = &trk[0];
    host_preset(t, ENGI_ANALOG, 0);
    track_defaults_steps(t);
    t->p[P_SLEN] = (int16_t)len; t->p[P_SDIV] = 2; t->p[P_SGATE] = 64; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
    t->p[P_ROOT] = 0; t->p[P_SCALE] = 1;
    t->p[P_E4] = 40;
    for (i = 0; i < len; i++) t->step[i] = (step_t){{(uint8_t)(48 + (i % 4u) * 3u)}, 1, ST_NOTE, 0, 100};
    t->step[0].comp = CO_MAKE(CA_RAMP, CC_ALWAYS); t->step[0].carg = (uint8_t)v; t->step[0].clen[0] = (uint8_t)l;
    motion_clear(t);
    if (lock) motion_set_lock(t, 0, P_E4, to);
    return t;
}
static uint32_t rstep(const track_t *t) { return div_samples((uint32_t)t->p[P_SDIV]); }
/* the value of CUT at the end of each of `steps` steps (into v[]) */
static void rrun(track_t *t, uint32_t steps, int16_t *v)
{
    uint32_t s, b, per = rstep(t), done = 0;
    for (s = 0; s < steps; s++) {
        for (b = 0; done + CTL <= (s + 1u) * per; b++, done += CTL)
            seq_tick(t, CTL);
        v[s] = t->p[P_E4];
    }
}
static int ramp_tests(void)
{
    int16_t v[200];
    uint32_t i, c;
    int ok;
    track_t *t;
    {   /* the curves: 0 .. 1, monotonic (all but ZIGZAG) */
        ok = 1;
        for (c = 0; c < CV_RAMP_NC; c++) {
            int32_t p = -1, x, y, back = 0;
            ok &= ramp_curve(c, 0) == 0 && ramp_curve(c, 32768) == 32768;
            for (x = 0; x <= 32768; x += 64) {
                y = ramp_curve(c, x);
                ok &= y >= 0 && y <= 32768;
                if (y < p) back = 1;
                p = y;
            }
            ok &= c == 5u ? back : !back;
        }
        ok &= ramp_curve(1, 8192) < 8192 && ramp_curve(2, 8192) > 8192 && ramp_curve(3, 8192) < 8192 && ramp_curve(3, 24576) > 24576;
        rcheck("curves LIN EXP LOG S STEPS ZIGZAG: 0 to 1; monotonic, ZIGZAG bouncing; EXP slow first, LOG fast, S both", ok);
    }
    {   /* 24 steps (a pass of 16 and a half), LIN, OPP: CUT 0 -> 120, no restart at the second pass, then back to 40;
         * launched again at the third */
        t = rpat(16, 6u, 23, 120, 1);
        seq_start();
        rrun(t, 40, v);
        ok = v[0] <= 8 && v[22] >= 108 && v[23] >= 116;
        for (i = 1; i < 24u; i++) ok &= v[i] >= v[i - 1u];
        ok &= v[16] > v[15] && v[16] > 60;              /* (the step again at the second pass: on, not from 0) */
        ok &= v[25] == 40 && v[31] == 40;               /* (its end: the knob's value back) */
        ok &= v[32] < 10 && v[39] > 30;                 /* (pass 3: launched again) */
        printf("ramp:   CUT by step:"); for (i = 0; i < 40u; i++) printf(" %d", v[i]); printf("\n");
        rcheck("24 steps LIN from OPP, a pattern of 16: 0 -> 120 rising, no restart at the next pass, the knob's 40, again", ok);
        t = rpat(16, 6u, 31, 120, 1);          /* (32 steps: its end the step again, launched again at once) */
        seq_start();
        rrun(t, 70, v);
        ok = v[31] >= 116 && v[32] < 10 && v[48] > 60 && v[63] >= 116 && v[64] < 10;
        rcheck("  32 steps on 16: no restart at pass 2, its end and the launch of pass 3 together (a riser every 2 passes)", ok);
    }
    {   /* 4 steps (shorter than the pattern of 16), KNOB, EXP: each pass from 40 to 100, back to 40 by step 6 */
        int16_t w[48];
        t = rpat(16, 1u, 3, 100, 1);
        seq_start();
        rrun(t, 48, w);
        ok = w[2] > 40 && w[2] < 100 && w[3] >= 95 && w[5] == 40 && w[15] == 40;
        ok &= w[16 + 2] == w[2] && w[32 + 2] == w[2] && w[16 + 3] == w[3] && w[16 + 6] == 40;
        ok &= w[1] - 40 < (w[2] - w[1]);                /* (EXP: slow first) */
        printf("ramp:   CUT by step:"); for (i = 0; i < 24u; i++) printf(" %d", w[i]); printf("\n");
        rcheck("4 steps EXP from KNOB, a pattern of 16: 40 -> 100 each pass (restarts), the knob's 40 between", ok);
    }
    {   /* bars: 1 bar (16 steps of 1/16) from KNOB LIN over a pattern of 12 */
        t = rpat(12, CV_RAMP_BARS, 0, 120, 1);
        seq_start();
        rrun(t, 30, v);
        ok = v[7] > 60 && v[7] < 100 && v[12] > v[11] && v[15] >= 116 && v[17] == 40 && v[23] == 40 && v[24] < 50 && v[29] > 60;
        printf("ramp:   CUT by step:"); for (i = 0; i < 30u; i++) printf(" %d", v[i]); printf("\n");
        rcheck("LENGTH in bars: 1 bar over a pattern of 12 (no restart at step 12), the target at the bar's end, again at 24", ok);
    }
    {   /* STOP mid-way: the knob's value; no lock: nothing; the track's records off: nothing */
        t = rpat(16, 0, 31, 120, 1);
        seq_start();
        rrun(t, 10, v);
        ok = t->p[P_E4] > 40;
        seq_stop();
        ok &= t->p[P_E4] == 40;
        rrun(t, 4, v);                                  /* (stopped: nothing moves it) */
        ok &= t->p[P_E4] == 40;
        t = rpat(16, 6u, 31, 120, 0);
        seq_start();
        rrun(t, 20, v);
        for (i = 0; i < 20u; i++) ok &= v[i] == 40;
        ok &= !rmp[0].id;
        rcheck("a STOP puts the knob's value back; a RAMP without a LOCK does nothing", ok);
    }
    {   /* the knob's value back without a jump: RAMP_BACK samples of a line */
        int32_t mx = 0, pv;
        uint32_t b;
        t = rpat(16, 0, 1, 127, 1);
        t->p[P_E4] = 0;
        motion_clear(t); motion_set_lock(t, 0, P_E4, 127);
        seq_start();
        pv = 0;
        for (b = 0; b < 4u * rstep(t) / CTL; b++) {
            seq_tick(t, CTL);
            if (b > rstep(t) / CTL) { int32_t d = pv - t->p[P_E4]; mx = d > mx ? d : mx; }
            pv = t->p[P_E4];
        }
        ok = mx > 0 && mx <= 127 * CTL / (int32_t)RAMP_BACK + 2 && t->p[P_E4] == 0;
        printf("ramp:   back: at most %d a block\n", mx);
        rcheck("its end: the knob's value back over ~23 ms (a few steps of CUT a block), not a jump", ok);
    }
    {   /* a project of before with JUMPs (RAMP's code): a VALUE no RAMP can mean (REV 64, a step past 32, curve 6/7) none;
         * one that can (a JUMP to step 4: 3, S) a RAMP; legacy CP_JUMP / CP_REV none; the global lane's none */
        step_t ls[NSTEP];
        t = rpat(16, 0, 0, 100, 1);
        t->step[1].comp = CO_MAKE(CA_RAMP, CC_ALWAYS); t->step[1].carg = 64;            /* REV */
        t->step[2].comp = CO_MAKE(CA_RAMP, CC_NM); t->step[2].carg = 44; t->step[2].cnm[0] = CNM_MAKE(1u, 2u);
        t->step[3].comp2 = CO_MAKE(CA_RAMP, CC_ALWAYS); t->step[3].carg2 = 3;           /* step 4: S from KNOB */
        t->step[4].comp = CO_MAKE(CA_RAMP, CC_ALWAYS); t->step[4].carg = 13;
        t->step[5].comp = CO_MAKE(CA_RAMP, CC_ALWAYS); t->step[5].carg = 32 | 2;      /* (a step past 32) */
        glane[3].comp = CO_MAKE(CA_RAMP, CC_ALWAYS); glane[3].val = 5; glane[3].mask = 63; glane[3].nm = CNM_MAKE(1u, 2u);
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
        ok &= !trk[0].step[1].comp && !trk[0].step[2].comp && !trk[0].step[4].comp && !trk[0].step[5].comp;
        ok &= trk[0].step[3].comp2 == CO_MAKE(CA_RAMP, CC_ALWAYS) && trk[0].step[3].carg2 == 3u;
        ok &= CO_ACT(trk[0].step[0].comp) == CA_RAMP && !CO_ACT(glane[3].comp);
        memset(ls, 0, sizeof ls);
        comp_legacy(&ls[0], 0, CP_JUMP, 2); comp_legacy(&ls[1], 0, CP_REV, 1); comp_legacy(&ls[2], 1, CP_JUMP, 0x80u | 3u);
        comp_migrate(ls, 4);
        ok &= !ls[0].comp && !ls[1].comp && !ls[2].comp2 && ls[0].clen[0] != CL_LEGACY && ls[2].clen[1] != CL_LEGACY;
        seq_start(); rrun(&trk[0], 40, v); seq_stop();
        rcheck("old projects: JUMP whose VALUE is no RAMP none (REV, steps 33..64, 12..15), one that is a RAMP; "
               "CP_JUMP / CP_REV none; the global lane's none; it plays", ok);
    }
    {   /* SEQ > COMP: KNOB 1 to RAMP (16 steps, "LOCK A PARAM" without a lock), KNOB 2 past 32 steps into bars, KNOB 4
         * FROM + CURVE; the cards */
        uint32_t a;
        t = rpat(16, 0, 0, 100, 0);
        t->step[0].comp = t->step[0].carg = t->step[0].clen[0] = 0;
        go_page(GR_COMP); cursor_set(0); frame();
        for (a = 0; a < 40u && comp_view() != CA_RAMP; a++) turn(EN_K1, 1);
        frame();
        ok = comp_view() == CA_RAMP && t->step[0].clen[0] == 15u && strstr(ui.msg, "LOCK A PARAM") != 0;
        ok &= !strncmp(ui.col[3], "NO LOCK|LIN|", 12);
        motion_set_lock(t, 0, P_E4, 100);
        turn(EN_K4, 7); frame();                        /* OPP EXP */
        ok &= t->step[0].carg == 7u && !strncmp(ui.col[3], "OPP|EXP|", 8);
        turn(EN_K2, 4); turn(EN_K2, 4); turn(EN_K2, 4); turn(EN_K2, 4); turn(EN_K2, 4); frame();   /* 16 + 20: 4 bars */
        ok &= (t->step[0].carg & CV_RAMP_BARS) && t->step[0].clen[0] == 3u && strstr(ui.col[1], "BAR") != 0 &&
              (t->step[0].carg & ~CV_RAMP_BARS) == 7u;
        turn(EN_K4, 1); frame();                        /* OPP LOG: BARS kept */
        ok &= t->step[0].carg == (CV_RAMP_BARS | 8u);
        printf("ramp:   cards: [%s] [%s] [%s] [%s]\n", ui.col[0], ui.col[1], ui.col[2], ui.col[3]);
        for (a = 0; a < 20u; a++) turn(EN_K2, -4);
        ok &= !(t->step[0].carg & CV_RAMP_BARS) && t->step[0].clen[0] == 0u;
        rcheck("SEQ > COMP: ACTION RAMP (16 steps; LOCK A PARAM), LENGTH 32 steps then bars, VALUE FROM + CURVE, the cards", ok);
        {   /* every LENGTH in bars and every VALUE, the four skins and LARGE: the cards whole (as ui_test test_comp_cards) */
            static const uint8_t SK[4] = {SKIN_COLORS, SKIN_BOTTOM, SKIN_HUD, SKIN_AIR};
            uint32_t sk, lg, l, v, cut = 0;
            for (sk = 0; sk < 4u; sk++)
                for (lg = 0; lg < 2u; lg++)
                    for (l = 0; l < 16u; l++)
                        for (v = 0; v < 12u; v++) {
                            ui_skin = SK[sk];
                            if (lg) ui_prefs |= PREF_LARGE; else ui_prefs &= ~PREF_LARGE;
                            t->step[0].carg = (uint8_t)(CV_RAMP_BARS | v); t->step[0].clen[0] = (uint8_t)l;
                            ut_card_bad = 0; ut_card_txt = 0;
                            ui.force = 1; frame();
                            cut += ut_card_bad != 0u;
                        }
            ui_skin = SK[0]; ui_prefs &= ~PREF_LARGE;
            rcheck("  every LENGTH in bars x every VALUE, four skins and LARGE: the cards whole", !cut);
        }
    }
    return rbad;
}
int main(void)
{
    int r = ramp_tests();
    printf(r ? "ramp: %d FAILED\n" : "ramp: all ok\n", r);
    return r != 0;
}
