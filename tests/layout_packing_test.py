"""Only movable main-loop code may fill gaps before pinned audio runs."""
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import build

build.size_fns.size_names = lambda: {"song_erase_parts", "gen_go", "old_ui", "audio_fn", "radio_fn"}
build.late_fns = lambda: {"gen_go"}
base = build.APP_XIP
rows = [(64, "@", "audio_fn"), (70, "-", "old_ui"), (128, "@", "audio_next")]
symbols = {"_lay_gap0": base + 16, "_lay_gap1": base + 80}
inputs = [(".rodata.n.label", base + 1000, 12, "build/felucca.o"),
          (".text.song_erase_parts", base + 2000, 24, "build/felucca.o"),
          (".text.gen_go", base + 2100, 20, "build/felucca.o"),
          (".text.audio_fn", base + 64, 24, "build/felucca.o"),
          (".text.old_ui", base + 70, 24, "build/felucca.o"),
          (".text.radio_fn", base + 2200, 24, "build/bt_seg.o")]
fills, gap, padding = build.layout_fills(rows, symbols, inputs, {"label": 1}, {"audio_fn"})
items = [item for values in fills.values() for item in values]
assert set(items) == {"label", "@song_erase_parts", "@gen_go"}
assert len(items) == len(set(items)) and gap == 96 and padding == 40
with TemporaryDirectory() as tmp:
    build.LAYOUT_LD = Path(tmp) / "layout.ld"
    build.layout_ld(rows, fills)
    text = build.LAYOUT_LD.read_text()
    assert "*(.text.song_erase_parts)" in text and "*(.rodata.n.label)" in text
    assert text.count("*(.text.audio_fn)") == 1
    assert text.count("*(.text.old_ui)") == 1
    assert "radio_fn" not in text
    assert ". = 0x40;" in text and ". = 0x80;" in text
print("PASS gap packing: IRQ code, recorded UI and radio object excluded; audio anchors unchanged")
