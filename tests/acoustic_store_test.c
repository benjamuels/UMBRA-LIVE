/* SPDX-License-Identifier: GPL-3.0-only */
/* DRUM-X's ACOUST (ADD15, KIT 4 KNOB 1) was retired in V10.6o. A project saved with it set carries its 'D' 'A' block
 * (between the EQ's and SCENE's in the FUNC tail): it still loads, the value skipped and every block after it read as
 * stored; a project saved now never writes the block. KIT 4 keeps the KEY MAP (KNOB 2) and stays a DRUM page.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/acoustic_store_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
/* where q's SCENE block sits in its packed store (0: none) */
static uint32_t sc_at(const project_store_t *st, const project_t *q)
{
    uint32_t i;
    for (i = 68u; i + 2u + sizeof q->scene <= PROJC_PFX_OFF; i++)
        if (st->raw[i] == 'S' && st->raw[i + 1u] == 'C' && !memcmp(st->raw + i + 2u, q->scene, sizeof q->scene))
            return i;
    return 0;
}
int main(void)
{
    static project_t q, r;
    static project_store_t store, old;
    uint32_t at, sum, bad = 0, page;
    ui_power_on();
    scn_on = 1; scn_role[0] = 2; scn_role[3] = 1;        /* (SCENE on: its block follows where 'D' 'A' was written) */
    project_capture(&q);
    bad += check("a project packs", proj_pack(&store, &q));
    at = sc_at(&store, &q);
    bad += check("its SCENE block found", at != 0u);
    bad += check("saving writes no ACOUST block before SCENE", at < 3u || store.raw[at - 3u] != 'D' || store.raw[at - 2u] != 'A');
    /* the same project as one saved with ACOUST 77: 'D' 'A' 77 before SCENE, the rest of the tail 3 bytes on */
    old = store;
    memmove(old.raw + at + 3u, old.raw + at, PROJC_PFX_OFF - at - 3u);
    old.raw[at] = 'D'; old.raw[at + 1u] = 'A'; old.raw[at + 2u] = 77;
    sum = proj_hash(old.raw, PROJ_STORE_SIZE - 4u); memcpy(old.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
    bad += check("an old project with ACOUST loads", proj_import(&r, &old, sizeof old));
    bad += check(".. SCENE after the skipped block read as stored", !memcmp(r.scene, q.scene, sizeof q.scene));
    bad += check(".. its kit and its tracks as stored", !memcmp(r.dx, q.dx, sizeof q.dx) && r.t[0].engine == q.t[0].engine);
    bad += check(".. and saved again, no ACOUST block", proj_pack(&store, &r) && (at = sc_at(&store, &r)) != 0u &&
                 (store.raw[at - 3u] != 'D' || store.raw[at - 2u] != 'A' || store.raw[at - 1u] != 77));
    set_engine_of(TSEL, ENGI_DRUM);
    for (page = 0; page < NPAGES; page++) if (str_eq(PAGES[page].title, "KIT 4")) break;
    bad += check("KIT 4 is reachable only on DRUM", page < NPAGES && page_visible(page));
    ui.home = 0; ui.page = page;
    TSEL->p[P_DKEYMODE] = 0;
    dx_kit3_knob(1, 1);
    bad += check("KIT 4 knob 2 edits the KEY MAP", TSEL->p[P_DKEYMODE] == 1);
    dx_kit3_knob(0, 64);
    bad += check("KIT 4 knob 1 does nothing", TSEL->p[P_DKEYMODE] == 1);
    TSEL->p[P_DKEYMODE] = 0;
    set_engine_of(TSEL, ENGI_ANALOG);
    bad += check("KIT 4 hidden on other engines", !page_visible(page));
    bad += check("ADD kept at engine 13; no SAMPLE engine", ENGINES[13] == &ENG_ADD && eng_gone(ENGI_SAMPLE));
    printf("ACOUST retired: %s\n", bad ? "FAILED" : "passed");
    return !!bad;
}
