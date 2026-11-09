/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.6v) SWARM (eng_input.c ain_swarm) with fake advertisements (bt_air_tap, as the radio's interrupt calls it):
 * OCTAVE's scale walk in the track's scale and round, a new device's re-randomize glide, CHAOS 127 bounded (no overflow),
 * the block's cost. (V10.6z6) The LEAD: the closeness (PROX before AIR's nearest, else the walk), FAR / NEAR (the factory
 * pair from what old stores hold, the morph's ends), the melody on the grid (its direction from the curve, in the scale,
 * CHR the blues with its graces, FAR's pedal against NEAR's density), the devices' depth, bounded at the extremes.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/swarm_test.c -lm -o build/swarm
 * SWARM_WAV=dir: demos as WAVs into dir (V10.6z6: far_static near_static approach move_around chromatic_blues
 * devices_1_vs_8, through the track's chain and the buses); SWARM_BENCH=c: 6 devices at CHAOS c (valgrind) */
#define main hostsim_main
#define HOST_OWN_TICKS 1
static unsigned host_clock;
#define fm1_ticks() (host_clock)
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("swarm: %-78s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void pkt(uint32_t dev, uint32_t lv, uint32_t ch)
{
    uint8_t p[30];
    uint32_t i;
    p[0] = 0x00; p[1] = 28;
    for (i = 2; i < 30u; i++) p[i] = (uint8_t)(dev * 41u + i * (dev + 3u) * 17u);
    bt_air_tap(p, 30, lv | lv << 8, ch);
}
static void sw_fresh(uint32_t chaos)
{
    host_tracks_init();
    host_preset(&trk[1], ENGI_INPUT, 9u);                /* SWARM */
    trk[1].p[P_E7] = (int16_t)chaos;
    memset(sw, 0, sizeof sw);
    memset(&sm, 0, sizeof sm);
    sw_lmin = 127; sw_lmax = 0; sw_prev = 0; sw_tick = 0; sw_rnd = 0x9E3779B9u; sw_key = 0;
    sw_pxon = 0; sw_pxv = 0; sw_pxf = sw_pxn = 0;
    memset(sw_ko, 0, sizeof sw_ko); memset(sw_kt, 0, sizeof sw_kt);
    ain_tr = 1; ain_sw = 1; host_clock = 0;
    song.playing = 0; song.g[G_BPM] = 120;
}
static uint32_t fnv(uint32_t h, int32_t v) { return (h ^ (uint32_t)v) * 16777619u; }
/* blocks of SWARM; devs: packets of that many devices at their own intervals (0: the FM-1's own voice only); from[i]:
 * the block device i first speaks at (0: all from the start) */
static const uint32_t *run_from;
static uint32_t run(uint32_t blocks, uint32_t devs, FILE *w, double *rms, int32_t *peak)
{
    uint32_t b, i, h = 2166136261u;
    double e = 0;
    int32_t pk = 0;
    for (b = 0; b < blocks; b++) {
        host_clock += CTL * 24000000u / 44100u;
        for (i = 0; i < devs; i++)
            if (b % (60u + 23u * i) == i * 5u && (!run_from || b >= run_from[i])) pkt(i + 1u, 10u + 9u * (i % 4u), 37u + i % 3u);
        ain_swarm(CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = sw_on ? sw_l[i] : 0, r = sw_on ? sw_r[i] : 0;
            h = fnv(fnv(h, l), r);
            e += (double)l * l + (double)r * r;
            pk = l > pk ? l : -l > pk ? -l : r > pk ? r : -r > pk ? -r : pk;
            if (w) { int16_t o[2] = {(int16_t)clamp(l >> 1, -32767, 32767), (int16_t)clamp(r >> 1, -32767, 32767)}; fwrite(o, 2, 2, w); }
        }
    }
    if (rms) *rms = sqrt(e / (2.0 * blocks * CTL));
    if (peak) *peak = pk;
    return h;
}
static struct sw_dev *dev_of(uint32_t dev)              /* the voice of pkt's device dev */
{
    uint8_t p[30];
    uint32_t i, id;
    p[0] = 0x00;
    for (i = 2; i < 8u; i++) p[i] = (uint8_t)(dev * 41u + i * (dev + 3u) * 17u);
    id = (((uint32_t)p[2] | (uint32_t)p[3] << 8 | (uint32_t)p[4] << 16 | (uint32_t)p[5] << 24) ^ ((uint32_t)p[6] << 5 | (uint32_t)p[7] << 13)) | 1u;
    for (i = 0; i < SW_N; i++) if (sw[i].id == id) return &sw[i];
    return 0;
}
static void blocks(uint32_t n) { while (n--) { host_clock += CTL * 24000000u / 44100u; ain_swarm(CTL); } }
/* (V10.6z6) the lead's notes: a block is an onset when sw_step re-struck it (its FM envelope 32767, one block down) */
#define ONSET() (sw[SW_N].fme == 32767 - (32767 >> 5))
static int16_t (*curve)(uint32_t b);                    /* PROX's closeness at block b (0: none, AIR / the walk) */
static uint32_t lead_run(uint32_t nb, uint8_t *nt, uint32_t *at, uint32_t max)   /* nb blocks; the onsets' notes, blocks */
{
    uint32_t b, k = 0;
    for (b = 0; b < nb; b++) {
        if (curve) { sw_pxon = 1; sw_pxv = curve(b); }
        blocks(1);
        if (ONSET() && k < max) { if (nt) nt[k] = sw[SW_N].nt; if (at) at[k] = b; k++; }
    }
    return k;
}
static void states(int32_t far, int32_t near, uint32_t k) { sw_sv_put(&trk[1], 0, k, far); sw_sv_put(&trk[1], 1, k, near); }
static int16_t c_far(uint32_t b) { (void)b; return 0; }
static int16_t c_near(uint32_t b) { (void)b; return 32767; }
static int16_t c_up(uint32_t b) { return (int16_t)(2000u + (b * 2500u / 690u > 7500u ? 7500u : b * 2500u / 690u)); }   /* 2500 a second */
static int16_t c_down(uint32_t b) { return (int16_t)(9500 - (int32_t)(b * 2500u / 690u > 7500u ? 7500u : b * 2500u / 690u)); }
static int16_t c_wave(uint32_t b) { return (int16_t)(16384 + 14000 * sin(b / 690.0 * 2.0 * M_PI / 1.9) * cos(b / 690.0 * 0.7)); }
static void tests(void)
{
    uint32_t i, k, h0;
    {   /* (V10.6z6) FAR / NEAR: what old stores hold (127 in the lane / ROTOR slots, 0 in IMAGE / RTRG) reads as the factory pair */
        static const uint8_t FAR[SWS_N] = {30, 12, 24, 30, 14}, NEAR[SWS_N] = {96, 100, 100, 12, 88};
        int ok = 1, v;
        sw_fresh(0);
        for (k = 0; k < SWS_N; k++) ok &= sw_sv(&trk[1], 0, k) == FAR[k] && sw_sv(&trk[1], 1, k) == NEAR[k];
        for (k = 0; k < SWS_N; k++) ok &= TP[SW_SLOT[0][k]].def == 127 && trk[1].p[SW_SLOT[0][k]] == TP[SW_SLOT[0][k]].def;
        trk[1].p[P_LN6] = trk[1].p[P_WDUST] = trk[1].p[P_WRDEP] = 127; trk[1].p[P_DXIMG] = trk[1].p[P_LRTRG] = 0;
        for (k = 0; k < SWS_N; k++) ok &= sw_sv(&trk[1], 1, k) == NEAR[k];
        for (k = 0; k < SWS_N; k++) for (v = -5; v < 140; v++) {
            sw_sv_put(&trk[1], k & 1u, k, v);
            ok &= sw_sv(&trk[1], k & 1u, k) == clamp(v, 0, 127) && trk[1].p[SW_SLOT[k & 1u][k]] >= 0 && trk[1].p[SW_SLOT[k & 1u][k]] <= 127;
        }
        check("FAR / NEAR: the slots old projects and presets hold (127, 0) read as the factory pair; stored 0..127 both ways", ok);
    }
    {   /* the morph's ends: FAR's values at closeness 0, NEAR's at full (RATE's grid, TONE's tone, MOD's depth) */
        int ok;
        uint32_t ri0, ri1, tn0, tn1, md0, md1;
        sw_fresh(0);
        curve = c_far; lead_run(2000, 0, 0, 0);
        ri0 = sm.ri; tn0 = sw[SW_N].tn; md0 = sm.md;
        ok = sm.mq == 0 && sm.src == 2u;
        curve = c_near; lead_run(2000, 0, 0, 0);
        ri1 = sm.ri; tn1 = sw[SW_N].tn; md1 = sm.md;
        ok &= sm.mq == 128;
        ok &= ri0 == 30u * 6u / 128u && ri1 == 96u * 6u / 128u && tn0 == 40u + 24u * 3u / 2u && tn1 == 40u + 100u * 3u / 2u;
        ok &= md0 == 14u + 20u && md1 == 88u + 20u;   /* (PROX alone: its base, 20) */
        printf("swarm:   FAR: grid %u, tone %u, depth %u; NEAR: grid %u, tone %u, depth %u\n", ri0, tn0, md0, ri1, tn1, md1);
        check("the morph's ends: closeness 0 plays FAR's values exactly, full closeness NEAR's (grid, tone, depth)", ok);
        curve = 0;
    }
    {   /* PROX before AIR: with devices around and a central's PROX, PROX gives the closeness; without it the nearest */
        int ok;
        sw_fresh(0);
        for (k = 0; k < 400u; k++) { pkt(1, 8, 37); pkt(2, 12, 38); blocks(3); }   /* (two far devices) */
        ok = sm.src == 1u && sm.nd == 2u && sm.mq < 64;
        printf("swarm:   AIR two devices (the range's): closeness %u, source %u\n", sm.mq, sm.src);
        sw_pxon = 1; sw_pxv = 30000;
        for (k = 0; k < 100u; k++) { pkt(1, 8, 37); pkt(2, 12, 38); blocks(3); }
        ok &= sm.src == 2u && sm.mq > 110;
        sw_pxon = 0; sw_pxf = 10 << 8; sw_pxn = 50 << 8;   /* (PROX's points stored: AIR's devices through them) */
        for (k = 0; k < 1200u; k++) { pkt(1, 8, 37); pkt(2, 15, 38); blocks(3); }   /* (the nearer: level 30, half way) */
        ok &= sm.src == 1u && sm.mq >= 60 && sm.mq <= 68;
        printf("swarm:   AIR through PROX's points (10 .. 50), the nearest at 30: closeness %u of 128\n", sm.mq);
        sw_pxf = sw_pxn = 0;
        memset(sw, 0, sizeof sw);
        for (k = 0; k < 100u; k++) blocks(3);
        ok &= sm.src == 0u;
        check("the closeness: PROX (a central) before AIR's nearest device (its level, through PROX's points), else none", ok);
    }
    {   /* each device more depth: 1 mild .. 8 extreme; CHAOS the ceiling */
        uint8_t d1, d8;
        int ok;
        sw_fresh(127);
        for (k = 0; k < 300u; k++) { pkt(1, 20, 37); blocks(3); }
        d1 = sm.md;
        for (k = 0; k < 300u; k++) { for (i = 1; i <= 8u; i++) pkt(i, 20, 37); blocks(3); }
        d8 = sm.md;
        ok = sm.nd == 8u && d1 >= 20 && d1 < 50 && d8 >= 100 && d8 - d1 == 84;
        printf("swarm:   depth with 1 device %u, with 8 %u (of 127; CHAOS 127 x it: the chain, the fold, the ring)\n", d1, d8);
        check("each device heard adds depth (1 mild .. 8 extreme), CHAOS its ceiling", ok);
    }
    {   /* FAR: a pedal, slow; NEAR: fast, many notes, many places */
        uint8_t nt[400];
        uint32_t at[400], nf, nn, df = 0, dn = 0, grid = 1;
        uint32_t seen[128] = {0};
        sw_fresh(0);
        curve = c_far; lead_run(1000, 0, 0, 0);
        nf = lead_run(6900, nt, at, 400);
        for (i = 0; i < nf; i++) if (!seen[nt[i]]++) df++;
        memset(seen, 0, sizeof seen);
        curve = c_near; lead_run(1000, 0, 0, 0);
        nn = lead_run(6900, nt, at, 400);
        for (i = 0; i < nn; i++) if (!seen[nt[i]]++) dn++;
        {
            uint32_t len = beat_samples() / 4u;          /* (NEAR's 1/16) */
            for (i = 0; i < nn; i++) grid &= ((uint64_t)(at[i] + 1000u + 6900u + 1000u) * CTL) % len < CTL;   /* (on the grid) */
        }
        printf("swarm:   10 s: FAR %u notes (%u pitches), NEAR %u notes (%u pitches)\n", nf, df, nn, dn);
        check("FAR a slow pedal (1/2: one note, re-struck softly); NEAR fast and moving (1/16, many pitches); all on the grid",
              nf >= 9u && nf <= 11u && df == 1u && nn >= 55u && dn >= 6u && grid);
        curve = 0;
    }
    {   /* the curve's direction: rising up the scale, falling down, flat the same note (MOVE 0: no figures) */
        uint8_t nt[200];
        uint32_t nu, nd2, up = 1, dn = 1;
        sw_fresh(0);
        states(0, 0, SWS_MOVE); states(70, 70, SWS_RATE);   /* (1/8 both: 4 a second at 120) */
        trk[1].p[P_ROOT] = 2; trk[1].p[P_SCALE] = 2;
        curve = c_far; lead_run(1500, 0, 0, 0);
        curve = c_up; nu = lead_run(690u * 3u, nt, 0, 200);   /* (12 steps: within the two octaves) */
        for (i = 1; i < nu; i++) up &= nt[i] > nt[i - 1];
        printf("swarm:   rising:"); for (i = 0; i < nu && i < 14u; i++) printf(" %u", nt[i]);
        curve = c_down; nd2 = lead_run(690u * 3u, nt, 0, 200);
        for (i = 1; i < nd2; i++) dn &= nt[i] < nt[i - 1];
        printf("; falling:"); for (i = 0; i < nd2 && i < 14u; i++) printf(" %u", nt[i]); printf("\n");
        curve = c_far; lead_run(1500, 0, 0, 0);
        {
            uint8_t st[20];
            k = lead_run(690u * 4u, st, 0, 20);
            for (i = 1; i < k; i++) up &= st[i] == st[0];
        }
        check("the proximity's curve moves the melody: rising up the scale, falling down; still: the same note", up && dn && nu >= 10u && nd2 >= 10u && k >= 14u);
        curve = 0;
    }
    {   /* in the track's scale; CHR: the blues (1 b3 3 4 b5 5 b7), its blue notes scooped from below */
        uint8_t nt[600];
        uint32_t n1, n2, ok = 1, gr = 0, grok = 1, b;
        sw_fresh(0);
        trk[1].p[P_ROOT] = 2; trk[1].p[P_SCALE] = 2;    /* D minor */
        curve = c_wave;
        n1 = lead_run(690u * 20u, nt, 0, 600);
        for (i = 0; i < n1; i++) ok &= scale_snap(&trk[1], nt[i]) == nt[i];
        trk[1].p[P_SCALE] = 0;                          /* CHR */
        for (b = 0, n2 = 0; b < 690u * 20u; b++) {
            sw_pxon = 1; sw_pxv = c_wave(b); blocks(1);
            if (ONSET()) {
                uint32_t pc = (sw[SW_N].nt + 120u - 2u) % 12u;
                n2++;
                ok &= (0x4F9u >> pc & 1u) != 0u;
                if (sw[SW_N].dr == -14) { gr++; grok &= (0x4D8u >> pc & 1u) != 0u; }
            }
        }
        printf("swarm:   D minor %u notes; CHR (the blues) %u notes, %u scooped (b3 3 b5 5 b7)\n", n1, n2, gr);
        check("the lead in the track's scale; CHR plays the blues, its blue notes graced from a semitone below", ok && grok && n1 > 50u && n2 > 50u && gr > 5u);
        curve = 0;
    }
    {   /* AIR's hits wait for the grid (no irregular rhythm): a packet's accent comes on the next step */
        struct sw_dev *d;
        uint32_t w = 0;
        sw_fresh(0);
        states(0, 0, SWS_RATE);                          /* (1/1: a step every 2 s at 120) */
        blocks(10);
        pkt(1, 30, 38);
        for (d = 0; w < 1500u && (!(d = dev_of(1)) || d->hit); w++) blocks(1);
        printf("swarm:   a packet at block 10, its hit at block %u (1/1 = 1378.1 blocks)\n", 9u + w);
        check("a device's hit waits for the grid's next step (1/1 here: up to 2 s)", d && !d->hit && 9u + w == 1379u);
    }
    {   /* OCTAVE's second half: each hit the next degree of the scale (D minor), over 2..8 degrees, round */
        static const uint8_t OC[3] = {64, 80, 127}, RN[3] = {2, 3, 8};
        int ok = 1;
        for (k = 0; k < 3u; k++) {
            uint8_t nt[20];
            struct sw_dev *d;
            sw_fresh(0);
            trk[1].p[P_ROOT] = 2; trk[1].p[P_SCALE] = 2; trk[1].p[P_E6] = OC[k];
            states(127, 127, SWS_RATE);                  /* (1/32: the hits on its grid) */
            for (i = 0; i < 20u; i++) { pkt(1, 30, 38); do blocks(1); while ((d = dev_of(1)) && d->hit); nt[i] = d ? d->nt : 0; }
            for (i = 0; i < 20u; i++) {
                ok &= scale_snap(&trk[1], nt[i]) == nt[i] && nt[i] >= d->note && nt[i] <= d->note + 14u;
                if (i + RN[k] < 20u) ok &= nt[i + RN[k]] == nt[i];
                if (i && (i + 1u) % RN[k]) ok &= nt[i] > nt[i - 1u];
                if (!((i + 1u) % RN[k])) ok &= nt[i] == d->note;   /* (round: its own note again) */
            }
            if (k == 2u) { printf("swarm:   OCTAVE 127 (8 degrees) by hit:"); for (i = 0; i < 12u; i++) printf(" %u", nt[i]); printf("\n"); }
        }
        trk[1].p[P_E6] = 40;                            /* (the first half: octaves, 0..3 more: 40 / 16 = 2, 3 places) */
        {
            uint8_t o[6];
            struct sw_dev *d;
            for (i = 0; i < 6u; i++) { pkt(1, 30, 38); do blocks(1); while ((d = dev_of(1)) && d->hit); o[i] = d->oct; }
            ok &= o[1] == (o[0] + 1u) % 3u && o[2] == (o[1] + 1u) % 3u && o[3] == o[0];
        }
        check("OCTAVE's second half: a walk of the track's scale (2, 3, 8 degrees) on each hit, in the scale, round; its first half octaves", ok);
    }
    {   /* a new device: the knobs glide to new places (~1.5 s), the old one's packets do not move them */
        int16_t kt[4], k1[4];
        int ok, moved = 0, far = 0;
        sw_fresh(0);
        pkt(1, 30, 38); blocks(1);
        memcpy(kt, sw_kt, sizeof kt); memcpy(k1, sw_ko, sizeof k1);
        for (i = 0; i < 4u; i++) { moved |= kt[i] != 0; far |= abs(k1[i]) > abs(kt[i]) / 8 + 256; }
        ok = moved && !far;
        blocks(1030);                                   /* (~1.5 s) */
        for (i = 0; i < 4u; i++) ok &= abs(sw_ko[i] - kt[i]) <= abs(kt[i]) * 3 / 10 + 512 && abs(sw_ko[i]) >= abs(kt[i]) / 2 - 256;
        for (i = 0; i < 20u; i++) { pkt(1, 30, 38); blocks(20); }
        ok &= !memcmp(kt, sw_kt, sizeof kt);
        pkt(2, 20, 37); blocks(1);
        ok &= memcmp(kt, sw_kt, sizeof kt) != 0;
        printf("swarm:   knob offsets after a new device: PLUCK %d FM %d IMAGE %d DRIFT %d (of the knob's 127)\n",
               kt[0] / 256, kt[1] / 256, kt[2] / 256, kt[3] / 256);
        check("a new device: PLUCK FM IMAGE DRIFT glide to new places over ~1.5 s (not a jump); a known one's packets do not", ok);
    }
    {   /* each device its own timbre; its family from its advertisement (Apple, Microsoft, appearance) */
        static const uint8_t APL[16] = {0, 14, 1, 2, 3, 4, 5, 6, 2, 1, 6, 4, 0xFF, 0x4C, 0x00, 0x10};
        static const uint8_t MS[13] = {0, 11, 1, 2, 3, 4, 5, 6, 4, 0xFF, 0x06, 0x00, 0x01};
        static const uint8_t WT[12] = {0, 10, 1, 2, 3, 4, 5, 6, 3, 0x19, 0xC1, 0x00};   /* (appearance 0x00C1: a watch) */
        int ok = sw_family(APL, 16) == 1u && sw_family(MS, 13) == 2u && sw_family(WT, 12) == 3u;
        uint8_t z[30] = {0, 28, 9, 9, 9, 9, 9, 9};
        ok &= sw_family(z, 30) == 0u;
        sw_fresh(0);
        for (i = 1; i <= 6u; i++) pkt(i, 30, 38);
        blocks(1);
        {
            uint32_t sh = 0, rt = 0;
            for (i = 1; i <= 6u; i++) { struct sw_dev *d = dev_of(i); ok &= d && d->tn != 255u; sh |= 1u << (d->sh >> 5); rt |= 1u << d->rt; }
            ok &= __builtin_popcount(sh) >= 3 && __builtin_popcount(rt) >= 2;
            for (i = 1; i <= 6u; i++) { struct sw_dev *d = dev_of(i); if (d) printf("swarm:   device %u: shape %u ratio %u depth %u tone %u\n", i, d->sh, d->rt, d->fi, d->tn); }
        }
        printf("swarm:   families %u %u %u %u\n", sw_family(APL, 16), sw_family(MS, 13), sw_family(WT, 12), sw_family(z, 30));
        check("each device its own timbre (shape, FM ratio, depth, tone); a family by manufacturer / appearance", ok);
    }
    {   /* a swell in, a long fade out, a ninth device takes the oldest's voice without a click */
        struct sw_dev *d;
        int32_t a10, a300, a_before, a_after, m = 0;
        int ok;
        sw_fresh(0);
        pkt(1, 30, 38); blocks(10); a10 = dev_of(1)->amp;
        blocks(290); a300 = dev_of(1)->amp;
        ok = a10 < a300 / 2 && a300 > 500;
        blocks(6u * 690u - 300u);                       /* (6 s unheard: it leaves) */
        d = dev_of(1);
        a300 = d->amp;
        blocks(345); ok &= d->amp > a300 * 9 / 20 && d->amp < a300 * 9 / 10;   /* (half a second into its fade: most of it) */
        blocks(16u * 690u); ok &= !dev_of(1);
        sw_fresh(0);
        for (i = 1; i <= 8u; i++) { pkt(i, 30, 38); blocks(200); }
        a_before = 0;
        for (i = 0; i < SW_N; i++) if (sw[i].id == dev_of(1)->id) { k = i; a_before = sw[i].amp; }
        pkt(9, 30, 38); blocks(1);
        a_after = sw[k].amp;
        ok &= !dev_of(1) && dev_of(9) == &sw[k] && a_after > a_before / 2;
        for (i = 0; i < CTL; i++) m = abs(sw_l[i]) > m ? abs(sw_l[i]) : m;
        printf("swarm:   swell %d -> %d; fade %d; ninth %d -> %d\n", a10, a300, d ? d->amp : -1, a_before, a_after);
        check("arrival swells (~0.4 s), departure fades long (~1.5 s), a ninth device takes the oldest's voice, its level kept", ok);
    }
    {   /* none around, CHAOS 0: the FM-1's own voice and the lead play, the same each time */
        double r;
        sw_fresh(0);
        h0 = run(3000, 0, 0, &r, 0);
        sw_fresh(0);
        check("no device around: the FM-1's own voice and the lead play (the walk), the same run the same sound", r > 300.0 && run(3000, 0, 0, 0, 0) == h0);
    }
    {   /* CHAOS: 0 / 64 / 127 with six devices: bounded, another sound each, never silent, no overflow */
        static const uint8_t C[3] = {0, 64, 127};
        double r[3];
        int32_t pk[3];
        uint32_t hh[3];
        int ok = 1;
        for (k = 0; k < 3u; k++) {
            sw_fresh(C[k]);
            hh[k] = run(3000, 6, 0, &r[k], &pk[k]);
            ok &= r[k] > 500.0 && pk[k] < (1 << 20) && r[k] == r[k];
        }
        printf("swarm:   six devices: CHAOS 0 rms %.0f peak %d, 64 rms %.0f peak %d, 127 rms %.0f peak %d\n",
               r[0], pk[0], r[1], pk[1], r[2], pk[2]);
        ok &= hh[0] != hh[1] && hh[1] != hh[2];
        check("CHAOS 0 / 64 / 127 with six devices: each another sound, bounded (no overflow), never silent", ok);
    }
    {   /* the extremes: every value 127 (and 0), eight devices, CHAOS 127, PROX swinging: bounded */
        int32_t pk = 0, pk0 = 0;
        double r;
        sw_fresh(127);
        for (k = 0; k < SWS_N; k++) states(127, 127, k);
        for (k = 0; k < 2000u; k++) {
            if (!(k % 7u)) for (i = 1; i <= 8u; i++) pkt(i, 5u + 7u * i, 37u + i % 3u);
            sw_pxon = 1; sw_pxv = (int16_t)(k & 64u ? 32767 : 0);
            blocks(1);
            for (i = 0; i < CTL; i++) pk = abs(sw_l[i]) > pk ? abs(sw_l[i]) : abs(sw_r[i]) > pk ? abs(sw_r[i]) : pk;
        }
        sw_fresh(127);
        for (k = 0; k < SWS_N; k++) states(0, 0, k);
        run(2000, 8, 0, &r, &pk0);
        printf("swarm:   every value 127, 8 devices, CHAOS 127: peak %d; every value 0: peak %d\n", pk, pk0);
        check("the extremes (every FAR / NEAR value 127 or 0, 8 devices, CHAOS 127, PROX swinging): bounded", pk < (1 << 20) && pk > 1000 && pk0 < (1 << 20));
    }
}
/* (V10.6v) the RAMP demo: an ANALOG bass pattern (1/16), a 4-bar EXP RAMP on CUT from OPP (0) to the step's lock (120) on
 * step 1, 6 bars: the riser, then the knob's 40 */
static void wav_open(FILE *f, uint32_t frames)
{
    uint32_t v;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 1 | 2u << 16; fwrite(&v, 4, 1, f); v = 44100; fwrite(&v, 4, 1, f);
    v = 44100 * 4; fwrite(&v, 4, 1, f); v = 4 | 16u << 16; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f);
    v = frames * 4; fwrite(&v, 4, 1, f);
}
static void ramp_demo(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, b, blocks;
    int32_t o[2 * CTL];
    track_t *t = &trk[0];
    if (!f) return;
    host_tracks_init();
    song.g[G_BPM] = 124;
    host_preset(t, ENGI_ANALOG, 0);
    for (i = 1; i < NTRK; i++) trk[i].p[P_LEVEL] = 0;
    t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_SGATE] = 70;
    for (i = 0; i < 16u; i++) {
        uint8_t n = (uint8_t)(36 + (i % 8u == 3u ? 12 : i % 8u == 6u ? 7 : 0));
        put_step(t, i, 1, &n, ST_NOTE, i % 4u == 0u ? SF_ACCENT : 0u);
    }
    t->p[P_E4] = 40; t->p[P_E5] = 80;
    t->step[0].comp = CO_MAKE(CA_RAMP, CC_ALWAYS); t->step[0].carg = CV_RAMP_BARS | 7u; t->step[0].clen[0] = 3;
    motion_set_lock(t, 0, P_E4, 120);
    transport_req = 1;
    blocks = 6u * 16u * div_samples(2) / CTL;
    wav_open(f, blocks * CTL);
    for (b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) { int16_t q[2] = {(int16_t)clamp(o[2u * i], -32767, 32767), (int16_t)clamp(o[2u * i + 1u], -32767, 32767)}; fwrite(q, 2, 2, f); }
    }
    transport_req = 2;
    mix_block(o, CTL);
    fclose(f);
    printf("swarm: demo %s\n", path);
}
/* (V10.6z6) PROX as bt.c gives it: a packet ~every 6 blocks (~110 a second), its level from the distance (log-distance,
 * n = 2, -40 dBm at 1 m, the fading's few dB at random), the median of nine, one pole of 1/32, through FAR / NEAR points at
 * 6 m and 0.3 m (as MENU > SYSTEM > BLUETOOTH's PROX page stores them) */
static double px_w[9], px_s;
static uint32_t px_n, px_r = 12345u;
static double px_gauss(void) { double u = 0; int i; for (i = 0; i < 6; i++) { px_r = px_r * 1664525u + 1013904223u; u += (px_r >> 8) / 16777216.0; } return (u - 3.0) * 1.41; }
static int16_t prox_at(double d)
{
    double m[9], x;
    uint32_t i, j, k;
    px_w[px_n++ % 9u] = -40.0 - 20.0 * log10(d) + 3.0 * px_gauss();
    k = px_n < 9u ? px_n : 9u;
    for (i = 0; i < k; i++) { x = px_w[i]; for (j = i; j && m[j - 1] > x; j--) m[j] = m[j - 1]; m[j] = x; }
    px_s = px_n == 1u ? m[k / 2] : px_s + (m[k / 2] - px_s) / 32.0;
    x = (px_s - (-40.0 - 20.0 * log10(6.0))) / (20.0 * log10(6.0 / 0.3));
    return (int16_t)(x < 0 ? 0 : x > 1 ? 32767 : x * 32767);
}
static double d_far(double t) { (void)t; return 5.5; }
static double d_near(double t) { (void)t; return 0.35; }
static double d_approach(double t) { return t < 2 ? 6.0 : t > 20 ? 0.3 : 6.0 - 5.7 * (t - 2) / 18.0; }   /* (a walk in, 6 m .. 0.3 m) */
static double d_around(double t)                        /* (a hand over the antenna, moving: PROX's quick curve) */
{
    double d = 0.9 + 0.6 * sin(t * 2 * M_PI / 1.6) + 0.35 * sin(t * 2 * M_PI / 4.3 + 1) + 0.2 * sin(t * 2 * M_PI / 0.7);
    return d < 0.25 ? 0.25 : d;
}
static double d_blues(double t) { return 1.6 + 1.1 * sin(t * 2 * M_PI / 6.0) + 0.5 * sin(t * 2 * M_PI / 2.3); }
/* name: the WAV; secs; dist: the PROX curve (0: AIR, devices_1_vs_8's); chr: CHR (the blues), else D minor */
static void demo(const char *dir, const char *name, double secs, double (*dist)(double), int chr)
{
    char path[512];
    FILE *f;
    uint32_t b, i, nb = (uint32_t)(secs * 44100.0 / CTL), nd;
    int32_t o[2 * CTL];
    double e = 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "wb"))) return;
    sw_fresh(100);                                       /* (the SWARM preset: CHAOS 100) */
    for (i = 0; i < NTRK; i++) if (i != 1u) trk[i].p[P_LEVEL] = 0;
    trk[1].p[P_ROOT] = 2; trk[1].p[P_SCALE] = chr ? 0 : 2;
    px_n = 0;
    wav_open(f, nb * CTL);
    for (b = 0; b < nb; b++) {
        double t = b * (double)CTL / 44100.0;
        host_clock += CTL * 24000000u / 44100u;
        if (dist) {
            if (!(b % 6u)) sw_pxv = prox_at(dist(t));
            sw_pxon = 1;
        } else {                                         /* AIR: one device (moving slowly) for 10 s, then seven more
                                                          * around it, farther */
            sw_pxf = 10 << 8; sw_pxn = 70 << 8;
            nd = t < 10.0 ? 1u : 8u;
            for (i = 0; i < nd; i++)
                if (b % (60u + 23u * i) == i * 5u)
                    pkt(i + 1u, i ? 5u + i : (uint32_t)(20.0 + 9.0 * sin(t * 2 * M_PI / 7.0) + 2.0 * px_gauss()), 37u + i % 3u);
        }
        mix_block(o, CTL);
        if (getenv("SWARM_LOG") && ONSET())             /* (the lead's notes: when, which, the morph) */
            printf("swarm:   %s %6.2f s note %3u morph %3u devices %u depth %3u\n", name, t, sw[SW_N].nt, sm.mq, sm.nd, sm.md);
        for (i = 0; i < CTL; i++) {
            int16_t q[2] = {(int16_t)clamp(o[2u * i], -32767, 32767), (int16_t)clamp(o[2u * i + 1u], -32767, 32767)};
            e += (double)q[0] * q[0];
            fwrite(q, 2, 2, f);
        }
    }
    fclose(f);
    printf("swarm: demo %s (%.0f s, rms %.0f)\n", path, secs, sqrt(e / (nb * (double)CTL)));
}
int main(void)
{
    if (getenv("SWARM_DBG")) { uint32_t b,v; sw_fresh(0); pkt(1,30,38); for(b=0;b<6000;b++){ blocks(1); if(b%250==0){ printf("b%u on%u ",b,sw_on); for(v=0;v<SW_N;v++) if(sw[v].id) printf("[%u %x amp%d acc%d in%u]",v,sw[v].id,sw[v].amp,sw[v].acc,sw[v].in); printf("\n");} } return 0; }
    if (getenv("SWARM_BENCH")) {                         /* (valgrind: ain_swarm's instructions, 6 devices at CHAOS c) */
        sw_fresh((uint32_t)atoi(getenv("SWARM_BENCH")));
        run(2000, 6, 0, 0, 0);
        return 0;
    }
    tests();
    {
        double r; int32_t pk; struct timespec a, z; uint32_t k;
        for (k = 0; k < 2u; k++) {
            double best = 1e18, ns; uint32_t rep;
            for (rep = 0; rep < 5u; rep++) {
                sw_fresh(k ? 127u : 0u); run(500, 6, 0, 0, 0);
                clock_gettime(CLOCK_MONOTONIC, &a);
                run(6000, 6, 0, 0, 0);
                clock_gettime(CLOCK_MONOTONIC, &z);
                ns = ((z.tv_sec - a.tv_sec) * 1e9 + (z.tv_nsec - a.tv_nsec)) / 6000.0;
                best = ns < best ? ns : best;
            }
            printf("swarm: cost %.0f ns a block (6 devices, CHAOS %u)\n", best, k ? 127u : 0u);
        }
    }
    if (getenv("SWARM_WAV")) {
        char path[512];
        snprintf(path, sizeof path, "%s/ramp_analog_cut_4bar_exp_opp.wav", getenv("SWARM_WAV"));
        ramp_demo(path);
        demo(getenv("SWARM_WAV"), "far_static.wav", 15, d_far, 0);
        demo(getenv("SWARM_WAV"), "near_static.wav", 15, d_near, 0);
        demo(getenv("SWARM_WAV"), "approach.wav", 24, d_approach, 0);
        demo(getenv("SWARM_WAV"), "move_around.wav", 20, d_around, 0);
        demo(getenv("SWARM_WAV"), "chromatic_blues.wav", 20, d_blues, 1);
        demo(getenv("SWARM_WAV"), "devices_1_vs_8.wav", 20, 0, 0);
    }
    printf(bad ? "swarm: %d FAILED\n" : "swarm: all ok\n", bad);
    return bad != 0;
}
