"""Independent MS complete-message oracle; does not import implementation/header."""
import argparse
import hashlib
import struct
import subprocess


def record(kind):
    header = b'MS\x01' + bytes([kind]) + struct.pack('<H', 164) + b'\0\0'
    request = struct.pack('<II', 11, 5)
    image = struct.pack('<III', 22, 9, 4096) + bytes(range(32)) + bytes(range(32, 64))
    identity = (0, 0) if kind == 1 else (22, 10)
    metadata = struct.pack('<III', *identity, 1215) + bytes(range(64, 96))
    return header + request + image + metadata + bytes(range(96, 128)) + struct.pack('<H', 256) + b'\0\0'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', required=True)
    args = parser.parse_args()
    actual = subprocess.check_output([args.driver, '--vectors'], text=True)
    expected = ''.join(f'{kind} {record(kind).hex()}\n' for kind in range(1, 5))
    assert all(len(record(kind)) == 172 for kind in range(1, 5))
    assert actual == expected, 'MS bytes disagree with independent Python LE oracle'
    print('PASS4 independent setup vectors; SHA256=' + hashlib.sha256(actual.encode()).hexdigest())
