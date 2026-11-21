#!/usr/bin/env python3
"""Generate deterministic identity for a build only; never access a device."""
import argparse
from pathlib import Path
import re
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    sha = subprocess.check_output(['git', '-C', str(a.root), 'rev-parse', 'HEAD'], text=True).strip()
    if not re.fullmatch('[0-9a-f]{40}', sha):
        raise SystemExit('bench requires an exact source commit')
    dirty = bool(subprocess.check_output(['git', '-C', str(a.root), 'status', '--porcelain',
                                        '--untracked-files=normal'], text=True).strip())
    content = ('/* Generated build identity; no date/path/key or device reads. */\n'
               '#define TV4_BENCH_SOURCE_COMMIT "' + sha + '"\n'
               '#define TV4_BENCH_SOURCE_DIRTY ' + str(int(dirty)) + 'u\n')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    if not a.output.exists() or a.output.read_text() != content:
        a.output.write_text(content)


if __name__ == '__main__':
    main()
