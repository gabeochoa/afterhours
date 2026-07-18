#!/usr/bin/env python3
"""
Convert macOS `sample` output into collapsed stack format:
  frameA;frameB;leaf <count>

This helper is intentionally standalone and opt-in.
"""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

FRAME_RE = re.compile(r"^\s*\d+\s+(.+?)\s+\+\s+\d+")


def parse_file(path: Path, collapsed: Counter[str]) -> None:
    stack: list[str] = []
    with path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("Call graph:"):
                stack.clear()
                continue
            m = FRAME_RE.match(line.rstrip("\n"))
            if not m:
                continue
            frame = m.group(1).strip()
            if frame:
                stack.append(frame)
                collapsed[";".join(reversed(stack))] += 1


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(
            "usage: sample_to_collapsed.py <sample.txt> [more sample files...]",
            file=sys.stderr,
        )
        return 2

    collapsed: Counter[str] = Counter()
    for arg in argv[1:]:
        for path in sorted(Path().glob(arg)):
            parse_file(path, collapsed)

    for stack, count in collapsed.items():
        print(f"{stack} {count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
