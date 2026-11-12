/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the production song copy against persistence_test's NOR flash, including failed writes. */

static project_store_t source_before[4];
static uint8_t source_flash[8u * ST_SECTOR];
static song_index_t index_before;

static void song_fixture(void)
{
    reset();
    memset(&song_idx, 0, sizeof song_idx);
    song_idx.magic = SONG_MAGIC;
    song_idx.version = SONG_IDX_V;
    song_cur = 0;
    live.cur = -1;
    live.dirty = 0;
    live.mode = 1;
    proj_cur = PROJ_NO_SLOT;
    for (uint32_t s = 0; s < 4; s++) {
        trk[0].step[0].note[0] = (uint8_t)(48 + s);
        trk[0].step[0].n = 1;
        trk[0].step[0].time = ST_NOTE;
        section_store(s);
    }
    section_load(2);
    trk[0].step[0].note[0] = 90;             /* live C differs from stored C */
    song.g[G_BPM] = 143;
    chain_config.count = 4;
    dice_secs = 15;
    SONG_END(&chain_config) = 3;
    for (uint32_t r = 0; r < 4; r++) {
        chain_config.row[r] = (chain_row_t){(uint8_t)r, (uint8_t)(r + 2)};
        song_idx.scene[0][r].on = 1;
        song_idx.scene[0][r].mute = (uint8_t)(r + 1);
        song_idx.scene[0][r].mac[0] = (uint8_t)(40 + r);
        song_idx.tsrc[0][r] = 2u;           /* T1 plays B */
        song_idx.tvar[0][r] = RV_OCTU;
    }
    song_flush();
    song_index_save();
    memcpy(source_before, proj_slot, sizeof source_before);
    memcpy(source_flash, nor + 0x97000u, sizeof source_flash);
    index_before = song_idx;
}

static int song_save_test(void)
{
    int bad = 0, ok;
    uint32_t before;
    song_fixture();
    ok = !song_save_as(3, "MY SONG");
    bad += check("SAVE SONG AS copies a complete song to project 4 ((UMBRA LIVE) the last of 4)", ok);
    ok = song_cur == 0 && live.cur == 2 && trk[0].step[0].note[0] == 90 && dice_secs == 15 &&
         !memcmp(source_before, proj_slot, sizeof source_before) &&
         !memcmp(source_flash, nor + 0x97000u, sizeof source_flash);
    bad += check("source stays open, live edits and source flash remain intact", ok);
    ok = 1;
    for (uint32_t s = 0; s < 4; s++) {
        uint32_t obj = OBJ_SONG1 + 2u * 4u + s;
        ok &= st_load(obj, &proj_wire, sizeof proj_wire) == sizeof proj_wire &&
              proj_import(&proj_scratch, &proj_wire, sizeof proj_wire);
        ok &= proj_scratch.t[0].step[0].note[0] == (s == 2 ? 90 : 48 + s) && proj_scratch.g[G_BPM] == 143;
        if (!s) ok &= !memcmp(proj_scratch.name, "MY SONG", 7);
    }
    bad += check("all sections copied, unsaved C included, current tempo in A-D", ok);
    ok = !memcmp(song_idx.scene[3], song_idx.scene[0], sizeof song_idx.scene[0]) &&
         !memcmp(song_idx.tsrc[3], song_idx.tsrc[0], sizeof song_idx.tsrc[0]) &&
         !memcmp(song_idx.tvar[3], song_idx.tvar[0], sizeof song_idx.tvar[0]) &&
         song_idx.rows[3].count == 4 && SONG_END(&song_idx.rows[3]) == 3 && song_idx.rows[3].rsv[1] == 0;
    bad += check("order, repeats, scenes, track sources and variations preserved", ok);
    memset(&song_idx, 0, sizeof song_idx);
    song_index_load();                          /* read the committed index back, as on reboot */
    live.mode = 0;
    ok = !song_select(3) && song_cur == 3 && live.mode == 1 && song.g[G_BPM] == 143 && chain_config.count == 4;
    bad += check("reload from flash restores sections, arrangement and SONG mode", ok);

    song_fixture();
    before = erases;
    song.playing = 1; ok = song_save_as(1, "X") != 0; song.playing = 0;
    transport_req = 1; ok &= song_save_as(1, "X") != 0; transport_req = 0;
    ok &= song_save_as(NSONG, "X") != 0 && song_save_as(7, "X") != 0;   /* (V10.6p: 7, 8 retired) */
    ok &= song_save_as(NSONG_USE, "X") != 0;              /* ((UMBRA LIVE) 5: the plate bank's flash) */
    flash_ok = 0; ok &= song_save_as(1, "X") != 0; flash_ok = 1;
    ok &= erases == before;
    bad += check("playing, pending PLAY, invalid slot and no flash refuse without writing", ok);

    song_fixture();
    memset(&proj_slot[3], 0, sizeof proj_slot[3]);
    before = erases;
    bad += check("missing arrangement section refuses before any destination write", song_save_as(1, "X") != 0 && erases == before);

    song_fixture();
    erase_error = 1;
    ok = song_save_as(1, "X") != 0 && !memcmp(&index_before, &song_idx, sizeof song_idx);
    erase_error = 0;
    ok &= !memcmp(source_flash, nor + 0x97000u, sizeof source_flash) && !song_save_as(1, "RETRY");
    bad += check("erase failure reports failure, source intact, retry succeeds", ok);

    song_fixture();
    fail_after = 64;                            /* 4 x (15 payload pages + commit), fail in the index */
    ok = song_save_as(1, "X") != 0 && !memcmp(&index_before, &song_idx, sizeof song_idx);
    fail_after = -1;
    ok &= !memcmp(source_flash, nor + 0x97000u, sizeof source_flash) && !song_save_as(1, "RETRY");
    bad += check("index write failure does not report success or alter RAM index; retry works", ok);

    song_fixture();
    song_save_as(1, "OLD FOUR");
    memset(&proj_slot[2], 0, sizeof proj_slot[2]);
    memset(&proj_slot[3], 0, sizeof proj_slot[3]);
    live.cur = 1;
    chain_config.count = 2;
    chain_config.row[0].slot = 0;
    chain_config.row[1].slot = 1;
    ok = !song_save_as(1, "TWO PARTS");
    { char name[13]; ok &= lib_scan(1, name) == 3u && !strcmp(name, "TWO PARTS"); }
    ok &= !song_select(1) && project_used(0) && project_used(1) && !project_used(2) && !project_used(3);
    bad += check("overwriting with two sections hides old C/D in library and on reload", ok);
    printf("song save: %s\n", bad ? "FAILED" : "passed");
    return bad;
}
