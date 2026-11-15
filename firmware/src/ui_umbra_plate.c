/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (included by ui_umbra.c, at its end) The picture's settings saved: the ground's plate with JIANT's settings, LIVE VIZ
 * CTRLS' store (ui_umbra.c umc) in its own flash object.
 * Built for size, as the main loop is (tools/size_fns.py) */
/* every input pass: saved once the knobs have rested 1.5 s (a flash erase a save: never a detent at a time; the store's
 * only with the transport stopped, as project.c settings_poll's: an erase stops the sound for its ~40 ms) */
static uint8_t umv_hr __attribute__((section(".pool")));   /* LOOP ROW: the row laid (+ 1; 0: none, or not looping) */
static void umv_poll(void)
{
    if (!chain.running)
        umv_own = umv_hr = 0;                           /* (the song stopped: nothing taken over any more) */
    if (umv_req) {                                      /* (the song: a row gone in, its plates; a new one, its cells none.
                                                         * Taken over in TOTALITY: let be; handed back by GLO: the row's
                                                         * plates again, every layer shown) */
        uint32_t q = umv_req, g;
        umv_req = 0;
        if (q == 0xFFu)
            umv_rows(CHAIN_ROWS, 0);
        else if (umv_own != 1u) {
            if (!umv_own && chain.hold && umv_hr == q)
                ;                                       /* (the user, 2026-11-15: LOOP ROW's next round lays nothing anew,
                                                         * its transitions go on, what ENV / SEL show stays; a cell set
                                                         * meanwhile is laid at once, umv_sync) */
            else {
                const umc_cell_t *c = umv_cell(q - 1u, 1u);
                if (umv_own)                            /* (handed back: the layers hidden shown again) */
                    umt.lh = 0;
                umv_own = 0;
                umv_apply(q - 1u);
                umv_st.row = (uint8_t)(q - 1u);
                for (g = 0; g < 2u; g++)                /* (a cell of the row's own: its plate comes in as it says) */
                    if (umv_cell(q - 1u, g)->key)
                        umv_st.t0[g] = fm1_ms | 1u;
                if (c->key && c->key != UMC_OFF) {      /* (OVER's own: its veil shown, or ON ENV hidden until ENV shows it,
                                                         * the user 2026-11-15) */
                    g = umv_tf(umv_trv(c), 0u) == 4u;
                    umt.lh = (uint8_t)((umt.lh & ~2u) | g << 1);
                    if (g)
                        umt.ba[1] = 0;                  /* (no glimpse of it fading out) */
                }
            }
            umv_hr = (uint8_t)(chain.hold ? q : 0u);
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
