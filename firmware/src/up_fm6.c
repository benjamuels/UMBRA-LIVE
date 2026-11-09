/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The user presets' FM6 patches (since 1.0.3): an FM6 user preset carries its track's patch, as a project does.
 * One storage.c object (OBJ_UPFM6, A/B: 0x9F000 / 0xFE000), mirrored in RAM (the pool): per user preset slot the
 * 128-byte packed patch (eng_fm6.c fm6_pack) with its 7-bit bytes packed 8 to the byte (112 bytes), and a tag.
 *
 * The tag ties an entry to its record: a hash of the user preset record (upreset.c up_rec_t) without its name. An
 * entry counts only while its record is an FM6 sound and still has that hash, so a rename keeps the patch, and any
 * other change of the record (UP_PUT from an editor that sends no patch, an erase, a restore of the banks, a power
 * cut between the record's write and this object's) leaves the record without one. Such a record loads as before
 * 1.0.3: SLOT F n -> that factory patch, OWN -> the init voice. The record's write comes first, this object's second
 * (up_store): a cut between them never gives a record another record's patch.
 *
 * The FM6 patch bank of 1.0..1.0.2 (OBJ_FM6BANK, 27 slots B1..B27 that SLOT 8..34 loaded; the editor's FM6 target
 * 1, backup id 8) is retired, and since V10.6s so is its one-time move into the user presets (firmware 1.0.3 .. V10.6r
 * made it): a user preset that never got its patch loads the init voice; an older archive's bank is accepted and
 * ignored (editor_backup.c). Included by upreset.c. */
typedef struct { uint8_t raw[3472]; } fm6_bank_t;   /* the retired bank's size (an older backup's id 8) */

#define UPF_MAGIC 0x36465055u                    /* "UPF6" */
#define UPF_PK 112u                              /* 128 x 7 bits */
typedef struct {
    uint32_t tag;                                /* upf_tag of the record it belongs to */
    uint8_t pk[UPF_PK];
} upf_ent_t;
typedef struct {
    uint32_t magic;
    uint16_t ver, nslot;                         /* 1, UP_SLOTS */
    uint32_t used;                               /* bit k: e[k] holds an FM6 patch or VOICE lyric */
    uint32_t rsv;
    upf_ent_t e[UP_SLOTS];
} upf_t;
_Static_assert(sizeof(upf_t) == 3728u, "user preset FM6 patches layout (at most ST_PAYLOAD_MAX, 3840)");
_Static_assert(sizeof(up_rec_t) == 192u && sizeof(((up_rec_t *)0)->name) == 12u, "upf_tag: the name at bytes 4..15");
static upf_t upf __attribute__((section(".pool")));   /* (0.6.53: in .bss, was .pool; 0.8.4: in the pool again, the
                                                   * reverb's lines shared: main loop only) */

static int upf_valid(const upf_t *u) { return u->magic == UPF_MAGIC && u->ver == 1u && u->nslot == UP_SLOTS; }

static void upf_empty(void)
{
    memset(&upf, 0, sizeof upf);
    upf.magic = UPF_MAGIC;
    upf.ver = 1;
    upf.nslot = UP_SLOTS;
}

static uint32_t upf_tag(const up_rec_t *r)       /* FNV-1a of the record without its name (bytes 4..15) */
{
    const uint8_t *b = (const uint8_t *)r;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < sizeof *r; i++)
        if (i < 4u || i >= 16u)
            h = (h ^ b[i]) * 16777619u;
    return h;
}

static int upf_fm6(uint32_t k) { return up_used(k) && up_rec(k)->engine == ENGI_FM6; }

/* a 128-byte packed record's 7-bit bytes, 8 to 7 bytes (UPF_PK) and back (the voice bank too, fm6_vbank.c) */
static void upf_pk_get(const uint8_t *d, uint8_t *pk)
{
    uint32_t i, acc = 0, n = 0, o = 0;
    for (i = 0; i < FM6_PACKED; i++) {
        while (n < 7u) {
            acc |= (uint32_t)d[o++] << n;
            n += 8u;
        }
        pk[i] = (uint8_t)(acc & 127u);
        acc >>= 7;
        n -= 7u;
    }
}
static void upf_pk_put(const uint8_t *pk, uint8_t *d)
{
    uint32_t i, acc = 0, n = 0, o = 0;
    memset(d, 0, UPF_PK);
    for (i = 0; i < FM6_PACKED; i++) {
        acc |= (uint32_t)(pk[i] & 127u) << n;
        n += 7u;
        while (n >= 8u) {
            d[o++] = (uint8_t)acc;
            acc >>= 8;
            n -= 8u;
        }
    }
}

/* slot k's patch -> pk (128 bytes), 0 = it has one */
static int upf_get(uint32_t k, uint8_t *pk)
{
    if (k >= UP_SLOTS || !upf_valid(&upf) || !((upf.used >> k) & 1u) || !upf_fm6(k) ||
        upf.e[k].tag != upf_tag(up_rec(k)))
        return 1;
    upf_pk_get(upf.e[k].pk, pk);
    return 0;
}

/* slot k's patch = pk (128 bytes; every value into its range), tagged with the record as it is now. RAM only */
static void upf_set(uint32_t k, const uint8_t *pk)
{
    uint8_t v[FP_SIZE + 1u], c[FM6_PACKED];
    if (k >= UP_SLOTS)
        return;
    if (!upf_valid(&upf))
        upf_empty();
    fm6_unpack(pk, v);
    fm6_pack(v, c);
    upf_pk_put(c, upf.e[k].pk);
    upf.e[k].tag = upf_tag(up_rec(k));
    upf.used |= 1u << k;
}

/* the object to flash: 0 ok, 2 flash error, 3 no flash (kept in RAM) */
static int upf_save(void)
{
#if FELUCCA_FLASH
    if (!flash_ok)
        return 3;
    return st_save_to(OBJ_UPFM6, &upf, sizeof upf, -1) ? 2 : 0;
#else
    return 3;
#endif
}

static void upf_boot(void)                       /* up_boot, after the user preset banks */
{
#if FELUCCA_FLASH
    if (flash_ok && st_load(OBJ_UPFM6, &upf, sizeof upf) == (int)sizeof upf && upf_valid(&upf))
        return;
    upf_empty();
#else
    upf_empty();
#endif
}

/* VOICE uses the same per-slot auxiliary payload as FM6 (the engine and record tag distinguish them).
 * No extra sectors, RAM bank or record-size change. Old VOICE slots have no text and load an empty lyric. */
_Static_assert(4u + LYR_LEN <= UPF_PK, "VOICE lyric fits the existing auxiliary payload");
static const char *upf_lyric(uint32_t k)
{
    const uint8_t *text;
    uint32_t i;
    if (k >= UP_SLOTS || !upf_valid(&upf) || !((upf.used >> k) & 1u) || !up_used(k) ||
        up_rec(k)->engine != ENGI_VOICE || upf.e[k].tag != upf_tag(up_rec(k)) ||
        memcmp(upf.e[k].pk, "LYR1", 4u)) return "";
    text = upf.e[k].pk + 4u;
    for (i = 0; i < LYR_LEN; i++) {
        if (!text[i]) return (const char *)text;
        if (text[i] < 32u || text[i] > 126u) return "";
    }
    return "";
}
static int upf_lyric_store(uint32_t k, uint32_t tr)
{
    upf_ent_t old;
    uint32_t used;
    int rc;
    if (k >= UP_SLOTS || tr >= NTRK) return 2;
    if (!upf_valid(&upf)) upf_empty();
    old = upf.e[k]; used = upf.used;
    memset(upf.e[k].pk, 0, UPF_PK);
    memcpy(upf.e[k].pk, "LYR1", 4u);
    memcpy(upf.e[k].pk + 4u, lyr_get(tr), LYR_LEN);
    upf.e[k].tag = upf_tag(up_rec(k));
    upf.used |= 1u << k;
    rc = upf_save();
    if (rc == 2) { upf.e[k] = old; upf.used = used; }
    return rc;
}

/* up_load: track t (its values just loaded from slot k) gets the record's patch; without one, as before 1.0.3:
 * SLOT F n that factory patch, OWN the init voice. SLOT then shows F n or OWN (fm6_adopt) */
static void upf_track_load(track_t *t, uint32_t k)
{
    uint8_t pk[FM6_PACKED], v[FP_SIZE + 1u];
    uint32_t tr = (uint32_t)(t - trk);
    if (tr >= NTRK) return;
    if (t->eng_req == ENGI_VOICE) { lyr_set(tr, upf_lyric(k)); return; }
    if (t->eng_req != ENGI_FM6) return;
    if (upf_get(k, pk)) {
        int32_t s = t->p[P_E7];
        memcpy(pk, s >= 0 && s < (int32_t)FM6_NFACTORY ? FM6_FACTORY[s] : FM6_INIT, FM6_PACKED);
    }
    fm6_unpack(pk, v);
    fm6_set_patch(tr, v);
    fm6_adopt(tr);
}

/* up_store: slot k (its record just written from track tr) gets that track's patch; upf_save's result */
static int upf_store(uint32_t k, uint32_t tr)
{
    uint8_t pk[FM6_PACKED];
    fm6_pack(fm6_patch[tr % NTRK], pk);
    upf_set(k, pk);
    return upf_save();
}

