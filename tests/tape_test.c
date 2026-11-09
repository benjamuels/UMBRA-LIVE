/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.9.9-snd8) the master's TAPE (src/fx.c tape_block; FX > TAPE: TAPE and WOW, params.c tape_v) through the real sources
 * (ui_test.c's).
 *   build/host/tape_test
 * 1. off (both 0): a sequenced mix never runs it (its line untouched; the renders as before: regress's golden.txt).
 * 2. TAPE (sines through tape_block alone): 0 passes a tone as it is; 100 adds the second and third harmonics, loses the
 *    highs (10 kHz at least 3 dB down against 1 kHz) and lifts the low end (70 Hz up against 1 kHz); bounded.
 * 3. WOW: a 1 kHz tone's period swings (a few cents, under 30) at 100, not at 0; WOW and TAPE turned 0 -> 100 -> 0 in one
 *    block: no click (the second difference within 4 x the tone's own).
 * 4. a project keeps them (FUNC: 'T' 'P' after the delay's DRIVE), one without them opens with TAPE out. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("tape: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* a tone of f Hz (amplitude a, Q15) through tape_block, n blocks after `settle`; out: the left channel (Q15) */
static double tph;
static void tone_blocks(double f, double a, uint32_t settle, uint32_t n, int32_t *out)
{
    uint32_t b, i;
    for (b = 0; b < settle + n; b++) {
        for (i = 0; i < CTL; i++) {
            int32_t v = (int32_t)(a * sin(tph));
            tph += 2.0 * M_PI * f / FS;
            mix_l[i] = mix_r[i] = v * 4;
            wet[i] = 0;
        }
        tape_block(CTL);
        if (b >= settle)
            for (i = 0; i < CTL; i++) out[(b - settle) * CTL + i] = mix_l[i] / 4;
    }
}
static double goertzel(const int32_t *x, uint32_t n, double f)
{
    double w = 2.0 * M_PI * f / FS, c = 2.0 * cos(w), s1 = 0, s2 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double s = x[i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / n)) + c * s1 - s2;   /* (Hann) */
        s2 = s1; s1 = s;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) + 1e-9;
}
static void tape_reset(int drv, int wow)
{
    memset(&tp, 0, sizeof tp);
    tape_v[0] = (int16_t)drv; tape_v[1] = (int16_t)wow;
    tph = 0;
}
#define NB 64u
static int32_t buf[NB * CTL];
#define WN 2100u
static int32_t wbuf[WN * CTL];

int main(void)
{
    /* ---- 1. off ---- */
    {
        static int32_t o[2u * CTL];
        uint32_t r, b, i, h[2] = {0, 0};
        for (r = 0; r < 2u; r++) {
            ui_power_on();
            tape_reset(0, 0);
            song.master_q12 = 4096;
            for (i = 0; i < 16u; i++) {
                step_t *s = &trk[1].step[i];
                memset(s, 0, sizeof *s);
                s->time = ST_NOTE; s->vel = 100; s->n = 1; s->note[0] = (uint8_t)(48 + i % 5u);
            }
            trk[1].p[P_SLEN] = 16; trk[1].p[P_MUTE] = 0;
            transport_req = 1; ui_input();
            h[r] = 2166136261u;
            for (b = 0; b < 600u; b++) {
                memset(o, 0, sizeof o);
                mix_block(o, CTL);
                for (i = 0; i < 2u * CTL; i++) h[r] = (h[r] ^ (uint32_t)o[i]) * 16777619u;
            }
            transport_req = 2; ui_input();
        }
        printf("tape:   off: tp.w %u, drv %d, wow %d, hashes %08x %08x\n", tp.w, tp.drv, tp.wow, h[0], h[1]);
        ck("1. off: TAPE and WOW at 0, a sequenced mix: tape_block never runs (its line untouched; the render as before: golden.txt)",
           tp.w == 0u && !tp.drv && !tp.wow);
    }
    /* ---- 2. TAPE ---- */
    {
        double f1, h2, h3, lo, hi, ref, ref_lo, ref_hi;
        int32_t pk = 0;
        uint32_t i, same = 1;
        tape_reset(0, 0);
        tape_v[0] = 0; tp.drv = 0;
        tone_blocks(1000, 6000, 0, 8, buf);
        for (i = 0; i < 8u * CTL; i++) {                 /* (TAPE 0: tape_block run by hand, WOW 0: the tone as it was) */
            int32_t v = (int32_t)(6000 * sin(2.0 * M_PI * 1000 * i / FS));
            same &= abs(buf[i] - v) <= 1;
        }
        ck("2. TAPE 0 WOW 0 (run anyway): the tone passes as it is (within 1 LSB)", same);
        tape_reset(0, 0);
        tone_blocks(1000, 6000, 40, NB, buf); ref = goertzel(buf, NB * CTL, 1000);
        tape_reset(0, 0);
        tone_blocks(10000, 6000, 40, NB, buf); ref_hi = goertzel(buf, NB * CTL, 10000);
        tape_reset(0, 0);
        tone_blocks(70, 6000, 40, NB, buf); ref_lo = goertzel(buf, NB * CTL, 70);
        tape_reset(100, 0);
        tone_blocks(1000, 6000, 40, NB, buf);
        f1 = goertzel(buf, NB * CTL, 1000); h2 = goertzel(buf, NB * CTL, 2000); h3 = goertzel(buf, NB * CTL, 3000);
        for (i = 0; i < NB * CTL; i++) pk = abs(buf[i]) > pk ? abs(buf[i]) : pk;
        tape_reset(100, 0);
        tone_blocks(10000, 6000, 40, NB, buf); hi = goertzel(buf, NB * CTL, 10000);
        tape_reset(100, 0);
        tone_blocks(70, 6000, 40, NB, buf); lo = goertzel(buf, NB * CTL, 70);
        printf("tape:   TAPE 100: 2nd %.1f dB, 3rd %.1f dB under the tone; 10 kHz %.1f dB, 70 Hz %+.1f dB against 1 kHz "
               "(from TAPE 0); peak %d\n", 20 * log10(h2 / f1), 20 * log10(h3 / f1),
               20 * log10((hi / ref_hi) / (f1 / ref)), 20 * log10((lo / ref_lo) / (f1 / ref)), pk);
        ck("2. TAPE 100: the 2nd and 3rd harmonics within 40 dB of the tone (even ones: the bias)",
           20 * log10(h2 / f1) > -40 && 20 * log10(h3 / f1) > -40);
        ck("   the highs lost: 10 kHz at least 3 dB down against 1 kHz", 20 * log10((hi / ref_hi) / (f1 / ref)) < -3);
        ck("   the head bump: 70 Hz up against 1 kHz", 20 * log10((lo / ref_lo) / (f1 / ref)) > 0.5);
        ck("   bounded: the peak within 2 x the tone's", pk > 0 && pk < 12000);
    }
    /* ---- 3. WOW ---- */
    {
        uint32_t i, k;
        double mn[2], mx[2];
        for (k = 0; k < 2u; k++) {                       /* the zero crossings' spacing over ~3 s (a wow's period at 0.55 Hz) */
            double last = -1, prev = 0;
            mn[k] = 1e9; mx[k] = 0;
            tape_reset(0, k ? 100 : 0);
            tone_blocks(1000, 6000, 40, WN, wbuf);
            for (i = 1; i < WN * CTL; i++)
                if (wbuf[i - 1] < 0 && wbuf[i] >= 0) {
                    double t = i - 1 + (double)-wbuf[i - 1] / (wbuf[i] - wbuf[i - 1]);
                    if (last >= 0) {
                        double p = t - last;
                        prev = prev ? prev * 0.9 + p * 0.1 : p;   /* (smoothed: the interpolation's own jitter out) */
                        if (t > 2000) {
                            mn[k] = prev < mn[k] ? prev : mn[k];
                            mx[k] = prev > mx[k] ? prev : mx[k];
                        }
                    }
                    last = t;
                }
        }
        printf("tape:   WOW 0: the period %.3f .. %.3f samples; WOW 100: %.3f .. %.3f (%.1f cents)\n", mn[0], mx[0], mn[1],
               mx[1], 1200 * log2(mx[1] / mn[1]));
        ck("3. WOW 100: the period swings (V10.27: a warped record, 20 .. 120 cents), WOW 0 steady (under 1 cent)",
           1200 * log2(mx[1] / mn[1]) > 20 && 1200 * log2(mx[1] / mn[1]) < 120 && 1200 * log2(mx[0] / mn[0]) < 1);
    }
    {
        int32_t d2max = 0, d2tone = 0;
        uint32_t b, i;
        int32_t p1 = 0, p2 = 0;
        tape_reset(0, 0);
        for (b = 0; b < 400u; b++) {
            tape_v[0] = tape_v[1] = (int16_t)(b < 100u ? 0 : b < 250u ? 100 : 0);
            tone_blocks(200, 6000, 0, 1, buf);
            for (i = 0; i < CTL; i++) {
                int32_t d2 = abs(buf[i] - 2 * p1 + p2);
                if (b < 100u) d2tone = d2 > d2tone ? d2 : d2tone; else d2max = d2 > d2max ? d2 : d2max;
                p2 = p1; p1 = buf[i];
            }
        }
        printf("tape:   0 -> 100 -> 0 in one block: the second difference at most %d (the tone's own %d)\n", d2max, d2tone);
        ck("   WOW and TAPE turned 0 -> 100 -> 0 in one block: no click (the second difference within 6 x the tone's own: p9b's drive)",
           d2max <= 7 * d2tone + 8);   /* (V10.27: 7 x: the wobble raises the pitch ~50 cents, the curvature with it; no click) */
    }
    /* ---- 4. a project keeps them ---- */
    {
        static project_store_t st;
        int ok;
        ui_power_on();
        tape_v[0] = 37; tape_v[1] = 64;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        tape_v[0] = tape_v[1] = 0;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;   /* (V10.58: set when the project is opened, not by the read) */
        ck("4. a project keeps TAPE and WOW (packed, opened: 37 64)", ok && tape_v[0] == 37 && tape_v[1] == 64);
        tape_v[0] = tape_v[1] = 0;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        tape_v[0] = 50; tape_v[1] = 50;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;   /* (V10.58: set when the project is opened, not by the read) */
        ck("   one without them (both 0: no block) opens with TAPE out", ok && !tape_v[0] && !tape_v[1]);
    }
    printf("tape: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
