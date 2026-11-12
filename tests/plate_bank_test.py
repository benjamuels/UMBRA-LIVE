#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 UMBRA LIVE contributors
"""(UMBRA LIVE) the plates an install brings (tools/build.py plate_bank, fm1pkg_make.py's "plates.bin", the update
loader's step 3c), on the host, with no toolchain and no SDK:
  1. a host plate file (tests/plate_slot_test image) placed in five slots as build.py places the real ones; the bank's
     own check (src/umb_slot.c, tests/plate_slot_test bank) must take every slot;
  2. two packages made by fm1pkg_make.py (the SDK's three files stood in for by filler: the loader reads none of them),
     the new one with that bank as plates.bin; the update loader's test (tests/ldr_test.c) installs it over the old one.
  python3 tests/plate_bank_test.py [OUTDIR]"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build  # noqa: E402
import fm1pkg_make  # noqa: E402

out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(tempfile.mkdtemp())
out.mkdir(parents=True, exist_ok=True)
cc = os.environ.get("CC", "cc").split()


def run(*a):
    r = subprocess.run([str(x) for x in a], cwd=ROOT, capture_output=True, text=True)
    sys.stdout.write(r.stdout[-3000:])
    if r.returncode:
        sys.stdout.write(r.stderr[-2000:])
        raise SystemExit(f"FAIL: {' '.join(str(x) for x in a[:3])} .. ({r.returncode})")


run(*cc, "-w", "-Ifirmware/src", "-Ifirmware/hal", "-o", out / "plate_slot_test", "tests/plate_slot_test.c")
run(out / "plate_slot_test", "image", out / "host.plate")
plate = (out / "host.plate").read_bytes()
bank = b"".join(build.plate_slot(plate, i) for i in range(5))
(out / "bank.bin").write_bytes(bank)
run(out / "plate_slot_test", "bank", out / "bank.bin", 5)

fm1pkg_make.sdk_file = lambda rel: bytes((i * 7 + len(rel)) & 255 for i in range(400))   # (filler: see above)
ota = bytes(range(256)) * 16
bank = bytearray(bank)
bank[0:4] = fm1pkg_make.PL_MAGIC.to_bytes(4, "little")                # (a host plate's magic is the firmware's: as is)
for name, app, pl in (("old", bytes([1, 2, 3, 4]) * 0x8000, None), ("new", bytes([9, 8, 7, 6]) * 0x9000, bytes(bank))):
    pkg = fm1pkg_make.ufw(fm1pkg_make.flash_image(app, fm1pkg_make.KEY), ota, "FM-1_900", None, pl)
    (out / f"{name}.fwsc").write_bytes(pkg)
run(*cc, "-w", "-o", out / "ldr_test", "tests/ldr_test.c")
run(out / "ldr_test", out / "old.fwsc", out / "new.fwsc")
print("plate bank test: ok")
