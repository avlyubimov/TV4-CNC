#!/usr/bin/env python3
"""Read a linked sizeof constant; no execution, allocation or device access."""
import json
from pathlib import Path
import re
import struct
import subprocess
import sys


def inspect(path):
    path = Path(path)
    raw = path.read_bytes()
    if raw[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', raw, 18)[0] != 94:
        raise ValueError('expected little-endian ELF32 Xtensa')
    symbols = subprocess.check_output(['readelf', '-Ws', str(path)], text=True)
    matches = [line.split() for line in symbols.splitlines()
               if line.split() and line.split()[-1] == 'tv4_ota_workspace_bytes']
    if len(matches) != 1:
        raise ValueError('missing/ambiguous linked workspace constant')
    symbol = matches[0]
    if symbol[2] != '4' or symbol[3] != 'OBJECT' or not symbol[6].isdigit():
        raise ValueError('workspace constant must be a defined uint32 object')
    address = int(symbol[1], 16)
    sections = subprocess.check_output(['readelf', '-SW', str(path)], text=True)
    for line in sections.splitlines():
        match = re.match(r'\s*\[\s*(\d+)\]\s+\S+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)', line)
        if not match or match[1] != symbol[6]:
            continue
        start, offset, size = (int(match[i], 16) for i in (3, 4, 5))
        if match[2] != 'PROGBITS' or not start <= address or address + 4 > start + size:
            raise ValueError('constant is outside file-backed section')
        location = offset + address - start
        if location + 4 > len(raw):
            raise ValueError('truncated ELF section')
        workspace, = struct.unpack_from('<I', raw, location)
        if not 16384 <= workspace <= 36 * 1024:
            raise ValueError('workspace outside reviewed 16..36KiB bound')
        return dict(workspace_bytes=workspace, method='linked target sizeof constant; not runtime measurement')
    raise ValueError('workspace section not found')


if __name__ == '__main__':
    try:
        print(json.dumps(inspect(sys.argv[1]), indent=2))
    except (ValueError, IndexError, OSError, struct.error, subprocess.CalledProcessError) as error:
        raise SystemExit(f'OTA memory inspection failed: {error}')
