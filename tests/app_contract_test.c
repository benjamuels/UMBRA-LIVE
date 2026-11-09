/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The editor protocol's stability contract (web/EDITOR_PROTOCOL.md "Stability contract (1.0)"), from the side of the
 * apps that depend on it (the JIANT Stems app's Fm1Link / Fm1Archive, the web editor): the real handler (editor.c,
 * through editor_test.c's host harness) is fed the exact byte sequences such an app sends, as USB-MIDI event packets
 * cut the way Fm1Link.packets cuts them, and every reply is compared byte for byte with a golden description of it
 * (the frozen fields exact, the state-dependent ones by their kind and place). Also: the engine id table, the WATCH
 * push opt-ins (an old client's WATCH 3 sees nothing new), the NRPN map in both directions (midi_control.c, seq.c
 * mo_knobs) and docs/NRPN.md / web/NRPN.md generated from params.c, so the table cannot drift.
 *   cc ... tests/app_contract_test.c -o app-contract && app-contract           (the workflow, from the repository root)
 *   app-contract --write-nrpn                                                 (writes docs/NRPN.md and web/NRPN.md) */
#include <stdint.h>
struct track;
static void excite_turn(struct track *, const int16_t *, int32_t);
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"

static void excite_turn(struct track *t, const int16_t *vp, int32_t old) { (void)t; (void)vp; (void)old; }

static int bad;
static void ok(const char *what, int c)
{
    printf("contract: %-96s %s\n", what, c ? "ok" : "FAIL");
    bad += !c;
}

/* ------------------------------------------------------------------ the wire, as the app drives it */
/* a request as Fm1Link.req builds it: F0 7D 46 4C cmd args F7 */
static uint32_t app_req(uint8_t *f, uint32_t cmd, const int *args, uint32_t n)
{
    uint32_t i;
    f[0] = 0xF0; f[1] = 0x7D; f[2] = 0x46; f[3] = 0x4C; f[4] = (uint8_t)cmd;
    for (i = 0; i < n; i++) f[5 + i] = (uint8_t)(args[i] & 0x7F);
    f[5 + n] = 0xF7;
    return 6 + n;
}
/* the frame as Fm1Link.packets sends it (cable 0): CIN 4 for three bytes, 5 / 6 / 7 for the last one to three */
static void app_send(const uint8_t *f, uint32_t n)
{
    uint32_t i = 0;
    while (i < n) {
        uint32_t left = n - i, cin = left > 3u ? 4u : left == 1u ? 5u : left == 2u ? 6u : 7u, k, pkt = cin;
        for (k = 0; k < 3u && k < left; k++)
            pkt |= (uint32_t)f[i + k] << (8u * (k + 1u));
        midi_in_event(pkt);
        i += left < 3u ? left : 3u;
    }
}
/* one request, the main loop's turn (pushes first, then the reply), everything sent drained into host_wire */
static void app(uint32_t cmd, const int *args, uint32_t n)
{
    uint8_t f[300];
    uint32_t len = app_req(f, cmd, args, n);
    host_wire_n = 0;
    ota_frame_done();
    app_send(f, len);
    ed_service();
    host_drain();
}
#define APP(cmd, ...) do { static const int a_[] = {__VA_ARGS__}; app(cmd, a_, sizeof a_ / sizeof a_[0]); } while (0)
#define APP0(cmd) app(cmd, 0, 0)
/* the main loop a little later, a few passes (each sends what the SysEx ring has room for): only the pushes */
static void pump(void)
{
    uint32_t i;
    host_wire_n = 0;
    for (i = 0; i < 6u; i++) {
        fm1_ms += 25;
        ed_w.last_ms = fm1_ms;
        ed_sync();
        host_drain();
    }
}
/* the k-th frame (0 the first) with cmd in host_wire: the bytes after F0 7D 46 4C cmd up to F7, *n of them */
static const uint8_t *frame_k(uint32_t cmd, uint32_t k, uint32_t *n)
{
    uint32_t i = 0, j;
    while (i + 5u < host_wire_n) {
        if (host_wire[i] != 0xF0) { i++; continue; }
        for (j = i + 1u; j < host_wire_n && host_wire[j] != 0xF7; j++) {}
        if (j < host_wire_n && host_wire[i + 1] == 0x7D && host_wire[i + 2] == 0x46 && host_wire[i + 3] == 0x4C &&
            host_wire[i + 4] == cmd && !k--) {
            *n = j - i - 5u;
            return host_wire + i + 5u;
        }
        i = j + 1u;
    }
    *n = 0;
    return 0;
}
static const uint8_t *frame(uint32_t cmd, uint32_t *n) { return frame_k(cmd, 0, n); }
static uint32_t frames(uint32_t cmd)
{
    uint32_t n, k = 0;
    while (frame_k(cmd, k, &n)) k++;
    return k;
}

/* ------------------------------------------------------------------ golden descriptions */
/* a reply's fields in order: an exact byte, a byte of any value (state, not layout), a v14, a string. tail: bytes after
 * the description are allowed (a reply that grows at its end, additive: the app reads its prefix) */
typedef struct {
    uint8_t b[1100], any[1100];
    const char *what[1100];
    uint32_t n, tail;
} golden_t;
static golden_t G;
static void g_new(void) { memset(&G, 0, sizeof G); }
static void g_b(uint32_t v, const char *what) { G.what[G.n] = what; G.b[G.n++] = (uint8_t)v; }
static void g_any(const char *what) { G.what[G.n] = what; G.any[G.n++] = 1; }
static void g_v(int32_t v, const char *what) { uint32_t u = (uint32_t)(v + 8192); g_b(u & 127u, what); g_b(u >> 7, what); }
static void g_vany(const char *what) { g_any(what); g_any(what); }
static void g_s(const char *s, const char *what) { while (*s) g_b((uint8_t)*s++, what); g_b(0, what); }
static void g_u32(uint32_t v, const char *what) { for (uint32_t i = 0; i < 5u; i++) g_b((v >> (7u * i)) & 127u, what); }
static void g_u32any(const char *what) { for (uint32_t i = 0; i < 5u; i++) g_any(what); }
static int g_check(const char *what, const uint8_t *r, uint32_t n)
{
    uint32_t i;
    if (!r) { ok(what, 0); printf("    no reply\n"); return 0; }
    if (n < G.n || (!G.tail && n != G.n)) {
        ok(what, 0);
        printf("    reply %u bytes, golden %u%s\n", n, G.n, G.tail ? " or more" : "");
        return 0;
    }
    for (i = 0; i < G.n; i++)
        if (!G.any[i] && r[i] != G.b[i]) {
            ok(what, 0);
            printf("    byte %u (%s): %u, golden %u\n", i, G.what[i], r[i], G.b[i]);
            return 0;
        }
    ok(what, 1);
    return 1;
}
static int32_t v14_at(const uint8_t *r) { return (int32_t)(r[0] | r[1] << 7) - 8192; }

/* ------------------------------------------------------------------ the frozen tables */
/* the engine byte: numbers fixed for good, new engines appended. The names INFO sends (the display may differ) */
static const char *const ENGINE_NAMES[16] = {"ANALOG", "-", "PHASE", "MATH", "-", "VOICE", "-", "WHEEL",
                                             "STRING", "-", "DRUM", "NOISE", "FM6", "ADD", "ANIMAL", "EXT"};
/* the app's own table (Fm1Link Fm1State.engineName, VisFeed ENG): the number of each engine it names */
static const struct { uint32_t id; const engine_t *e; const char *app; } APP_ENGINES[] = {
    {0, &ENG_ANALOG, "ANALOG"}, {2, &ENG_PHASE, "PHASE"}, {3, &ENG_LOFI, "LOFI"}, {5, &ENG_FORMANT, "VOICE"},
    {7, &ENG_WHEEL, "WHEEL"}, {10, &ENG_DRUM, "DRUM"}, {11, &ENG_NOISE, "NOISE"}, {12, &ENG_FM6, "FM6"},
    {14, &ENG_ANIMAL, "ANIMAL"}, {8, &ENG_STRING, "STRING"}, {13, &ENG_ADD, "ADD"}, {15, &ENG_INPUT, "EXT"},
};
/* the contract's command numbers (frozen) and the app's parameter ids */
enum { C_INFO = 1, C_GET = 2, C_SET = 3, C_WATCH = 22, C_CHANGED = 23, C_RELOAD = 24, C_PING = 25, C_TRACK = 27,
       C_TRACK_DUMP = 29, C_TRACK_PARAM = 31, C_TRACK_CHANGED = 32, C_SONG = 33, C_TRANSPORT = 40, C_PUNCH = 41,
       C_MUTATE = 42, C_LIST = 65, C_BGET = 66, C_PUT = 67, C_MENU_DESC = 72, C_MENU_SET = 73 };
enum { APP_P_MUTE = 40, APP_MO_INDEX = 20, APP_MO_ID = 20, APP_MO_ALL = 2 };
/* frozen since 1.0; additive changes only (core.h: common parameters go just before P_E0, so P_E0 and P_COUNT may
 * grow, a version bump each; G_COUNT may grow) */
enum { K_P_COUNT = 123, K_P_E0 = 115, K_G_COUNT = 73, K_NSTEP = 64, K_NTRK = 6, K_NENGINES = 16 };

static int contract_constants(void)
{
    uint32_t i;
    int same = 1;
    ok("command numbers of the contract are the handler's (INFO .. MENU_SET, the 1.0 pushes 40..42)",
       ED_INFO == C_INFO && ED_GET == C_GET && ED_SET == C_SET && ED_WATCH == C_WATCH && ED_CHANGED == C_CHANGED &&
       ED_RELOAD == C_RELOAD && ED_PING == C_PING && ED_TRACK == C_TRACK && ED_TRACK_DUMP == C_TRACK_DUMP &&
       ED_TRACK_PARAM == C_TRACK_PARAM && ED_TRACK_CHANGED == C_TRACK_CHANGED && ED_SONG == C_SONG &&
       ED_TRANSPORT == C_TRANSPORT && ED_PUNCH == C_PUNCH && ED_MUTATE == C_MUTATE && ED_BACKUP_LIST == C_LIST &&
       ED_BACKUP_GET == C_BGET && ED_BACKUP_PUT == C_PUT && ED_MENU_DESC == C_MENU_DESC && ED_MENU_SET == C_MENU_SET);
    ok("protocol version 1.x (a major never changes: that would be a break)", ED_PROTO_MAJOR == 1u);
    ok("P_COUNT 123, P_E0 115 (E1..E8 the last 8), G_COUNT 73, NSTEP 64, NTRK 6, NENGINES 16",
       P_COUNT == K_P_COUNT && P_E0 == K_P_E0 && P_COUNT - P_E0 == 8 && G_COUNT == K_G_COUNT && NSTEP == K_NSTEP &&
       NTRK == K_NTRK && NENGINES == K_NENGINES);
    ok("the app's ids: P_LEVEL 0, P_PAN 39, P_MUTE 40, G_BPM 0, G_DJF 17; MENU's MIDI OUT index 20 id 20, ALL 2",
       P_LEVEL == 0 && P_PAN == 39 && P_MUTE == APP_P_MUTE && G_BPM == 0 && G_DJF == 17 &&
       ED_MENU[APP_MO_INDEX][1] == APP_MO_ID && ED_MENU[APP_MO_INDEX][0] == MI_MIDIOUT && MO_ALL == APP_MO_ALL);
    for (i = 0; i < NENGINES; i++)
        same &= !strcmp(ENGINES[i]->name, ENGINE_NAMES[i]);
    ok("engine id table: 0 ANALOG 2 PHASE 3 MATH 5 VOICE 7 WHEEL 8 STRING 10 DRUM 11 NOISE 12 FM6 13 ADD 14 ANIMAL 15 EXT", same);
    same = 1;
    for (i = 0; i < NELEM(APP_ENGINES); i++)
        same &= ENGINES[APP_ENGINES[i].id] == APP_ENGINES[i].e;
    ok("every engine the app names is at the number it uses (3 is the LOFI engine, shown as MATH)", same);
    same = 1;
    for (i = 0; i < NENGINES; i++)                       /* the reserved numbers stay reserved */
        if (i == 1 || i == 4 || i == 6 || i == 9)
            same &= ENGINES[i]->name[0] == '-' && !ENGINES[i]->npresets;
    ok("reserved engine numbers 1 4 6 9: named '-', no presets", same);
    return 0;
}

/* ------------------------------------------------------------------ the requests the app sends */
static void golden_info(void)
{
    uint32_t i;
    g_new();
    g_s("JIANT " FELUCCA_VERSION, "version");
    g_b(K_NENGINES, "NENGINES"); g_b(K_P_COUNT, "P_COUNT"); g_b(K_G_COUNT, "G_COUNT"); g_b(K_NSTEP, "NSTEP");
    g_b(K_P_E0, "P_E0 (Fm1Link: the 5th byte after the version)");
    for (i = 0; i < K_NENGINES; i++)
        g_s(ENGINE_NAMES[i], "engine name");
    g_b(K_NTRK, "NTRK"); g_b(CHAIN_ROWS, "CHAIN_ROWS");
    g_b(0x55, "tag U"); g_b(1, "U"); g_any("ui caps");
    g_b(0x4D, "tag M"); g_b(1, "M"); g_b(MOTION_MAX, "motion max"); g_b(1, "chance");
    g_b(0x42, "tag B"); g_b(1, "B"); g_b(3, "backup caps");
    g_b(0x46, "tag F"); g_b(1, "F"); g_b(FM6_NFACTORY, "FM6 factory"); g_b(0, "FM6 bank");
    g_b(0x53, "tag S"); g_b(1, "S"); g_b(3, "live sync caps");
    g_b(0x50, "tag P"); g_b(1, "P"); g_b(3, "FM6 v2");
    g_b(0x4E, "tag N"); g_b(1, "N"); g_any("MENU settings count (grows)");
    g_b(0x52, "tag R"); g_b(1, "R"); g_b(4, "ratchet");
    g_b(0x4C, "tag L"); g_b(1, "L"); g_b(1, "locks");
    g_b(0x56, "tag V"); g_b(1, "V"); g_b(FM6_NBANK, "FM6 voice bank");
    g_b(0x4A, "tag J"); g_b(1, "J"); g_b(1, "JIANT");
    g_b(0x4F, "tag O"); g_b(1, "O"); g_b(LM_N, "lane modes");
    g_b(0x43, "tag C (1.0)"); g_b(2, "C"); g_b(1, "protocol major"); g_any("protocol minor");
    g_b(0x59, "tag Y (the web editor reads it last)"); g_b(1, "Y"); g_b(LYR_LEN - 1u, "lyric");
}

static void requests(void)
{
    const uint8_t *r;
    uint32_t n, i, t;
    reset();
    /* INFO: F0 7D 46 4C 01 F7 */
    APP0(C_INFO);
    r = frame(C_INFO, &n);
    golden_info();
    g_check("INFO (F0 7D 46 4C 01 F7): version, counts, P_E0, the 16 engine names, NTRK, the tags, 'C' 1.x, 'Y' last", r, n);
    if (r) {                                               /* the way Fm1Link reads it */
        uint32_t p = 0, neng, pe0, k;
        while (p < n && r[p]) p++;
        p++;
        neng = r[p]; pe0 = r[p + 4]; p += 5;
        for (k = 0; k < neng; k++) { while (p < n && r[p]) p++; p++; }
        ok("INFO read as Fm1Link does: 16 names, P_E0 115, then bytes it ignores", neng == 16 && pe0 == 115 && p < n);
        ok("INFO: the protocol version tag 43 02 01 xx sits right before the last tag", n >= 7 && r[n - 7] == 0x43 &&
           r[n - 6] == 2 && r[n - 5] == ED_PROTO_MAJOR && r[n - 4] == ED_PROTO_MINOR);
    }
    /* WATCH 3: F0 7D 46 4C 16 03 F7 */
    APP(C_WATCH, 3);
    r = frame(C_WATCH, &n);
    g_new(); g_b(3, "on | TRACK_CHANGED");
    g_check("WATCH 3 -> 3 (exactly: no new bit for an old client)", r, n);
    /* TRACK: F0 7D 46 4C 1B F7 (never with an argument from the app) */
    APP0(C_TRACK);
    r = frame(C_TRACK, &n);
    g_new(); g_b(song.sel, "selected"); g_b(K_NTRK, "NTRK");
    for (t = 0; t < NTRK; t++) {
        g_b(trk_eng(&trk[t]), "engine"); g_b(trk[t].preset, "preset"); g_v(trk[t].p[P_LEVEL], "level");
        g_b(trk[t].p[P_MUTE] != 0, "mute"); g_b(0, "armed");
    }
    g_check("TRACK (1B): selected, NTRK, 6 x (engine, preset, level v14, mute, armed)", r, n);
    /* TRACK_DUMP t: F0 7D 46 4C 1D t F7 */
    for (t = 0; t < NTRK; t++) {
        int a[1] = {(int)t};
        trk[t].p[P_E0 + 2] = (int16_t)(t + 3u);
        app(C_TRACK_DUMP, a, 1);
        r = frame(C_TRACK_DUMP, &n);
        g_new(); g_b(t, "track"); g_b(trk_eng(&trk[t]), "engine"); g_b(trk[t].preset, "preset");
        for (i = 0; i < P_COUNT; i++)
            g_v(motion_base_value(&trk[t], i), i >= P_E0 ? "E value" : "P value");
        if (!g_check(t ? "TRACK_DUMP of tracks 2..6" : "TRACK_DUMP (1D t): track, engine, preset, P_COUNT x v14", r, n))
            break;
        if (t == NTRK - 1u)
            ok("TRACK_DUMP: E3 at 3 + 2 (P_E0 + 2), mute at 3 + 2 P_MUTE (as Fm1Link reads them)",
               r && v14_at(r + 3 + 2 * (P_E0 + 2)) == (int32_t)(t + 3u) && v14_at(r + 3 + 2 * APP_P_MUTE) == 0);
    }
    /* GET of global 0 (BPM): F0 7D 46 4C 02 01 00 F7 */
    song.g[G_BPM] = 120;
    APP(C_GET, 1, 0);
    r = frame(C_GET, &n);
    g_new(); g_b(1, "scope global"); g_b(0, "id BPM"); g_v(120, "BPM");
    g_check("GET 1 0 (BPM): 01 00 v14", r, n);
    /* SET of BPM (TOOLS > TAP): F0 7D 46 4C 03 01 00 lo hi F7 */
    APP(C_SET, 1, 0, (97 + 8192) & 127, (97 + 8192) >> 7);
    r = frame(C_SET, &n);
    g_new(); g_b(1, "scope"); g_b(0, "id"); g_v(97, "BPM after clamping");
    g_check("SET 1 0 v14 (the app's TAP): 01 00 v14, the tempo set", r, n);
    ok("SET of BPM: the device's tempo is 97", song.g[G_BPM] == 97);
    /* SONG query: F0 7D 46 4C 21 00 F7 */
    chain_config.count = 2; chain_config.row[0].slot = 1; chain_config.row[0].bars = 4;
    chain_config.row[1].slot = 2; chain_config.row[1].bars = 8;
    APP(C_SONG, 0);
    r = frame(C_SONG, &n);
    g_new(); g_b(0, "op"); g_b(0, "rc"); g_b(2, "count"); g_b(chain.running, "running"); g_b(chain.row, "row");
    g_b(chain.remaining, "to go"); g_b(1, "row 0 slot"); g_b(4, "bars"); g_b(2, "row 1 slot"); g_b(8, "bars");
    g_check("SONG 0: op, rc, count, running, row, to go, count x (slot, bars)", r, n);
    /* MENU_DESC of MIDI OUT: F0 7D 46 4C 48 14 F7 */
    midi_out_mode = 1;
    APP(C_MENU_DESC, APP_MO_INDEX);
    r = frame(C_MENU_DESC, &n);
    g_new(); G.tail = 1; g_b(APP_MO_INDEX, "index"); g_b(APP_MO_ID, "id"); g_b(0, "kind ENUM"); g_v(1, "value CLOCK");
    g_check("MENU_DESC 20: 20, id 20, kind, value v14 (MIDI OUT; then its range, names, tab)", r, n);
    /* MENU_SET of MIDI OUT ALL: F0 7D 46 4C 49 14 02 40 F7 */
    APP(C_MENU_SET, APP_MO_ID, (2 + 8192) & 127, (2 + 8192) >> 7);
    r = frame(C_MENU_SET, &n);
    g_new(); g_any("rc (0 saved, 3 RAM only, 4 queued; never 1)"); g_b(APP_MO_ID, "id"); g_v(2, "value ALL");
    ok("MENU_SET 20 ALL: rc, 20, v14 2, applied", g_check("MENU_SET 20 2: rc, id, v14", r, n) && r[0] != 1 &&
       midi_out_mode == 2);
    /* PING */
    APP0(C_PING);
    r = frame(C_PING, &n);
    g_new(); g_b(0, "0");
    g_check("PING: 0", r, n);
    /* a VIS FEED subscription (F0 7D 4A 56 01 00 seq 0 0 rate 0 F7): not this firmware's; never answered */
    {
        static const uint8_t vis[] = {0xF0, 0x7D, 0x4A, 0x56, 1, 0, 0, 0, 0, 30, 0, 0xF7};
        host_wire_n = 0; ota_frame_done(); app_send(vis, sizeof vis); ed_service(); host_drain();
        ok("VIS FEED SUB (7D 4A 56): no reply, no editor frame (not in this firmware)", !host_wire_n);
        APP0(C_PING);
        ok("the editor answers after it", frame(C_PING, &n) != 0);
        ota_frame_done();
    }
    /* unknown command: no reply */
    APP0(43);
    ok("an unknown command (43) gets no reply", !host_wire_n);
}

/* ------------------------------------------------------------------ the backup protocol (Fm1Archive) */
static void backup(void)
{
    const uint8_t *r;
    uint32_t n, i;
    reset();
    /* LIST: F0 7D 46 4C 41 F7 */
    APP0(C_LIST);
    r = frame(C_LIST, &n);
    g_new(); g_b(1, "protocol"); g_b(0, "rc"); g_b(ED_BK_N, "count");
    for (i = 0; i < ED_BK_N; i++) { g_b(ED_BK_IDS[i], "id"); g_u32any("size"); g_u32any("crc"); }
    g_check("BACKUP_LIST (41): 1, rc, count, count x (id, size u32, crc u32)", r, n);
    ok("BACKUP_LIST: 35 objects (ids 0..10, 40..63: 6 songs; Fm1Archive's Bk.IDS expects 43, 8 songs: see the report)",
       ED_BK_N == 35u && r && r[2] == 35);
    /* GET: id 1 (the settings), offset 0, 16 bytes: F0 7D 46 4C 42 01 00 00 00 00 00 10 00 F7 */
    APP(C_BGET, 1, 0, 0, 0, 0, 0, 16, 0);
    r = frame(C_BGET, &n);
    g_new(); g_b(1, "id"); g_b(0, "rc"); g_u32(0, "offset"); g_b(16, "count lo"); g_b(0, "count hi");
    for (i = 0; i < 16u + 3u; i++) g_any("pack7 (3 masks, 16 bytes)");
    g_check("BACKUP_GET 1 0 16: id, rc, offset u32, count lo hi, pack7 data", r, n);
    /* PUT begin of a wrong size, then abort (the app's restore path; restores themselves: backup_test.c) */
    APP(C_PUT, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    r = frame(C_PUT, &n);
    g_new(); g_b(0, "op begin"); g_b(1, "id"); g_any("rc");
    ok("BACKUP_PUT begin: op, id, rc (a 1-byte settings object refused: rc 1)", g_check("BACKUP_PUT 0: op, id, rc", r, n) &&
       r[2] == 1);
    APP(C_PUT, 3, 1);
    r = frame(C_PUT, &n);
    g_new(); g_b(3, "op abort"); g_b(1, "id"); g_any("rc");
    g_check("BACKUP_PUT 3 (abort): op, id, rc", r, n);
    /* SONG op 2 (the setlist's play): F0 7D 46 4C 21 02 F7 */
    APP(C_SONG, 2);
    r = frame(C_SONG, &n);
    g_new(); G.tail = 1; g_b(2, "op"); g_any("rc"); g_any("count"); g_any("running"); g_any("row"); g_any("to go");
    g_check("SONG 2: op 2, rc, then the query's bytes", r, n);
    transport_req = 0;
}

/* ------------------------------------------------------------------ the pushes */
static void pushes(void)
{
    const uint8_t *r;
    uint32_t n;
    reset();
    APP(C_WATCH, 3);
    pump();
    /* CHANGED: a knob of the selected track, the tempo */
    TSEL->p[P_E0 + 1] = 77;
    song.g[G_BPM] = 133;
    pump();
    r = frame_k(C_CHANGED, 0, &n);
    g_new(); g_b(0, "scope track"); g_b(P_E0 + 1, "id"); g_v(77, "value");
    g_check("CHANGED (23) push: scope 0, id, v14 (E2 of the selected track)", r, n);
    r = frame_k(C_CHANGED, 1, &n);
    g_new(); g_b(1, "scope global"); g_b(0, "BPM"); g_v(133, "value");
    g_check("CHANGED (23) push: scope 1, id 0, v14 (the tempo)", r, n);
    /* TRACK_CHANGED: another track's mute */
    trk[2].p[P_MUTE] = 1;
    pump();
    r = frame(C_TRACK_CHANGED, &n);
    g_new(); g_b(2, "track"); g_b(APP_P_MUTE, "P_MUTE"); g_v(1, "on");
    g_check("TRACK_CHANGED (32) push: track, id, v14 (WATCH bit 1)", r, n);
    /* RELOAD: another track selected on the device */
    track_select(3);
    pump();
    r = frame(C_RELOAD, &n);
    g_new(); g_b(trk_eng(&trk[3]), "engine"); g_b(trk[3].preset, "preset"); g_b(3, "selected track");
    g_check("RELOAD (24) push: engine, preset, the selected track", r, n);
    /* an old client (WATCH 3) sees none of the 1.0 pushes */
    pump();
    song.playing = 1; beat_n = 0; bar_n = 0;
    perf_held = PF_BIT(PF_OCTU);
    mut_n[1]++;
    pump();
    ok("WATCH 3: no TRANSPORT / PUNCH / MUTATE frames (the 1.0 pushes are opt-in)",
       !frames(C_TRANSPORT) && !frames(C_PUNCH) && !frames(C_MUTATE));
    song.playing = 0; perf_held = 0;
}

static void pushes_10(void)
{
    const uint8_t *r;
    uint32_t n;
    reset();
    song.playing = 0; beat_n = 2; bar_n = 0; perf_held = perf_latched = 0; scene_pfx = 0; pfx_lnow = 0; perf_kill = 0;
    pfx_tgt = 0;
    APP(C_WATCH, 3 | 4 | 8 | 16);
    r = frame(C_WATCH, &n);
    g_new(); g_b(31, "on | TRACK_CHANGED | TRANSPORT | PUNCH | MUTATE");
    g_check("WATCH 31 -> 31 (1.0: bits 2..4 asked for; older firmware answers 3)", r, n);
    pump();
    r = frame(C_TRANSPORT, &n);
    g_new(); g_b(0, "playing"); g_b(2, "beat"); g_b(0, "bar lo"); g_b(0, "bar hi");
    g_check("TRANSPORT (40): the state as it is, once WATCH asks for it: playing, beat, bar lo, bar hi", r, n);
    r = frame(C_PUNCH, &n);
    g_new(); g_b(0, "mask"); g_b(0, "mask"); g_b(0, "mask"); g_b(0, "mask"); g_b(0, "tracks: ALL");
    g_check("PUNCH (41): the state as it is: 4 x 7 bits of the effects running, LSB first, then their tracks", r, n);
    ok("no MUTATE until one happens", !frames(C_MUTATE));
    beat_n = 3;
    pump();
    ok("stopped: the beat moving pushes nothing", !frames(C_TRANSPORT));
    song.playing = 1; beat_n = 0; bar_n = 0;
    pump();
    r = frame(C_TRANSPORT, &n);
    g_new(); g_b(1, "playing"); g_b(0, "beat"); g_b(0, "bar lo"); g_b(0, "bar hi");
    g_check("TRANSPORT on PLAY: 1, beat 0, bar 0", r, n);
    beat_n = 1; bar_n = 200;
    pump();
    r = frame(C_TRANSPORT, &n);
    g_new(); g_b(1, "playing"); g_b(1, "beat"); g_b(200 & 127, "bar lo"); g_b(200 >> 7, "bar hi");
    g_check("TRANSPORT each beat while playing: 1, beat 1, bar 200 (lo, hi)", r, n);
    pump();
    ok("the same beat: no push", !frames(C_TRANSPORT));
    perf_held = PF_BIT(PF_OCTU) | PF_BIT(PF_LP5);
    pfx_tgt = 2;
    pump();
    r = frame(C_PUNCH, &n);
    {
        uint32_t m = PF_BIT(PF_OCTU) | PF_BIT(PF_LP5);
        g_new(); g_b(m & 127, "mask"); g_b(m >> 7 & 127, "mask"); g_b(m >> 14 & 127, "mask"); g_b(m >> 21 & 127, "mask");
        g_b(2, "tracks: DRM");
    }
    g_check("PUNCH: OCT+ (bit 6) and LOOP 5 (bit 23) held, on the DRUM tracks", r, n);
    ok("PUNCH bits frozen: R8 0, LPF 3, OCT- 5, OCT+ 6, RANDOM 14, mutes 15..20, GLITCH 21, LOOP3/5/7 22..24, 25 bits",
       PF_R8 == 0 && PF_LPF == 3 && PF_OCTD == 5 && PF_OCTU == 6 && PF_RND == 14 && PF_M1 == 15 && PF_GLT == 21 &&
       PF_LP3 == 22 && PF_LP7 == 24 && PF_N == 25);
    pfx_lnow = PF_BIT(PF_S16);
    perf_held = 0;
    pump();
    r = frame(C_PUNCH, &n);
    ok("PUNCH: the lane's effect playing (STUTTER 1/16, bit 11) counts", r && n == 5u &&
       (r[0] | r[1] << 7) == PF_BIT(PF_S16));
    pfx_lnow = 0;
    perf_kill = 1; perf_latched = PF_BIT(PF_ARP);
    pump();
    ok("PUNCH: every effect killed (a menu open): mask 0", (r = frame(C_PUNCH, &n)) && !r[0] && !r[1] && !r[2] && !r[3]);
    perf_kill = 0; perf_latched = 0;
    pump();
    mut_n[4]++;
    mut_n[4]++;
    mut_n[1]++;
    pump();
    r = frame_k(C_MUTATE, 0, &n);
    g_new(); g_b(1, "track"); g_b(mut_n[1] & 127, "count");
    g_check("MUTATE (42): track, its count (7 bits, wrapping): track 2", r, n);
    r = frame_k(C_MUTATE, 1, &n);
    g_new(); g_b(4, "track"); g_b(mut_n[4] & 127, "count");
    g_check("MUTATE: one frame per track however many came between two passes (track 5, count + 2)", r, n);
    APP(C_WATCH, 3);
    r = frame(C_WATCH, &n);
    song.playing = 1; beat_n = 2;
    mut_n[0]++;
    pump();
    ok("WATCH 3 again: the 1.0 pushes stop, the reply is 3", r && r[0] == 3 && !frames(C_TRANSPORT) && !frames(C_MUTATE));
    APP(C_WATCH, 1 | 4);
    r = frame(C_WATCH, &n);
    ok("WATCH 5 (TRANSPORT alone, no TRACK_CHANGED): reply 5", r && r[0] == 5);
    pump();
    ok("WATCH 5: TRANSPORT only", frames(C_TRANSPORT) == 1u && !frames(C_PUNCH) && !frames(C_MUTATE));
    APP(C_WATCH, 0);
    r = frame(C_WATCH, &n);
    ok("WATCH 0 -> 0", r && r[0] == 0);
    song.playing = 0; beat_n = 0;
}

/* ------------------------------------------------------------------ NRPN (midi_control.c, seq.c) */
static void cc_in(uint32_t ch, uint32_t cc, uint32_t v)
{
    midi_in_event(0x0Bu | (0xB0u | ch) << 8 | cc << 16 | v << 24);
    while (mi_r != mi_w) {                                 /* (events_block's channel messages) */
        uint32_t pkt = midi_in_q[mi_r++ % MQ], st = (pkt >> 8) & 0xFFu;
        midi_event(st & 0xF0u, st & 15u, (pkt >> 16) & 127u, (pkt >> 24) & 127u);
    }
}
static void nrpn_in(uint32_t ch, uint32_t msb, uint32_t lsb, uint32_t x, int bits14)
{
    cc_in(ch, 99, msb);
    cc_in(ch, 98, lsb);
    cc_in(ch, 6, x >> 7);
    if (bits14)
        cc_in(ch, 38, x & 127u);
}
static uint32_t out_n(void) { return mo_w - mo_r; }
static uint32_t out_pkt(uint32_t k) { return midi_out_q[(mo_r + k) % MQ]; }
static uint32_t cc_pkt(uint32_t ch, uint32_t cc, uint32_t v) { return 0x0Bu | (0xB0u | ch) << 8 | cc << 16 | v << 24; }

static void nrpn(void)
{
    uint32_t e, id, k, rt = 1, cnt = 0, gl = 1;
    track_t *t;
    reset();
    song.g[G_ROUTE] = 0;                                    /* ROUT CH1-6 */
    nrpn_in(0, 0, P_REV, 8192, 1);
    ok("NRPN 0 / P_REV on channel 1, 14 bits 8192: track 1's REV 64", trk[0].p[P_REV] == 64);
    nrpn_in(1, 0, P_REV, 127u << 7, 0);
    ok("NRPN 7 bits (CC6 alone) 127 on channel 2: track 2's REV at its top (127)", trk[1].p[P_REV] == 127);
    cc_in(1, 6, 0);
    ok("a held NRPN takes another value (CC6 0): 0", trk[1].p[P_REV] == 0);
    nrpn_in(2, 0, P_PAN, 64u << 7, 0);
    ok("NRPN 7 bits 64 on a bipolar parameter (PAN -64..63): 0, the middle", trk[2].p[P_PAN] == 0);
    nrpn_in(2, 0, P_PAN, 0, 1);
    ok("NRPN 14 bits 0: PAN -64", trk[2].p[P_PAN] == -64);
    trk[3].p[P_MUTE] = 0; trk[3].p[P_SLEN] = 16;
    nrpn_in(3, 0, P_MUTE, 16383, 1);
    nrpn_in(3, 0, P_SLEN, 0, 1);
    ok("NRPN of a parameter automation never records (MUTE 40, LEN 28): ignored", !trk[3].p[P_MUTE] && trk[3].p[P_SLEN] == 16);
    nrpn_in(3, 0, P_COUNT, 16383, 1);
    nrpn_in(3, 0, 127, 16383, 1);
    ok("NRPN 0 past P_COUNT (123, 127): ignored", 1);
    nrpn_in(6, 0, P_REV, 16383, 1);
    ok("ROUT CH1-6: channel 7 ignored", trk[0].p[P_REV] == 64 && trk[1].p[P_REV] == 0);
    song.sel = 4;
    nrpn_in(15, 0, P_REV, 16383, 1);
    ok("ROUT CH1-6: channel 16's NRPN 0 sets the selected track (5)", trk[4].p[P_REV] == 127);
    song.sel = 0;
    /* globals */
    nrpn_in(0, 1, G_BPM, 8192, 1);
    ok("NRPN 1 / 0 (BPM 40..240) 8192: 140", song.g[G_BPM] == 140);
    nrpn_in(15, 1, G_BPM, 16383, 1);
    ok("NRPN 1 on channel 16 (ROUT CH1-6 too): BPM 240", song.g[G_BPM] == 240);
    nrpn_in(15, 1, G_DJF, 0, 1);
    ok("NRPN 1 / 17 (the DJ FILTER) 0: -64", song.g[G_DJF] == -64);
    k = (uint32_t)song.g[G_ROUTE];
    nrpn_in(0, 1, G_ROUTE, 16383, 1);
    nrpn_in(0, 1, G_CLOCK, 16383, 1);
    nrpn_in(0, 1, G_ENGSEL, 16383, 1);
    ok("NRPN 1 of ROUTE, CLOCK, ENGSEL: ignored (NRPN_G_NO)", (uint32_t)song.g[G_ROUTE] == k && !song.g[G_CLOCK] &&
       trk_eng(TSEL) == 0);
    /* RPN 0 (the bend range) still works after an NRPN, and an RPN selection ends the NRPN */
    nrpn_in(0, 0, P_REV, 8192, 1);
    cc_in(0, 101, 0); cc_in(0, 100, 0); cc_in(0, 6, 7);
    ok("RPN 0 after an NRPN: the bend range 7, REV untouched", midi_ch[0].semis == 7 && trk[0].p[P_REV] == 64);
    cc_in(0, 99, 0); cc_in(0, 98, P_REV); cc_in(0, 6, 12);
    ok("NRPN after RPN: CC6 is the NRPN's, the bend range stays 7", midi_ch[0].semis == 7 && trk[0].p[P_REV] == 12);
    cc_in(0, 99, 127); cc_in(0, 98, 127); cc_in(0, 6, 100);
    ok("NRPN 127 / 127 (null): CC6 does nothing", trk[0].p[P_REV] == 12);
    /* the fixed CCs: unchanged (CC16 = E1, 7 bits over its range) */
    t = &trk[0];
    cc_in(0, 16, 100);
    {
        const param_desc_t *d = param_desc_of(eng_idx(t->eng_req), P_E0);
        ok("CC16 still E1, (v x range + 63) / 127 over its range", t->p[P_E0] == d->min + (100 * (d->max - d->min) + 63) / 127);
    }
    /* every automatable parameter of every engine: out (mo_knobs' scaling) and back in give the value */
    for (e = 0; e < NENGINES; e++) {
        if (ENGINES[e]->name[0] == '-') continue;
        t = &trk[1];
        set_engine_of(t, e); apply_preset_to(t, 0); t->engine = t->eng_req;
        for (id = 0; id < P_COUNT; id++) {
            const param_desc_t *d;
            int32_t v;
            if (!motion_param(id)) continue;
            d = track_desc(t, id);
            if (d->max <= d->min) continue;
            for (v = d->min; v <= d->max; v += (d->max - d->min) / 9 + 1) {
                int16_t keep0 = t->p[P_E0];
                nrpn_in(1, 0, id, nrpn_of(d, v), 1);
                cnt++;
                if (t->p[id] != v && track_desc(t, id) == d) {
                    if (rt) printf("    %s id %u: %d -> %d\n", ENGINES[e]->name, id, v, t->p[id]);
                    rt = 0;
                }
                if (id != P_E0) t->p[P_E0] = keep0;     /* (a mode keeps the others' descriptors) */
            }
        }
    }
    ok("NRPN round trip (nrpn_of out, 14 bits in) for every automatable parameter of every engine", rt && cnt > 1000u);
    for (id = 0; id < G_COUNT; id++) {
        int32_t v;
        if (!nrpn_g_ok(id) || id == G_DJF) continue;
        for (v = GP[id].min; v <= GP[id].max; v += (GP[id].max - GP[id].min) / 7 + 1) {
            nrpn_in(15, 1, id, nrpn_of(&GP[id], v), 1);
            gl &= song.g[id] == v;
        }
    }
    ok("NRPN round trip of every global NRPN 1 reaches", gl);
    /* MIDI OUT ALL: a knob move goes out as an NRPN */
    reset();
    mo_r = mo_w;
    mo_lvl = MO_ALL;
    trk[2].p[P_REV] = 100;
    mo_knob(2, P_REV);
    mo_knobs();
    k = nrpn_of(&TP[P_REV], 100);
    ok("MIDI OUT ALL, track 3's REV turned: CC99 0, CC98 36, CC6, CC38 on channel 3",
       out_n() == 4u && out_pkt(0) == cc_pkt(2, 99, 0) && out_pkt(1) == cc_pkt(2, 98, P_REV) &&
       out_pkt(2) == cc_pkt(2, 6, k >> 7) && out_pkt(3) == cc_pkt(2, 38, k & 127u));
    {                                                       /* .. and the FM-1 takes its own NRPN back */
        uint32_t i;
        trk[2].p[P_REV] = 0;
        for (i = 0; i < 4u; i++) {
            uint32_t p = out_pkt(i);
            cc_in(2, (p >> 16) & 127u, p >> 24);
        }
        ok("the NRPN sent, received: the same value", trk[2].p[P_REV] == 100);
    }
    mo_r = mo_w;
    song.g[G_BPM] = 150;
    mo_knob(7, G_BPM);
    mo_knob(7, G_ROUTE);
    mo_knob(0, P_MUTE);
    mo_knobs();
    k = nrpn_of(&GP[G_BPM], 150);
    ok("a global turned: CC99 1, CC98 0 on channel 16; ROUTE and MUTE send nothing",
       out_n() == 4u && out_pkt(0) == cc_pkt(15, 99, 1) && out_pkt(1) == cc_pkt(15, 98, G_BPM) &&
       out_pkt(2) == cc_pkt(15, 6, k >> 7) && out_pkt(3) == cc_pkt(15, 38, k & 127u));
    mo_r = mo_w;
    for (k = 0; k < 6u; k++) mo_knob(0, P_REV);
    mo_knobs();
    ok("the same knob queued again before it went: one NRPN (or two), the latest value", out_n() == 4u || out_n() == 8u);
    mo_r = mo_w;
    mo_lvl = MO_KEYS;
    mo_knob(0, P_REV);
    mo_knobs();
    ok("MIDI OUT KEYS: knob moves send nothing", !out_n());
    mo_lvl = 0;
}

/* ------------------------------------------------------------------ docs/NRPN.md, web/NRPN.md (generated) */
static char md[200000];
static uint32_t mdn;
static void mdp(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void mdp(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    mdn += (uint32_t)vsnprintf(md + mdn, sizeof md - mdn, fmt, ap);
    va_end(ap);
}
static void md_desc(const param_desc_t *d)
{
    mdp("%s | %d..%d | %d", d->label, d->min, d->max, d->def);
    if (d->fmt == F_ENUM && d->names) {
        int32_t i;
        mdp(" | ");
        for (i = 0; i <= d->max - d->min && i < 40; i++)
            mdp("%s%s", i ? " " : "", d->names[d->min + i]);
        if (i <= d->max - d->min)
            mdp(" ...");
    } else
        mdp(" |");
}
/* each engine's descriptors (by value: a mode's may be built on the fly) as a track with its preset 1 has them */
static param_desc_t md_d[NENGINES][P_COUNT];
static int md_same(const param_desc_t *a, const param_desc_t *b)
{
    int32_t i;
    if (strcmp(a->label, b->label) || a->fmt != b->fmt || a->min != b->min || a->max != b->max || a->def != b->def ||
        !a->names != !b->names)
        return 0;
    for (i = 0; a->names && a->fmt == F_ENUM && i <= a->max - a->min; i++)
        if (strcmp(a->names[a->min + i], b->names[b->min + i])) return 0;
    return 1;
}
static void nrpn_doc(void)
{
    uint32_t id, e, k;
    track_t keep = trk[1];
    mdn = 0;
    for (e = 0; e < NENGINES; e++) {
        track_defaults(&trk[1]); set_engine_of(&trk[1], e); apply_preset_to(&trk[1], 0); trk[1].engine = trk[1].eng_req;
        for (id = 0; id < P_COUNT; id++)
            md_d[e][id] = *track_desc(&trk[1], id);
    }
    trk[1] = keep;
    mdp("# JIANT FM: the NRPN map\n\n"
        "<!-- Generated from firmware/src/params.c by tests/app_contract_test.c (`app-contract --write-nrpn`); the test fails\n"
        "     when this file and the firmware differ. Do not edit by hand. -->\n\n"
        "Editor protocol 1.0 (web/EDITOR_PROTOCOL.md, \"Stability contract\"). Every parameter that automation records\n"
        "(the knobs of the sound, the mix and the effects) is reachable by **NRPN**; the fixed CCs (CC5 7 10 16..23 71..75\n"
        "91 93 94 102..105) are unchanged.\n\n"
        "- **Track parameters:** CC99 (NRPN MSB) **0**, CC98 (NRPN LSB) the parameter's id below, on the channel the\n"
        "  track plays (ROUTE as notes: CH1-6, channel n is track n; SEL, every channel the selected track). Channel 16\n"
        "  is heard for NRPN also with CH1-6: the selected track.\n"
        "- **Globals:** CC99 **1**, CC98 the global's id, on channel 16 or any channel the FM-1 hears.\n"
        "- **The value:** CC6 (data entry MSB) then CC38 (LSB): 14 bits, 0..16383, over the parameter's range as DESC\n"
        "  gives it, `v = min + (x * (max - min) + 8191) / 16383`. CC6 alone is a 7-bit value (as a CC: 0..127 over the\n"
        "  range, x = msb * 129). A selected NRPN stays selected: more CC6 / CC38 set it again. CC101 / CC100 (an RPN)\n"
        "  or CC99 127 end it; RPN 0 (the bend range) works as before.\n"
        "- **AUTOMATION** records a track's NRPN as a knob move (the selected track, armed and playing). Values outside\n"
        "  the table (an id automation does not record, a global of the transport, routing or loads) are ignored.\n"
        "- **MIDI OUT ALL** (MENU > CONTROL > MIDI OUT): a knob turned on the panel goes out the same way (CC99, CC98,\n"
        "  CC6, CC38 with the 14-bit value `x = ((v - min) * 16383 + (max - min) / 2) / (max - min)`): a track's on its\n"
        "  channel, a global's on channel 16. Sent back to the FM-1 it sets the same value. Only the panel's knobs send\n"
        "  (not MIDI in, not the editor, not automation playing).\n\n");
    mdp("## Track parameters (CC99 0)\n\n"
        "The range is the one DESC (scope 0) and TRACK_PARAM give for the track's engine; where an engine reads a slot\n"
        "its own way the engines are named. P_E0..P_E7 (CC98 %u..%u) are the engine's EDIT values: see the next table.\n\n"
        "| CC98 | parameter | range | default | values |\n| --- | --- | --- | --- | --- |\n", P_E0, P_E0 + 7u);
    for (id = 0; id < P_E0; id++) {
        const param_desc_t *seen[NENGINES];
        uint32_t ns = 0, j;
        if (!motion_param(id)) continue;
        for (e = 0; e < NENGINES; e++) {                    /* the distinct descriptors, each with its engines */
            const param_desc_t *d;
            if (ENGINES[e]->name[0] == '-') continue;
            d = &md_d[e][id];
            for (j = 0; j < ns && !md_same(seen[j], d); j++) {}
            if (j == ns) seen[ns++] = d;
        }
        for (j = 0; j < ns; j++) {
            uint32_t m = 0;
            mdp("| %u | ", id);
            md_desc(seen[j]);
            if (ns > 1u) {
                mdp(" (");
                for (e = 0; e < NENGINES; e++)
                    if (ENGINES[e]->name[0] != '-' && md_same(&md_d[e][id], seen[j]))
                        mdp("%s%s", m++ ? " " : "", ENGINES[e]->name);
                mdp(")");
            }
            mdp(" |\n");
        }
    }
    mdp("\n## Engine parameters (CC99 0, CC98 %u..%u: E1..E8)\n\n"
        "Each engine's EDIT values as its preset 1 shows them (a mode may rename one: DESC says what the track has now).\n\n"
        "| engine | CC98 | parameter | range | default | values |\n| --- | --- | --- | --- | --- | --- |\n", P_E0, P_E0 + 7u);
    for (e = 0; e < NENGINES; e++) {
        if (ENGINES[e]->name[0] == '-') continue;
        for (k = 0; k < 8u; k++) {
            mdp("| %s %u | %u | ", ENGINES[e]->name, e, P_E0 + k);
            md_desc(&md_d[e][P_E0 + k]);
            mdp(" |\n");
        }
    }
    mdp("\n## Globals (CC99 1)\n\n| CC98 | parameter | range | default | values |\n| --- | --- | --- | --- | --- |\n");
    for (id = 0; id < G_COUNT; id++) {
        if (!nrpn_g_ok(id)) continue;
        mdp("| %u | ", id);
        md_desc(&GP[id]);
        mdp(" |\n");
    }
}
static int file_is(const char *path)
{
    static char b[sizeof md];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(b, 1, sizeof b, f);
    fclose(f);
    return n == mdn && !memcmp(b, md, n);
}

int main(int argc, char **argv)
{
    nrpn_doc();
    if (argc > 1 && !strcmp(argv[1], "--write-nrpn")) {
        static const char *const out[] = {"docs/NRPN.md", "web/NRPN.md"};
        for (uint32_t i = 0; i < 2u; i++) {
            FILE *f = fopen(out[i], "wb");
            if (!f || fwrite(md, 1, mdn, f) != mdn) { perror(out[i]); return 1; }
            fclose(f);
            printf("wrote %s (%u bytes)\n", out[i], mdn);
        }
        return 0;
    }
    contract_constants();
    requests();
    backup();
    pushes();
    pushes_10();
    nrpn();
    nrpn_doc();
    ok("docs/NRPN.md is the table params.c gives (else: app-contract --write-nrpn)", file_is("docs/NRPN.md"));
    ok("web/NRPN.md is the same", file_is("web/NRPN.md"));
    if (bad) printf("contract: %d FAILED\n", bad);
    else puts("contract: the app's protocol, pushes, engine ids and NRPN map hold");
    return bad != 0;
}
