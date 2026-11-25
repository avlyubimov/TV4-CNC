#!/usr/bin/env python3
"""Compare two owned build outputs. No build, SDK import, network or device access.

The caller establishes source/clean-build provenance; equal files alone do not
authenticate a release or prove independent compilation. Never normalize bytes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

from check_idf_build import verify as validate
from check_idf_build import require

FILES = {'tv4_hmi.bin': 0x200000, 'tv4_hmi.elf': 32 * 1024 * 1024,
         'bootloader/bootloader.bin': 0x8000, 'partition_table/partition-table.bin': 4096}


def read(root, name, maximum):
    path = root / name
    require(path.is_file() and not path.is_symlink() and 0 < path.stat().st_size <= maximum,
            'invalid build artifact: ' + name)
    raw = path.read_bytes()
    require(0 < len(raw) <= maximum, 'artifact size changed: ' + name)
    return raw


def compare(first, second, sdk, source_commit, *, memory_diagnostics=False):
    require(isinstance(source_commit, str) and re.fullmatch(r'[0-9a-f]{40}', source_commit),
            'explicit full source commit claim required')
    first, second = Path(first).resolve(), Path(second).resolve()
    require(first != second and first.is_dir() and second.is_dir(), 'two different build directories required')
    reports = [validate(p, 'rev12-ota', sdk, write_report=False, reproducible=True,
                        memory_diagnostics=memory_diagnostics) for p in (first, second)]
    configs = [json.loads(read(p, 'config/sdkconfig.json', 1024 * 1024)) for p in (first, second)]
    require(configs[0] == configs[1], 'effective SDK configurations differ')
    require(reports[0] == reports[1], 'validated images/configurations differ')
    artifacts = {}
    for name, maximum in FILES.items():
        a, b = read(first, name, maximum), read(second, name, maximum)
        require(a == b, 'different build bytes: ' + name)
        artifacts[name] = dict(bytes=len(a), sha256=hashlib.sha256(a).hexdigest())
    return dict(schema='tv4-idf-reproducibility-1', sdk=sdk, profile='rev12-ota',
                source_commit_claim=source_commit, independent_compilation_verified_by_this_tool=False,
                equal_build_outputs=True, effective_configurations_equal=True,
                compile_date_time_absent=True, memory_diagnostics=memory_diagnostics, artifacts=artifacts,
                device_access=False, install_authorized=False, release_published=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--first', required=True)
    parser.add_argument('--second', required=True)
    parser.add_argument('--sdk', choices=('5.3.2', '6.1.1'), required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--memory-diagnostics', action='store_true')
    args = parser.parse_args()
    try:
        result = compare(args.first, args.second, args.sdk, args.source_commit,
                         memory_diagnostics=args.memory_diagnostics)
    except (ValueError, KeyError, TypeError, OSError) as error:
        raise SystemExit('Reproducibility check rejected: ' + str(error)) from None
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
