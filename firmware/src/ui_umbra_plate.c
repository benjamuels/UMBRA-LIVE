/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, at its end) HOME > ECO > PLATE, the visuals' page (ui_lens.c CE_HOME: ECO, MACROS, PLATE,
 * TOTALITY): the plate drawn as on ECO, the knob row its settings (ui_draw.c draw_columns): KNOB 1 the plate (CINDER SAROS
 * FILAMENT GRANULE CORONA TARN), KNOB 2 and 3 its own two (CINDER: RISE COOL; SAROS: SPIN FADE; FILAMENT: SWING RING;
 * GRANULE: BOIL FADE; CORONA: REACH FADE; TARN: DRAG BEND), KNOB 4 LIGHT (one for all), 0..99, kept (ui_umbra.c umv).
 * Built for size, as the main loop is (tools/size_fns.py) */
static int umb_vis(void) { return ui.home && !gen_on() && !ui.hlv && ui.hset[0] == 2u; }
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                        uint32_t icon);                 /* (ui_draw.c) */
static void umv_cards(void)
{
    uint32_t pl = umb_pl(), j, v;
    char b[4];
    draw_column(0, "PLATE", UMV_PL[pl], "", VAL(0u), (int32_t)pl * 200, ICON_NONE);
    for (j = 0; j < 3u; j++) {
        v = umv(pl, j);
        fmt_int(b, (int32_t)v);
        draw_column(j + 1u, j > 1u ? "LIGHT" : UMV_N[pl][j], b, "", VAL(j + 1u), (int32_t)v * 1000 / 99, ICON_NONE);
    }
}
/* KNOB k turned d (accelerated): the plate (a detent a plate), or the plate's setting */
static void umv_turn(uint32_t k, int32_t d)
{
    uint32_t pl = umb_pl();
    if (!k) {
        umb_plate = (uint8_t)(0x80 | umb_clamp((int32_t)pl + (d > 0) - (d < 0), 0, (int32_t)UMP_NPL - 1));
        umv_dirty = 1;
        umv_ms = fm1_ms;
    } else
        umv_put(pl, k - 1u, (uint32_t)umb_clamp((int32_t)umv(pl, k - 1u) + d, 0, 99));
}
/* every input pass: the settings saved once the knobs have rested 1.5 s (a flash erase a save: never a detent at a time;
 * project.c settings_poll waits for the transport to stop) */
static void umv_poll(void)
{
    if (umv_dirty && fm1_ms - umv_ms > 1500u) {
        umv_dirty = 0;
        settings_save();
    }
}
