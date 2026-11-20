#!/usr/bin/env python3
"""Compile-only library regression budgets; never a board memory/stack report."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def inspect(directory):
    directory = Path(directory)
    archives = [directory / f'libtv4_boot_{name}.a' for name in ('admission', 'crypto')]
    size = subprocess.check_output(['arm-none-eabi-size', '--format=berkeley', *map(str, archives)], text=True)
    rows = [line.split() for line in size.splitlines()[1:] if line.strip()]
    if not rows or any(len(row) < 6 for row in rows):
        raise ValueError('missing object sizes')
    totals = {name: sum(int(row[i]) for row in rows) for i, name in enumerate(('text', 'data', 'bss'))}
    if not (0 < totals['text'] <= 65536 and totals['data'] <= 64 and totals['bss'] <= 128):
        raise ValueError('library object budget exceeded')
    nm = subprocess.check_output(['arm-none-eabi-nm', '-g', '--format=posix', *map(str, archives)], text=True)
    defined, undefined = set(), set()
    for line in nm.splitlines():
        parts = line.split()
        if len(parts) < 2 or len(parts[1]) != 1: continue
        name, kind = parts[:2]
        if kind in ('U', 'w', 'v'): undefined.add(name)
        else: defined.add(name)
    external = undefined - defined
    allowed = {'memcmp', 'memcpy', 'memmove', 'memset', 'strcmp', 'strlen', '__aeabi_uldivmod'}
    if not external <= allowed:
        raise ValueError('unreviewed heap/OS/driver imports: ' + repr(sorted(external - allowed)))
    frames = {}
    for name, limit in (('admission', 1536), ('crypto', 512)):
        found = []
        for path in (directory / f'CMakeFiles/tv4_boot_{name}.dir').rglob('*.su'):
            for row in path.read_text().splitlines():
                fields = row.split('\t')
                if len(fields) != 3 or fields[2] != 'static' or int(fields[1]) > limit:
                    raise ValueError('individual compiler frame budget exceeded: ' + row)
                found.append(int(fields[1]))
        if not found: raise ValueError('missing compiler frames')
        frames[name] = max(found)
    for path in directory.rglob('*.bin'):
        # Four public upstream data vectors ship in the official source archive.
        # They are immutable dependency inputs, never firmware build outputs.
        relative = path.relative_to(directory).as_posix()
        prefix = '_deps/tv4_boot_mbedtls-src/'
        origin = json.loads((Path(__file__).resolve().parents[1] / 'third_party/mbedtls/origin.json').read_text())
        expected = origin['archive_binary_test_inputs'].get(relative.removeprefix(prefix)) if relative.startswith(prefix) else None
        if expected is not None and hashlib.sha256(path.read_bytes()).hexdigest() == expected:
            continue
        if path.name != 'CMakeDetermineCompilerABI_C.bin' or 'CMakeFiles' not in path.parts:
            raise ValueError('unexpected firmware binary')
    return {'scope': 'static library objects, not deployable firmware', 'objects': totals,
            'unresolved_imports': sorted(external), 'max_individual_frames': frames,
            'full_stack_verified': False, 'board_memory_verified': False, 'device_access': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_directory')
    args = parser.parse_args()
    try:
        result = inspect(args.build_directory)
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        parser.exit(1, f'Admission build rejected: {exc}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
