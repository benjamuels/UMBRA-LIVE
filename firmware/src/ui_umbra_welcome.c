/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, after TOTALITY) HOME > WELCOME: UMBRA explained in six pages over the plate (the user's words,
 * 2026-11-09). The first time the FM-1 starts with UMBRA it starts here. OCT+ the next page (it breathes while there is
 * one), OCT- the one before (round: from the first, the last); OCT- held: after a second a count from 5 over the page,
 * a second a step, and at its end WELCOME is gone, from HOME's pages and from the next power-ons (MENU > SYSTEM >
 * WELCOME ON brings it back); let go before, it stays (and that let-go is no page back). On WELCOME the octave keys do
 * this and nothing else (the keyboard's octave stays). OCT- and OCT+ held together are UPDATE MODE's, as everywhere
 * (main.c): no count here. Built for size (tools/size_fns.py) */
#define UMW_NP 6u                                       /* the pages */
#define UMW_SHOW 1000u                                  /* OCT- held this long (ms): the count shows, from 5 .. */
#define UMW_GONE (UMW_SHOW + 5000u)                     /* .. and at its end WELCOME is gone */
static uint8_t umw_eat;                                 /* the count shown: OCT-'s let-go no page back */
static uint32_t umw_t0;                                 /* OCT- held alone on WELCOME since (| 1; 0: not) */
/* (umw_pg, the page shown, and umw_gone / umw_put, MENU > SYSTEM > WELCOME's setting: menu_items.c) */
static void umw_show(uint32_t on)                       /* .. and saved once things rest (ui_umbra_plate.c) */
{
    umw_put(on);
    umv_dirty = 1;
    umv_ms = fm1_ms;
    ui.force = 1;
}
static int umw_on(void)                                 /* WELCOME shown */
{
    return UMB_ON && home_bare() && !ui.hset[0] && !umw_gone() && !umt_shown() && !ui.confirm && !ui.layer;
}
/* each input pass, after the octave keys' taps (ui_input.c oct_taps: on release, none of a chord) */
static void umw_input(uint32_t oct)
{
    uint32_t dn = (fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u, up = (fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u, el;
    if (umw_gone() && !ui.hlv && !ui.hset[0])           /* (gone: HOME's first page the next one) */
        ui.hset[0] = 1;
    if (!dn || up || !umw_on())
        umw_t0 = 0;
    else if (!umw_t0)
        umw_t0 = fm1_ms | 1u;
    else if ((el = fm1_ms - umw_t0) >= UMW_GONE) {      /* the count's end: gone, the next page (LIVE AUDIO CTRLS) */
        umw_t0 = 0;
        umw_show(0);
        nav_go(SEC_HOME, 1u, 0);
        ui_message("WELCOME OFF");
    } else if (el >= UMW_SHOW && !umw_eat) {            /* the count shows */
        umw_eat = 1;
        ui.force = 1;
    }
    if (umw_eat && (oct & 1u)) {                        /* (the hold let go: no page back) */
        oct &= ~1u;
        ui.force = 1;
    }
    if (!dn)
        umw_eat = 0;
    if (oct && umw_on()) {
        umw_pg = (uint8_t)((umw_pg + ((oct & 2u) ? 1u : UMW_NP - 1u)) % UMW_NP);
        ui.force = 1;
    }
}
/* the pages, a line each (a '\n' ends one; an empty one a gap); "\001KEY\001" a key of the FM-1, framed as its button.
 * The first is the greeting, drawn large */
static const char *const UMW_TX[UMW_NP] = {
    "Welcome!",
    "Umbra Obscura is\na custom firmware fork of Jiant.\n\nIt preserves the complete audio system\nand sound engine characters "
    "from Jiant",
    "This version removes the Jiant\nECO and \"beings\"\nand reduces the project save slots\nfrom 8 to 4.\n\nIn that space it "
    "adds a\nlive visual performance system",
    "ECO and BEING pages\nare replaced with\nUMBRA where you set\nlive performance controls.",
    "You still change pages\nwith the SELECT knob and\n\nActivate TOTALITY by tapping \001HOME\001\n(fullscreen live "
    "performance mode)\n\nAnd exit by tapping \001HOME\001 again",
    "Find more info in settings MENU\nLong press \001HOME\001\n\nDismiss welcome screen\nby long pressing \001Oct -\001"};
#define UMW_LH 12                                       /* a line's height (AF_X 9, and 3: a key's frame clear of the next) */
/* AF_X's glyphs sc times as large, in c (grow: each pixel that much larger: the outline under them) */
static void umw_glyphs(int32_t x, int32_t y, const char *s, int32_t sc, uint16_t c, int32_t grow)
{
    const aafont_t *f = &AF_X;
    int32_t pen = 0;
    for (; *s; s++) {
        aag_t g = ag(f, glyph(f, (uint8_t)*s));
        uint32_t k, n = (uint32_t)g.bw * g.bh;
        int32_t gx = x + ((pen + 8) >> 4) * sc + g.bx * sc, gy = y + g.by * sc;
        for (k = 0; k < n; k++)
            if ((f->data[g.off + (k >> 3)] >> (7u - (k & 7u))) & 1u)
                cv_rect(gx + (int32_t)(k % g.bw) * sc - grow, gy + (int32_t)(k / g.bw) * sc - grow, sc + 2 * grow, sc + 2 * grow, c);
        pen += g.adv;
    }
}
/* a line's run from x, its caps' top at y (its keys framed); its width (draw 0: measured only) */
static int32_t umw_run(int32_t x, int32_t y, const char *s, uint32_t n, int draw)
{
    char b[48];
    uint32_t i = 0, key = 0;
    int32_t w = 0;
    while (i < n) {
        uint32_t j = 0;
        while (i < n && s[i] != '\001' && j < sizeof b - 1u)
            b[j++] = s[i++];
        b[j] = 0;
        if (j) {
            int32_t sw = text_w(&AF_X, b) + (key ? 8 : 0);
            if (draw && key) {                          /* (a key: the button, its name in it, no outline: on its own */
                uint8_t ha = cv_halo;                   /* ground) */
                cv_rect(x + w - 1, y - 4, sw + 2, 17, T_BG);
                cv_frame(x + w, y - 3, sw, 15, T_TEXT);
                cv_halo = 0;
                cv_text(x + w + 4, y - AF_X_CAP_Y, &AF_X, b, T_TEXT);
                cv_halo = ha;
            } else if (draw)
                cv_text(x + w, y - AF_X_CAP_Y, &AF_X, b, T_TEXT);
            w += sw;
        }
        if (i < n && s[i] == '\001') {
            key ^= 1u;
            i++;
            w += 2;                                     /* (a gap either side of a key) */
        }
    }
    return w;
}
static int32_t umw_line(int32_t cx, int32_t y, const char *s, uint32_t n, int draw)   /* .. centred on cx */
{
    int32_t w = umw_run(0, y, s, n, 0);
    if (draw)
        (void)umw_run(cx - w / 2, y, s, n, 1);
    return w;
}
/* the page over the plate, between top and bot (the canvas's y), x0 the strip's place */
static void umw_draw(int32_t x0, int32_t top, int32_t bot)
{
    const char *s = UMW_TX[umw_pg % UMW_NP], *e;
    uint8_t ha = cv_halo;
    int32_t n = 1, y, k;
    cv_halo = 1;
    if (!umw_pg) {                                      /* the greeting: three times as large, the theme's colour */
        int32_t w = text_w(&AF_X, s) * 3, gy = (top + bot) / 2 - 18;
        umw_glyphs(120 - w / 2 - x0, gy, s, 3, T_BG, 1);
        umw_glyphs(120 - w / 2 - x0, gy, s, 3, T_THEME, 0);
    } else {
        for (e = s; *e; e++)
            n += *e == '\n';
        y = top + (bot - 14 - top - n * UMW_LH) / 2;    /* (centred above the way on) */
        for (; ; s = e + 1, y += UMW_LH) {
            for (e = s; *e && *e != '\n'; e++)
                ;
            umw_line(120 - x0, y, s, (uint32_t)(e - s), 1);
            if (!*e)
                break;
        }
    }
    for (k = 0; k < (int32_t)UMW_NP; k++)               /* the pages: a dot each, this one the theme's */
        cv_rect(10 + k * 7 - x0, bot - 10, 4, 4, k == umw_pg ? T_THEME : T_MID);
    if (umw_pg < UMW_NP - 1u) {                         /* the way on: an arrow, OCT+ */
        int32_t hw = umw_line(0, 0, "\001Oct +\001", 7, 0), ax = 230 - hw - 14 - x0, ay = bot - 13;
        for (k = 1; k >= 0; k--) {                      /* (its outline, then it) */
            uint16_t c = k ? T_BG : T_TEXT;
            cv_rect(ax - k, ay + 3 - k, 8 + 2 * k, 1 + 2 * k, c);
            cv_rect(ax + 5 - k, ay + 1 - k, 1 + 2 * k, 5 + 2 * k, c);
            cv_rect(ax + 6 - k, ay + 2 - k, 1 + 2 * k, 3 + 2 * k, c);
            cv_rect(ax + 7 - k, ay + 3 - k, 1 + 2 * k, 1 + 2 * k, c);
        }
        umw_line(ax + 14 + hw / 2, ay, "\001Oct +\001", 7, 1);
    }
    if (umw_t0 && fm1_ms - umw_t0 >= UMW_SHOW) {        /* OCT- held: the count, over the page (5 .. 1, a second a step;
                                                         * the bar runs out with it) */
        uint32_t el = fm1_ms - umw_t0, left = el >= UMW_GONE ? 0u : UMW_GONE - el;
        int32_t cy = (top + bot) / 2, w;
        char d[2] = {(char)('0' + (left + 999u) / 1000u), 0};
        cv_halo = 0;
        cv_rect(54 - x0, cy - 38, 132, 76, T_BG);
        cv_frame(54 - x0, cy - 38, 132, 76, T_THEME);
        w = text_w(&AF_X, d) * 4;
        umw_glyphs(120 - w / 2 - x0, cy - 34, d, 4, T_THEME, 0);
        umw_line(120 - x0, cy + 6, "Dismissing WELCOME", 18, 1);
        umw_line(120 - x0, cy + 18, "let go to keep it", 17, 1);
        cv_rect(62 - x0, cy + 30, 116, 3, T_RAISE);
        cv_rect(62 - x0, cy + 30, (int32_t)(116u * left / 5000u), 3, T_THEME);
    }
    cv_halo = ha;
}
