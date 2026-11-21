#!/usr/bin/env python3
"""Compile-only opt-in two-actor driver dispatch archive budget; not a board memory/stack report."""
import argparse
import json
from pathlib import Path
import subprocess


def inspect(directory):
    directory = Path(directory)
    archive = directory / 'libtv4_driver_dispatch.a'
    output = subprocess.check_output(['arm-none-eabi-size', '--format=berkeley', str(archive)], text=True)
    rows = [line.split() for line in output.splitlines()[1:] if line.strip()]
    if not rows or any(len(row) < 6 for row in rows):
        raise ValueError('missing driver dispatch object sizes')
    totals = {name: sum(int(row[i]) for row in rows) for i, name in enumerate(('text', 'data', 'bss'))}
    if not 0 < totals['text'] <= 4096 or totals['data'] != 0 or totals['bss'] != 0:
        raise ValueError('driver dispatch object budget exceeded: text<=4096, no mutable globals')
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
    allowed |= {'tv4_completion_owner_' + suffix for suffix in (
        'init', 'begin', 'pump', 'claim', 'post', 'poll', 'service', 'advance', 'cancel', 'clear')}
    allowed.add('tv4_completion_mailbox_empty')
    if not imported <= allowed:
        raise ValueError('unreviewed driver dispatch import: ' + repr(sorted(imported - allowed)))
    expected = {'tv4_driver_dispatch_' + suffix for suffix in (
        'init', 'begin', 'pump', 'publish', 'poll', 'service', 'advance', 'cancel', 'clear',
        'take', 'check', 'report', 'retry')}
    if exported != expected:
        raise ValueError('unexpected driver dispatch exports: ' + repr(sorted(exported)))
    frames = []
    for path in (directory / 'CMakeFiles/tv4_driver_dispatch.dir').rglob('*.su'):
        for line in path.read_text().splitlines():
            fields = line.split('\t')
            if len(fields) != 3 or fields[2] != 'static' or not 0 <= int(fields[1]) <= 128:
                raise ValueError('driver dispatch individual frame budget exceeded: ' + line)
            frames.append(int(fields[1]))
    if not frames:
        raise ValueError('missing driver dispatch compiler frames')
    assembly = subprocess.check_output(['arm-none-eabi-objdump', '-d', str(archive)], text=True)
    barriers = assembly.count('dmb')
    if barriers < 8:
        raise ValueError('missing native atomic barriers for pinned Cortex-M4 code generation')
    return dict(scope='opt-in two-actor owned frame dispatch/completion static archive, not firmware', objects=totals,
                native_atomic_barriers=barriers, isr_or_rtos_verified=False,
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
        parser.exit(1, 'Driver dispatch build rejected: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))
