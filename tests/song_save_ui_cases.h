/* SPDX-License-Identifier: GPL-3.0-only */
static int test_song_save_ui(void)
{
    int bad = 0, ok;
    uint32_t target;
    char a[24], b[24];
    ui_power_on();
    song_cur = 0;
    memset(&song_idx, 0, sizeof song_idx);
    memset(&chain, 0, sizeof chain);
    live.cur = -1;
    name_close();
    nav_put(SEC_SAVE, "SAVE SONG AS"); frame();
    ok = cur_page()->graph == GR_SONGSAVE && save_level() == SV_PROJ && ui.act == 4u && song_save_target == song_cur;
    bad += check("SAVE SONG AS is a PROJECT page with SAVE armed and the current destination", ok);
    target = song_save_target;
    turn(EN_K1, 1); frame();
    ok = song_save_target == target + 1u && song_cur == 0 && live.cur == -1;
    bad += check("destination knob does not open another song or change the source", ok);
    press(B_OCTUP); frame();
    ok = name_on() && nm.kind == NK_SONG_SAVE && nm.slot == song_save_target;
    nm_title(a);
    ok &= strstr(a, "SAVE SONG TO PROJECT") != 0;
    press(B_OCTDN); frame();
    ok &= !name_on() && !ui.confirm && song_cur == 0;
    bad += check("empty destination opens naming; cancel keeps the source", ok);
    song_idx.rows[song_save_target].count = 1;
    turn(EN_K4, 1); press(B_OCTUP); frame();
    ok = ui.confirm == CF_OVR_SONG && !name_on();
    confirm_text(a, b);
    ok &= strstr(a, "OVERWRITE PROJECT") != 0 && !strcmp(b, "ALL A-D + ARRANGEMENT");
    press(B_OCTDN); frame();
    ok &= !ui.confirm && !name_on();
    bad += check("occupied destination asks whole-song overwrite; cancel writes nothing", ok);
    turn(EN_K4, 1); press(B_OCTUP); frame();
    press(B_OCTUP); frame();
    ok = name_on() && nm.kind == NK_SONG_SAVE;
    press(B_OCTUP); frame();
    ok &= name_on() && strstr(ui.msg, "NO FLASH") != 0;  /* ui_test has no flash: never a false SAVED */
    press(B_OCTDN); frame();
    bad += check("overwrite confirmation leads to naming; failed save keeps the name open", ok);
    song_save_target = song_cur;
    turn(EN_K4, 1); press(B_OCTUP); frame();
    bad += check("current project is available as an explicit save destination", name_on() || ui.confirm == CF_OVR_SONG);
    if (name_on()) { press(B_OCTDN); frame(); }
    else if (ui.confirm) { press(B_OCTDN); frame(); }
    song_save_target = 2;
    song.playing = 1;
    turn(EN_K4, 1); press(B_OCTUP); frame();
    bad += check("playing refuses before naming or overwrite confirmation", !name_on() && !ui.confirm && strstr(ui.msg, "STOP TO SAVE") != 0);
    song.playing = 0;
    for (uint32_t skin = 0; skin < 4; skin++) {
        ux.skin = skin;
        ui.force = 1;
        frame();
    }
    ui_power_on();
    nav_put(SEC_SAVE, "SAVE SONG AS"); frame();
    turn(EN_K2, 1); frame();
    song_idx.rows[song_save_target].count = 1;
    press(B_OCTUP); frame();
    confirm_text(a, b);
    bad += check("LOOP overwrite explains replacement of A and clearing B-D",
                 song_save_loop && ui.confirm == CF_OVR_SONG && strstr(b, "CLEAR B-D") != 0);
    press(B_OCTDN); frame();
    ui_power_on();
    song_draft = 1; song_save_target = song_cur;
    nav_put(SEC_SAVE, "PROJECTS"); frame();
    target = song_cur;
    turn(EN_K1, 1); frame();
    bad += check("PROJECTS destination browsing preserves the playing draft buffers",
                 song_draft && song_cur == target && song_save_target == target + 1u);
    turn(EN_K3, 1); frame();
    bad += check("PROJECTS LOAD availability depends on transport, not the draft's selected part", act_ready());
    turn(EN_K2, 1); press(B_OCTUP); frame();
    bad += check("PROJECTS ERASE targets the selected saved project and can be cancelled",
                 ui.confirm == CF_ERASE_SONG && ui.confirm_trk == song_save_target);
    press(B_OCTDN); frame();
    song.playing = 1;
    turn(EN_K2, 1); press(B_OCTUP); frame();
    bad += check("PROJECTS ERASE refuses during playback without discarding the draft",
                 !ui.confirm && song_draft && strstr(ui.msg, "STOP TO ERASE") != 0);
    song.playing = 0;
    song_draft = 0;
    return bad;
}
