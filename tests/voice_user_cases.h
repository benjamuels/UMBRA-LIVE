/* SPDX-License-Identifier: GPL-3.0-only */
static int voice_user_test(void)
{
    int bad = 0, rc;
    char text[LYR_LEN];
    uint8_t patch[FM6_PACKED], got[FM6_PACKED];
    static upf_t previous;
    reset(); upf_empty();
    set_engine_of(&trk[0], ENGI_VOICE);
    trk[0].engine = ENGI_VOICE;
    trk[0].p[P_VLANG] = 2; trk[0].p[P_VLYR] = 0;
    for (uint32_t i = 0; i < LYR_LEN - 1u; i++) text[i] = "Abc de'-_0123"[i % 13u];
    text[LYR_LEN - 1u] = 0;
    lyr_set(0, text);
    bad += check("VOICE USER: stores all 63 lyric characters", !up_store(0, "VOICE ONE"));
    lyr_set(0, "OTHER WORDS");
    bad += check("VOICE USER: reload restores exact lyric, language and TEXT mode", !up_load(0) &&
                 !strcmp(lyr_get(0), text) && trk[0].p[P_VLANG] == 2 && !trk[0].p[P_VLYR]);
    memset(up_bank, 0, sizeof up_bank); upf_empty(); up_boot();
    lyr_set(0, "AFTER BOOT");
    bad += check("VOICE USER: lyric survives reload of actual NOR banks", !up_load(0) && !strcmp(lyr_get(0), text));
    bad += check("VOICE USER: rename keeps the attached text", !up_rename(0, "RENAMED") && !strcmp(upf_lyric(0), text));
    lyr_set(0, "SECOND LYRIC");
    bad += check("VOICE USER: two equal sounds can carry different lyrics", !up_store(1, "VOICE TWO") &&
                 !strcmp(upf_lyric(0), text) && !strcmp(upf_lyric(1), "SECOND LYRIC"));
    set_engine_of(&trk[0], ENGI_FM6); trk[0].engine = ENGI_FM6;
    fm6_pack(fm6_patch[0], patch);
    bad += check("FM6 USER: patch still stores and reloads alongside VOICE lyrics", !up_store(2, "FM SIX") &&
                 !upf_get(2, got) && !memcmp(patch, got, sizeof patch) && !strcmp(upf_lyric(0), text));
    bad += check("payload types cannot be confused between VOICE and FM6", upf_get(0, got) && !upf_lyric(2)[0]);
    up_load(0);
    { up_rec_t legacy = *up_rec(0);
      bad += check("old VOICE preset without lyric clears unrelated text", !up_put(3, &legacy) &&
                   !up_load(3) && !lyr_get(0)[0]); }
    lyr_set(0, "");
    bad += check("VOICE USER: empty text is explicitly saved", !up_store(4, "EMPTY TEXT"));
    lyr_set(0, "DO NOT LEAK");
    bad += check("VOICE USER: empty text restores empty", !up_load(4) && !lyr_get(0)[0]);
    up_load(0);
    previous = upf;
    lyr_set(0, "NEW TEXT AFTER RETRY");
    fail_after = (int)((sizeof(up_bank_t) + 255u) / 256u + 1u);  /* bank commits, auxiliary write fails */
    rc = up_store(0, "RENAMED"); fail_after = -1;
    bad += check("VOICE USER: auxiliary flash failure reports error and rolls back its RAM entry", rc == 2 &&
                 !memcmp(&upf, &previous, sizeof upf));
    lyr_set(0, "NEW TEXT AFTER RETRY");
    bad += check("VOICE USER: retry commits and restores the new text", !up_store(0, "RENAMED") &&
                 !up_load(0) && !strcmp(lyr_get(0), "NEW TEXT AFTER RETRY"));
    lyr_set(0, text);
    bad += check("VOICE USER: final slot 32 supports the complete lyric", !up_store(31, "LAST SLOT") && !strcmp(upf_lyric(31), text));
    { uint8_t saved = upf.e[31].pk[4]; upf.e[31].pk[4] = 1u;
      bad += check("VOICE USER: malformed text is refused", !upf_lyric(31)[0]);
      upf.e[31].pk[4] = saved; }
    return bad;
}
