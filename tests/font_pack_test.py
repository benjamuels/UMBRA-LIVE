"""Lossless glyph records, including the wide/multiphase fallback."""
import random
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from aa_raster import glyph_tables

rng = random.Random(48)
rows = [(0, 0, -1, 0, 0, 0), (4095, 511, 6, 31, 31, 31)]
rows += [(rng.randrange(4096), rng.randrange(512), rng.randrange(-1, 7),
          rng.randrange(32), rng.randrange(32), rng.randrange(32)) for _ in range(10000)]
text, size = glyph_tables("TEST", rows, 1)
packed = bytes(int(x) for x in re.findall(r"\b\d+\b", text.split("{", 1)[1].split("}", 1)[0]))
assert size == len(rows) * 5 == len(packed)
for i, expected in enumerate(rows):
    n = int.from_bytes(packed[i*5:i*5+5], "little")
    actual = (n & 4095, n >> 12 & 511, (n >> 21 & 7) - 1,
              n >> 24 & 31, n >> 29 & 31, n >> 34 & 31)
    assert actual == expected, (actual, expected)
for row in [(4096, 512, -1, 32, 32, 32), (65535, 65535, 6, 63, 127, 63)]:
    text, size = glyph_tables("WIDE", [row], 1)
    assert size == 8 and "_PACK" not in text and "0x" in text
text, size = glyph_tables("PHASE", [(100, 32, 0, 1, 2, 3), (103, 32, 1, 1, 2, 3)], 2)
assert size == 12 and "_PACK" not in text
print("font metadata: 10002 exact round trips; wide and multiphase fallbacks preserved")
