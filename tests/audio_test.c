/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The real audio ISR with a simulated DMA and clock: overload must free work
 * within one block without cutting another part's lead or bass. */
#define main hostsim_main
#define HOST_OWN_TICKS 1                    /* (fm1_ticks below) */
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_half_reads, host_dma_advanced, host_acks, host_nest;
static uint8_t host_pending;
static volatile uint32_t t5_nested_ticks;              /* audio.c's: TIMER5 nested in the render (main.c) */
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return host_dma_advanced && host_half_reads++ > 0u; }
static void fm1_audio_ack_half(void) { host_acks++; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1                                  /* audio.c taps USB audio: here into host_tap */
static int32_t host_tap[2 * 64];
static uint32_t host_tap_n;
static void uac_tap(const int32_t *out, uint32_t n) { memcpy(host_tap, out, 8u * n); host_tap_n = n; }
static uint32_t host_acks_at_start;
static void uac_render_start(void) { host_acks_at_start = host_acks; }   /* (just before the blocks) */
static void uac_render_end(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t host_pot = 1023;                        /* the MASTER pot as the ADC reads it (-1: timeout) */
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? host_pot : 620; }
#include "../firmware/src/master.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("audio: %-74s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fresh(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    host_tracks_init();
    for (p = 0; p < NPART; p++) host_preset(&trk[p], 0, 0);
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    audio_cpu_rem = audio_halves = audio_max_us = 0;
    shed_req = 0; shed_count = 0;
    audio_last_half = 2u; audio_lost = 0;
    load_tight = 0; voice_headroom = poly_calm = 0; memset((void *)&load_diag, 0, sizeof load_diag);
    load_avg_q8 = load_all_q8 = load_pred_q8 = 0; guard_run = guard_cap = 0; guard_calm = guard_trips = 0;
    host_clock = host_us = host_half_reads = host_dma_advanced = host_acks = host_nest = 0;
    host_pending = FM1_AUDIO_HALF;
    transport_req = panic_req = 0;
}
static voice_t *held(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == note) return &t->v[i];
    return 0;
}
static void overload(void)
{
    voice_t *lead, *bass, *extra;
    uint32_t mode, i;
    int32_t out[2 * CTL];
    fresh();
    trk[0].p[P_VOICE] = V_MONO;
    trk_note_on(&trk[0], 40, 100);
    trk[1].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[1], 48, 100);
    trk_note_on(&trk[1], 60, 100);
    lead = held(&trk[0], 40); bass = held(&trk[1], 48); extra = held(&trk[1], 60);
    shed_voice();
    check("overload keeps each part's MONO lead and lowest POLY note",
          lead && bass && extra && lead->gate && bass->gate && extra->stage == 4u && shed_count == 1u);
    check("overload: its victim leaves the budget at once (it fades, 0.6.74)", voices_busy() == 2u);
    for (i = 0; i < 6u; i++) mix_block(out, CTL);
    check("overload frees its victim within 6 control blocks (~4 ms, faded: 0.6.74, was one)", extra && !extra->active && voices_busy() == 2u);
    shed_voice();
    check("a lead alone on each part stays protected", lead->gate && bass->gate && shed_count == 1u);

    fresh();
    host_preset(&trk[0], ENGI_DRUM, 0);
    trk_note_on(&trk[0], 36, 100); trk_note_on(&trk[0], 38, 100);
    lead = held(&trk[0], 36); extra = held(&trk[0], 38);
    shed_voice();
    for (i = 0; i < 6u; i++) mix_block(out, CTL);
    check("overload frees a held one-shot drum without waiting for its decay (faded over ~4 ms)",
          lead && extra && lead->active && !extra->active && voices_busy() == 1u);

    fresh();
    trk[0].p[P_VOICE] = V_UNISON;
    trk_note_on(&trk[0], 55, 100);
    shed_voice(); mix_block(out, CTL);
    check("UNISON loses an extra voice and retains its lead", trk[0].v[0].active && trk[0].v[0].gate && voices_busy() == NVOICE - 1u);

    for (mode = V_MONO; mode <= V_LEGATO; mode++) {
        fresh(); trk[0].p[P_VOICE] = V_POLY;
        for (i = 0; i < NVOICE; i++) trk_note_on(&trk[0], 48u + 2u * i, 100);
        trk[0].p[P_VOICE] = (int16_t)mode;
        shed_voice(); mix_block(out, CTL);
        check("overload can reclaim stale POLY voices after a MONO / LEGATO change",
              shed_count == 1u && trk[0].v[0].gate && voices_busy() == NVOICE - 1u);
    }

    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[0], 60, 100); trk_note_on(&trk[0], 64, 100);
    lead = held(&trk[0], 60); extra = held(&trk[0], 64);
    trk_note_off(&trk[0], 60); trk_note_off(&trk[0], 64);
    lead->env = 10000; extra->env = 10;
    shed_voice();
    check("released voices shed the quieter tail first", extra->stage == 4u && lead->stage != 4u);
}
static void dma(void)
{
    uint32_t i, expected;
    fresh();
    host_pending = 0;
    fm1_alnk0_irq();
    check("an auxiliary interrupt does not render a DMA half", !host_acks && !audio_halves && !felucca_dbg.in_audio);
    host_pending = FM1_AUDIO_HALF;
    host_us = 5000;
    fm1_alnk0_irq();
    check("one slow half records elapsed load, neither tightens the budget nor sheds (cpu5: one spike is no overload)",
          host_acks == 1u && !shed_req && felucca_dbg.last_us == 5000u && !felucca_dbg.late && !load_tight &&
          load_diag.peak >= 80u);
    fm1_alnk0_irq();
    check("a second slow half in a row: still nothing", host_acks == 2u && !shed_req && !load_tight);
    fm1_alnk0_irq();
    check("the third in a row trips the guard (GUARD_RUN), no voice shed: nothing came late",
          host_acks == 3u && !shed_req && load_tight && guard_trips == 1u && guard_cap == GUARD_MIN);
    host_half_reads = 0; host_dma_advanced = 1; host_us = 100;
    fm1_alnk0_irq();
    check("DMA advancing during a render is counted once, and is the last resort: a voice shed",
          host_acks == 4u && felucca_dbg.late == 1u && !felucca_dbg.in_audio && shed_req);
    shed_req = 0;
    check("(0.8.9) the half is acknowledged before it renders: a flip during a late render stays pending",
          host_acks_at_start == host_acks && host_acks_at_start == 4u);
    {   /* the free half alternates: none lost; the same half twice in a row: the one between was never rendered */
        uint32_t lost0 = audio_lost;
        host_dma_advanced = 0;                         /* (free half 0 each time) */
        fm1_alnk0_irq();
        check("(0.8.9) the same free half twice in a row counts a lost half (audio_lost)", audio_lost == lost0 + 1u);
    }
    fresh();
    expected = HALF_FRAMES * 1000000u / FS;            /* the half's deadline in us */
    host_us = expected * 90u / 100u; host_nest = expected * 30u / 100u;   /* the render 60 %, TIMER5 nested 30 % */
    for (i = 0; i < 64u; i++) fm1_alnk0_irq();
    check("TIMER5 nested in the render counts toward the deadline (the extra voices), not toward the guard",
          !shed_req && !load_tight && felucca_dbg.last_us == host_us - host_nest &&
          load_all_q8 > (HEAD_OFF << 8) && load_avg_q8 < (GUARD_OFF << 8));
    fresh(); host_nest = 0;
    host_us = 5000;
    fm1_alnk0_irq(); host_us = 100; fm1_alnk0_irq(); host_us = 5000; fm1_alnk0_irq();
    check("a lone slow half between fast ones does not shed (0.6.19: a spike is not an overload)", !shed_req && !load_tight);
    fresh();                                           /* (cpu5) the averaged guard: a 95 % spike in a 70 % song is nothing */
    host_us = expected * 70u / 100u;
    for (i = 0; i < 256u; i++) fm1_alnk0_irq();
    host_us = expected * 95u / 100u; fm1_alnk0_irq();
    host_us = expected * 70u / 100u;
    for (i = 0; i < 256u; i++) fm1_alnk0_irq();
    check("guard: a song at 70 % with one 95 % half never trips (average 70, no run of 3)",
          !load_tight && !guard_trips && !shed_req && load_avg_q8 / 256u >= 68u && load_avg_q8 / 256u <= 71u);
    host_us = expected * 88u / 100u;                   /* .. at 88 % it does, on the average (no half past 92) */
    for (i = 0; i < 256u && !load_tight; i++) fm1_alnk0_irq();
    check("guard: the average past 85 % trips it (88 % halves, none past 92)", load_tight && guard_trips == 1u && i > 8u);
    host_us = expected * 75u / 100u;                   /* .. and 75 % is not calm enough to let go */
    for (i = 0; i < GUARD_STEP * 3u; i++) fm1_alnk0_irq();
    check("guard: at 75 % it holds (calm is under 70)", load_tight);
    host_us = expected * 50u / 100u;
    for (i = 0; i < GUARD_STEP * (NVOICE - GUARD_MIN + 2u) && load_tight; i++) fm1_alnk0_irq();
    check("guard: under 70 % it lets go a voice every half second, then clears",
          !load_tight && !guard_cap && i >= GUARD_STEP * (NVOICE - GUARD_MIN) && !shed_req);
    fresh();
    host_us = 580;
    for (i = 0; i < 512u; i++) fm1_alnk0_irq();
    expected = host_us * 256u / (HALF_FRAMES * 1000000u / FS);
    check("steady CPU load converges without integer smoothing bias", song.cpu_q8 >= expected - 1u && song.cpu_q8 <= expected);
}
/* MENU > USB LEVEL (fx.c fx_usb_fixed, audio.c audio_block): MASTER (default) as before, USB = the DAC's signal and
 * follows the knob; FIXED: USB at the full level whatever MASTER is (MASTER 0: USB still plays, the DAC is silent),
 * the DAC exactly USB scaled by MASTER */
static double block_rms(const int32_t *b, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < 2u * n; i++) s += (double)b[i] * b[i];
    return sqrt(s / (2.0 * n));
}
static double usb_run(uint32_t fixed, uint32_t master, int dac_is_tap_scaled_ok[1], double *dac_rms)
{
    int32_t out[2 * 32];
    uint32_t b, i;
    double usb = 0, dac = 0;
    fresh();
    lim_env = LIM_T; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; dc_l = dc_r = dce_l = dce_r = 0;    /* (the master's state as at power-on: runs compare) */
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;   /* (no tails between runs) */
    trk_note_on(&trk[0], 48, 110); trk_note_on(&trk[0], 55, 110); trk_note_on(&trk[0], 64, 110);
    fx_usb_fixed = (uint8_t)fixed;
    song.master_q12 = master;
    dac_is_tap_scaled_ok[0] = 1;
    for (b = 0; b < 400u; b++) {                       /* ~0.3 s */
        audio_block(out, 32);
        for (i = 0; i < 64u; i++) {
            int32_t want = fixed ? ((host_tap[i] * (int32_t)master) >> 12) << OUT_SHIFT : host_tap[i] << OUT_SHIFT;
            if (out[i] != want) dac_is_tap_scaled_ok[0] = 0;
        }
        if (b >= 100u) { usb += block_rms(host_tap, 32); dac += block_rms(out, 32) / (1 << OUT_SHIFT); }
    }
    fx_usb_fixed = 0;
    *dac_rms = dac / 300.0;
    return usb / 300.0;
}

/* (0.7.5) MENU > USB IN+OUT (fx.c uin_mix): the host's audio (uin_l / uin_r, as usb.c uac_in_fill leaves them) comes in
 * on the last live track: into the dry mix with its LEVEL and PAN, its REVERB send from the sum, silenced by its MUTE
 * or LEVEL 0, nothing when uin_on is clear */
static void uin_run(int32_t level, int32_t pan, int mute, int32_t rev, int on, double *lr, double *rr, double *wr)
{
    int32_t out[2 * 32];
    uint32_t b, i, t, ph = 0;
    double l = 0, r = 0, w = 0;
    fresh();
    lim_env = LIM_T; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; dc_l = dc_r = dce_l = dce_r = 0;
    song.master_q12 = 4096;
    for (t = 0; t < NTRK; t++) trk[t].p[P_DIST] = trk[t].p[P_CHOR] = trk[t].p[P_DLY] = trk[t].p[P_REV] = 0;
    t = trk_live - 1u;
    trk[t].p[P_LEVEL] = (int16_t)level; trk[t].p[P_PAN] = (int16_t)pan; trk[t].p[P_MUTE] = (int16_t)mute; trk[t].p[P_REV] = (int16_t)rev;
    for (b = 0; b < 400u; b++) {
        for (i = 0; i < 32u; i++, ph++) {
            uin_l[i] = (int32_t)(8000.0 * sin(2.0 * M_PI * 440.0 * ph / 44100.0));
            uin_r[i] = uin_l[i];
        }
        uin_on = (uint8_t)on;
        audio_block(out, 32);
        if (b >= 100u) {
            for (i = 0; i < 32u; i++) {
                l += (double)(out[2u * i] >> OUT_SHIFT) * (out[2u * i] >> OUT_SHIFT);
                r += (double)(out[2u * i + 1u] >> OUT_SHIFT) * (out[2u * i + 1u] >> OUT_SHIFT);
                w += (double)wet[i] * wet[i];
            }
        }
    }
    uin_on = 0;
    *lr = sqrt(l / 9600.0); *rr = sqrt(r / 9600.0); *wr = sqrt(w / 9600.0);
}
/* (V10.54) the INPUT engine: track 2 takes the input (preset k: 0 IN 1, 1 IN 2, 2 1+2, 3 STEREO; play: KEYS), IN 1 a
 * sine on the left, IN 2 silent; the last track's USB IN+OUT mix steps aside */
static void ain_run(uint32_t k, int keys, double *lr, double *rr)
{
    int32_t out[2 * 32];
    uint32_t b, i, t, ph = 0;
    double l = 0, r = 0;
    fresh();
    lim_env = LIM_T; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; dc_l = dc_r = dce_l = dce_r = 0;
    song.master_q12 = 4096;
    for (t = 0; t < NTRK; t++) trk[t].p[P_DIST] = trk[t].p[P_CHOR] = trk[t].p[P_DLY] = trk[t].p[P_REV] = 0;
    host_preset(&trk[1], ENGI_INPUT, k);
    trk[1].p[P_E3] = (int16_t)keys;
    trk[1].p[P_LEVEL] = 100; trk[1].p[P_PAN] = 0;
    trk[1].p[P_CHOR] = trk[1].p[P_DLY] = trk[1].p[P_REV] = 0;
    for (b = 0; b < 400u; b++) {
        for (i = 0; i < 32u; i++, ph++) {
            uin_l[i] = (int32_t)(8000.0 * sin(2.0 * M_PI * 440.0 * ph / 44100.0));
            uin_r[i] = 0;
        }
        uin_on = 1;
        audio_block(out, 32);
        if (b >= 100u)
            for (i = 0; i < 32u; i++) {
                l += (double)(out[2u * i] >> OUT_SHIFT) * (out[2u * i] >> OUT_SHIFT);
                r += (double)(out[2u * i + 1u] >> OUT_SHIFT) * (out[2u * i + 1u] >> OUT_SHIFT);
            }
    }
    uin_on = 0;
    *lr = sqrt(l / 9600.0); *rr = sqrt(r / 9600.0);
}
/* (SWARM) devices heard around (bt_air_tap, as the radio's interrupt calls it) play on an INPUT track with no USB
 * input: three devices at their own intervals, then none (they fade) */
static void sw_pkt(uint32_t dev, uint32_t lv, uint32_t ch)
{
    uint8_t p[30];
    uint32_t i;
    p[0] = 0x00; p[1] = 28;
    for (i = 2; i < 30u; i++) p[i] = (uint8_t)(dev * 41u + i * (dev + 3u) * 17u);
    bt_air_tap(p, 30, lv | lv << 8, ch);
}
static FILE *sw_wav;                                     /* (SWARM_WAV=path: the run as 16-bit stereo, to listen) */
static void sw_run(uint32_t blocks, int pkts, double *rms, double *lr)
{
    int32_t out[2 * CTL];
    uint32_t b, i;
    double e = 0, el = 0, er = 0;
    host_us = 0;
    for (b = 0; b < blocks; b++) {
        host_clock += CTL * 24000000u / 44100u;
        if (pkts && b % 70u == 0u) sw_pkt(1, 30, 37);
        if (pkts && b % 110u == 3u) sw_pkt(2, 20, 38);  /* (~160 ms) */
        if (pkts && b % 250u == 7u) sw_pkt(3, 10, 39);  /* (~360 ms) */
        uin_on = 0;                                     /* (no USB input: audio.c) */
        audio_block(out, CTL);
        for (i = 0; i < CTL; i++) {
            double l = out[2u * i] >> OUT_SHIFT, r = out[2u * i + 1u] >> OUT_SHIFT;
            e += l * l; el += l * l; er += r * r;
            if (sw_wav) { int16_t o2[2] = {(int16_t)(out[2u * i] >> 8), (int16_t)(out[2u * i + 1u] >> 8)}; fwrite(o2, 2, 2, sw_wav); }
        }
    }
    *rms = sqrt(e / (blocks * (double)CTL));
    *lr = er > 0 ? 10.0 * log10(el / er) : 99.0;
}
static void swarm_input(void)
{
    double a, q, lr, q2, f0;
    uint32_t t, v, n = 0;
    fresh();
    lim_env = LIM_T; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; dc_l = dc_r = dce_l = dce_r = 0;
    song.master_q12 = 4096;
    for (t = 0; t < NTRK; t++) trk[t].p[P_DIST] = trk[t].p[P_CHOR] = trk[t].p[P_DLY] = trk[t].p[P_REV] = 0;
    host_preset(&trk[1], ENGI_INPUT, getenv("SWARM_PRESET") ? (uint32_t)atoi(getenv("SWARM_PRESET")) : 9u);   /* (SWARM) */
    check("INPUT: SWARM is a SOURCE, its own knobs (PLUCK FM PLAY IMAGE DRIFT OCTAVE CHAOS)", trk[1].p[P_E0] == AIN_SW &&
          str_eq(ENG_INPUT.desc(&trk[1], 1)->label, "PLUCK") && str_eq(ENG_INPUT.desc(&trk[1], 7)->label, "CHAOS") &&
          !ENG_INPUT.desc(&trk[0], 1));
    trk[1].p[P_LEVEL] = 100;
    trk[1].p[P_CHOR] = trk[1].p[P_DLY] = trk[1].p[P_REV] = 0;
    if (getenv("SWARM_WAV")) sw_wav = fopen(getenv("SWARM_WAV"), "wb");
    sw_run(2000, 1, &a, &lr);
    for (v = 0; v < SW_N; v++) n += sw[v].id != 0 && sw[v].per > 0;
    printf("audio: SWARM: %.0f with three devices (%u voices timed, L/R %.1f dB), ", a, n, lr);
    if (sw_wav) sw_run(4000, 1, &q, &lr), sw_run(3000, 0, &q, &lr), fclose(sw_wav), sw_wav = 0;
    sw_run(16000, 0, &q, &lr);
    sw_run(2000, 0, &q2, &lr);
    for (v = n = 0; v < SW_N; v++) n += sw[v].id != 0;
    printf("%.1f with none around (%u voices: the FM-1's own)\n", q2, n);
    check("INPUT: SWARM: three devices heard are three voices on an INPUT track, each its own period", a > 200.0);
    check("INPUT: SWARM: none around, it still plays (the FM-1's own clocks), the devices gone", q2 > 20.0 && n == 1u);
    {   /* OCTAVE: each hit of a device an octave up, round */
        struct sw_dev *d = &sw[0];
        uint32_t o[4], k;
        trk[1].p[P_E6] = 63;                            /* (V10.6v: OCTAVE's first half, 4 octaves) */
        for (k = 0; k < 4u; k++) { uint8_t p8[30] = {0, 28, 9, 9, 9, 9, 9, 9}; bt_air_tap(p8, 30, 40, 38); for (v = 0; v < SW_N && sw[v].id != (((9u | 9u << 8 | 9u << 16 | 9u << 24) ^ (9u << 5 | 9u << 13)) | 1u); v++) {} d = &sw[v % SW_N]; uin_on = 0; { int32_t out[2 * CTL]; uint32_t w = 0; do audio_block(out, CTL); while (d->hit && ++w < 3000u); } o[k] = d->oct; }   /* (V10.6z6: a hit on the grid's next step) */
        check("INPUT: SWARM OCTAVE: each hit of a device an octave up, round", o[1] == (o[0] + 1u) % 4u && o[2] == (o[1] + 1u) % 4u);
        trk[1].p[P_E6] = 0;
    }
    (void)f0;
    {   /* SOURCE chooses: SWARM stays SWARM with USB audio coming in; another INPUT track takes the USB input */
        int32_t out[2 * CTL];
        uint32_t i, k;
        double e2 = 0, e1 = 0;
        sw_run(300, 1, &a, &lr);
        host_preset(&trk[2], ENGI_INPUT, 0);            /* (IN 1) */
        trk[2].p[P_LEVEL] = 100; trk[2].p[P_CHOR] = trk[2].p[P_DLY] = trk[2].p[P_REV] = 0;
        fx_stems = 1;
        for (k = 0; k < 200u; k++) {
            for (i = 0; i < CTL; i++) uin_l[i] = uin_r[i] = (int32_t)(6000.0 * sin(0.05 * (k * CTL + i)));
            uin_on = 1;                                 /* (uac_in_fill: a computer, a phone or an interface) */
            host_clock += CTL * 24000000u / 44100u;
            audio_block(out, CTL);
            if (k >= 100u) for (i = 0; i < CTL; i++) { e1 += (double)stem_buf[1][i] * stem_buf[1][i]; e2 += (double)stem_buf[2][i] * stem_buf[2][i]; }
        }
        printf("audio: SWARM with USB audio in: SWARM track %.0f, IN 1 track %.0f (stems)\n", sqrt(e1 / 6400.0), sqrt(e2 / 6400.0));
        check("INPUT: SOURCE SWARM and a USB input on another track play together (SWARM is not cancelled)",
              sw_on && e1 > 6400.0 * 100.0 && e2 > 6400.0 * 100.0);
        uin_on = 0;
        trk[2].p[P_E0] = AIN_IN1;
        fx_stems = 0;
    }
    trk[1].p[P_E0] = AIN_IN1;
}
static void input_engine(void)
{
    double l, r, l1, r1;
    ain_run(0, 0, &l, &r);
    printf("audio: INPUT IN 1: L %.0f R %.0f\n", l, r);
    check("INPUT: SOURCE IN 1 plays the left input on its track, centred", l > 500.0 && fabs(20.0 * log10(l / r)) < 0.2);
    check("INPUT: FOLLOW (the matrix's source) follows the input", mod_follow > 4000);
    ain_run(1, 0, &l1, &r1);
    check("INPUT: SOURCE IN 2 (silent here): nothing, the last track's USB IN+OUT mix stepped aside", l1 < 1.0 && r1 < 1.0);
    ain_run(2, 0, &l1, &r1);
    check("INPUT: SOURCE 1+2: the sum, centred", fabs(20.0 * log10(l1 / l)) < 0.3 && fabs(20.0 * log10(l1 / r1)) < 0.2);
    ain_run(3, 0, &l1, &r1);
    printf("audio: INPUT STEREO: L %.0f R %.0f\n", l1, r1);
    check("INPUT: SOURCE STEREO keeps the image: the left input on the left only", l1 > 500.0 && r1 < l1 * 0.03);
    ain_run(0, 1, &l1, &r1);
    check("INPUT: PLAY KEYS with no note: silent", l1 < 1.0 && r1 < 1.0);
}
static void usb_in(void)
{
    double l, r, w, l0, r0, w0;
    uin_run(100, 0, 0, 0, 1, &l0, &r0, &w0);           /* (V10.56) IN+OUT: no track takes it (an app monitoring its
                                                         * recording sent the FM-1's output back: a feedback loop) */
    check("USB IN+OUT: the host's audio not mixed by itself (an INPUT track takes it): no feedback loop", l0 < 1.0 && r0 < 1.0);
    fx_stems = 2;                                       /* (STEMS+IN keeps the last track's mix, below) */
    uin_run(100, 0, 0, 0, 1, &l, &r, &w);
    printf("audio: USB IN on the last track: L %.0f R %.0f wet %.0f\n", l, r, w);
    check("USB STEMS+IN: the host's audio sounds through the last live track (both sides, centred), no send: no wet",
          l > 500.0 && fabs(20.0 * log10(l / r)) < 0.1 && w == 0.0);
    uin_run(100, 0, 0, 0, 0, &l0, &r0, &w0);
    check("USB STEMS+IN: nothing when no audio comes in (uin_on clear)", l0 < 1.0 && r0 < 1.0);
    uin_run(100, 0, 1, 0, 1, &l0, &r0, &w0);
    check("USB STEMS+IN: the track's MUTE silences it", l0 < 1.0 && r0 < 1.0);
    uin_run(0, 0, 0, 0, 1, &l0, &r0, &w0);             /* (twice: the master's DC block settles after the run before) */
    uin_run(0, 0, 0, 0, 1, &l0, &r0, &w0);
    check("USB STEMS+IN: the track's LEVEL 0 silences it", l0 < 1.0 && r0 < 1.0);
    uin_run(100, -64, 0, 0, 1, &l0, &r0, &w0);
    check("USB STEMS+IN: PAN full left: the right side out", l0 > 500.0 && r0 < l0 * 0.02);
    uin_run(100, 0, 0, 127, 1, &l0, &r0, &w0);
    printf("audio: USB IN with REVERB 127: wet %.0f\n", w0);
    check("USB STEMS+IN: the track's REVERB send takes it into the reverb", w0 > 100.0);
    {   /* (0.8.0) STEMS+IN (fx_stems 2): the host's audio in a stem of its own, not in the last track's; IN+OUT with
         * the 8 stems (fx_stems 1): in the track's, as before */
        double ms = 0, ts = 0;
        uint32_t i, t = trk_live - 1u;
        fx_stems = 2;
        uin_run(100, 0, 0, 0, 1, &l0, &r0, &w0);
        for (i = 0; i < 32u; i++) { ms += fabs((double)stem_buf[NPART][i]); ts += fabs((double)stem_buf[t][i]); }
        check("USB STEMS+IN: the host's audio in its own stem, the last track's stem without it", ms > 32.0 * 1000.0 && ts < 32.0);
        fx_stems = 1;
        uin_run(100, 0, 0, 0, 1, &l0, &r0, &w0);
        ms = ts = 0;
        for (i = 0; i < 32u; i++) { ms += fabs((double)stem_buf[NPART][i]); ts += fabs((double)stem_buf[t][i]); }
        check("USB IN+OUT with the 8 stems: the host's audio in no stem (V10.56: an INPUT track takes it)", ts < 32.0 && ms == 0.0);
        fx_stems = 0;
    }
}
static void usb_level(void)
{
    int ok_full, ok_quiet, ok_off, ok_fixed, ok_fixed0;
    double d_full, d_quiet, d_off, d_fixed, d_fixed0;
    double u_full = usb_run(0, 4096, &ok_full, &d_full), u_quiet = usb_run(0, 1024, &ok_quiet, &d_quiet);
    double u_off = usb_run(0, 0, &ok_off, &d_off), u_fixed = usb_run(1, 1024, &ok_fixed, &d_fixed);
    double u_fixed0 = usb_run(1, 0, &ok_fixed0, &d_fixed0);
    printf("audio: USB / DAC rms: MASTER full %.0f / %.0f, 1/4 %.0f / %.0f, 0 %.0f / %.0f; FIXED 1/4 %.0f / %.0f, 0 %.0f / %.0f\n",
           u_full, d_full, u_quiet, d_quiet, u_off, d_off, u_fixed, d_fixed, u_fixed0, d_fixed0);
    check("USB LEVEL MASTER (default): USB is the DAC's signal and follows the knob (0: silent)",
          ok_full && ok_quiet && ok_off && u_full > 1000.0 && u_quiet < u_full * 0.5 && u_off == 0.0);
    check("USB LEVEL FIXED: USB at the full level whatever MASTER is (within 0.1 dB), MASTER 0 too",
          fabs(20.0 * log10(u_fixed / u_full)) < 0.1 && fabs(20.0 * log10(u_fixed0 / u_full)) < 0.1);
    check("USB LEVEL FIXED: the DAC gets exactly USB scaled by MASTER (MASTER 0: silent, 1/4: -12 dB)",
          ok_fixed && ok_fixed0 && d_fixed0 == 0.0 && fabs(20.0 * log10(d_fixed / u_fixed) + 12.04) < 0.2);
}

/* #137: keys pressed under the splash. main.c: master_boot before audio_init (the pot's level from the first block),
 * kb_boot_hold until the splash ends (seq.c keyboard_block: the keys sound nothing; one held across stays silent until
 * pressed again); the splash polls the pot (master_poll). boot 0: as before the fix */
static void boot_fresh(void)
{
    fresh();
    lim_env = LIM_T; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; dc_l = dc_r = dce_l = dce_r = 0;
    memset(kb_note, 0, sizeof kb_note); kb_prev = 0; kb_layer = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}
static double boot_run(int32_t pot, int boot, uint32_t *splash_peak, uint32_t *first_master, uint32_t *busy)
{
    int32_t out[2 * 32];
    uint32_t b, i, peak = 0;
    double r = 0;
    boot_fresh();
    song.master_q12 = 2048;                            /* felucca_init */
    master_knob = 512 * 16;
    host_pot = pot;
    if (boot) {
        master_boot();
        kb_boot_hold = 1;
    }
    *first_master = song.master_q12;
    for (b = 0; b < 600u; b++) {                       /* the splash, ~0.43 s: keys hammered, one held at its end */
        fm1_in.notes = b >= 580u ? 1u << 12 : (b / 40u) & 1u ? (1u << 7) | (1u << 11) : 1u << 4;
        if (b % 14u == 0u && boot)
            master_poll();
        audio_block(out, 32);
        for (i = 0; i < 64u; i++)
            if ((uint32_t)abs(out[i]) > peak) peak = (uint32_t)abs(out[i]);
    }
    *splash_peak = peak;
    kb_boot_hold = 0;                                  /* the UI */
    *busy = 0;
    for (b = 0; b < 400u; b++) {                       /* the held key (silent), let go; then a chord pressed */
        fm1_in.notes = b < 40u ? 1u << 12 : b < 60u ? 0u : (1u << 0) | (1u << 4) | (1u << 7);
        audio_block(out, 32);
        if (b >= 60u) r += block_rms(out, 32);
        if (b == 39u) *busy = voices_busy();
    }
    fm1_in.notes = 0;
    return r / 340.0;
}
static void boot_keys(void)
{
    uint32_t peak, peak0, peak_old, m, m0, m_old, busy, busy0, busy_old, ref_m, b;
    double quiet, silent, old, ref = 0;
    int32_t out[2 * 32];
    quiet = boot_run(300, 1, &peak, &m, &busy);
    silent = boot_run(0, 1, &peak0, &m0, &busy0);
    old = boot_run(300, 0, &peak_old, &m_old, &busy_old);
    boot_fresh();                                      /* the reference: the chord, MASTER at the pot's level by hand */
    song.master_q12 = ref_m = master_of_pot(300);
    for (b = 0; b < 340u; b++) {
        fm1_in.notes = (1u << 0) | (1u << 4) | (1u << 7);
        audio_block(out, 32);
        ref += block_rms(out, 32);
    }
    fm1_in.notes = 0;
    ref /= 340.0;
    printf("audio: boot, pot 300 (MASTER %u): splash peak %u, then rms %.0f (by hand %.0f); before the fix: "
           "MASTER %u, splash peak %u, then rms %.0f\n", m, peak, quiet, ref, m_old, peak_old, old);
    check("#137 boot: MASTER is the pot's level before the first block (pot 300, pot 0)",
          m == ref_m && m0 == 0u && m_old == 2048u);
    check("#137 boot: keys hammered under the splash sound nothing (before the fix: they did)",
          peak == 0u && peak0 == 0u && peak_old > 0u);
    check("#137 boot: a key held across the splash's end stays silent until pressed again",
          busy == 0u && busy0 == 0u && busy_old > 0u);
    check("#137 boot: after the splash the keys play at the pot's level, as set by hand (pot 0: silent)",
          ref > 100.0 && fabs(quiet - ref) < 1e-9 && silent == 0.0);
}

/* Admission changes the total budget, never the per-engine slots or the
 * overload guard. Six two-note parts expose the old global-eight restriction. */
static void polyphony_headroom(void)
{
    uint32_t k, i, kills, gates = 0;
    int32_t out[2 * CTL];
    fresh(); mc_host = 1; mc_off = 0; sys_load = LOAD_FULL;
    check("polyphony: starts with the conservative eight-voice budget", voice_cap() == 8);
    for (i = 0; i < 15; i++) shed_check(HALF_TICKS * 40u / 100u);
    check("polyphony: one short calm interval cannot expand admission", voice_cap() == 8);
    shed_check(HALF_TICKS * 40u / 100u);
    check("polyphony: 16 calm halves on two cores admit 12 shared voices", voice_cap() == 12);
    for (k = 0; k < NPART; k++) {
        host_preset(&trk[k], k & 1 ? ENGI_VOICE : ENGI_FM6, 0);
        trk[k].p[P_VOICE] = V_POLY; trk[k].p[P_SUS] = 127;
        trk_note_on(&trk[k], 48, 100); trk_note_on(&trk[k], 55, 100);
    }
    kills = voice_kills;
    check("polyphony: six FM6/VOICE parts keep both notes (12 voices)", voices_busy() == 12);
    for (i = 0; i < 128; i++) mix_block(out, CTL);
    for (k = 0; k < NPART; k++)
        for (i = 0; i < NVOICE; i++) gates += trk[k].v[i].active && trk[k].v[i].gate;
    check("polyphony: rendering retains all 12 gates without stealing", gates == 12 && voice_kills == kills);
    for (i = 0; i < 96; i++) shed_check(HALF_TICKS * 70u / 100u);
    check("polyphony: higher measured load (70 % on average) closes extra admission without killing held notes",
          voice_cap() == 8 && voices_busy() == 12 && voice_kills == kills && !load_tight);
    voice_headroom = poly_calm = 0;
    for (i = 0; i < 64; i++) { t5_nested_ticks = HALF_TICKS * 60u / 100u; shed_check(HALF_TICKS * 70u / 100u); }
    check("polyphony: nested IRQ load cannot open extra admission", voice_cap() == 8 && !load_tight);
    t5_nested_ticks = 0; sys_load = LOAD_LITE;
    for (i = 0; i < 32; i++) shed_check(0);
    check("polyphony: LITE stays at eight", voice_cap() == 8);
    sys_load = LOAD_ECO;
    for (i = 0; i < 32; i++) shed_check(0);
    check("polyphony: ECO stays at six", voice_cap() == 6);
    sys_load = LOAD_FULL; mc_host = 0;
    for (i = 0; i < 32; i++) shed_check(0);
    check("polyphony: single-core fallback stays at eight", voice_cap() == 8);
    fresh(); mc_host = 1; mc_off = 1;
    for (i = 0; i < 32; i++) shed_check(0);
    check("polyphony: disabled second core cannot open extra admission", voice_cap() == 8);
    mc_host = mc_off = 0; sys_load = LOAD_FULL; fresh();
}

/* A drum's MIDI key-up is not the end of its sound: its one-shot envelope
 * still owns a live hit. Reproduce loss with DRUM-X plus three synth tracks. */
static void drum_budget(void)
{
    uint32_t p, i;
    voice_t *kick, *hat;
    int32_t out[2*CTL];
    fresh(); host_preset(&trk[0], ENGI_DRUM, 0);
    trk_note_on(&trk[0], 36, 100); trk_note_off(&trk[0], 36);
    for(p=1;p<4;p++) {
        host_preset(&trk[p],p&1?ENGI_FM6:ENGI_VOICE,0);
        trk[p].p[P_VOICE]=V_MONO; trk_note_on(&trk[p],60+p,100);
    }
    for(i=0;i<8;i++) mix_block(out,CTL);
    kick=held(&trk[0],36); shed_voice();
    check("DRUM-X + 3 tracks: overload cannot shed the last live hit just because its key is up",
          kick && kick->active && kick->stage!=4 && shed_count==0);

    fresh(); host_preset(&trk[0], ENGI_DRUM, 0);
    trk_note_on(&trk[0],36,100); trk_note_off(&trk[0],36);
    voice_trims=0; load_tight=1; trk[1].p[P_VOICE]=V_MONO; trk_note_on(&trk[1],60,100);
    kick=held(&trk[0],36);
    check("DRUM-X: an unrendered pending hit is not a quiet release tail",kick && kick->stage!=4 && voice_trims==0);

    fresh(); host_preset(&trk[0], ENGI_DRUM, 0);
    trk_note_on(&trk[0],36,100); trk_note_off(&trk[0],36);
    for(p=1;p<4;p++) {
        host_preset(&trk[p],p&1?ENGI_FM6:ENGI_VOICE,0);
        trk[p].p[P_VOICE]=V_POLY;
        trk_note_on(&trk[p],48+p,100); trk_note_off(&trk[p],48+p);
        trk_note_on(&trk[p],60+p,100);
    }
    trk_note_on(&trk[1],67,100); /* eight admitted voices, including released synth tails */
    kick=held(&trk[0],36); trk_note_on(&trk[0],42,100); hat=held(&trk[0],42);
    check("DRUM-X: new hat at capacity retains the kick and takes a released synth tail",
          kick && kick->active && kick->note==36 && kick->stage!=4 && hat && hat!=kick && voices_busy()==8);
    fresh();
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    usb_level();
    boot_keys();
    overload(); dma();
    input_engine();
    swarm_input();
    usb_in();                                          /* (last: its reverb tail would reach the tests after it) */
    polyphony_headroom();
    drum_budget();
    printf(bad ? "audio: %d FAILED\n" : "audio: all passed\n", bad);
    return bad != 0;
}
