/* SPDX-License-Identifier: GPL-3.0-only */
static int fit_storage_test(void)
{
    int bad = 0;
    uint32_t sig;
    reset();
    memset(&song_idx, 0, sizeof song_idx);
    song_idx.magic = SONG_MAGIC; song_idx.version = SONG_IDX_V;
    song_cur = 0; live.cur = -1; live.dirty = 0;
    trk[0].seq_fit = 128u | 16u;
    trk[1].seq_fit = 23u;
    bad += check("FIT: write actual NOR flash", !project_save(0));
    trk[0].seq_fit = trk[1].seq_fit = 0;
    project_load(0); transport_req = 0;
    bad += check("FIT: flash load restores ON and a disabled SPAN", trk[0].seq_fit == 144u && trk[1].seq_fit == 23u);
    sig = autosave_sig();
    trk[0].seq_fit = 128u | 1u;
    bad += check("FIT edit changes the autosave signature", sig != autosave_sig());
    bad += check("SAVE SONG AS also copies fitted clocks", !song_save_as(3, "FIT COPY")   /* ((UMBRA LIVE) project 4, the last) */);
    bad += check("copied FIT survives decoding the destination flash", st_load(OBJ_SONG1 + 8u, &proj_wire, sizeof proj_wire) == sizeof proj_wire &&
                 proj_import(&proj_scratch, &proj_wire, sizeof proj_wire) && proj_scratch.fit[0] == 129u && proj_scratch.fit[1] == 23u);
    proj_scratch.fit[0] = 255;
    proj_scratch.sum = proj_sum(&proj_scratch);
    bad += check("invalid FIT payload refuses serialization", !proj_pack(&proj_wire, &proj_scratch));
    return bad;
}
