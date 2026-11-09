#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(V10.6i) A factory preset table's entries as web/editor.html writes them: P(name, e, env, mono, pat, xp, mod), mod the
preset's MODS (core.h) expanded to [parameter, value] numbers. tools/preset_mods_js.py ENGINE_FILE ARRAY prints them, one
a line; the editor's tables are kept equal to the firmware's (web/test_web.mjs compares the names)"""
import re, sys
from pathlib import Path
SRC = Path(__file__).resolve().parent.parent / "firmware" / "src"


def param_ids():
    s = (SRC / "core.h").read_text()
    i = s.index("enum {                          /* per-track parameters */")
    body = re.sub(r"/\*.*?\*/", "", s[i:s.index("P_COUNT", i)], flags=re.S).split("{", 1)[1]
    ids, n = {}, 0
    for tok in (t.strip() for t in body.split(",")):
        if not tok:
            continue
        if "=" in tok:
            a, b = (x.strip() for x in tok.split("="))
            m = re.match(r"(P_\w+)\s*\+\s*(\d+)", b)
            n = ids[m.group(1)] + int(m.group(2))
            ids[a] = n
        else:
            ids[tok] = n
        n += 1
    for a, b in re.findall(r"#define (P_\w+) (P_\w+)\b", s):
        if b in ids:
            ids.setdefault(a, ids[b])
    return ids


CONST = {"PS_LFO": 1, "PS_ENV": 2, "PS_VEL": 3, "PS_KEY": 4, "PS_RAND": 5, "PS_MODW": 6, "PS_AT": 7, "PS_NOISE": 14,
         "PS_ENV2": 16, "PD_PITCH": 1, "PD_CUT": 2, "PD_SHP": 3, "PD_AMP": 4, "PD_PAN": 5, "PD_DIST": 6, "PD_CHO": 7,
         "PD_DLY": 8, "PD_REV": 9, "PD_LRATE": 10, "PD_VIB": 11, "PD_E1": 12, "PD_FCUT": 30, "PD_LEVEL": 31,
         "LW_SIN": 0, "LW_TRI": 1, "LW_SAW": 2, "LW_SQR": 3, "LW_SH": 4, "LW_DRIFT": 5}


def expand(txt, ids):
    txt = re.sub(r"MMOD\((\d),\s*([^,]+),\s*([^,]+?(?:\([^)]*\))?),\s*([^)]+)\)",
                 lambda m: f"P_M{m[1]}SRC, {m[2]}, P_M{m[1]}DST, {m[3]}, P_M{m[1]}AMT, {m[4]}", txt)
    txt = re.sub(r"MLFO\(([^,]+),([^,]+),([^,]+),([^)]+)\)", r"P_LWAVE, \1, P_LRATE, \2, P_LD_PIT, \3, P_LD_FLT, \4", txt)
    txt = re.sub(r"MENV2\(([^,]+),([^,]+),([^,]+),([^)]+)\)", r"P_E2ATK, \1, P_E2DEC, \2, P_E2SUS, \3, P_E2REL, \4", txt)
    txt = re.sub(r"PD_E\((\d)\)", lambda m: str(12 + int(m[1])), txt)
    out = []
    for tok in (t.strip() for t in txt.split(",")):
        out.append(ids[tok] if tok in ids else CONST[tok] if tok in CONST else int(tok))
    return [out[i:i + 2] for i in range(0, len(out), 2)]


def main(fn, arr):
    ids = param_ids()
    full = re.sub(r"/\*.*?\*/", "", (SRC / fn).read_text(), flags=re.S)
    shared = {m[1]: " ".join(m[2].split()).rsplit(",", 1)[0]   # (the shared lists: static const int8_t M_x[] = {.., -1};)
              for m in re.finditer(r"static const int8_t (\w+)\[\]\s*=\s*\{(.*?)\};", full, re.S)}
    consts = {}                                      # (the file's and core.h's enums and #defines: an e[] value by name)
    for src in [re.sub(r"/\*.*?\*/", "", (SRC / f).read_text(), flags=re.S) for f in ("core.h", "sing.c")] + [full]:
        for body in re.findall(r"enum\s*\w*\s*\{([^}]*)\}", src):
            n = 0
            for tok in (t.strip() for t in body.split(",")):
                if not tok:
                    continue
                if "=" in tok:
                    a, b = (x.strip() for x in tok.split("=", 1))
                    try:
                        n = int(eval(b.replace("u", ""), {}, consts))
                    except Exception:
                        continue
                    consts[a] = n
                else:
                    consts[tok] = n
                n += 1
        for a, b in re.findall(r"#define (\w+) \(?(-?\d+)\)?\s*$", src, re.M):
            consts.setdefault(a, int(b))
    s = full[full.index(f"{arr}[] = {{"):]
    s = s[:s.index("\n};")]
    tags = {"BASS": 1, "LEAD": 2, "PAD": 3, "KEYS": 4, "PLUCK": 5, "BELL": 6, "VOX": 7, "DRUM": 8, "FX": 9}
    body = s.split("{", 1)[1]
    ents, d, cur = [], 0, ""
    for ch in body:                                  # the entries: the top-level {...} of the array
        if ch == "{":
            d += 1
        if d:
            cur += ch
        if ch == "}":
            d -= 1
            if not d:
                ents.append(" ".join(cur.split()))
                cur = ""
    for ent in ents:
        m = re.match(r'\{"([^"]+)",\s*\{([^}]*)\},\s*\{([^}]*)\},\s*(-?\d+),\s*([^,]*),\s*FX\(([^)]*)\)(.*)\}$', ent)
        name, e, env, fenv, mono, fx, rest = m.groups()
        e = [int(eval(x, {}, consts)) for x in e.split(",")]
        mono1 = 1 if re.match(r"\s*1\s*\|", mono) else 0
        pat = re.search(r"PAT\((\d+)\)", rest)
        mods = re.search(r"MODS\((.*)\)\s*$", rest.strip().rstrip(","))
        named = re.search(r"\.mod\s*=\s*(\w+)", rest)
        mod = expand(mods.group(1), ids) if mods else expand(shared[named.group(1)], ids) if named else []
        w = re.search(r"WORDS\(([^,]+),([^)]+)\)", rest)
        b = re.search(r"BREATHE\(([^,]+),([^,]+),([^)]+)\)", rest)
        xp = (int(eval(w[1], {}, consts)) | int(eval(w[2], {}, consts)) << 2) if w else \
            (int(b[1]) | int(b[2]) << 6 | int(b[3]) << 7) if b else 0
        print(f'P("{name}", {e}, [{env.replace(" ", "").replace(",", ", ")}], {mono1}, {pat.group(1) if pat else 0}, {xp}, {mod})'.replace("'", ""))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
