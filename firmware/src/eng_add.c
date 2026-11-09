/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* ADD (JIANT 0.9.9-snd5): additive, nine sine partials a voice whose ratios, levels and decays are the sound.
 * MODE picks the partials' ratios: HARM the first nine harmonics, ODD the odd ones (a clarinet's, a square's), WIDE
 * 1 2 3 4 6 8 11 13 16 (bright, airy), BELL a church bell's (hum, prime, tierce, quint, nominal ..), BAR a free bar's
 * (a vibraphone's, a xylophone's), DRUM a membrane's (Bessel's zeros). (gen6d, the owner: the old knobs moved too
 * little) MORPH fades the ratios to the next MODE's (a saw into a bell), BRIGHT tilts the levels (dark .. flat at 96 ..
 * the highs over the fundamental), PARTIAL how many sound (1 .. 9, the last by its fraction: an additive filter),
 * SHIFT down stretches the ratios, up adds the same Hz to every partial (inharmonic, a ring's, a radio's), COMB notches
 * sweeping over the partials (|cos k phi|: a phaser of partials; LFO / ENV -> SHAPE sweep it), DECAY each partial's
 * ring (127: held), the higher ones sooner, MOTION down a BLOOM (the higher partials come in later), up a SHIMMER
 * (each partial its own slow wander). A
 * partial past ~19 kHz is silent (nothing aliases).
 *
 * Memory: none of its own. WHEEL's per voice state (eng_wheel.c drw_v: nine phases and gains a voice, by part) is a
 * part's either way (a part is WHEEL or ADD); the voice's age in blocks in its perc. Cost: WHEEL's (a table lookup and a
 * multiply-add a partial a sample); the levels and the decays once a block. A note from silence starts its partials in
 * phase (a bell's, a pluck's attack the same every time). */
static const char *const N_ADD_MODE[] = {"HARM", "ODD", "WIDE", "BELL", "BAR", "DRUM", "PRIME", "FIFTH", "SUB", "PLATE"};
enum { AD_HARM, AD_ODD, AD_WIDE, AD_BELL, AD_BAR, AD_DRUM, AD_PRIME, AD_FIFTH, AD_SUB, AD_PLATE, AD_N };
#define AD_R(x) ((uint16_t)((x) * 1024.0 + 0.5))         /* a ratio, Q10 */
static const uint16_t ADD_RATIO[AD_N][DRW_NP] = {
    {AD_R(1), AD_R(2), AD_R(3), AD_R(4), AD_R(5), AD_R(6), AD_R(7), AD_R(8), AD_R(9)},
    {AD_R(1), AD_R(3), AD_R(5), AD_R(7), AD_R(9), AD_R(11), AD_R(13), AD_R(15), AD_R(17)},
    {AD_R(1), AD_R(2), AD_R(3), AD_R(4), AD_R(6), AD_R(8), AD_R(11), AD_R(13), AD_R(16)},
    {AD_R(0.5), AD_R(1), AD_R(1.183), AD_R(1.506), AD_R(2), AD_R(2.514), AD_R(2.662), AD_R(3.011), AD_R(4.166)},
    {AD_R(1), AD_R(2.756), AD_R(5.404), AD_R(8.933), AD_R(13.344), AD_R(17.4), AD_R(9.85), AD_R(3.93), AD_R(15.1)},
    {AD_R(1), AD_R(1.594), AD_R(2.136), AD_R(2.296), AD_R(2.653), AD_R(2.918), AD_R(3.156), AD_R(3.501), AD_R(3.6)},
    /* (p5, the owner: what is additive's own) PRIME the prime harmonics only (a hollow, glassy organ: no octaves past the
     * 2nd), FIFTH an organ's mixture of fifths and octaves (a Quint stop), SUB under the note (the 1/4 and 1/2 a sub, a
     * deep drone over its own octaves), PLATE a plate's square-root partials (a gong, a thunder sheet). MORPH between
     * any two neighbours: a choir of spectra no other engine reaches */
    {AD_R(1), AD_R(2), AD_R(3), AD_R(5), AD_R(7), AD_R(11), AD_R(13), AD_R(17), AD_R(19)},
    {AD_R(1), AD_R(1.5), AD_R(2), AD_R(3), AD_R(4), AD_R(4.5), AD_R(6), AD_R(8), AD_R(9)},
    {AD_R(0.25), AD_R(0.5), AD_R(1), AD_R(1.5), AD_R(2), AD_R(3), AD_R(4), AD_R(5), AD_R(6)},
    {AD_R(1), AD_R(1.414), AD_R(1.732), AD_R(2), AD_R(2.236), AD_R(2.449), AD_R(2.646), AD_R(2.828), AD_R(3)},
};
/* log2(k + 1), Q8: the tilt over the partials' ranks */
static const uint16_t ADD_LOG2[DRW_NP] = {0, 256, 406, 512, 594, 662, 719, 768, 811};
#define AD_TOP 1932735283u                               /* ~19.8 kHz as a phase increment: above it a partial is silent */

static void add_note_on(track_t *t, voice_t *v)
{
    uint32_t vi = (uint32_t)(v - t->v) % NVOICE, k;
    drw_vc_t *V = &drw_v[drw_part(t)][vi];
    if (!v->env) {                                       /* (from silence: in phase, its partials from nothing) */
        for (k = 0; k < DRW_NP; k++) {
            V->ph[k] = 0;
            V->gp[k] = 0;
        }
    }
    V->perc = 0;                                         /* (its age in blocks: the partials' decays from the note) */
    V->init = 1;
}

static void add_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    drw_vc_t *V = &drw_v[drw_part(t)][(uint32_t)(v - t->v) % NVOICE];
    uint32_t mo = (uint32_t)p[P_E0] % AD_N;
    const uint16_t *R = ADD_RATIO[mo], *R2 = ADD_RATIO[(mo + 1u) % AD_N];
    int32_t acc[CTL], lv[DRW_NP], sum = 0, norm, mp = clamp(p[P_E1], 0, 127), slope = (96 - clamp(p[P_E2], 0, 127)) * 5;
    int32_t np = 256 + clamp(p[P_E3], 0, 127) * 2048 / 127, sh = clamp(p[P_E4], -64, 63), dec = clamp(p[P_E6], 0, 127);
    int32_t mot = clamp(p[P_E7], -64, 63), cmb = clamp(p[P_E5] + ((m->shape - (64 << 8)) >> 8), 0, 127) * 2;
    uint32_t i, k, age = (uint32_t)V->perc, rate = dec >= 127 ? 0u : str_exp2n((uint32_t)dec * 2201u / 127u) * 19u / 65u;
    uint32_t shz = sh > 0 ? (uint32_t)(sh * sh) * 19478u : 0u;   /* (SHIFT up: + sh^2 / 5 Hz to every partial) */
    if (n > CTL)
        n = CTL;
    for (k = 0; k < DRW_NP; k++) {                       /* the levels: BRIGHT's tilt, COMB's notches, PARTIALS */
        uint32_t lt = slope >= 0 ? (uint32_t)(slope * ADD_LOG2[k]) : (uint32_t)(-slope * (ADD_LOG2[DRW_NP - 1u] - ADD_LOG2[k]));
        int32_t l = (int32_t)(str_exp2n(lt >> 8) >> 1), kq = (int32_t)k << 8;
        if (cmb) {                                       /* |cos((k + 1) phi)|, phi 0 .. pi/2: notches sweeping */
            int32_t c = SINE[((k + 1u) * (uint32_t)cmb + 256u) & 1023u];
            c = c < 0 ? -c : c;
            l = (l * (c + 1024)) >> 15;              /* (l < 2^15, c < 2^15: in 32 bits) */
        }
        if (kq >= np) l = 0;
        else if (np - kq < 256) l = l * (np - kq) >> 8;  /* (the last one in by its fraction: a smooth sweep) */
        lv[k] = l;
        sum += l;
    }
    norm = (int32_t)((32768u * 32768u) / (uint32_t)(sum + 16384));
    for (i = 0; i < n; i++)
        acc[i] = 0;
    for (k = 0; k < DRW_NP; k++) {
        int32_t r = (int32_t)R[k] + ((((int32_t)R2[k] - (int32_t)R[k]) * mp * 258) >> 15), g0 = V->gp[k], g1;
        uint32_t inc, ph = V->ph[k];
        if (sh < 0)                                      /* SHIFT down: the ratios stretched (a stiff string, a bell bent) */
            r = clamp(r + ((r * ((-sh * (r - 1024)) >> 10)) >> 10), 256, 40000);
        inc = (m->inc >> 10) * (uint32_t)r + shz;
        if (inc >= AD_TOP || !lv[k])
            g1 = 0;
        else {
            uint32_t rk = rate + rate * k / 2u, x;       /* (its decay: Q16 octaves a block; the higher ones sooner) */
            x = !rk ? 0u : age >= 0xFFFFFFFFu / rk ? 0xFFFFu : (age * rk) >> 8;
            g1 = mulq15(mulq15(lv[k], norm), (int32_t)(str_exp2n(x) >> 1));
            if (mot < 0 && k) {                          /* BLOOM: partial k full after k * |MOTION| * 2 blocks */
                uint32_t span = k * (uint32_t)-mot * 2u;
                if (age < span) g1 = g1 * (int32_t)age / (int32_t)span;
            } else if (mot > 0) {                        /* SHIMMER: each partial its own slow wander */
                int32_t w = SINE[((age * (2u + 3u * k)) / 2u + k * 397u) & 1023u];
                g1 = (g1 * (32768 - ((mot * 300 * ((w + 32767) >> 1)) >> 15))) >> 15;
            }
        }
        V->gp[k] = g1;
        if (!g0 && !g1) {
            V->ph[k] = ph + inc * n;
            continue;
        }
        if (g0 == g1) {
            for (i = 0; i < n; i++) {
                ph += inc;
                acc[i] += (SINE[ph >> 22] * g1) >> 15;
            }
        } else {                                         /* the gain ramped over the block */
            int32_t gq = g0 << CTL_LOG2, dg = g1 - g0;
            for (i = 0; i < n; i++) {
                ph += inc;
                gq += dg;
                acc[i] += (SINE[ph >> 22] * (gq >> CTL_LOG2)) >> 15;
            }
        }
        V->ph[k] = ph;
    }
    V->perc = (int32_t)(age < 0x7FFFFFu ? age + 1u : age);
    for (i = 0; i < n; i++)
        out[i] += voice_amp(acc[i] >> 1, m, i) << 1;
}

/* {MODE, MORPH, BRIGHT, PARTIAL, SHIFT, COMB, DECAY, MOTION}, {A D S R}, fenv, mono */
static const preset_t ADD_PRESETS[] = {
    /* (V10.6i, reworked in place: the stored numbers kept, PROGRAM CHANGE too; browsed in ADD_ORDER) SHP moves COMB;
     * no CUT of its own: the part's FILTER (F.CUT under 127: ENV / LFO -> FILTER on it) */
    {"ADD ORGAN", {AD_HARM, 0, 76, 127, 0, 0, 127, 0}, {0, 64, 127, 40}, 0, TAG(KEYS), FX(0, 15, 0, 30), PAT(5),
     MODS(P_LRATE, 72, P_LD_AMP, 14, MMOD(1, PS_MODW, PD_LRATE, 30))},                   /* a rotor; MODW: fast */
    {"ADD SAW", {AD_HARM, 0, 92, 127, 0, 0, 127, 0}, {2, 70, 100, 35}, 30, 1 | TAG(LEAD), FX(0, 20, 0, 25), PAT(4),
     MODS(P_FCUT, 70, P_FRES, 40, P_LRATE, 88, MMOD(1, PS_MODW, PD_VIB, 2))},           /* the filter opens */
    {"HOLLOW", {AD_ODD, 0, 84, 127, 0, 30, 127, 0}, {30, 70, 115, 70}, 0, TAG(PAD), FX(0, 10, 10, 35), PAT(5),
     MODS(P_LWAVE, LW_TRI, P_LRATE, 40, P_LD_SHP, 30, MMOD(1, PS_AT, PD_E(2), 30))},     /* a phaser of partials */
    {"INIT ADD", {AD_HARM, 0, 70, 127, 0, 0, 127, 0}, {0, 64, 127, 40}, 0, TAG(KEYS), FX(0, 0, 0, 0)},
    {"BELL", {AD_BELL, 0, 100, 127, 0, 0, 74, 0}, {0, 64, 127, 70}, 0, TAG(BELL), FX(0, 0, 15, 55), PAT(6),
     MODS(MMOD(1, PS_VEL, PD_E(2), 30))},                                                 /* struck harder: brighter */
    {"RADIO", {AD_HARM, 0, 96, 127, 40, 0, 127, 20}, {0, 64, 127, 80}, 0, TAG(FX), FX(0, 0, 10, 60), PAT(6),
     MODS(P_LWAVE, LW_SH, P_LRATE, -8, MMOD(1, PS_LFO, PD_E(4), 12), MMOD(2, PS_RAND, PD_PAN, 30), P_FTYPE, 2,
          P_FCUT, 80)},                                                                    /* tuning between stations */
    {"VIBES", {AD_BAR, 0, 72, 127, 0, 0, 76, 0}, {0, 64, 127, 55}, 0, TAG(BELL), FX(0, 25, 0, 40), PAT(6),
     MODS(P_LRATE, 78, P_LD_AMP, 30)},                                                    /* the motor */
    {"GLASS", {AD_WIDE, 0, 118, 127, -20, 0, 80, 30}, {0, 64, 127, 70}, 0, TAG(BELL), FX(0, 20, 20, 60), PAT(5),
     MODS(MMOD(1, PS_KEY, PD_E(2), -24))},   /* the high notes softer */
    {"SKIN TOM", {AD_DRUM, 0, 92, 127, 0, 0, 46, 0}, {0, 64, 127, 30}, 0, TAG(DRUM), FX(0, 0, 0, 20), PAT(4),
     MODS(MENV2(0, 25, 0, 20), MMOD(1, PS_ENV2, PD_PITCH, 12))},                         /* the skin falls in pitch */
    {"AIR PAD", {AD_WIDE, 64, 100, 127, -10, 20, 127, 50}, {64, 64, 127, 80}, 0, TAG(PAD), FX(0, 30, 15, 75), PAT(5),
     MODS(P_FCUT, 100, P_LWAVE, LW_DRIFT, P_LRATE, -2, P_LD_FLT, 20)},
    {"SWELL", {AD_HARM, 0, 104, 127, 0, 0, 127, -50}, {20, 64, 127, 60}, 0, TAG(PAD), FX(0, 20, 20, 50), PAT(5),
     MODS(MENV2(60, 100, 40, 80), MMOD(1, PS_ENV2, PD_E(1), 50))},   /* HARM into ODD and back */
    {"SUB ADD", {AD_ODD, 0, 20, 40, 0, 0, 127, 0}, {0, 64, 80, 25}, 0, 1 | TAG(BASS), FX(0, 0, 0, 5), PAT(2),
     MODS(P_GLIDE, 10, MMOD(1, PS_VEL, PD_E(3), 30))},                                    /* harder: more partials */
    {"PRIME ORG", {AD_PRIME, 0, 84, 127, 0, 0, 127, 0}, {2, 64, 127, 45}, 0, TAG(KEYS), FX(0, 20, 0, 35), PAT(5),
     MODS(P_LRATE, 84, P_LD_PIT, 1, P_LFADE, 60)},
    {"QUINT", {AD_FIFTH, 0, 80, 127, 0, 24, 127, 20}, {6, 64, 127, 50}, 0, TAG(PAD), FX(0, 25, 10, 45), PAT(5),
     MODS(P_LWAVE, LW_TRI, P_LRATE, 24, MMOD(1, PS_LFO, PD_E(1), 20))},                  /* FIFTH towards SUB, slowly */
    {"DRONE", {AD_SUB, 50, 70, 127, 0, 30, 127, 40}, {40, 64, 127, 90}, 0, TAG(PAD), FX(0, 20, 15, 70), PAT(5),
     MODS(P_LWAVE, LW_DRIFT, P_LRATE, -1, MMOD(1, PS_LFO, PD_E(1), 30), P_LD_SHP, 20)},  /* the spectrum drifts */
    {"GONG", {AD_PLATE, 30, 104, 127, -12, 0, 88, 30}, {0, 64, 127, 90}, 0, TAG(BELL), FX(0, 0, 20, 70), PAT(6),
     MODS(MENV2(0, 60, 0, 60), MMOD(1, PS_ENV2, PD_PITCH, 4), MMOD(2, PS_RAND, PD_REV, 30))},
};
/* (V10.6i) the browsing order: to play, then the experiments, then INIT */
static const uint8_t ADD_ORDER[] = {0, 12, 1, 2, 9, 13, 4, 6, 7, 11, 8,   /* to play */
                                    10, 14, 15, 5,                         /* experiments */
                                    3};                                    /* INIT */
_Static_assert(sizeof ADD_ORDER == NELEM(ADD_PRESETS), "ADD's browsing order: every preset once");

static const engine_t ENG_ADD = {
    .name = "ADD",
    .page_title = {"SPECTRUM", "MOTION"},
    .edit = {
        {"MODE", F_ENUM, 0, AD_N - 1, AD_HARM, N_ADD_MODE, 0},
        {"MORPH", F_PCT, 0, 127, 0, 0, 0},
        {"BRIGHT", F_PCT, 0, 127, 70, 0, 0},
        {"PARTIAL", F_PCT, 0, 127, 127, 0, 0},
        {"SHIFT", F_INT, -64, 63, 0, 0, 0},
        {"COMB", F_PCT, 0, 127, 0, 0, 0},
        {"DECAY", F_PCT, 0, 127, 127, 0, 0},
        {"MOTION", F_INT, -64, 63, 0, 0, 0},
    },
    .presets = ADD_PRESETS,
    .npresets = NELEM(ADD_PRESETS),
    .note_on = add_note_on,
    .render = add_render,
    .knob = {P_E0, P_E3, P_E5, P_E6},
};
