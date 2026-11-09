/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fit_bad;
static void fit_check(const char *what, int ok) { fit_bad += check(what, ok); }

int main(void)
{
    static const uint32_t tempos[] = {40, 61, 120, 239, 240}, swings[] = {0, 37, 100};
    track_t *t = &trk[0];
    int exact = 1, off_same = 1, positive = 1;
    uint32_t checked = 0;
    ui_power_on();
    song.g[G_SWING] = 0;
    for (uint32_t b = 0; b < NELEM(tempos); b++) {
        song.g[G_BPM] = tempos[b];
        for (uint32_t div = 0; div < NELEM(N_DIV); div++) {
            uint32_t period = div_samples(div);
            for (uint32_t len = 1; len <= NSTEP; len++) {
                t->p[P_SLEN] = len;
                for (uint32_t s = 0; s < NELEM(swings); s++) {
                    t->p[P_SSWING] = swings[s];
                    for (uint32_t span = 1; span <= 64; span++) {
                        uint32_t total = 0;
                        t->seq_fit = 128u | span;
                        for (uint32_t i = 0; i < len; i++) {
                            uint32_t duration = step_samples(t, period, i);
                            total += duration;
                            positive &= duration > 0;
                        }
                        exact &= total == period * span;
                        checked++;
                    }
                    t->seq_fit = 64;
                    for (uint32_t i = 0; i < len; i++)
                        off_same &= step_samples(t, period, i) == swing_step_len(t, period, i);
                }
            }
        }
    }
    fit_check("FIT: every LEN/SPAN 1..64, all DIV, five tempos and three swings sum exactly", exact && positive);
    fit_check("FIT OFF: the previous clock is sample-identical, including odd lengths and swing", off_same);
    printf("fit combinations checked: %u\n", checked);

    fit_check("long-step gates avoid 32-bit intermediate overflow", fit_scale_samples(67737600u, 127u, 7u) == (uint32_t)(67737600ull * 127u / 128u));
    fit_check("long-step microtiming avoids intermediate overflow", fit_scale_samples(67737600u, 64u, 6u) == 67737600u);

    ui_power_on();
    song.g[G_BPM] = 123;
    song.g[G_SWING] = 0;
    song.playing = 1;
    memset(&lv, 0, sizeof lv);
    for (uint32_t k = 0; k < 2; k++) {
        track_t *q = &trk[k];
        q->p[P_SLEN] = k ? 18 : 16;
        q->p[P_SDIV] = 2;
        q->p[P_SSWING] = 0;
        q->seq_fit = k ? 128u | 16u : 0;
        q->seq_idx = q->p[P_SLEN] - 1;
        q->seq_pos = 0x7FFFFFFFu;
        q->loops = q->ext = 0;
        for (uint32_t i = 0; i < NSTEP; i++) step_clear(&q->step[i]);
        seq_tick(q, 0);
    }
    {
        uint32_t loop = 16u * div_samples(2), elapsed = 0, count = 200;
        int aligned = 1;
        for (uint32_t r = 1; r <= count; r++) {
            while (elapsed < r * loop) {
                uint32_t n = r * loop - elapsed;
                if (n > CTL) n = CTL;
                seq_tick(&trk[0], n); seq_tick(&trk[1], n);
                elapsed += n;
            }
            aligned &= trk[0].seq_idx == 0 && trk[1].seq_idx == 0 &&
                       trk[0].seq_pos == 0 && trk[1].seq_pos == 0 &&
                       trk[0].loops == r && trk[1].loops == r;
        }
        fit_check("production sequencer: 18 steps vs 16 realign for 200 loops with no sample drift", aligned);
    }

    ui_power_on();
    t->seq_fit = 128u | 16u;
    t->p[P_SLEN] = 18; t->p[P_SGATE] = 64; t->seq_idx = 0;
    t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
    for (uint32_t i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    {
        step_t s; uint32_t period = div_samples(2), unit = step_samples(t, period, 0);
        memset(&s, 0, sizeof s); s.time = ST_NOTE; s.n = 1; s.note[0] = 60; s.vel = 100;
        ev_off = 0; seq_step(t, &s, period, 0);
        fit_check("normal gates follow the fitted step duration", t->seq_n == 1 && t->seq_off == unit / 2u);
        s.ctime = 4u << 3; seq_step(t, &s, period, 0);
        fit_check("per-step LENGTH follows fitted duration", t->seq_off == unit * 4u / 16u);
        s.ctime = 0; s.flags = 3u << SF_RATCH_SH; seq_step(t, &s, period, 0);
        fit_check("ratchet subdivisions and gates follow fitted duration", t->seq_off == unit / 4u / 2u);
        t->step[1].time = ST_TIE;
        fit_check("tie chains use the sum of fitted steps", tie_span(t, period, 18) == unit + step_samples(t, period, 1));
    }

    ui_power_on();
    for (uint32_t p = 0; p < NPAGES; p++)
        if (FIT_PAGE(&PAGES[p])) { ui.home = 0; ui.page = p; page_entered(); break; }
    frame();
    fit_check("SEQ > LENGTH has RHYTHM with default SPAN 16 / FIT OFF", FIT_PAGE(cur_page()) && fit_span(TSEL) == 16 && !TSEL->seq_fit);
    t->seq_pos = 123; t->ext = 42;
    fit_ui_set(t, 16);
    fit_check("editing SPAN while FIT is OFF preserves phase and pending microtiming", t->seq_pos == 123 && t->ext == 42);
    turn(EN_K2, 2); turn(EN_K3, 1);
    fit_check("SPAN and FIT knobs set the selected track without changing DIV or another track", fit_span(TSEL) == 18 && (TSEL->seq_fit & 128u) && !trk[1].seq_fit);
    turn(EN_K3, -1);
    fit_check("FIT OFF remembers the chosen SPAN", fit_span(TSEL) == 18 && !(TSEL->seq_fit & 128u));
    t->seq_pos = 1000000;
    fit_ui_set(t, 128u | 1u);
    fit_check("live ratio change cannot leave a huge catch-up queue", t->seq_pos < step_samples(t, div_samples(t->p[P_SDIV]), t->seq_idx));
    lv.act = 1u; lv.gidx[0] = 0; lv.gpos[0] = 1000000u;
    fit_ui_set(t, 128u | 2u);
    fit_check("LIVE ghost also rescales safely on a ratio change", lv.gpos[0] < step_samples(t, div_samples(t->p[P_SDIV]), lv.gidx[0]));
    lv.act = 0;


    ui_power_on();
    trk[0].seq_fit = 128u | 16u;
    trk[1].seq_fit = 23u;
    project_capture(&proj_scratch);
    fit_check("project packing includes FIT and disabled SPAN", proj_pack(&proj_wire, &proj_scratch));
    memset(proj_scratch.fit, 255, sizeof proj_scratch.fit);
    fit_check("project import restores per-track FIT/SPAN", proj_import(&proj_scratch, &proj_wire, sizeof proj_wire) && proj_scratch.fit[0] == 144 && proj_scratch.fit[1] == 23);
    trk[0].seq_fit = 0;
    project_restore_runtime(&proj_scratch);
    fit_check("project load applies FIT/SPAN to the runtime clock", trk[0].seq_fit == 144 && trk[1].seq_fit == 23);
    transport_req = 0; song.playing = 0;
    project_save(0);
    trk[0].seq_fit = 0;
    chain_config.count = 1; chain_config.row[0] = (chain_row_t){0, 1};
    memset(song_idx.tsrc[song_cur], 0, sizeof song_idx.tsrc[song_cur]);
    fit_check("arrangement section stages the track's fitted clock", !song_stage(0, 0) && sec_stage_fit[0] == 144);
    sec_apply();
    fit_check("arrangement section applies its fitted clock", trk[0].seq_fit == 144);
    project_capture(&proj_scratch);
    memset(proj_scratch.fit, 0, sizeof proj_scratch.fit);
    proj_scratch.sum = proj_sum(&proj_scratch);
    proj_pack(&proj_wire, &proj_scratch);
    memset(proj_scratch.fit, 255, sizeof proj_scratch.fit);
    int defaults = proj_import(&proj_scratch, &proj_wire, sizeof proj_wire);
    for (uint32_t k = 0; k < NTRK; k++) defaults &= !proj_scratch.fit[k];
    fit_check("projects without the new extension load with FIT OFF", defaults);
    printf("FIT tests: %s\n", fit_bad ? "FAILED" : "passed");
    return fit_bad != 0;
}
