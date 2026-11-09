/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.6z4) the expression keys on the real sources (ui_test.c's): OCT- / OCT+ pressed while a key's note sounds
 * (ui_input.c oct_key, kx_frame; seq.c kx_block), MENU > AUDIO > OCT BEND, their recording and MIDI OUT.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/expr_keys_test.c -lm
 * 1. no note held: OCT- / OCT+ the octave as before (both: back to 0), no expression;
 * 2. a note held: OCT+ bends that track up along the eased curve to OCT BEND's range (2 ST by default: 512 / 256 st)
 *    in ~300 ms, smooth (no jump), the octave kept, and back to 0 once let go; OCT- a vibrato ~5.5 Hz, +-0.35 st, faded
 *    in; 12 ST reaches 12 semitones; OFF: OCT is the octave with a note held too; a DRUM track none;
 * 3. MIDI OUT ALL: CC1 up and back to 0, pitch bend to full (MSB 127) and back to the middle (64);
 * 4. recording (armed, playing): the gates into the selected track's automation (P_XPR: 125 at the press, 127 at the
 *    release), played back without the keys: the bend comes and goes there;
 * 5. MENU: the row in AUDIO, its eight values; an older setting (byte 0) reads 2 ST (the editor's id 26: editor_test.c). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("expr: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void blocks(uint32_t ms)                       /* the audio side: ms of blocks */
{
    uint32_t k, n = ms * (FS / 1000u) / CTL;
    for (k = 0; k < n; k++)
        events_block(CTL);
}
static uint32_t ui_frames;
static void ms_run(uint32_t ms)                       /* the UI and the audio together, a frame (16 ms) at a time */
{
    uint32_t k;
    for (k = 0; k < ms / 16u; k++) {
        frame();
        blocks(16);
        ui_frames++;
    }
}
static void setup(void)
{
    ui_power_on();
    usb.config = 1;
    mo_r = mo_w = 0;
    mo_lvl = MO_KEYS;
    song.sel = 0;
    frame();
}
static uint32_t KEY = 12u;                            /* a white key mid keyboard */

int main(void)
{
    char what[160];
    int32_t mx, mn, prev, jump;
    uint32_t k, cross;
    /* ---- 5. the MENU row ---- */
    setup();
    ck("MENU > AUDIO > OCT BEND: in AUDIO, eight values, 2 ST by default (an older setting's 0)",
       MI_TAB[MI_OCTBEND] == MTAB_AUDIO && menu_n(MI_OCTBEND) == 8u && menu_get(MI_OCTBEND) == 2u && !oct_xp &&
       str_eq(menu_vname(MI_OCTBEND, 2), "2 ST") && str_eq(menu_vname(MI_OCTBEND, 0), "OFF") && kx_semi == 2u);
    ck("  AUDIO's rows still fit its tab", mtab_rows(MTAB_AUDIO) <= MTAB_ROWS);
    /* ---- 1. no note: the octave ---- */
    setup();
    press(B_OCTUP);
    blocks(100);
    ck("no note held: OCT+ the octave up, no expression", song.octave == 1 && !kx_g[0] && !kx_g[1] && !kx_q8[0]);
    press(B_OCTDN);
    press(B_OCTDN);
    ck("no note held: OCT- down again (to -1)", song.octave == -1);
    /* ---- 2. a note held: the bend ---- */
    setup();
    key_down(KEY);
    frame();
    btn_down(B_OCTUP);
    frame();
    ck("a note held: OCT+ gates the bend on its track, the octave kept", (kx_g[1] & 1u) && song.octave == 0);
    prev = 0; jump = 0; mx = 0;
    for (k = 0; k < 50u; k++) {                       /* 50 x 8 ms */
        blocks(8);
        if (kx_q8[0] - prev > jump) jump = kx_q8[0] - prev;
        prev = kx_q8[0];
        if (k == 6u) mx = kx_q8[0];                   /* (at ~56 ms) */
    }
    snprintf(what, sizeof what, "  eased in: %d / 512 at ~56 ms, 512 after 400 ms, no step over 24 (largest %d)", (int)mx, (int)jump);
    ck(what, mx > 0 && mx < 100 && kx_q8[0] == 512 && jump <= 24);
    btn_up(B_OCTUP);
    frame();
    blocks(400);
    ck("  let go: back to 0 (and no octave change)", kx_q8[0] == 0 && !kx_g[1] && song.octave == 0);
    /* OCT-: the vibrato */
    btn_down(B_OCTDN);
    frame();
    blocks(300);
    mx = mn = 0; cross = 0; prev = kx_q8[0];
    for (k = 0; k < 689u; k++) {                      /* ~1 s of blocks */
        events_block(CTL);
        if (kx_q8[0] > mx) mx = kx_q8[0];
        if (kx_q8[0] < mn) mn = kx_q8[0];
        if ((prev < 0) != (kx_q8[0] < 0)) cross++;
        prev = kx_q8[0];
    }
    snprintf(what, sizeof what, "  OCT- held: a vibrato +%d / %d (of 90: 0.35 st), %u crossings a second (5.5 Hz: 11)", (int)mx, (int)mn, cross);
    ck(what, mx >= 85 && mx <= 90 && mn <= -85 && mn >= -90 && cross >= 10u && cross <= 12u && song.octave == 0);
    btn_up(B_OCTDN);
    frame();
    blocks(300);
    ck("  let go: the vibrato fades out to 0", kx_q8[0] == 0);
    /* 12 ST */
    menu_put(MI_OCTBEND, 7u);
    frame();
    btn_down(B_OCTUP);
    frame();
    blocks(400);
    ck("OCT BEND 12 ST: an octave up (12 x 256)", kx_q8[0] == 12 * 256);
    btn_up(B_OCTUP);
    frame();
    blocks(400);
    /* OFF */
    menu_put(MI_OCTBEND, 0u);
    frame();
    press(B_OCTUP);
    blocks(100);
    ck("OCT BEND OFF: OCT+ with a note held the octave, as before", song.octave == 1 && !kx_g[1] && !kx_q8[0]);
    key_up(KEY);
    frame();
    /* a DRUM track */
    setup();
    song.sel = 3;                                     /* (TRK_OLD: T4 DRUM-X) */
    frame();
    key_down(KEY);
    frame();
    btn_down(B_OCTUP);
    frame();
    blocks(400);
    ck("a DRUM track: no bend (its pitch is the kit's)", drum_track(&trk[3]) && kx_q8[3] == 0);
    btn_up(B_OCTUP);
    key_up(KEY);
    frame();
    /* ---- 3. MIDI OUT ALL ---- */
    setup();
    midi_out_mode = MO_ALL;                           /* (ui.c sets mo_lvl from it every frame) */
    key_down(KEY);
    frame();
    btn_down(B_OCTUP);
    btn_down(B_OCTDN);
    frame();
    {
        uint32_t cc_max = 0, cc_last = 255, pb_max = 0, pb_last = 0, n = 0, b;
        for (b = 0; b < 2u * 400u * (FS / 1000u) / CTL; b++) {   /* 400 ms held, 400 let go: the queue read each block */
            if (b == 400u * (FS / 1000u) / CTL) {
                btn_up(B_OCTUP);
                btn_up(B_OCTDN);
                frame();
            }
            events_block(CTL);
            while (mo_r != mo_w) {
                uint32_t pkt = midi_out_q[mo_r++ % MQ], st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 127u, d2 = pkt >> 24;
                if (st == 0xB0u && d1 == 1u) { cc_last = d2; if (d2 > cc_max) cc_max = d2; n++; }
                if (st == 0xE0u) { pb_last = d2; if (d2 > pb_max) pb_max = d2; n++; }
            }
        }
        snprintf(what, sizeof what, "MIDI OUT ALL: CC1 to %u and back to %u, pitch bend MSB to %u and back to %u (%u messages)",
                 cc_max, cc_last, pb_max, pb_last, n);
        ck(what, cc_max == 127u && cc_last == 0u && pb_max == 127u && pb_last == 64u && n <= 4u * 128u);
    }
    key_up(KEY);
    frame();
    /* ---- 4. recording ---- */
    setup();
    trk[0].p[P_SLEN] = 16; trk[0].p[P_SDIV] = 2; song.g[G_BPM] = 120;   /* (a step: 125 ms) */
    motion.on = 0;
    song.rec = 1;
    transport_req = 1;
    ms_run(32);                                       /* playing */
    key_down(KEY);
    ms_run(240);                                      /* about step 2 */
    btn_down(B_OCTUP);
    ms_run(480);                                      /* held some 4 steps */
    btn_up(B_OCTUP);
    ms_run(48);
    key_up(KEY);
    ms_run(48);
    {
        uint32_t i, on = 0, off = 0, on_at = 99, off_at = 99;
        for (i = 0; i < motion.count; i++) {
            const motion_event_t *e = &motion.event[i];
            if (MOTION_TRK(e) != 0u || MOTION_ID(e) != P_XPR) continue;
            if (MOTION_VAL(e) == 125) { on++; on_at = MOTION_STEP(e); }
            if (MOTION_VAL(e) == 127) { off++; off_at = MOTION_STEP(e); }
        }
        snprintf(what, sizeof what, "recording: the bend's gate as automation (P_XPR 125 at step %u, 127 at step %u)", on_at, off_at);
        ck(what, on == 1u && off == 1u && on_at >= 1u && on_at <= 3u && off_at >= on_at + 3u && off_at <= on_at + 5u);
        song.rec = 0;
        frame();
        mx = 0;
        prev = 9999;
        for (k = 0; k < 2u * 16u * 125u / 16u; k++) {   /* two passes, no keys: a step a 125 ms */
            ms_run(16);
            if (kx_q8[0] > mx) mx = kx_q8[0];
            if (mx == 512 && kx_q8[0] < prev) prev = kx_q8[0];   /* (after it: how far back) */
        }
        snprintf(what, sizeof what, "  played back without the keys: the bend comes (to %d of 512) and goes (back to %d)", (int)mx, (int)prev);
        ck(what, mx == 512 && prev == 0 && !kx_g[0] && !kx_g[1]);
    }
    transport_req = 2;
    ms_run(32);
    ck("stopped: the recorded gates gone (the sound's value back)", trk[0].p[P_XPR] == 127 && (blocks(400), kx_q8[0] == 0));
    printf("expr: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
