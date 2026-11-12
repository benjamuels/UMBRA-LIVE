/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
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
    return UMB_ON && home_bare() && !ui.hset[0] && !umw_hid() && !umt_shown() && !ui.confirm && !ui.layer;
}
/* each input pass, after the octave keys' taps (ui_input.c oct_taps: on release, none of a chord) */
static void umw_input(uint32_t oct)
{
    uint32_t dn = (fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u, up = (fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u, el;
    if (umw_hid() && !ui.hlv && !ui.hset[0])            /* (gone, or no segment: HOME's first page the next one) */
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
 * (2026-11-10, the user's words) a few large lines a page, every page set as the power-on title is: AF_M's capitals (it
 * has no lowercase), pale on the plate */
/* ((UMBRA LIVE) on V10.7.6: in the Bluetooth segment, bt.c BTAPP_RO, as JIANT's own ABOUT texts: the app slot is
 * full; read once it is this build's, bt_hw_seg) the pages one after the other, each ended by its '\0' */
static const char UMW_TX[] BTAPP_RO =
    "W E L C O M E"
    "\0"
    "THIS IS JIANT WITH A\nLIVE VISUALS SYSTEM"
    "\0"
    "ENTER/EXIT TOTALITY\n(FULLSCREEN MODE)\nBY TAPPING \001HOME\001"
    "\0"
    "EDIT CONTROLS FOR\nTOTALITY IN UMBRA\n\nSCROLL WITH SELECT KNOB"
    "\0"
    "MORE INFO IN\nSETTINGS MENU\nLONG PRESS HOME"
    "\0"
    "FINISH WELCOME BY\nLONG PRESSING \001OCT -\001";
BTAPP_FN static const char *umw_page(uint32_t n)   /* the n-th page */
{
    const char *s = UMW_TX;
    while (n--)
        s += str_len(s) + 1u;
    return s;
}
#define UMW_LH 23                                       /* a line's height (AF_M's capitals 11, a key's frame clear of the next) */
#define UMW_INK 0xBF7Du                                 /* the pages' ink: the title's pale teal */
/* a line's run from x, its caps' top at y (its keys framed), big (AF_M) or small (AF_X: the way on); its width (draw 0:
 * measured only) */
BTAPP_FN static int32_t umw_run(int32_t x, int32_t y, const char *s, uint32_t n, int draw, int big)
{
    const aafont_t *f = big ? &AF_M : &AF_X;
    int32_t capy = big ? AF_M_CAP_Y : AF_X_CAP_Y, caph = big ? 11 : 7, w = 0;
    uint16_t ink = big ? UMW_INK : T_TEXT;
    char b[48];
    uint32_t i = 0, key = 0;
    while (i < n) {
        uint32_t j = 0;
        while (i < n && s[i] != '\001' && j < sizeof b - 1u)
            b[j++] = s[i++];
        b[j] = 0;
        if (j) {
            int32_t sw = text_w(f, b) + (key ? 8 : 0);
            if (draw && key) {                          /* (a key: the button, its name in it, no outline: on its own */
                uint8_t ha = cv_halo;                   /* ground) */
                cv_rect(x + w - 1, y - 4, sw + 2, caph + 10, T_BG);
                cv_frame(x + w, y - 3, sw, caph + 8, ink);
                cv_halo = 0;
                cv_text(x + w + 4, y - capy, f, b, ink);
                cv_halo = ha;
            } else if (draw)
                cv_text(x + w, y - capy, f, b, ink);
            w += sw;
        }
        if (i < n && s[i] == '\001') {
            key ^= 1u;
            i++;
            w += big ? 5 : 2;                           /* (a gap either side of a key) */
        }
    }
    return w;
}
BTAPP_FN static int32_t umw_line(int32_t cx, int32_t y, const char *s, uint32_t n, int draw, int big)   /* .. centred on cx */
{
    int32_t w = umw_run(0, y, s, n, 0, big);
    if (draw)
        (void)umw_run(cx - w / 2, y, s, n, 1, big);
    return w;
}
/* the page over the plate, between top and bot (the canvas's y), x0 the strip's place */
BTAPP_FN static void umw_draw(int32_t x0, int32_t top, int32_t bot)
{
    const char *s = umw_page(umw_pg % UMW_NP), *e;
    uint8_t ha = cv_halo;
    int32_t n = 1, y, k;
    cv_halo = 1;
    for (e = s; *e; e++)
        n += *e == '\n';
    y = top + (bot - 16 - top - n * UMW_LH + (UMW_LH - 11)) / 2;   /* (centred above the way on) */
    for (; ; s = e + 1, y += UMW_LH) {
        for (e = s; *e && *e != '\n'; e++)
            ;
        umw_line(120 - x0, y, s, (uint32_t)(e - s), 1, 1);
        if (!*e)
            break;
    }
    for (k = 0; k < (int32_t)UMW_NP; k++)               /* the pages: a dot each, this one the theme's */
        cv_rect(10 + k * 7 - x0, bot - 10, 4, 4, k == umw_pg ? T_THEME : T_MID);
    if (umw_pg < UMW_NP - 1u) {                         /* the way on, small at the lower right: "next:" and OCT+ as its button */
        static const char NX[] = "next: \001Oct +\001";
        (void)umw_run(232 - umw_run(0, 0, NX, sizeof NX - 1u, 0, 0) - x0, bot - 13, NX, sizeof NX - 1u, 1, 0);
    }
    if (umw_t0 && fm1_ms - umw_t0 >= UMW_SHOW) {        /* OCT- held: the count, over the page (5 .. 1, a second a step;
                                                         * the bar runs out with it) */
        uint32_t el = fm1_ms - umw_t0, left = el >= UMW_GONE ? 0u : UMW_GONE - el;
        int32_t cy = (top + bot) / 2, w;
        char d[2] = {(char)('0' + (left + 999u) / 1000u), 0};
        cv_halo = 0;
        cv_rect(54 - x0, cy - 38, 132, 76, T_BG);
        cv_frame(54 - x0, cy - 38, 132, 76, T_THEME);
        w = text_w(&AF_L, d);
        cv_text(120 - w / 2 - x0, cy - 27 - AF_L_CAP_Y, &AF_L, d, T_THEME);
        umw_line(120 - x0, cy + 6, "Dismissing WELCOME", 18, 1, 0);
        umw_line(120 - x0, cy + 18, "let go to keep it", 17, 1, 0);
        cv_rect(62 - x0, cy + 30, 116, 3, T_RAISE);
        cv_rect(62 - x0, cy + 30, (int32_t)(116u * left / 5000u), 3, T_THEME);
    }
    cv_halo = ha;
}
