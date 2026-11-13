/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) SAVE's lens (ui.c save_*): the panels of its three levels, drawn as the approved HTML mock-up (SAVE, the
 * proposal "LENTE"), in the lens's panel (240 x 140 at y 84; the mock-up's coordinates less 84). Included by
 * ui_graph.c.
 *   LIBRARY  PROJECT: the eight projects as tiles (4 x 2, 56 x 66): the number, the name (its section A's), its four
 *            sections as chips A..D (stored: the section's colour; empty: dark), the project shown TINT with a THEME
 *            frame, its section picked framed TEXT. TOOLS: the six tools as rows (the one picked TINT).
 *   PROJECT  the project's four sections large (stored: their colour, the one picked full with a TEXT frame, the
 *            others half; the music's: NOW), ARRANGE and the song's rows as chips ("A16"; a scene: a dot; ARRANGE's
 *            row picked full and framed), the song as a strip (its rows by their bars, the one picked full; playing:
 *            a TEXT line where it is) and its length and END under it.
 *   SOUND    the track's being (its engine's species, its HOME knobs, moving with its sound) with the track and its
 *            sound under it; USER: the 32 slots (4 x 8; a stored one its engine's being small and its name, the one
 *            picked TINT with a THEME frame); PRESETS: the sounds around the one loaded (TINT), their engine at the
 *            right, a favourite's star.
 * The other projects' sections and names come from their flash headers (project.c lib_scan), one project a frame,
 * cached here until a project is written, loaded or renamed (lib_gen). */
/* a section's chip colour: stored, its colour (half: mixed with BG); empty: BG */
static uint16_t lz_sec(uint32_t s, int used, int half)
{
    uint16_t c = used ? sec_col(s) : half < 0 ? ux_mix(T_BG, T_RAISE, 50) : T_BG;
    return used && half ? ux_mix(T_BG, c, 50) : c;
}
#define LZ_TX(c) (4 + (int32_t)(c) * 117 / 2)          /* four columns of 56 px, 58 or 59 apart (the mock-up's 58.5) */
static const char *umb_bank(uint32_t i, int *cur);     /* (ui_umbra.c) the plate bank's slot i: its plate's name */
static void graph_lib(void)
{
    uint32_t i, j;
    {                                                   /* (UMBRA LIVE) under the four projects, where projects 5..8 were:
                                                         * the plate bank that took their flash, a cell a slot (four rows of
                                                         * eight): the plate there by its name's first four letters, the one
                                                         * drawn now framed; an empty slot its number, dim */
        cv_text(4, 72, &AF_X, "PLATE BANK", T_THEME);
        cv_text(232 - text_w(&AF_X, "WAS PROJECTS 5-8"), 72, &AF_X, "WAS PROJECTS 5-8", T_DIM);
        for (i = 0; i < 32u; i++) {
            int32_t x = 4 + (int32_t)(i % 8u) * 29, y = 83 + (int32_t)(i / 8u) * 16;
            int cur;
            const char *nm = umb_bank(i, &cur);
            uint16_t bg = nm[0] ? (cur ? T_TINT : T_RAISE) : T_BG;
            char b[4];
            cv_rect(x, y, 27, 14, bg);
            cv_frame(x, y, 27, 14, cur ? T_THEME : nm[0] ? T_RAISE : ux_mix(T_BG, T_RAISE, 50));
            fmt_int(b, (int32_t)i + 1);
            if (nm[0])
                cv_clip_text(x + 2, y + 4, &AF_X, nm, T_TEXT, bg, 24);
            else
                cv_text_on(x + 2, y + 4, &AF_X, b, T_DIM, bg);
        }
    }
    for (i = 0; i < NSONG_USE; i++) {
        int32_t x = LZ_TX(i % 4u), y = (int32_t)(i / 4u) * 70;
        int cur = i == song_cur && !song_draft;
        int pick = cur_page()->graph == GR_SONGSAVE || song_draft ? i == song_save_target : cur;
        uint16_t bg = pick ? T_TINT : T_RAISE;
        char b[4], nm[13];
        cv_rect(x, y, 56, 66, bg);
        if (pick)
            cv_frame(x, y, 56, 66, T_THEME);
        fmt_int(b, (int32_t)i + 1);
        GFX_HOOK_ALIGN(0, y + 3, 0, y + 12, AL_V | AL_PASS, "library tile number");
        cv_text_on(x + 4, y + 3, &AF_X, b, cur ? T_THEME : T_DIM, bg);
        if (cur)
            str_cpy(nm, graph_project_used(0) ? graph_project_name(0) : "", sizeof nm);
        else
            str_cpy(nm, lib.name[i], sizeof nm);
        if (!nm[0] && (cur ? graph_project_used(0) : (lib.mask[i] & 1u)))
            str_cpy(nm, "USED", sizeof nm);
        cv_clip_text(x + 4, y + 14, &AF_X, nm[0] ? nm : "--", nm[0] ? T_TEXT : T_DIM, bg, 50);
        for (j = 0; j < 4u; j++) {
            int32_t sx = x + 4 + (int32_t)(j % 2u) * 25, sy = y + 28 + (int32_t)(j / 2u) * 18;
            int used = cur ? graph_project_used(j) : (int)((lib.mask[i] >> j) & 1u);
            uint16_t c = lz_sec(j, used, 0);
            char l[2] = {(char)('A' + j), 0};
            cv_rect(sx, sy, 23, 15, c);
            GFX_HOOK_ALIGN(sx, sy, sx + 23, sy + 15, AL_HV, "library section chip letter centred");
            cv_text_in(sx, sy + CAP_IN(X, 15), 23, &AF_X, l, used ? T_INK : T_DIM, c);
            if (cur && (int32_t)j + 1 == song.g[G_SLOT])
                cv_frame(sx - 1, sy - 1, 25, 17, T_TEXT);
        }
    }
}
/* (V10.24, the user's pick A) TOOLS by scope: at the left a picture of what the action takes (the steps, the sound's
 * wave, the song's rows, everything), at the right the scope's actions; under them a bar fills while OCT+ is held */
static __attribute__((noinline)) void graph_lz_tools(void)
{
    static const char *const WHAT[7] = {"CLEAR SEQ", "INIT SOUND", "DELETE ROW", "CLEAR SONG", "NEW PROJECT", "CPU TEST", "CLEAR PLATES"};
    static const char *const ON[7] = {"STEPS + AUTO", "ENGINE DEFAULT", "", "ALL ROWS", "EVERYTHING", "HEAVY SONG", "THE SONG'S PICTURE"};
    uint32_t a = ui.act ? ui.act - 1u : 0u, sc = TL_SC[a], c, n = 0, h = graph_h(), i;
    int32_t hold = tl_t0 ? (int32_t)(fm1_ms - tl_t0) * 232 / (int32_t)TL_HOLD : 0;
    uint16_t col = !ui.act ? T_DIM : sc == 0u ? T_THEME : sc == 1u ? T_ACCENT : sc == 2u ? T_MID : T_TEXT;
    char b[12];
    cv_frame(4, 4, 76, 76, col);                         /* the picture */
    if (sc == 0u)
        for (i = 0; i < 16u; i++)                         /* the track's steps */
            cv_rect(10 + (int32_t)(i % 8u) * 8, 14 + (int32_t)(i / 8u) * 30, 6, 24, TSEL->step[i].time == ST_NOTE ? col : T_DIM);
    else if (sc == 1u)
        for (i = 0; i < 64u; i++)                         /* a wave */
            cv_rect(10 + (int32_t)i, 42 + ((24 * og_sin(i * 2048u)) >> 15), 1, 2, col);
    else if (sc == 2u)
        for (i = 0; i < 4u; i++)                          /* the song's rows */
            cv_rect(10, 12 + (int32_t)i * 16, 64, 12, i < chain_config.count ? col : T_DIM);
    else
        cv_text_in(4, 38, 76, &AF_M, "ALL", col, T_SURF);
    for (c = 0; c < 7u; c++) {                           /* the scope's actions */
        int32_t y = 4 + (int32_t)n * 26;
        int sel = ui.act == c + 1u;
        uint16_t bg = sel ? T_TINT : T_SURF;
        if (TL_SC[c] != sc) continue;
        n++;
        cv_rect(88, y, 148, 24, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 12, AL_V, "tools row text centred up/down");
        cv_text_on(94, y + CAP_IN(M, 14), &AF_M, WHAT[c], sel ? T_TEXT : T_MID, bg);
        if (c == 2u) { str_cpy(b, "ROW ", sizeof b); fmt_int(b + 4, (int32_t)ui.song_row + 1); }
        cv_text(94, y + 15, &AF_X, c == 2u ? b : ON[c], sel ? T_MID : T_DIM);
    }
    cv_rect(4, (int32_t)h - 20, 232, 6, T_RAISE);        /* OCT+ held: the bar */
    if (hold > 0) cv_rect(4, (int32_t)h - 20, hold > 232 ? 232 : hold, 6, col);
    cv_text_in(4, (int32_t)h - 11, 232, &AF_X, ui.act ? "HOLD OCT+ TO DO IT" : "TURN A KNOB TO PICK", T_DIM, T_SURF);
}
/* the song's length and END ("120 BARS . LOOP") into b (24) */
static void song_len_txt(char *b, uint32_t total)
{
    uint32_t end = SONG_END(&chain_config);
    fmt_int(b, (int32_t)total);
    str_cpy(b + str_len(b), " BARS \xB7 ", 9);
    if (end == SONG_FOREVER) str_cpy(b + str_len(b), "LOOP", 6);
    else if (end >= 2u) { uint32_t k = str_len(b); b[k] = 'x'; fmt_int(b + k + 1u, (int32_t)end); }
    else str_cpy(b + str_len(b), "STOP", 6);
}
static uint32_t song_bars(uint32_t s)                  /* the song's bars (s 4), part s's in it */
{
    uint32_t i, t = 0;
    for (i = 0; i < chain_config.count; i++)
        if (s > 3u || chain_config.row[i].slot == s) t += chain_config.row[i].bars;
    return t;
}
/* (V10.6o) the song as a strip of blocks at y, h high in x 4..236 (a row its bars wide, its part's colour; hl: the part
 * lit, the others half; 4 all lit, 5 the cursor's, 6 none; the cursor's framed on PARTS; tl: TIMELINE's, a row with
 * tracks of its own (FROM, VAR) half below, its letter and bars, the cursor's framed twice); playing, a line where it is */
static void song_strip(int32_t y, int32_t h, uint32_t hl, int tl)
{
    uint32_t i, n = chain_config.count, t = song_bars(4u), acc = 0, played = 0;
    char b[8];
    for (i = 0; i < n && t; i++) {
        const chain_row_t *rw = &chain_config.row[i];
        uint32_t sl = rw->slot & 3u;
        int32_t x = 4 + (int32_t)(acc * 232u / t), w = 4 + (int32_t)((acc + rw->bars) * 232u / t) - x - 1;
        uint16_t c = lz_sec(sl, 1, hl < 4u ? sl != hl : hl != 4u && (hl == 6u || i != ui.song_row));
        if (w < 1) w = 1;
        cv_rect(x, y, w, h, c);
        if (tl) {
            if (song_idx.tsrc[song_cur][i] || song_idx.tvar[song_cur][i])
                cv_rect(x, y + h - 4, w, 4, ux_mix(T_SURF, c, 50));
            if (w >= 12) {
                b[0] = (char)('A' + sl); b[1] = 0;
                if (w >= 24) fmt_int(b + 1, rw->bars);
                GFX_HOOK_ALIGN(0, y, 0, y + h - 4, AL_V | AL_PASS, "timeline block letter");
                cv_text_on(x + 3, y + CAP_IN(X, h - 4), &AF_X, b, T_INK, c);
            }
        }
        if (i == ui.song_row && (tl || hl < 4u)) {
            cv_frame(x - 1, y - 1, w + 2, h + 2, T_TEXT);
            if (tl) cv_frame(x, y, w, h, T_TEXT);
        }
        if (chain.running && i < chain.row) played += rw->bars;
        acc += rw->bars;
    }
    if (chain.running && chain.row < n && t) {
        played += chain_config.row[chain.row].bars - chain.remaining;
        cv_rect(4 + (int32_t)(played * 232u / t), y - 2, 1, h + 4, T_TEXT);
    }
}
/* (V10.6o, the owner's mock-up A) PARTS: the four sections as cards (2 x 2, their colours): the letter, the name (else
 * GENE's or PART), its bars in the song, its tracks that play (six marks); KNOB 1's ringed, the one looping a ring
 * pulsing; under them the song, the part's rows lit (the cursor's, OCT-'s, framed) */
static void graph_parts(void)
{
    uint32_t s, k;
    char b[24];
    for (s = 0; s < 4u; s++) {
        int32_t x = 4 + (int32_t)(s & 1u) * 118, y = 2 + (int32_t)(s >> 1) * 52;
        int used = graph_project_used(s), loop = used && live.cur == (int8_t)s && song.playing && !chain.running;
        uint16_t c = used ? sec_col(s) : T_RAISE, ink = used ? T_INK : T_DIM;
        const char *nm = graph_project_name(s);
        cv_rect(x, y, 114, 48, c);
        b[0] = (char)('A' + s); b[1] = 0;
        cv_text_on(x + 6, y + 4, &AF_M, b, ink, c);
        if (used && song_bars(s)) {                       /* (its bars in the song) */
            fmt_int(b, (int32_t)song_bars(s));
            str_cpy(b + str_len(b), " BARS", 6);
            cv_text_r(x + 109, y + 5, &AF_X, b, ink, c);
        }
        cv_free_text(x + 6, y + 22, &AF_X, !used ? "EMPTY" : nm[0] ? nm : (dice_secs >> s) & 1u ? "GENE" : "PART", ink, c, 102);
        for (k = 0; used && k < NTRK; k++)                /* the tracks that play in it */
            cv_rect(x + 6 + (int32_t)k * 17, y + 37, 15, 5, (part_play.b[s] >> k) & 1u ? ink : ux_mix(c, T_INK, 30));
        if (s == ui.part || loop) {
            uint16_t rc = loop && (anim_fr / 8u) & 1u ? T_THEME : T_TEXT;
            cv_frame(x - 2, y - 2, 118, 52, rc);
            if (s == ui.part) cv_frame(x - 1, y - 1, 116, 50, rc);
        }
    }
    if (chain_config.count)
        song_strip(110, 10, ui.part & 3u, 0);
    else
        cv_text(4, 110, &AF_X, "NO SONG ROWS", T_DIM);
    GFX_HOOK_ALIGN(0, 128, 0, 137, AL_V | AL_PASS, "parts hint");
    cv_text(4, 128, &AF_X, "OCT+ LOOP \xB7 OCT- SONG \xB7 F3-B3 A-D", T_DIM);
}
/* (V10.6s, the owner's mock-up C) CLIPS: the six tracks x the four parts, a cell a track's pattern in a part (its part's
 * colour calm where it plays there, grey where silent or the part empty); the part each track sounds framed in its
 * colour, the cell asked for (in on the next bar) framed dashed (a stop: the sounding frame dashed); the cell KNOB 1 / 2
 * pick ringed; SONG REC on the right */
static void clip_dash(int32_t x, int32_t y, uint16_t c)   /* a cell's frame, dashed */
{
    int32_t i;
    for (i = 0; i < 50; i += 4) {
        cv_rect(x + i, y, 2, 2, c); cv_rect(x + i, y + 13, 2, 2, c);
        if (i < 15) { cv_rect(x, y + i, 2, 2, c); cv_rect(x + 48, y + i, 2, 2, c); }
    }
}
static void graph_clips(void)
{
    uint32_t s, k;
    char b[3] = "T1";
    if (live.srec)
        cv_text(4, 2, &AF_X, "REC", T_ACCENT);         /* (SONG REC: the launches become rows) */
    for (k = 0; k < NTRK; k++) {
        int32_t y = 13 + (int32_t)k * 18;
        b[1] = (char)('1' + k);
        cv_text(4, y + CAP_IN(X, 15), &AF_X, b, k == song.sel ? T_THEME : T_MID);
        for (s = 0; s < 4u; s++) {
            int32_t x = 24 + (int32_t)s * 53;
            uint16_t c = sec_col(s);
            int used = graph_project_used(s);
            if (!k) {                                      /* (the parts' letters over them) */
                char l[2] = {(char)('A' + s), 0};
                cv_text(x + 22, 2, &AF_X, l, used ? c : T_DIM);
            }
            cv_rect(x, y, 50, 15, lz_sec(s, used && (part_play.b[s] >> k) & 1u, -1 + 2 * (used && (part_play.b[s] >> k) & 1u)));
            if (clip_now[k] == s + 1u) {
                if (clip_go[k] == CLIP_STOP) clip_dash(x, y, c);
                else { cv_frame(x, y, 50, 15, c); cv_frame(x + 1, y + 1, 48, 13, c); }
            } else if (clip_go[k] == s + 1u)
                clip_dash(x, y, c);
            if (k == song.sel && s == ui.part)
                cv_frame(x - 2, y - 2, 54, 19, T_TEXT);
        }
    }
    cv_text(4, 128, &AF_X, "OCT+ GO \xB7 OCT- STOP \xB7 F3-B3 PART \xB7 REC", T_DIM);
}
/* (V10.6o, the owner's mock-up B) TIMELINE: the song as blocks (the cursor's row framed), what plays; under it the row
 * opened: each track's bar (its own part's colour calm, another part's bright with FROM and its letter,
 * MUTE dark, silent in its part --), its VAR on the right; ALGORITHM's track lit. ((UMBRA LIVE) in the left half; the
 * right half the row's two plates, MAIN and OVER, previewed: ui_umbra_mac.c umv_tl; a cell open: its editor, umv_draw) */
static uint32_t tl_muted(uint32_t r, uint32_t k)
{
    uint32_t j = r + 1u;
    const scene_t *sc;
    if (row_var(r, k) == RV_MUTE) return 1;
    while (j-- > 0u && !song_idx.scene[song_cur][j].on) {}
    if (j > r) return 0;
    sc = &song_idx.scene[song_cur][j];
    return k < 4u ? (sc->mute >> k) & 1u : (sc->on >> (k - 3u)) & 1u;
}
static int umv_draw(void);                            /* ((UMBRA LIVE) ui_umbra_mac.c: TIMELINE's picture tracks) */
static void umv_tl(void);
static __attribute__((noinline)) void graph_tl(void)
{
    uint32_t n = chain_config.count, r = ui.song_row < n ? ui.song_row : n - 1u, k;
    char b[24];
    if (umv_draw())                                      /* ((UMBRA LIVE) a picture cell open: its editor) */
        return;
    if (!n) {
        cv_text_in(4, 60, 232, &AF_X, "NO ROWS: OCT+ ADDS ONE", T_DIM, T_SURF);
        return;
    }
    song_strip(3, 18, 4u, 1);
    if (chain.running && !chain.loop) {
        str_cpy(b, chain.hold ? "LOOP ROW " : "ROW ", sizeof b);
        fmt_int(b + str_len(b), (int32_t)chain.row + 1);
    } else str_cpy(b, "PLAY: LOOP THE ROW", sizeof b);
    cv_text(4, 25, &AF_X, b, chain.hold ? T_THEME : T_DIM);
    song_len_txt(b, song_bars(4u));
    cv_text_r(236, 25, &AF_X, b, T_DIM, T_SURF);
    for (k = 0; k < NTRK; k++) {
        int32_t y = 38 + (int32_t)k * 14;
        uint32_t sl = chain_config.row[r].slot & 3u, src = row_src(r, k, sl), v = row_var(r, k), mu = tl_muted(r, k);
        uint32_t on = !mu && ((part_play.b[src] >> k) & 1u);
        uint16_t c = src != sl ? sec_col(src) : lz_sec(src, 1, 1), ink = src != sl ? T_INK : T_TEXT;
        b[0] = 'T'; b[1] = (char)('1' + k); b[2] = 0;
        GFX_HOOK_ALIGN(0, y, 0, y + 12, AL_V | AL_PASS, "timeline track row");
        cv_text(4, y + CAP_IN(X, 12), &AF_X, b, k == song.sel && !umv_tk ? T_THEME : T_MID);
        if (k == song.sel && !umv_tk) cv_rect(0, y + 2, 2, 8, T_THEME);
        if (!on) {
            cv_frame(24, y, 92, 12, T_DIM);
            cv_text(28, y + CAP_IN(X, 12), &AF_X, mu ? "MUTE" : "--", T_DIM);
        } else {
            cv_rect(24, y, 92, 12, c);
            if (src != sl) {
                str_cpy(b, "FROM  ", sizeof b); b[5] = (char)('A' + src);
                cv_text_on(28, y + CAP_IN(X, 12), &AF_X, b, ink, c);
            }
        }
        if (v > RV_MUTE) {
            cv_rect(75, y + 1, 1, 10, T_SURF);
            cv_rect(76, y + 1, 39, 10, T_ACCENT);
            GFX_HOOK_ALIGN(76, y + 1, 115, y + 11, AL_HV, "timeline variation chip");
            cv_text_in(76, y + 1 + CAP_IN(X, 10), 39, &AF_X, RV_NAME[v < RV_N ? v : 0], T_INK, T_ACCENT);
        }
    }
    umv_tl();                                            /* ((UMBRA LIVE) the right half: the row's two plates) */
    cv_text(4, 128, &AF_X, umv_tk ? "KNOB 4 PLATE \xB7 OCT+ OPENS IT" : "KEYS: F3-D4 MUTE \xB7 F4-B4 FROM", T_DIM);
}
/* PROJECT: the sections, the arrangement's chips, the song's strip (ar: ARRANGE, its rows picked; 2: GENE's lanes) */
static void graph_proj(int ar)
{
    uint32_t i, total = 0, sel = (uint32_t)song.g[G_SLOT] - 1u, n = chain_config.count;
    char b[24];
    if (ar == 2) {                                       /* (beta5) GENE's lanes (TIMELINE's until V10.6o): the song left to
                                                         * right, its bars the width; a lane for the rows (SEC) and one a track:
                                                         * its own section calm (half), another bright with its letter, MUTE an
                                                         * empty frame, a VAR its short name. (V10.6o: no scan lines) */
        static const char *const LN[NTRK + 1] = {"SEC", "T1", "T2", "T3", "T4", "T5", "T6"};
        static const char *const RV_SH[RV_N] = {"", "", "TH", "FL", "HF", "RV", "SH", "+8", "-8", "V1", "V2", "V3", "V4"};
        uint32_t t = song_bars(4u), acc = 0, played = 0, j;
        int32_t ls = 15, lh = ls - 2;                    /* (V10.6i) lanes 2 px shorter, the helix a band of its own */
        for (j = 0; j <= NTRK; j++)
            cv_text(5, 4 + (int32_t)j * ls + (j ? 3 : 2), &AF_X, LN[j], T_DIM);
        for (i = 0; i < n && t; i++) {
            const chain_row_t *r = &chain_config.row[i];
            uint32_t s = r->slot & 3u;
            int32_t x = 24 + (int32_t)(acc * 212u / t), w = 24 + (int32_t)((acc + r->bars) * 212u / t) - x;
            if (w < 2) w = 2;
            for (j = 0; j <= NTRK; j++) {          /* (V10.3) the columns lead: a row's edge a 1 px line of its colour
                                                    * well darker; between lanes 2 px of it only a little darker */
                int32_t y = 4 + (int32_t)j * ls, h = lh;
                uint32_t src = j ? row_src(i, j - 1u, s) : s, v = j ? row_var(i, j - 1u) : 0u, own = !j || src == s;
                uint16_t c = j && own ? lz_sec(src, 1, 1) : sec_col(src);
                if (v == RV_MUTE) {
                    cv_rect(x, y, w, h, T_BG);
                    cv_frame(x + 1, y + 1, w - 1, h - 2, T_DIM);
                } else {
                    cv_rect(x, y, w, h, c);
                    if (j < NTRK) cv_rect(x, y + h, w, 2, ux_mix(T_BG, c, 45));
                    if (i) cv_rect(x, y, 1, j < NTRK ? h + 2 : h, ux_mix(T_BG, c, 15));
                    if (w >= 12 && (!j || !own)) {
                        b[0] = (char)('A' + src); b[1] = 0;
                        if (!j && w >= 22) fmt_int(b + 1, r->bars);
                        cv_text_on(x + 3, y + 2 + (h - 15) / 2, &AF_X, b, T_INK, c);
                    }
                    if (v && v < RV_N && w >= 24) cv_text_r(x + w - 2, y + 2 + (h - 15) / 2, &AF_X, RV_SH[v], T_INK, c);
                }
            }
            if (chain.running && i < chain.row) played += r->bars;
            acc += r->bars;
        }
        if (chain.running && chain.row < n && t) {         /* the playhead */
            played += chain_config.row[chain.row].bars - chain.remaining;
            cv_rect(24 + (int32_t)(played * 212u / t), 2, 1, 4 + 7 * ls, T_TEXT);
        }
        song_len_txt(b, t);
        cv_text_r(236, 128, &AF_X, n ? b : "NO ROWS: OCT+ ADDS ONE", T_DIM, T_SURF);
        return;
    } else
    for (i = 0; i < 4u; i++) {
        int32_t x = LZ_TX(i);
        int used = graph_project_used(i), pick = !ar && i == (sel & 3u);
        uint16_t c = lz_sec(i, used, used ? !pick : -1), ink = used ? T_INK : T_DIM;
        const char *nm = graph_project_name(i);
        b[0] = (char)('A' + i); b[1] = 0;
        cv_rect(x, 2, 56, 49, c);
        GFX_HOOK_ALIGN(0, 8, 0, 19, AL_V | AL_PASS, "project section letter");
        cv_text_on(x + 6, 4, &AF_M, b, ink, c);
        cv_free_text(x + 6, 25, &AF_X, !used ? "EMPTY" : nm[0] ? nm : "USED", ink, c, 47);
        if (live.cur == (int8_t)i && used)
            cv_text_r(x + 52, 38, &AF_X, "NOW", ink, c);
        if (pick) {                                      /* (2 px, round it) */
            cv_frame(x - 2, 0, 60, 53, T_TEXT);
            cv_frame(x - 1, 1, 58, 51, T_TEXT);
        }
    }
    GFX_HOOK_ALIGN(0, 58, 0, 67, AL_V | AL_PASS, "project arrange title");
    cv_text(6, 58, &AF_X, ar == 2 ? "ARRANGE 2" : "ARRANGE", ar ? T_THEME : T_DIM);
    for (i = 0; i < n && i < CHAIN_ROWS; i++) {
        const chain_row_t *r = &chain_config.row[i];
        int32_t x = 4 + (int32_t)(i % 8u) * 293 / 10, y = 72 + (int32_t)(i / 8u) * 22;
        int pick = ar && i == ui.song_row, used = graph_project_used(r->slot);
        uint16_t c = lz_sec(r->slot, 1, !pick);
        b[0] = (char)('A' + (r->slot & 3u));
        fmt_int(b + 1, r->bars);
        cv_rect(x, y, 27, 20, c);
        GFX_HOOK_ALIGN(x, y, x + 27, y + 20, AL_HV, "arrange chip centred");
        cv_text_in(x, y + CAP_IN(X, 20), 27, &AF_X, b, used ? T_INK : T_DIM, c);
        if (pick)
            cv_frame(x - 1, y - 1, 29, 22, T_TEXT);
        if (song_idx.scene[song_cur][i].on)              /* a scene: a dot */
            cv_rect(x + 22, y + 3, 2, 2, T_TEXT);
        if (song_idx.tsrc[song_cur][i] || song_idx.tvar[song_cur][i])   /* (add15) tracks of their own: a dot below */
            cv_rect(x + 3, y + 15, 2, 2, T_TEXT);
        total += r->bars;
    }
    song_strip(118, 6, ar ? 5u : 6u, 0);                /* the song: its rows by their bars (V10.6o: song_strip's) */
    song_len_txt(b, total);
    GFX_HOOK_ALIGN(0, 128, 0, 137, AL_V | AL_PASS, "project song length");
    cv_text_r(236, 128, &AF_X, n ? b : "NO ROWS: OCT+ ADDS ONE", ar && ui.song_row >= song_end_row() ? T_THEME : T_DIM,
              T_SURF);
}
/* a being small at (cx, cy) radius r (px): engine e's species, its outline in heat c; 24 vertices (bg_outline's form,
 * its spikes and lobes, at a slot's size: a quarter of the lines) */
static void lz_mini_being(uint32_t e, int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    const genome_t *g = &GENOME[EGEN[e % NENGINES]];
    int32_t i, px = 0, py = 0, R = r * 16, k = g->k0 + g->kk / 2;
    for (i = 0; i <= 24; i++) {
        uint32_t th = (uint32_t)(i % 24) * 65536u / 24u;
        int32_t rr = R + ((R * 90 >> 8) * (9175 * bg_sin((uint32_t)g->lob * th) >> 15) >> 15), x, y, c2, p, n;
        c2 = bg_sin(((uint32_t)k * th >> 1) + (uint32_t)g->tw * 3000u + 0x4000u);
        p = c2 > 0 ? c2 : 0;
        for (n = 1; n < g->sh; n++)
            p = (p * c2) >> 15;
        if (p < 0) p = 0;
        rr += ((R * 110 >> 8) * p >> 15) * 26 >> 4;
        x = cx * 16 + ((rr * bg_sin(th + 0x4000u) >> 15) * g->ax >> 4);
        y = cy * 16 + (rr * bg_sin(th) >> 15);
        if (i)
            og_line(px, py, x, y, c);
        px = x;
        py = y;
    }
}
/* SOUND: the track's being (as an engine page's: its HOME knobs, alive with its sound), the track and its sound */
static void lz_track_being(void)
{
    static uint32_t tm, f0;
    const track_t *t = TSEL;
    const genome_t *g = &GENOME[EGEN[trk_eng(t)]];
    int32_t en = bg_energy();
    bfeat_t f;
    char b[20];
    bg_feat(t, g, trk_engp(t)->knob, &f);
    bg_excite(&f, en, 2);
    if (en > 2) tm += (ui.frame - f0) * (uint32_t)(160 + en * 3);
    f0 = ui.frame;
    being_lines(g, &f, 42 * 16, 40 * 16, (8 * 16) * (256 + en / 3) / 256, en, tm);   /* (its spikes inside 4 .. 82) */
    b[0] = 'T'; b[1] = (char)('1' + song.sel % NTRK); b[2] = 0;
    GFX_HOOK_ALIGN(4, 0, 82, 0, AL_H | AL_PASS, "sound track centred");
    cv_text_in(4, 85, 78, &AF_X, b, T_THEME, T_SURF);
    sound_name(t, b);
    cv_free_text(42 - HALF_UP(text_w(&AF_X, b) < 76 ? text_w(&AF_X, b) : 76), 99, &AF_X, b, T_TEXT, T_SURF, 76);
}
static __attribute__((noinline)) void graph_sound(void)
{
    uint32_t k;
    lz_track_being();
    for (k = 0; k < UP_SLOTS; k++) {
        int32_t x = 88 + (int32_t)(k % 4u) * 37, y = 2 + (int32_t)(k / 4u) * 17;
        int sel = k == ui.uslot, used = up_used(k);
        uint16_t bg = sel ? T_TINT : T_RAISE;
        char b[13];
        cv_rect(x, y, 35, 15, bg);
        if (used) {
            lz_mini_being(up_engine(k), x + 5, y + 7, 3, bg_col(150));
            up_name(k, b);
            cv_clip_text(x + 10, y + CAP_IN(X, 15), &AF_X, b, T_TEXT, bg, 24);
        } else {
            up_slot_label(b, k);
            GFX_HOOK_ALIGN(x, y, x + 35, y + 15, AL_HV, "sound slot label centred");
            cv_text_in(x, y + CAP_IN(X, 15), 35, &AF_X, b, T_DIM, bg);
        }
        if (sel)
            cv_frame(x, y, 35, 15, T_THEME);
    }
}
/* (V10.24, the user's pick A) PRESETS in two columns: at the left the engines (their being, how many presets), the
 * selected track's lit; at the right the list (LIST's) around the sound, its engine named only when another's. KNOB 1
 * plays each as it passes; OCT+ keeps it, OCT- brings back the sound from before the browse (ui_input.c br_undo) */
static __attribute__((noinline)) void graph_lz_presets(void)
{
    uint32_t total, cur = preset_pos(&total), e, k, i, first, r, rc = 0, rows = (graph_h() - 2u) / 15u;
    rows = rows > 9u ? 9u : rows;
    for (r = 0; r < NENG_SHOWN; r++) if (eng_vis(r) == TSEL->eng_req) rc = r;
    first = rc < rows / 2u ? 0u : rc - rows / 2u;
    if (first + rows > NENG_SHOWN) first = NENG_SHOWN > rows ? NENG_SHOWN - rows : 0u;
    for (r = first; r < NENG_SHOWN && r < first + rows; r++) {
        int32_t y = 2 + (int32_t)(r - first) * 15;
        int sel = r == rc;
        char n[4];
        e = eng_vis(r);
        lz_mini_being(e, 9, y + 7, 3, sel ? bg_col(230) : ux_mix(bg_col(120), T_SURF, 45));
        cv_clip_text(21, y + CAP_IN(X, 14), &AF_X, ENGINES[e]->name, sel ? T_TEXT : T_DIM, T_SURF, 46);
        n[0] = 0; fmt_int(n, (int32_t)preset_shown(e));
        cv_text_r(82, y + CAP_IN(X, 14), &AF_X, n, sel ? T_MID : T_DIM, T_SURF);
        if (sel) cv_rect(0, y + 2, 2, 10, T_ACCENT);
    }
    cv_rect(86, 2, 1, (int32_t)rows * 15 - 2, T_DIM);
    if (!total) {
        cv_text_in(88, 63, 148, &AF_X, "NO FAVORITES", T_MID, T_SURF);
        return;
    }
    first = cur < rows / 2u ? 0u : cur - rows / 2u;
    if (total > rows && first + rows > total)
        first = total - rows;
    for (i = first; i < total && i < first + rows; i++) {
        int32_t y = 2 + (int32_t)(i - first) * 15;
        int sel = i == cur;
        uint16_t bg = sel ? T_TINT : T_SURF;
        char nm[13];
        const char *en;
        e = preset_at(i, &k);
        if (e == NENGINES) {
            up_name(k, nm);
            en = "USER";
        } else {
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
            en = ENGINES[e]->name;
        }
        if (sel)
            cv_rect(90, y, 146, 14, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset row centred up/down");
        cv_free_text(96, y + CAP_IN(X, 14), &AF_X, nm, sel ? T_TEXT : T_MID, bg, 92);
        if (favorite_has(e, k)) {
            GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset star centred up/down");
            cv_text_on(226, y + CAP_IN(S, 14), &AF_S, "*", T_ACCENT, bg);   /* (a favourite; 0.8.5 no icons) */
        }
        if (e != TSEL->eng_req) {                       /* (another engine's: LIST crosses engines) */
            GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset row centred up/down");
            cv_text_r(220, y + CAP_IN(X, 14), &AF_X, eng_abbr(en), T_DIM, bg);
        }
    }
}
/* the lens's panel for the SAVE page shown */
static void graph_save_lz(const page_t *pg)
{
    uint32_t lv = save_level();
    if (pg->graph == GR_SONGSAVE) graph_lib();
    else if (pg->graph == GR_TOOLS) graph_lz_tools();
    else if (SONG_TRK(pg)) graph_tl();
    else if (SONG_PARTS(pg)) graph_parts();
    else if (SONG_CLIPS(pg)) graph_clips();
    else if (pg->graph == GR_SONG) graph_proj(1);
    else if (pg->graph == GR_SLOTS) { if (lv == SV_PROJ) graph_proj(0); else graph_lib(); }
    else if (pg->graph == GR_USER) graph_sound();
    else graph_lz_presets();
}

