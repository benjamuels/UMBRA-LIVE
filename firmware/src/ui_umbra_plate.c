/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (included by ui_umbra.c, at its end) The picture's settings saved: the ground's plate with JIANT's settings, LIVE VIZ
 * CTRLS' store (ui_umbra.c umc) in its own flash object.
 * Built for size, as the main loop is (tools/size_fns.py) */
/* every input pass: saved once the knobs have rested 1.5 s (a flash erase a save: never a detent at a time; the store's
 * only with the transport stopped, as project.c settings_poll's: an erase stops the sound for its ~40 ms) */
static void umv_poll(void)
{
    if (umv_req) {                                      /* (the song: a row gone in, its plates; a new one, its cells none) */
        uint32_t q = umv_req, g;
        umv_req = 0;
        if (q == 0xFFu)
            umv_rows(CHAIN_ROWS, 0);
        else {
            umv_apply(q - 1u);
            umv_st.row = (uint8_t)(q - 1u);
            for (g = 0; g < 2u; g++)                    /* (a cell of the row's own: its plate comes in as it says) */
                if (umv_cell(q - 1u, g)->key)
                    umv_st.t0[g] = fm1_ms | 1u;
        }
    }
    if (fm1_ms - umv_ms <= 1500u)
        return;
    if (umv_dirty) {
        umv_dirty = 0;
        settings_save();
    }
    if (umc_dirty && !umv_ed) {                         /* (a cell open: nothing kept until OCT+ says so) */
        umc_dirty = (uint8_t)(umc_flash(&umc, sizeof umc, 1) != 0);       /* (the transport running, or it failed: again in 1.5 s) */
        umv_ms = fm1_ms;
    }
}
