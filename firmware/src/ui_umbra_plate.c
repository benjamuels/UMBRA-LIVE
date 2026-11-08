/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, at its end) HOME > ECO > PLATE, NOTES and ROUTING, the visuals' pages (ui_lens.c CE_HOME: ECO,
 * MACROS, PLATE, NOTES, ROUTING), the plate drawn behind as on ECO (its name over it, ui_lzhome.c eco_draw), the knob row
 * theirs (ui_draw.c draw_columns). On all three ALGORITHM picks the plate (round; ui_input.c), not the track.
 * PLATE: the plate's own three (CINDER: RISE COOL SWAY; SAROS: SPIN FADE SIZE; FILAMENT: SWING RING SPREAD; GRANULE: BOIL
 * FADE GLOW; CORONA: REACH FADE SIZE; TARN: DRAG BEND SIZE), KNOB 4 LIGHT (one for all), 0..99, kept (ui_umbra.c umv).
 * NOTES: for every plate: KNOB 1 MOVEMENT (STATIC MOVE DRIFT RANDOM, a detent each: where a track's notes land, ui_umbra.c
 * umn), KNOB 2 TRAVEL (how far MOVE and DRIFT step), KNOB 3 SPEED (the plates' clock: 50 as the wall's, 0 still, 99 four
 * times), KNOB 4 DISSOLVE (TOTALITY's change of plate: 0 a cut, 50 a second, 99 four), kept (ui_umbra.c umt_get).
 * ROUTING: what KNOB 1..4 turn in TOTALITY on this plate (ui_umbra_tot.c), each one's card the target: the plate's own
 * three, LIGHT, SPEED, REACT (how hard a note reaches it), MOVEMENT, TRAVEL, DISSOLVE; a detent a target, kept a plate each
 * (ui_umbra.c umr_get: KNOB 1..4 at first its three and LIGHT).
 * Built for size, as the main loop is (tools/size_fns.py) */
static int umb_vis(void) { return ui.home && !gen_on() && !ui.hlv && ui.hset[0] - 2u < 3u; }
static const uint8_t UMN_T[4] = {UMT_MOVE, UMT_TRAVEL, UMT_SPEED, UMT_FADE};   /* NOTES: KNOB 1..4's */
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                        uint32_t icon);                 /* (ui_draw.c) */
static uint32_t umv_tg(uint32_t k) { return ui.hset[0] == 3u ? UMN_T[k] : k; }   /* PLATE, NOTES: KNOB k's setting */
static void umv_cards(void)
{
    static const char *const K[4] = {"KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"};
    uint32_t pl = umb_pl(), j, v, t, r = ui.hset[0] == 4u;
    char b[4];
    for (j = 0; j < 4u; j++) {                          /* (ROUTING: each knob's target; else the settings) */
        t = r ? umr_get(pl, j) : umv_tg(j);
        v = r ? t : umt_val(pl, t);
        fmt_int(b, (int32_t)v);
        draw_column(j, r ? K[j] : umt_tname(pl, t), r ? umt_tname(pl, t) : t == UMT_MOVE ? UMN_MV[v & 3u] : b, "", VAL(j),
                    (int32_t)v * 1000 / (int32_t)(r ? UMT_NT - 1 : t == UMT_MOVE ? 3 : 99), ICON_NONE);
    }
}
/* KNOB k turned d (accelerated): its setting (MOVEMENT a detent a step); ROUTING: KNOB k's target (a detent one) */
static void umv_turn(uint32_t k, int32_t d)
{
    uint32_t pl = umb_pl();
    if (ui.hset[0] == 4u)
        umr_put(pl, k, (int32_t)umr_get(pl, k) + d);
    else
        umt_add(pl, umv_tg(k), d);
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
