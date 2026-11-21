#!/usr/bin/env python3
"""Compile-only opt-in update channel archive budget; not a board memory/stack report."""
import argparse
import json
from pathlib import Path
import subprocess


def inspect(directory):
    directory = Path(directory)
    archive = directory / 'libtv4_update_channel.a'
    output = subprocess.check_output(['arm-none-eabi-size', '--format=berkeley', str(archive)], text=True)
    rows = [line.split() for line in output.splitlines()[1:] if line.strip()]
    if not rows or any(len(row) < 6 for row in rows):
        raise ValueError('missing channel object sizes')
    totals = {name: sum(int(row[i]) for row in rows) for i, name in enumerate(('text', 'data', 'bss'))}
    if not 0 < totals['text'] <= 4096 or totals['data'] != 0 or totals['bss'] != 0:
        raise ValueError('channel object budget exceeded: text<=4096, no mutable globals')
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
    allowed = {'memcmp', 'memcpy', 'memset', 'tv4_update_wire_encode', 'tv4_update_wire_decode',
               'tv4_metadata_setup_encode', 'tv4_metadata_setup_decode'}
    allowed |= {'tv4_fragment_' + suffix for suffix in (
        'tx_init_raw', 'rx_init_raw', 'tx_begin_raw', 'tx_peek', 'tx_commit', 'rx_receive',
        'rx_take_raw', 'tx_poll', 'rx_poll', 'tx_cancel', 'rx_cancel', 'tx_clear', 'rx_clear')}
    if not imported <= allowed:
        raise ValueError('unreviewed channel import: ' + repr(sorted(imported - allowed)))
    expected = {'tv4_channel_' + suffix for suffix in (
        'tx_init', 'rx_init', 'tx_begin', 'tx_peek', 'tx_commit', 'rx_receive', 'rx_take',
        'tx_poll', 'rx_poll', 'tx_advance', 'rx_advance', 'tx_cancel', 'rx_cancel', 'tx_clear', 'rx_clear')}
    if exported != expected:
        raise ValueError('unexpected channel exports: ' + repr(sorted(exported)))
    frames = []
    for path in (directory / 'CMakeFiles/tv4_update_channel.dir').rglob('*.su'):
        for line in path.read_text().splitlines():
            fields = line.split('\t')
            if len(fields) != 3 or fields[2] != 'static' or not 0 <= int(fields[1]) <= 128:
                raise ValueError('channel individual frame budget exceeded: ' + line)
            frames.append(int(fields[1]))
    if not frames:
        raise ValueError('missing channel compiler frames')
    return dict(scope='opt-in mixed-codec channel static archive, not firmware', objects=totals,
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
        parser.exit(1, 'Update channel build rejected: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))
