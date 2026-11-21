#!/usr/bin/env python3
"""Compile-only opt-in driver event inbox archive budget; not a board memory/stack report."""
import argparse
import json
from pathlib import Path
import subprocess


def inspect(directory):
    directory = Path(directory)
    archive = directory / 'libtv4_update_driver_events.a'
    output = subprocess.check_output(['arm-none-eabi-size', '--format=berkeley', str(archive)], text=True)
    rows = [line.split() for line in output.splitlines()[1:] if line.strip()]
    if not rows or any(len(row) < 6 for row in rows):
        raise ValueError('missing driver events object sizes')
    totals = {name: sum(int(row[i]) for row in rows) for i, name in enumerate(('text', 'data', 'bss'))}
    if not 0 < totals['text'] <= 4096 or totals['data'] != 0 or totals['bss'] != 0:
        raise ValueError('driver events object budget exceeded: text<=4096, no mutable globals')
    symbols = subprocess.check_output(['arm-none-eabi-nm', '-g', '--format=posix', str(archive)], text=True)
    exported, imported = set(), set()
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) < 2 or len(fields[1]) != 1:
            continue
        if fields[1] == 'U':
            imported.add(fields[0])
        else:
            exported.add(fields[0])
    allowed = {'memcpy', 'memset'}
    allowed |= {'tv4_update_egress_' + suffix for suffix in (
        'init', 'begin', 'pump', 'claim', 'complete', 'poll', 'advance', 'cancel', 'clear')}
    if not imported <= allowed:
        raise ValueError('unreviewed driver events import: ' + repr(sorted(imported - allowed)))
    expected = {'tv4_driver_events_' + suffix for suffix in (
        'init', 'begin', 'pump', 'claim', 'post', 'poll', 'service', 'advance', 'cancel', 'clear')}
    if exported != expected:
        raise ValueError('unexpected driver events exports: ' + repr(sorted(exported)))
    frames = []
    for path in (directory / 'CMakeFiles/tv4_update_driver_events.dir').rglob('*.su'):
        for line in path.read_text().splitlines():
            fields = line.split('\t')
            if len(fields) != 3 or fields[2] != 'static' or not 0 <= int(fields[1]) <= 128:
                raise ValueError('driver events individual frame budget exceeded: ' + line)
            frames.append(int(fields[1]))
    if not frames:
        raise ValueError('missing driver events compiler frames')
    return dict(scope='opt-in bound egress/driver event inbox static archive, not firmware', objects=totals,
                unresolved_imports=sorted(imported), exported_functions=sorted(exported),
                max_individual_frame=max(frames), full_stack_verified=False,
                board_memory_verified=False, runtime_caller=False, device_access=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_directory')
    args = parser.parse_args()
    try:
        result = inspect(args.build_directory)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, 'Driver events build rejected: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))
