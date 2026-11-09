/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, at its end) The visuals' settings saved. HOME > ECO > PLATE, NOTES and ROUTING, the pages that
 * set them, are gone (the user, 2026-11-09: HOME's UMBRA tab, MACROS - AUDIO and MACROS - VISUALS, to hold what replaces
 * them; ui_lens.c CE_HOME). What they set stays as it was set, kept with the settings, and TOTALITY reads it and turns it
 * (ui_umbra_tot.c): the plate's own three and LIGHT (ui_umbra.c umv), MOVEMENT, TRAVEL, SPEED, DISSOLVE (umt_get), each
 * plate's KNOB 1..4 in TOTALITY (umr_get: at first its three and LIGHT).
 * Built for size, as the main loop is (tools/size_fns.py) */
/* every input pass: the settings saved once the knobs have rested 1.5 s (a flash erase a save: never a detent at a time;
 * project.c settings_poll waits for the transport to stop) */
static void umv_poll(void)
{
    if (umv_dirty && fm1_ms - umv_ms > 1500u) {
        umv_dirty = 0;
        settings_save();
    }
}
