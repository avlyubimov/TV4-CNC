#!/usr/bin/env python3
"""Read a candidate CSV; invoke only SDK partition parser into a temporary dir."""
import argparse
import csv
from pathlib import Path
import subprocess
import tempfile

EXPECTED = [
    ('nvs', 'data', 'nvs', 0x9000, 0x6000),
    ('phy_init', 'data', 'phy', 0xf000, 0x1000),
    ('factory', 'app', 'factory', 0x10000, 0x200000),
    ('otadata', 'data', 'ota', 0x210000, 0x2000),
    ('ota_0', 'app', 'ota_0', 0x220000, 0x300000),
    ('ota_1', 'app', 'ota_1', 0x520000, 0x300000),
    ('resources', 'data', '0x40', 0x820000, 0x300000),
    ('store', 'data', '0x41', 0xb20000, 0x4e0000),
]


def verify(candidate, sdk, python):
    rows = []
    with Path(candidate).open() as source:
        for row in csv.reader(line for line in source if line.strip() and not line.lstrip().startswith('#')):
            row = [field.strip() for field in row]
            if len(row) != 6 or row[5]:
                raise ValueError('invalid candidate fields/flags')
            rows.append((*row[:3], int(row[3], 0), int(row[4], 0)))
    if rows != EXPECTED:
        raise ValueError('candidate differs from reviewed layout')
    end = 0x9000
    for name, kind, _, offset, size in rows:
        if offset < end or offset % 4096 or size <= 0 or size % 4096:
            raise ValueError('overlap/erase alignment rejected')
        if kind == 'app' and offset % 65536:
            raise ValueError('app alignment rejected')
        end = offset + size
    if end != 16 * 1024 * 1024:
        raise ValueError('expected 16 MiB map')
    parser = Path(sdk) / 'components/partition_table/gen_esp32part.py'
    with tempfile.TemporaryDirectory(prefix='tv4-partition-check-') as directory:
        binary = Path(directory) / 'candidate.bin'
        subprocess.run([python, str(parser), '--flash-size', '16MB', str(Path(candidate).resolve()),
                        str(binary)], check=True)
        subprocess.run([python, str(parser), str(binary), str(Path(directory) / 'roundtrip.csv')], check=True)
    print('PASS: candidate layout/parser only; active partitions.csv unchanged; device_access=false')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', required=True)
    p.add_argument('--python', default='python3')
    p.add_argument('--candidate', default='firmware/esp32-hmi/ota/partitions.ab.candidate.csv')
    args = p.parse_args()
    verify(args.candidate, args.sdk, args.python)
