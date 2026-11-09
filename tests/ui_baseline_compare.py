"""Fail on new full-UI failures or crashes; report existing baseline debt explicitly."""
import collections
import re
import sys
from pathlib import Path


def failures(path, rc):
    text = Path(path).read_text(errors="replace")
    failed = collections.Counter(line for line in text.splitlines() if line.endswith("FAIL"))
    reported = re.search(r"ui: (\d+) FAILED\s*$", text)
    if failed:
        assert int(rc) == 1 and reported and int(reported[1]) == sum(failed.values()), "UI crashed or failure report is incomplete"
    else:
        assert int(rc) == 0, "UI failed without an assertion report"
    return failed


base = failures(sys.argv[1], sys.argv[3])
current = failures(sys.argv[2], sys.argv[4])
new = current - base
print(f"Full UI: {sum(base.values())} known baseline failures, {sum(current.values())} candidate failures")
for line, count in new.items():
    print(f"NEW ({count}): {line}")
assert not new, "new UI regression"
print("PASS no new full-UI assertions; existing baseline failures remain visible in logs")
