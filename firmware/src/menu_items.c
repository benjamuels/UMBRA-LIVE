/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MENU's rows and their settings: what ui_menu.c draws and steps, and what the editor reads and sets (editor_menu.c
 * MENU_DESC / MENU_SET). One apply path for both: menu_put. Grouped: the screen (COLOR, SKIN (0.8.9), STYLE, LARGE, ANIM, LEDS; 1.1.5:
 * SCREEN OFF), the
 * controls (HOLD: the layer threshold, KNOB ACCEL, FX LATCH, BPM LOCK; 1.2: SCALE LEDS, the keys), the sound (SPEAKER EQ: FLAT LOWCUT BASS+,
 * USB LEVEL; 1.1: CLICK, CLICK LEVEL, COUNT-IN), USB SERIAL, RESTORE LAST (1.2), then CALIBRATION (the setup screen: HARDWARE CALIBRATION) and ABOUT, the two rows with no
 * value (MI_VALUES: the rows before them hold one). 1.0.5: in four tabs (MI_TAB). */
enum { MI_COLOR, MI_SKIN, MI_SCOPE, MI_STYLE, MI_LARGE, MI_ANIM, MI_LEDS, MI_SCROFF, MI_HOLD, MI_ACCEL, MI_LATCH, MI_BPMLOCK, MI_SCLLED, MI_MIDIOUT,
       MI_MIDIMAP, MI_LEARN, MI_VEL, MI_VELMIN, MI_AT, MI_VLEARN, MI_LOWCUT, MI_USB,
       MI_CLICK, MI_CLKLVL, MI_COUNTIN, MI_TRACKS, MI_LOAD, MI_OCTBEND, MI_SERIAL, MI_RESTORE, MI_CPU, MI_CLOCK, MI_BT, MI_WELCOME, MI_PANEL, MI_INIT, MI_ABOUT, MI_COUNT };   /* ((UMBRA LIVE) WELCOME: HOME's WELCOME page on or dismissed, ui_umbra_welcome.c) */   /* (1.1: the
                                                                                             * metronome's rows in AUDIO; 1.2: RESTORE LAST in
                                                                                             * SYSTEM, SCALE LEDS in CONTROL; 0.9.9-stab: CPU
                                                                                             * CLOCK in SYSTEM) */
#define MI_VALUES MI_PANEL
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "SKIN", "SCOPE", "STYLE", "LARGE", "ANIM", "LEDS", "SCREEN OFF", "HOLD", "KNOB ACCEL", "FX LATCH", "BPM LOCK",
                                              "SCALE LEDS", "MIDI OUT", "MIDI MAP", "LEARN",
                                              "VELOCITY", "VEL MIN", "AFTERTOUCH", "VEL LEARN",
                                              "SPEAKER EQ", "USB LEVEL", "CLICK", "CLICK LEVEL", "COUNT-IN", "TRACKS", "LOAD",
                                              "OCT BEND", "USB",
                                              "RESTORE LAST", "CPU CORES", "CPU CLOCK", "BLUETOOTH", "WELCOME", "CALIBRATION", "INIT SYSTEM", "ABOUT"};
/* 1.0.5: the MENU's tabs (ui_menu.c: ALGORITHM steps between them, PRESETS among one tab's rows; the editor gets a
 * row's tab after its MENU_DESC reply). A tab's rows follow each other in MI order (tests/ui_test.c checks it); at
 * most MTAB_ROWS each (the page does not scroll: ui_menu.c fits them). A new row joins a tab here, a new tab is
 * appended (its index is what the editor is told) */
enum { MTAB_DISPLAY, MTAB_CONTROL, MTAB_AUDIO, MTAB_SYSTEM, MTAB_COUNT };
#define MTAB_ROWS 12u                          /* (0.8.9: DISPLAY's seven with SKIN; 0.9.7: eight with SCOPE; V10.6z6:
                                                * CONTROL's twelve, the keys' four as its third set; (UMBRA
                                                * LIVE) SYSTEM's nine with WELCOME) */
static const char *const MTAB_NAME[MTAB_COUNT] = {"DISPLAY", "CONTROL", "AUDIO", "SYSTEM"};
static const uint8_t MI_TAB[MI_COUNT] = {
    MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY, MTAB_DISPLAY,   /* COLOR SKIN (0.8.9) SCOPE (0.9.7) STYLE LARGE */
    MTAB_DISPLAY,                                                           /* ANIM */
    MTAB_DISPLAY,                                                           /* LEDS */
    MTAB_DISPLAY,                                                           /* SCREEN OFF (1.1.5) */
    MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL,                 /* HOLD KNOB ACCEL FX LATCH BPM LOCK */
    MTAB_CONTROL,                                                           /* SCALE LEDS (1.2) */
    MTAB_CONTROL,                                                           /* MIDI OUT (JIANT 0.6.72) */
    MTAB_CONTROL, MTAB_CONTROL,                                             /* MIDI MAP, LEARN (p1, main 0.9.12's LEARN) */
    MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL,                 /* VELOCITY, VEL MIN, AFTERTOUCH, VEL LEARN
                                                                             * (V10.6z6, vel.c) */
    MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO,             /* SPEAKER EQ, USB LEVEL, CLICK, CLICK LEVEL,
                                                                             * COUNT-IN */
    MTAB_AUDIO,                                                             /* TRACKS (JIANT 0.6.18) */
    MTAB_AUDIO,                                                             /* LOAD (JIANT 0.6.30; p1, as main 0.9.12: was
                                                                             * SYSTEM's, whose tile holds seven rows) */
    MTAB_AUDIO,                                                             /* OCT BEND (V10.6z4: CONTROL's eight are full) */
    MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM,                     /* USB SERIAL, RESTORE LAST, CPU CORES (p1,
                                                                             * main 0.9.12), CPU CLOCK (0.9.9-stab) */
    MTAB_SYSTEM,                                                            /* BLUETOOTH (V10.12, bt.c) */
    MTAB_SYSTEM,                                                            /* (UMBRA LIVE) WELCOME */
    MTAB_SYSTEM,                                                            /* CALIBRATION */
    MTAB_SYSTEM, MTAB_SYSTEM,                                               /* INIT SYSTEM (0.6.58), ABOUT */
};
static uint32_t mtab_first(uint32_t t)                 /* a tab's first row */
{
    uint32_t i = 0;
    while (i < MI_COUNT && MI_TAB[i] != t)
        i++;
    return i;
}
static uint32_t mtab_rows(uint32_t t)                  /* .. and how many it has */
{
    uint32_t i, n = 0;
    for (i = 0; i < MI_COUNT; i++)
        n += MI_TAB[i] == t;
    return n;
}
/* STYLE (ui_style, gfx.c ST_*): FLAT the filled cards; LINE black areas divided by 1 px rules (#50, #57: the 0.9 look)
 * (1.0.2: PIXEL retired, a saved PIXEL reads as LINE) */
static const char *const STYLE_N[2] = {"FLAT", "LINE"};
/* (UMBRA LIVE) MENU > SYSTEM > WELCOME (HOME's WELCOME pages, ui_umbra_welcome.c): OFF (dismissed) is bit 6 of the
 * plate's byte (ui_umbra.c umb_pick keeps the low six bits; bit 7 its layout, made first as umb_pick makes it), saved with
 * the settings; the page shown, from the first whenever it comes back */
static uint8_t umw_pg;
static int umw_gone(void) { return (favorites.factory[15][31] & 0xC0u) == 0xC0u; }
/* ((UMBRA LIVE) V10.7.6) WELCOME left out of HOME: dismissed, or its pages' segment not this build's (bt_hw_seg: they
 * are drawn from it), then as if dismissed, nothing saved: the octave keys their own, HOME's first page the next one */
static int umw_hid(void) { return umw_gone() || !bt_hw_seg(); }
static void umw_put(uint32_t on)
{
    uint8_t *f = &favorites.factory[15][31];
    if (!(*f & 0x80u)) {
        memset(favorites.factory[15] + 4, 0, 13);
        *f = (uint8_t)(*f % 3u | 0x80u);
    }
    *f = (uint8_t)(on ? *f & ~0x40u : *f | 0x40u);
    umw_pg = 0;
}
static const char *const SCOPEOPT_N[2] = {"OFF", "ON"};   /* (0.9.7) ui.c ui_scope */
/* (0.8.9) SKIN (ui_skin, gfx.c SKIN_*): COLORS the lens in the 0.6 colours (older settings: 0); BOTTOM the knobs a slim row
 * at the foot, the middle free; HUD the picture full screen, the words floating over it; (0.9.7) AIR, HUD's look with the
 * knobs' row on top (the default of new settings). By the stored value; the menu steps them AIR COLORS BOTTOM HUD
 * (SKIN_ORDER: the editor's MENU_DESC lists them so too) */
static const char *const SKIN_NAME[SKIN_N] = {"COLORS", "BOTTOM", "HUD", "AIR"};
static const uint8_t SKIN_ORDER[SKIN_N] = {SKIN_AIR, SKIN_COLORS, SKIN_BOTTOM, SKIN_HUD};
static uint32_t skin_pos(uint32_t s)                   /* the menu's place of stored skin s */
{
    uint32_t i = 0;
    while (i + 1u < SKIN_N && SKIN_ORDER[i] != s)
        i++;
    return i;
}
/* the two-valued rows: a bit of ui_prefs (PREF_REC: of ui_rec_prefs, ui.c PREF_SCALE_LEDS) and its names (bit clear =
 * the default, bit set). A step up (a knob right,
 * OCT+) = ON on the ON / OFF rows (the switch's knob to the right; ANIM, USB SERIAL: their bit clears), else the
 * second name (STYLE LINE, USB LEVEL FIXED); a step down the other (menu_step) */
typedef struct { uint8_t row; uint16_t bit; const char *name[2]; } menu_flag_t;
static const menu_flag_t MENU_FLAGS[] = {
    {MI_LARGE, PREF_LARGE, {"OFF", "ON"}},            /* #15 / Discussion #80: ON, big knob labels and values (ui.c large_kind) */
    {MI_ANIM, PREF_ANIM_OFF, {"ON", "OFF"}},          /* #46: OFF, values snap (ui_draw.c roll_note, ui_graph.c pr_follow) */
    {MI_ACCEL, PREF_ACCEL_OFF, {"ON", "OFF"}},        /* #52: ui_input.c accel (JIANT: ON by default) */
    {MI_LATCH, PREF_LATCH, {"OFF", "ON"}},
    {MI_BPMLOCK, PREF_BPM_LOCK, {"OFF", "ON"}},        /* #58: ON, DICE ALL keeps the tempo (0.6.67: SELECT alone never sets it) */
    {MI_RESTORE, PREF_RESTORE_ON, {"OFF", "ON"}},      /* 1.2, #130; JIANT 0.6.40: OFF (the default), no autosave, power-on as
                                                        * INIT SYSTEM; ON, the last session back (project.c) */
    {MI_SCLLED, PREF_SCALE_LEDS, {"OFF", "ON"}},      /* 1.2, Discussion #127: ON, the keys show the scale (ui_input.c) */
    {MI_TRACKS, PREF_SIX, {"4", "6"}},                /* (JIANT 0.6.18) 4 tracks (the default) or all 6 (voice.c trk_live) */
};
/* SPEAKER EQ (settings.lowcut, fx.c fx_lowcut): an EQ on the master for the small speaker, not a speaker switch
 * (#42: "OFF" read as the speaker off). FLAT is the old OFF (0, stored as before). The built-in speaker cannot be
 * turned off from the firmware (no amp enable or mute line is known); the EQ reaches
 * the headphone / line out and USB audio too (one DAC, uac_tap reads the master) */
static const char *const SPK_EQ[3] = {"FLAT", "LOWCUT", "BASS+"};
/* (0.7.0) USB LEVEL: MASTER (USB follows the knob, the default), FIXED (full level, the knob after: #42), then fixed
 * and attenuated (-6 .. -24 dB: a phone recording over USB clipped at the full level). ui.c usb_lvl, fx.c fx_usb_gain */
static const char *const USBLVL_N[6] = {"MASTER", "FIXED", "-6 dB", "-12 dB", "-18 dB", "-24 dB"};
/* (0.7.1) MENU > SYSTEM > USB (was USB SERIAL ON / OFF, #67): CONSOLE, the serial console with the stereo audio (the
 * default, PREF_SERIAL_OFF clear: the descriptors as before); STEREO, audio + MIDI only (class 0: the old OFF); STEMS,
 * audio as 8 channels (the six parts mono, the FX return L R; fx.c, usb.c), no console; (0.7.5) IN+OUT, the stereo
 * audio and the host's audio output too (usb.c usb_duplex: the computer or the phone plays into the last track), no
 * console; (0.8.0) STEMS+IN, the stems with the host's audio output too (10 channels in: the stems, then the host's
 * audio as the last track mixed it and as it came; the phone app records a voice through the FM-1 and moves it by
 * the latency). ui.c usb_out */
static const char *const USBOUT_N[6] = {FELUCCA_CDC ? "CONSOLE" : "AUDIO", "STEREO", "STEMS", "IN+OUT", "STEMS+IN", "HOST"};   /* (no console
                                                       * built: the first as STEREO) */
static uint8_t usb_host_want;                          /* (V10.41) USB > HOST (usb_host.c), applied when the menu closes; never saved */
static const char *const HOLD_N[4] = {"0.3 s", "0.4 s", "0.5 s", "0.6 s"};   /* (the editor's names; the menu draws its own) */
/* 1.1 (Discussion #131): the metronome (click.c) and the count-in (seq.c); ui.c ui_rec_prefs holds them */
static const char *const CLICK_N[3] = {"OFF", "REC", "ON"};          /* REC: while a track is armed and playing */
static const char *const CLKLVL_N[3] = {"LOW", "MID", "HIGH"};
static const char *const COUNTIN_N[3] = {"OFF", "1 BAR", "2 BARS"};
/* 1.1.5: SCREEN OFF (ui.c scr_*: the screen dark after this long without panel input; NEVER the default since 1.1.5.1) */
static const char *const SCROFF_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
/* (JIANT 0.6.72) MIDI OUT over USB (ui.c midi_out_mode, seq.c MO_*): the keys; and the clock + START / STOP; and every
 * note the FM-1 plays; nothing */
static const char *const MIDIOUT_N[4] = {"KEYS", "CLOCK", "ALL", "OFF"};
/* (V10.6z4) OCT BEND (ui_input.c kx_press, seq.c kx_block): OCT+ held on a note bends up this far, OCT- a vibrato; OFF:
 * OCT is the octave with a note held too, as before. Stored as its place ^ 2 (oct_xp): 0, every older setting, 2 ST */
static const char *const OCTBEND_N[8] = {"OFF", "1 ST", "2 ST", "3 ST", "4 ST", "5 ST", "7 ST", "12 ST"};
/* (V10.6z6) the keys' velocity and pressure (vel.c: VEL_SET, VEL_SET2): VELOCITY's curve (OFF: every key 100), VEL MIN's
 * floor, AFTERTOUCH's sensitivity (OFF: none), VEL LEARN (RESET: the calibration's seeds back, then ON) */
/* each: its settings byte, the field's shift, mask and ^ (its stored form), its count, its names (w bytes apart) */
typedef struct { uint8_t *b; uint8_t sh, m, x, n, w; const char *nm; } vel_row_t;
static const vel_row_t VEL_ROW[4] = {{&VEL_SET, 0, 3, 2, 4, 7, "OFF\0\0\0\0SOFT\0\0\0NORMAL\0HARD"},
                                     {&VEL_SET, 2, 7, 2, 8, 3, "1\0\00010\00020\00030\00040\00050\00064\00080"},
                                     {&VEL_SET2, 0, 3, 2, 4, 5, "OFF\0\0LOW\0\0MID\0\0HIGH"},
                                     {&VEL_SET2, 2, 3, 0, 3, 6, "ON\0\0\0\0OFF\0\0\0RESET"}};
static const vel_row_t *vel_row(uint32_t row) { return row >= MI_VEL && row <= MI_VLEARN ? &VEL_ROW[row - MI_VEL] : 0; }
/* (JIANT 0.9.9-stab, as 0.9.12) MENU > SYSTEM > CPU CLOCK: 360 MHZ (the default: the boot stage's clock, M-VAVE's V15
 * runs it so with the core rails at 1.60 / 1.38 V, hal/fm1_sys.h fm1_rails_boot) or 320 MHZ (JieLi's rated clock, as
 * M-VAVE's open firmware and Skiff run it: ~11 % fewer cycles a block), switched at boot before the audio and the second
 * core (main.c cpu_clock, hal/fm1_clk320.h: curist's, MIT); from the next power-on (a soft reset keeps the PLL).
 * (p1, main 0.9.12) CPU CORES 2 / 1 beside it, the byte (cpu_cfg) and the last crash: cpu_safe.c */
#include "cpu_safe.c"
#include "bt.c"                                /* (V10.12) MENU > SYSTEM > BLUETOOTH */
/* MENU > USB SERIAL (#67). The serial console is a developer tool (README: FELUCCA_CDC). ON (the default) presents it,
 * the descriptors byte for byte as before; OFF re-enumerates as audio + MIDI only, device class 0 (the bytes of a
 * FELUCCA_CDC=0 build): macOS 13-15 then attach their USB audio driver (with the console Apple's CDC composite driver
 * takes the device and the audio input never appears). USB-MIDI stays, and with it the editor, the update installer
 * and the soft key (SysEx on EP1). Applied at boot before USB starts (main.c: enumerated with it from the start, never
 * on then off) and while the menu is closed (ui_input: one re-enumeration when it closes, not one per KNOB 1 step).
 * A change from the editor (MENU_SET) waits 200 ms (menu_serial_at) so its reply leaves before the device drops off
 * the bus: the whole device re-enumerates, USB-MIDI and audio too */
static uint32_t menu_serial_at;                        /* fm1_ms | 1 of the editor's change; 0 none */
static void usb_serial_apply(void)
{
    if (menu_serial_at && fm1_ms - menu_serial_at < 200u)
        return;
    menu_serial_at = 0;
    if (!FELUCCA_USB_IN && usb_out >= 2u)              /* (UMBRA LIVE: no USB input) a saved IN+OUT: STEREO; STEMS+IN: */
        usb_out = (uint8_t)(usb_out == 3u ? 1u : 0u);  /* STEMS */
#if FELUCCA_USB_HOST
    uh.want = usb_host_want;                           /* (V10.41: uh_task switches the port, the main loop) */
#endif
#if FELUCCA_CDC
    usb_cdc_switch(FELUCCA_CDC_DEFAULT && !(ui_prefs & PREF_SERIAL_OFF) && !usb_out);   /* (a FELUCCA_CDC_DEFAULT=0 build: off) */
#endif
#if FELUCCA_UAC
    usb_stems_switch(usb_out == 1u || usb_out == 3u);  /* (0.7.1) MENU > USB STEMS: the 8-channel device */
    usb_duplex_switch(usb_out == 2u || usb_out == 3u); /* (0.7.5) MENU > USB IN+OUT: the audio output too */
#endif                                                 /* ((0.8.0) STEMS+IN: both, the 10-channel device) */
    fx_stems = (uint8_t)(usb_out == 1u ? 1u : usb_out == 3u ? 2u : 0u);   /* (the parts write their stems from the next block) */
}
static const menu_flag_t *menu_flag(uint32_t row)
{
    uint32_t i;
    for (i = 0; i < NELEM(MENU_FLAGS); i++)
        if (MENU_FLAGS[i].row == row)
            return &MENU_FLAGS[i];
    return 0;
}

/* a row's value as 0..menu_n(row) - 1 in the order the menu steps it (LEDS: OFF DIM LO DIM HI INV, LEDS_MENU) */
static uint32_t menu_n(uint32_t row)
{
    const vel_row_t *vr = vel_row(row);
    return row == MI_COLOR ? NPALETTES : row == MI_SKIN ? SKIN_N : row == MI_LOWCUT || (row >= MI_CLICK && row <= MI_COUNTIN) || row == MI_LOAD ? 3u : row == MI_BT ? 2u :
           row == MI_HOLD || row == MI_MIDIOUT || row == MI_LEARN ? 4u : vr ? vr->n : row == MI_OCTBEND ? 8u : row == MI_MIDIMAP ? MLM_N : row == MI_USB ? 6u : row == MI_SERIAL ? (FELUCCA_USB_IN ? 5u + FELUCCA_USB_HOST : 3u) : row == MI_LEDS ? LEDS_COUNT : row == MI_SCROFF ? NELEM(SCROFF_N) : 2u;
}
static uint32_t menu_get(uint32_t row)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t i = 0;
    if (f)
        return ((ui_prefs | (uint32_t)ui_rec_prefs << 8) & f->bit) != 0;
    const vel_row_t *vr = vel_row(row);
    if (vr)                                            /* (V10.6z6) */
        return ((*vr->b >> vr->sh) ^ vr->x) & vr->m;
    switch (row) {
    case MI_COLOR: return settings.palette % NPALETTES;
    case MI_SKIN: return skin_pos(ui_skin);
    case MI_SCOPE: return ui_scope != 0u;
    case MI_STYLE: return ui_style == ST_LINE;
    case MI_LOWCUT: return settings.lowcut % 3u;
    case MI_USB: return (ui_prefs & PREF_USB_FIXED) ? 1u + usb_lvl % 5u : 0u;
    case MI_SERIAL:
        if (!FELUCCA_USB_IN && usb_out >= 2u)          /* (UMBRA LIVE: no IN+OUT, STEMS+IN; usb_serial_apply) */
            return usb_out == 3u ? 2u : 1u;
        return usb_host_want ? 5u : usb_out == 3u ? 4u : usb_out == 2u ? 3u : usb_out ? 2u : (ui_prefs & PREF_SERIAL_OFF) ? 1u : 0u;
    case MI_HOLD: return settings_hold % 4u;
    case MI_SCROFF: return scr_get();
    case MI_WELCOME: return !umw_gone();
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: return rp_get(row - MI_CLICK);
    case MI_LOAD: return load_get();
    case MI_MIDIOUT: return midi_out_mode & 3u;
    case MI_OCTBEND: return (oct_xp ^ 2u) & 7u;
    case MI_MIDIMAP: return ml_map();
    case MI_LEARN: return ml.on & 3u;
    case MI_CPU: return cpu_get(0);
    case MI_CLOCK: return cpu_get(1);
    case MI_BT: return bt_get();
    case MI_LEDS:
        while (i + 1u < LEDS_COUNT && LEDS_MENU[i] != settings_leds)
            i++;
        return i;
    }
    return 0;
}
static const char *menu_vname(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f)
        return f->name[v & 1u];
    const vel_row_t *vr = vel_row(row);
    if (vr)
        return vr->nm + v % vr->n * vr->w;
    switch (row) {
    case MI_COLOR: return UI_PALETTES[v % NPALETTES].name;
    case MI_SKIN: return SKIN_NAME[SKIN_ORDER[v % SKIN_N]];
    case MI_SCOPE: case MI_WELCOME: return SCOPEOPT_N[v & 1u];
    case MI_STYLE: return STYLE_N[v & 1u];
    case MI_LOWCUT: return SPK_EQ[v % 3u];
    case MI_USB: return USBLVL_N[v % 6u];
    case MI_SERIAL: return USBOUT_N[v % 6u];
    case MI_HOLD: return HOLD_N[v & 3u];
    case MI_CLICK: return CLICK_N[v % 3u];
    case MI_CLKLVL: return CLKLVL_N[v % 3u];
    case MI_COUNTIN: return COUNTIN_N[v % 3u];
    case MI_LOAD: return N_LOAD[v % 3u];
    case MI_MIDIOUT: return MIDIOUT_N[v & 3u];
    case MI_OCTBEND: return OCTBEND_N[v & 7u];
    case MI_MIDIMAP: return MLMAP_N[v % MLM_N];
    case MI_LEARN: return MLON_N[v & 3u];
    case MI_CPU: return CPU_N[0][v & 1u];
    case MI_CLOCK: return CPU_N[1][v & 1u];
    case MI_BT: return bt_vname(v);
    case MI_SCROFF: return SCROFF_N[v % NELEM(SCROFF_N)];
    case MI_LEDS: return LEDS_NAME[LEDS_MENU[v % LEDS_COUNT]];
    }
    return "";
}
/* set a row (v below menu_n(row)) and do what the menu does on a change: the palette, the EQ and USB LEVEL at once;
 * STYLE (ui_draw.c style_apply), LARGE, ANIM, LEDS, HOLD, the flags are read where they are used, every frame;
 * USB SERIAL when the menu has closed (usb_serial_apply). Saved by the caller (menu_close, the editor) */
static void menu_put(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f) {
        if (f->bit & PREF_REC)
            ui_rec_prefs = (uint8_t)(v ? ui_rec_prefs | f->bit >> 8 : ui_rec_prefs & ~(f->bit >> 8));
        else
            ui_prefs = (uint8_t)(v ? ui_prefs | f->bit : ui_prefs & ~f->bit);
        fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;   /* (at once; every frame too: ui_input) */
        return;
    }
    if (vel_row(row)) {                                /* (V10.6z6: vel.c vel_poll takes it the next pass) */
        const vel_row_t *r = vel_row(row);
        if (row == MI_VLEARN && v == 2u && menu_get(row) != 2u)   /* (RESET: the seeds, then learning as ON: vel.c) */
            vk_cal = 2;
        *r->b = (uint8_t)((*r->b & ~(r->m << r->sh)) | ((v ^ r->x) & r->m) << r->sh);
        return;
    }
    switch (row) {
    case MI_COLOR:
        settings.palette = v;
        palette_set(v);                                /* (the menu signature redraws) */
        ui.force = 1;
        break;
    case MI_SKIN: ui_skin = SKIN_ORDER[v % SKIN_N]; break;              /* (ui_draw.c style_apply: the next frame) */
    case MI_SCOPE: ui_scope = (uint8_t)(v & 1u); ui.force = 1; break;   /* (0.9.7) the EDIT pages so from the next frame */
    case MI_WELCOME: umw_put(v & 1u); break;           /* ((UMBRA LIVE) HOME's WELCOME back, or gone) */
    case MI_STYLE: ui_style = (uint8_t)(v ? ST_LINE : ST_FLAT); break;   /* (drawn so from the next frame) */
    case MI_LOWCUT: settings.lowcut = v; fx_lowcut = (uint8_t)v; break;
    case MI_USB:                                   /* (at once; every frame too: ui_input) */
        ui_prefs = (uint8_t)(v ? ui_prefs | PREF_USB_FIXED : ui_prefs & ~PREF_USB_FIXED);
        usb_lvl = (uint8_t)(v ? (v - 1u) % 5u : 0u);
        fx_usb_fixed = v != 0u;
        fx_usb_gain = USB_GAIN[usb_lvl];
        break;
    case MI_SERIAL:                                /* (0.7.1) applied when the menu closes (usb_serial_apply) */
        usb_host_want = v == 5u;                       /* (V10.41) HOST: the device's own setting kept as it was */
        if (v == 5u)
            break;
        ui_prefs = (uint8_t)(v ? ui_prefs | PREF_SERIAL_OFF : ui_prefs & ~PREF_SERIAL_OFF);
        usb_out = (uint8_t)(v == 2u ? 1u : v == 3u ? 2u : v == 4u ? 3u : 0u);   /* (0.7.5: 2 IN+OUT; 0.8.0: 3 STEMS+IN) */
        break;
    case MI_HOLD: settings_hold = (uint8_t)v; break;
    case MI_SCROFF: scr_put(v); break;                 /* (read every frame: ui.c scr_frame) */
    case MI_LEDS: settings_leds = LEDS_MENU[v]; break;
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: rp_put(row - MI_CLICK, v); break;   /* (at once: click.c, seq.c) */
    case MI_LOAD: load_put(v % 3u); break;
    case MI_MIDIOUT: midi_out_mode = (uint8_t)(v & 3u); break;   /* (seq.c reads it every frame: ui_input) */         /* (at once: voice.c sys_load) */
    case MI_OCTBEND: oct_xp = (uint8_t)((v ^ 2u) & 7u); break;   /* (seq.c from the next frame: ui_input kx_frame) */
    case MI_MIDIMAP: ml_tab[0] = (uint8_t)(v % MLM_N); break;   /* (p1: at once, midi_control.c) */
    case MI_LEARN: ml_set(v); break;                   /* (p1: armed, or cleared, at the MENU's close; not saved) */
    case MI_CPU: cpu_put(0, v & 1u); break;            /* (p1: at the next power-on, main.c) */
    case MI_CLOCK: cpu_put(1, v & 1u); break;          /* (0.9.9-stab: at the next power-on, main.c cpu_clock) */
    case MI_BT: bt_put(v % 2u); break;                 /* (V10.12: at once, bt.c; V10.35 2 AIR) */
    }
}
/* the menu's step, any of KNOB 1..4 or OCT+ (s > 0) / OCT- (s < 0): the next / previous value, stopping at the ends;
 * COLOR wraps round its palettes. On an ON / OFF row up is ON (MENU_FLAGS) */
static uint32_t menu_step(uint32_t row, int32_t s)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t v = menu_get(row), n = menu_n(row);
    if (f && str_eq(f->name[0], "ON"))                 /* (ANIM, USB SERIAL, RESTORE LAST: ON is their value 0) */
        s = -s;
    if (s > 0)
        return v + 1u < n ? v + 1u : row == MI_COLOR ? 0u : v;
    if (s < 0)
        return v ? v - 1u : row == MI_COLOR ? n - 1u : 0u;
    return v;
}
