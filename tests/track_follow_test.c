/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static void route(track_t *t, unsigned src, unsigned dst, int amt)
{
    for (unsigned k = 0; k < 12; ++k) t->p[P_M1SRC + k] = 0;   /* (V10.6i: a factory preset's own slots off) */
    t->p[P_M1SRC] = src; t->p[P_M1DST] = dst; t->p[P_M1AMT] = amt;
}

int main(void)
{
    host_tracks_init(); mc_off = 1;
    for (unsigned t = 0; t < NTRK; ++t) host_preset(&trk[t], ENGI_ANALOG, 0);
    trk_live = 4;
    host_preset(&trk[4], ENGI_INPUT, 0);
    macro_v[0] = 127; song.g[G_CLIP] = 0;
    route(&trk[4], MS_M1, MD_CLIP, 63);
    macro_master();
    assert(!ain_any && !clip_eff);
    puts("inactive INPUT and matrix do not hijack the active mix");

    track_t *t = &trk[0];
    host_preset(t, ENGI_FM6, 0);
    t->p[P_E2ATK] = 0; t->p[P_E2SUS] = 127; t->p[P_E2REL] = 127;
    route(t, MS_ENV2, MD_CLIP, 63);
    trk_note_on(t, 60, 100);
    int32_t out[2 * CTL];
    for (unsigned i = 0; i < 100; ++i) mix_block(out, CTL);
    assert(mod_tsrc(t, MS_ENV2, 0) > 0);
    trk_note_off(t, 60);
    for (unsigned i = 0; i < 30 * FS / CTL; ++i) mix_block(out, CTL);
    for (unsigned i = 0; i < NVOICE; ++i) assert(!t->v[i].active);
    assert(!mod_tsrc(t, MS_ENV2, 0) && !clip_eff);
    puts("FM6 ENV2 stops driving the master when its owning voice ends");

    const unsigned e[] = {ENGI_WHEEL, ENGI_DRUM, ENGI_STRING, ENGI_INPUT};
    for (unsigned i = 0; i < sizeof e / sizeof e[0]; ++i) {
        host_preset(t, e[i], 0); t->p[P_FCUT] = 90;
        route(t, MS_M1, MD_CUT, -32);
        mod_begin(t); assert(t->p[P_FCUT] < 40); mod_end(t);
        assert(t->p[P_FCUT] == 90);
    }
    host_preset(t, ENGI_ANALOG, 0); t->p[P_E4] = 90;
    route(t, MS_M1, MD_FCUT, -32);
    mod_begin(t); assert(t->p[P_E4] < 40); mod_end(t); assert(t->p[P_E4] == 90);
    host_preset(t, ENGI_INPUT, 0); t->p[P_LEVEL] = 100;
    route(t, MS_M1, MD_LEVEL, -32);
    mod_begin(t); assert(t->p[P_LEVEL] < 40); mod_end(t); assert(t->p[P_LEVEL] == 100);
    puts("CUT fallback, universal F.CUT and LEVEL reach and restore the right parameters");

    for (unsigned k = 0; k < NTRK; ++k) {
        for (unsigned j = 0; j < NMSLOT; ++j) trk[k].p[P_M1AMT + 3 * j] = 0;
        track_env[k] = track_pk[k] = 0;
    }
    route(&trk[2], MS_T1, MD_FCUT, 32);
    macro_master(); assert(track_follow_mask == 1);
    host_preset(t, ENGI_DRUM, 0); mp_lvl[0] = 4096 << 8; mp_pan[0] = 0;
    int32_t b[CTL] = {0}; dx_xon = 1;
    for (unsigned i = 0; i < CTL; ++i) { dx_sl[i] = 16000; dx_sr[i] = -16000; }
    track_follow_peak(t, b, CTL);
    assert(track_pk[0] == 16000 && mod_tsrc(&trk[2], MS_T1, 0) == 0);
    /* The immutable snapshot is published only at the next block boundary. */
    macro_master(); assert(mod_tsrc(&trk[2], MS_T1, 0) == 8000 && track_pk[0] == 0);
    assert(mod_tsrc(&trk[0], MS_T1, 0) == mod_tsrc(&trk[3], MS_T1, 0));
    trk[0].peak = 999999; macro_master(); assert(track_env[0] < 8000);
    for (unsigned i = 0; i < 1000; ++i) macro_master();
    assert(track_env[0] == 0);
    puts("stereo/antiphase followers publish one block late, ignore UI meters and decay to zero");

    dx_xon = 0;
    for (unsigned i = 0; i < CTL; ++i) b[i] = 16000;
    mp_lvl[0] = 0; track_follow_peak(t, b, CTL); assert(track_pk[0] == 0);
    mp_lvl[0] = 4096 << 8; track_follow_peak(t, b, CTL); assert(track_pk[0] == 16000);
    route(&trk[2], MS_T1, MD_FCUT, 0); macro_master(); assert(!track_follow_mask);
    ain_pk[0] = ain_pk[1] = 30000; ain_fol = 30000; uin_on = 0;
    for (unsigned i = 0; i < 2000; ++i) ain_follow();
    assert(!ain_pk[0] && !ain_pk[1] && !mod_follow);
    puts("LEVEL affects the follower; unused routes skip measurement; INPUT meters reach zero");
    return 0;
}
