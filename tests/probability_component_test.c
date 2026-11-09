/* SPDX-License-Identifier: GPL-3.0-only */
/* Drive the real sequencer; the older probability test called seq_step directly and missed component replacement. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    static const uint32_t actions[] = {0, CA_SEQ, CA_STUT};
    static const uint32_t chances[] = {0, 25, 50, 100};
    int bad = 0;
    for (uint32_t kit = 0; kit < 2; kit++)
    for (uint32_t a = 0; a < NELEM(actions); a++)
    for (uint32_t p = 0; p < NELEM(chances); p++) {
        track_t *t = &trk[0];
        uint32_t heard = 0, iterations = 1000;
        char title[100];
        ui_power_on();
        memset(&chain, 0, sizeof chain);
        memset(&lv, 0, sizeof lv);
        memset(glane, 0, sizeof glane); gl_mono = 0; gl_reset();
        set_engine_of(t, kit ? ENGI_DRUM : ENGI_ANALOG);
        t->engine = t->eng_req;
        t->p[P_SLEN] = 1; t->p[P_SDIV] = 2; t->p[P_TCHAN] = 0;
        t->p[P_HVEL] = t->p[P_HTIM] = t->p[P_HGAT] = 127;
        song.playing = 1; song.rec = 0; transport_req = 0;
        for (uint32_t i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        step_t *s = &t->step[0];
        s->time = ST_NOTE; s->vel = 100;
        if (kit) s->hit = 1u << DV_KICK;
        else { s->n = 1; s->note[0] = 60; }
        s->comp = CO_MAKE(actions[a], CC_ALWAYS);
        s->carg = 0;
        step_set_chance(s, chances[p]);
        for (uint32_t i = 0; i < iterations; i++) {
            seq_release(t);
            t->seq_idx = 0; t->seq_pos = 0x7FFFFFFFu; t->ext = 0;
            t->loops = (uint16_t)i;
            seq_tick(t, 0);
            heard += t->seq_n != 0;
        }
        uint32_t expected = iterations * chances[p] / 100u;
        int ok = chances[p] == 0 ? !heard : chances[p] == 100 ? heard == iterations :
                 heard + 70u > expected && heard < expected + 70u;
        snprintf(title, sizeof title, "%s %s probability %u%%: %u/%u played", kit ? "DRUM" : "ANALOG",
                 a == 0 ? "plain" : a == 1 ? "SEQ" : "STUTTER", chances[p], heard, iterations);
        bad += check(title, ok && step_chance(s) == chances[p]);
    }
    {
        ui_power_on();
        track_t *t = &trk[0];
        t->p[P_SLEN] = 2; t->p[P_CLEN] = 1;
        for (uint32_t k = 0; k < 2; k++) {
            step_clear(&t->step[k]); t->step[k].time = ST_NOTE;
            t->step[k].n = 1; t->step[k].note[0] = k ? 67 : 60;
        }
        t->step[0].comp = CO_MAKE(CA_SEQ, CC_ALWAYS); t->step[0].carg = 1;
        step_set_chance(&t->step[0], 0); step_set_chance(&t->step[1], 100);
        const step_t *rendered = comp_run(t, comp_lane(t, &t->step[0]), 2);
        bad += check("C.LEN alias: SEQ replacement keeps destination's 0%, not source's 100%",
                     rendered->note[0] == 67 && step_chance(rendered) == 0 && step_chance(&t->step[0]) == 0);
    }
    return bad != 0;
}
