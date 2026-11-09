#!/usr/bin/env python3
"""Losslessly pack the original on-device notices. No text is removed."""
import ast
from collections import Counter
from pathlib import Path
import re
import sys

def notices(path):
    source = Path(path).read_text()
    out = []
    for name in ("PROJECT", "CREDITS"):
        body = re.search(r"ABOUT_"+name+r"\[\] BTAPP_RO =(.*?);\n", source, re.S).group(1)
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        text = "".join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', body))
        out.append((text+"\0").encode("ascii"))
    return out

def pack(raw):
    rows = [list(r) for r in raw]
    pairs = []
    best = (sum(map(len, rows)), [], rows)
    for code in range(128, 256):
        counts = Counter(p for row in rows for p in zip(row, row[1:]))
        pair, count = counts.most_common(1)[0]
        if count < 3: break
        changed = []
        for row in rows:
            new, i = [], 0
            while i < len(row):
                if tuple(row[i:i+2]) == pair: new.append(code); i += 2
                else: new.append(row[i]); i += 1
            changed.append(new)
        rows = changed
        pairs = pairs + [pair]
        size = 2*len(pairs)+sum(map(len, rows))
        if size < best[0]: best = size, pairs, rows
    return best[1:]

def unpack(pairs, row):
    def expand(c):
        return bytes([c]) if c < 128 else b"".join(expand(v) for v in pairs[c-128])
    return b"".join(expand(c) for c in row)

def main(out, source):
    raw = notices(source); pairs, rows = pack(raw)
    assert all(unpack(pairs, row) == original for row, original in zip(rows, raw))
    depths = [0]*128
    for a,b in pairs: depths.append(1+max(depths[a],depths[b]))
    assert max(depths) <= 12 and max(map(len,raw)) <= 1536
    lines = ["/* Generated losslessly from ui_menu.c; readable source notices remain there. */",
             f"#define ABOUT_MAX {max(map(len,raw))}u",
             f"static const uint8_t ABOUT_PAIRS[{len(pairs)}][2] BTAPP_RO = {{"]
    lines += [f"    {{{a},{b}}}," for a,b in pairs]
    lines.append("};")
    for name, row in zip(("PROJECT", "CREDITS"), rows):
        data = [len(row)&255, len(row)>>8]+row
        lines += [f"static const uint8_t ABOUT_{name}_PACK[{len(data)}] BTAPP_RO = {{",
                  ",".join(map(str,data)), "};"]
    Path(out).write_text("\n".join(lines)+"\n")
    print(f"about: {sum(map(len,raw))} -> {sum(map(len,rows))+2*len(pairs)+4} bytes; notices preserved; depth {max(depths)}")

if __name__ == "__main__": main(*sys.argv[1:])
