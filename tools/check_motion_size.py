#!/usr/bin/env python3
"""Object regression budget only; not a board Flash/RAM usage report."""
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_motion_size.py CORE_ARCHIVE")
    output = subprocess.check_output(
        ["arm-none-eabi-size", "--format=berkeley", sys.argv[1]], text=True
    )
    print(output, end="")
    rows = [line.split() for line in output.splitlines()[1:] if line.strip()]
    if not rows or any(len(row) < 6 for row in rows):
        raise SystemExit("missing object size data")
    text, data, bss = (sum(int(row[i]) for row in rows) for i in range(3))
    if not (0 < text <= 8192) or data != 0 or bss != 0:
        raise SystemExit("core budget exceeded: text <=8192; no mutable globals")
    print("PASS: static object budget only; no board Flash/RAM usage claim")


if __name__ == "__main__":
    main()
