#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 UMBRA LIVE contributors
"""The plates page's table of what each control does (web/plates.html, DOC: between its /*@DOC*/ and /*@END*/ marks),
written anew from the plates' parameters document: its "## NAME" sections, each one's "| CONTROL | default | what it
does |" rows.

  web/plates_doc.py path/to/umbra-plate-parameters.md

tests/plate_page_test.mjs checks the table against the plate files (every control named, in order, with its default)."""
import json
import re
import sys
from pathlib import Path

page = Path(__file__).resolve().parent / "plates.html"
doc = Path(sys.argv[1]).read_text(encoding="utf-8")
rows = []
for sec in re.split(r"^## ", doc, flags=re.M)[1:]:
    name = sec.split("\n", 1)[0].strip()
    ctl = [[n, int(d), w.strip()] for n, d, w in re.findall(r"^\| ([A-Z0-9]+) \| (\d+) \| (.+?) \|$", sec, flags=re.M)]
    if ctl:
        rows.append(f"  {json.dumps(name)}: {json.dumps(ctl, ensure_ascii=False)},")
html = page.read_text(encoding="utf-8")
a, b = html.index("/*@DOC*/"), html.index("/*@END*/")
page.write_text(html[:a] + "/*@DOC*/\n" + "\n".join(rows) + "\n" + html[b:], encoding="utf-8")
print(f"plates.html: {len(rows)} plates' controls from {sys.argv[1]}")
