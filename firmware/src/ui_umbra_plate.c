/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, at its end) HOME > ECO > PLATE, NOTES and ROUTING, the visuals' pages (ui_lens.c CE_HOME: ECO,
 * MACROS, PLATE, NOTES, ROUTING), the plate drawn behind as on ECO, the knob row theirs (ui_draw.c draw_columns).
 * PLATE: KNOB 1 the plate (CINDER SAROS FILAMENT GRANULE CORONA TARN), KNOB 2 and 3 its own two (CINDER: RISE COOL; SAROS:
 * SPIN FADE; FILAMENT: SWING RING; GRANULE: BOIL FADE; CORONA: REACH FADE; TARN: DRAG BEND), KNOB 4 LIGHT (one for all),
 * 0..99, kept (ui_umbra.c umv).
 * NOTES: how the notes reach every plate: KNOB 1 MOVEMENT (STATIC MOVE DRIFT RANDOM, a detent each: where a track's notes
 * land, ui_umbra.c umn), KNOB 2 TRAVEL (how far MOVE and DRIFT step), KNOB 3 SPEED (the plates' clock: 50 as the wall's,
 * 0 still, 99 four times), KNOB 4 REACT (how hard a note reaches them), kept (ui_umbra.c umt_get).
 * ROUTING: what KNOB 1..4 turn in TOTALITY (ui_umbra_tot.c), each one's card the target by the plate shown's names: its own
 * two, LIGHT, SPEED (its clock: 50 as the wall's, 0 still, 99 four times), REACT (how hard a note reaches it); a detent a
 * target, kept (ui_umbra.c umt_get: KNOB 1 2 3 4 at first the plate's two, SPEED, LIGHT).
 * Built for size, as the main loop is (tools/size_fns.py) */
static int umb_vis(void) { return ui.home && !gen_on() && !ui.hlv && ui.hset[0] - 2u < 3u; }
static const uint8_t UMN_T[4] = {UMT_MOVE, UMT_TRAVEL, UMT_SPEED, UMT_REACT};   /* NOTES: KNOB 1..4's (its byte t + 5) */
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                        uint32_t icon);                 /* (ui_draw.c) */
static void umv_cards(void)
{
    static const char *const K[4] = {"KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"};
    uint32_t pl = umb_pl(), j, v;
    char b[4];
    if (ui.hset[0] > 2u) {                              /* NOTES: their settings; ROUTING: each knob's target */
        for (j = 0; j < 4u; j++) {
            uint32_t r = ui.hset[0] == 4u, t = r ? umt_get(j) : UMN_T[j];
            v = r ? t : umt_get(t + 5u);
            fmt_int(b, (int32_t)v);
            draw_column(j, r ? K[j] : umt_tname(pl, t), r ? umt_tname(pl, t) : j ? b : UMN_MV[v & 3u], "", VAL(j),
                        (int32_t)v * 1000 / (int32_t)(r ? UMT_NT - 1 : j ? 99 : 3), ICON_NONE);
        }
        return;
    }
    draw_column(0, "PLATE", UMV_PL[pl], "", VAL(0u), (int32_t)pl * 200, ICON_NONE);
    for (j = 0; j < 3u; j++) {
        v = umv(pl, j);
        fmt_int(b, (int32_t)v);
        draw_column(j + 1u, j > 1u ? "LIGHT" : UMV_N[pl][j], b, "", VAL(j + 1u), (int32_t)v * 1000 / 99, ICON_NONE);
    }
}
/* KNOB k turned d (accelerated): the plate (a detent a plate), the plate's setting; NOTES: its setting (MOVEMENT a detent
 * one); ROUTING: KNOB k's target (a detent one) */
static void umv_turn(uint32_t k, int32_t d)
{
    uint32_t pl = umb_pl(), i = ui.hset[0] == 4u ? k : UMN_T[k] + 5u;
    if (ui.hset[0] > 2u)
        umt_put(i, (int32_t)umt_get(i) + d);
    else if (!k) {
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
