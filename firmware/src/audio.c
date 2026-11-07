/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* I2S output (ALNK0 -> external codec) and the
 * audio ISR: per half buffer, blocks of CTL samples: mix_block (fx.c: events ->
 * each synth part -> dist -> level / pan -> sends -> buses -> master) -> USB audio (uac_tap) -> the metronome's
 * click (click.c, the DAC only) -> 24-bit stereo. */
/* registers: hal/fm1_audio.h */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define DAC_TICKS 544u            /* TIMER4 ticks per I2S frame (24 MHz / 44,117.6 Hz) */
#define CPU_AVG (4096u / HALF_FRAMES)   /* halves the load meter averages: ~93 ms whatever the half */
#define OUT_SHIFT 7               /* Q15 -> 24-bit, -6 dBFS ceiling (M0f ran clean at -18 dBFS) */

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

/* diagnostics, kept across resets and UBOOT entry: read with `fm1t memr` */
#define DBG_MAGIC 0x44424731u                       /* "DBG1" */
struct felucca_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots;
    uint32_t stage, page, home;           /* where the main loop is (breadcrumbs) */
    uint32_t prev_stage, prev_page, prev_home, prev_rst, prev_frames;   /* as found at boot */
} felucca_dbg __attribute__((section(".noinit")));
static volatile uint32_t audio_halves, audio_max_us;
static volatile uint32_t t5_nested_ticks;              /* TIMER4 ticks TIMER5 spent nested in this ISR (main.c) */
static uint32_t audio_cpu_rem;
static uint32_t audio_last_half = 2u, audio_lost;    /* (0.8.9) halves never rendered: a render past two halves (console) */                         /* keep the fractional IIR step: no low-load bias */
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;

static void audio_block(int32_t *out, uint32_t n)       /* mix (fx.c), then Q15 -> 24 bit */
{
    uint32_t i;
#if FELUCCA_UAC && FELUCCA_USB_IN && defined(UAC_UIN)
    if (uin_feed)                                       /* (0.7.5) MENU > USB IN+OUT: the host's audio, for the mix;
                                                         * (V10.53) USB > HOST: an audio interface's input */
        uac_in_fill(n);
    else
        uin_on = 0;
#endif
    mix_block(out, n);
#if FELUCCA_UAC
#ifdef UAC_TAP8
    if (fx_stems)                                       /* (0.7.1) MENU > USB STEMS: 8 channels (fx.c stem_buf) */
        uac_tap8(n);
    else
#endif
        uac_tap(out, n);                                /* the USB audio input: the same master output */
#endif
    if (fx_usb_fixed)                                   /* USB LEVEL FIXED: USB took the full level, the DAC */
        usb_fixed_dac(out, n);                          /* (speaker, headphones) gets MASTER's (fx.c) */
    for (i = 1; i < n; i += 2u)                         /* the HOME scope: the music, before the click */
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)out[2u * i];
    click_render(out, n);                               /* the metronome (click.c): the DAC only, not USB */
    for (i = 0; i < n; i++) {
        out[2u * i] *= 1 << OUT_SHIFT;
        out[2u * i + 1u] *= 1 << OUT_SHIFT;
    }
}

/* Overload, the last resort: a half that came late (the DMA moved on while it rendered: a click) fades one voice over
 * the next block, and one more for each late half while it lasts. Keep each part's lowest POLY note and MONO / LEGATO /
 * UNISON lead, as the shared budget does. (Until 0.9.9 cpu5 it fired on two halves in a row over 80 %: a 2.9 ms half's
 * measurement is noisy, a chord's note-ons or a cache miss pushed two over and a tail was cut with nothing late yet.
 * The averaged guard, voice.c load_half, now stops the load growing ahead of this.) */
static volatile uint8_t shed_req;
static uint32_t shed_count;

/* end of a half: all = its ticks, TIMER5 nested in it included -> the render alone in us. The deadline sees both.
 * The render's share of the half goes to the guard (voice.c load_half: an average, not one half; the admission's
 * prediction); the admission of the extra voices (12) opens on the average with the nested interrupts included:
 * a key scan or USB burst nested in the render is not the sound's, but it is the deadline's (out of line: keeps the
 * ISR's render loops as they were) */
#define HALF_TICKS ((HALF_FRAMES * 1000000u / FS) * FM1_TICKS_PER_US)   /* a half's time in TIMER4 ticks */
#define HEAD_ON 50u                                    /* the extra voices: the whole half's average under 50 % .. */
#define HEAD_OFF 65u                                   /* .. for 16 halves; over 65 % (or the guard on) they close */
static uint8_t poly_calm;                             /* consecutive halves with room for extra voices */
static __attribute__((noinline)) uint32_t shed_check(uint32_t all)
{
    uint32_t r = all - t5_nested_ticks;                /* (0.6.19) the budget on the render alone */
    load_half(r * 100u / HALF_TICKS, all * 100u / HALF_TICKS, mc_live() && !mc_off);
    /* Admission hysteresis: include nested interrupts in the spare-time test.
     * Existing notes are never killed merely because this admission closes.
     * Only FULL and a healthy second core may grow beyond the original eight. */
    if (sys_load != LOAD_FULL || !mc_live() || mc_off || load_tight || load_all_q8 > (HEAD_OFF << 8)) {
        voice_headroom = poly_calm = 0;
    } else if (!voice_headroom) {
        if (load_all_q8 < (HEAD_ON << 8)) {
            if (++poly_calm >= 16u) voice_headroom = 1;
        } else poly_calm = 0;
    }
    return r / FM1_TICKS_PER_US;
}

/* Rare overload control stays out of the audio ISR's hot instruction loop. */
static __attribute__((noinline)) void shed_voice(void)
{
    uint32_t p, i;
    voice_t *best = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !voice_held(&trk[p], v) && v->stage != 4u && !v->sub && (!best || v->env < best->env))
                best = v;
        }
    if (best) {
        voice_kill(best);
        shed_count++;
        return;
    }
    for (p = 0; p < NPART; p++) {
        const track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t), low = mode == V_POLY ? lowest_held(t) : 0u;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && voice_held(t, v) && v->stage != 4u &&
                ((mode == V_POLY && i != low) || (mode != V_POLY && i > 0u))) {
                if (!best || v->age < best->age)
                    best = v;
            }
        }
    }
    if (best) {
        voice_kill(best);
        shed_count++;
    }
}

void fm1_alnk0_irq(void)                       /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p;
    uint32_t t0;
    felucca_dbg.in_audio = 1;                   /* first: TIMER5 nests from here on (main.c) */
    p = fm1_audio_pending();
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, us;
        int32_t *o = &abuf[half * HALF_WORDS];
        fm1_audio_ack_half();                           /* (0.8.9) first: a flip during a late render stays pending */
        if (half == audio_last_half)                    /* and this ISR runs again at once for the other half (until */
            audio_lost++;                               /* 0.8.8 the ack after the render cleared it: every late half */
        audio_last_half = half;                         /* lost the next one, its stale audio played again and USB short) */
        if (shed_req) {
            shed_req = 0;
            shed_voice();
        }
#if FELUCCA_UAC
        uac_render_start();
#endif
        for (b = 0; b < HALF_FRAMES; b += CTL) {
#ifdef FM1_INPUT_LAT
            kb_out_tick = t0 + (HALF_FRAMES + b) * DAC_TICKS;   /* when this block plays (seq.c kb_lat) */
#endif
            audio_block(o + 2u * b, CTL);
        }
#if FELUCCA_UAC
        uac_render_end();                               /* (0.8.9) the half's 48 kHz resampling: to core 1, off the render */
#endif
        audio_halves++;
        us = shed_check(fm1_ticks() - t0);
        if (us > audio_max_us)
            audio_max_us = us;
        {
            uint32_t load = song.cpu_q8 * (CPU_AVG - 1u) + (us * 256u) / (HALF_FRAMES * 1000000u / FS) + audio_cpu_rem;
            song.cpu_q8 = load / CPU_AVG;
            audio_cpu_rem = load % CPU_AVG;
        }
        if (fm1_audio_free_half() != half) {
            felucca_dbg.late++;                         /* the DMA moved on while we rendered (0.8.9: caught up at
                                                         * once, its half pending; a click only past ~1.7 halves) */
            shed_req = 1;                               /* (cpu5: the last resort, a voice faded: only a late half) */
        }
        {   /* (0.6.17) MENU > ABOUT's numbers (voice.c load_diag) */
            uint32_t pc = us * 100u / (HALF_FRAMES * 1000000u / FS);
            if (pc > load_diag.peak) load_diag.peak = pc;
            load_diag.late = felucca_dbg.late;
            load_diag.shed = shed_count;
        }
        felucca_dbg.halves++;
        felucca_dbg.last_us = us;
        if (us > felucca_dbg.max_us)
            felucca_dbg.max_us = us;
        felucca_dbg.cpu_q8 = song.cpu_q8;
    }
    felucca_dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)                   /* codec and ALNK0 bring-up (fm1_audio.h) */
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}
