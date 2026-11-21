"""Independent counted-byte container oracle, no imported C headers/encoder."""
import argparse
import hashlib
import struct
import subprocess


def record(size, signature, key):
    header = b'TM\x01\x00' + struct.pack('<HHH', size, signature, key) + b'\0\0'
    header += struct.pack('<III', 7, 9, 4096) + bytes(range(32)) + bytes(range(32, 64))
    assert len(header) == 88
    return (header + bytes(ord('a') + i % 26 for i in range(key)) +
            bytes(i + 64 for i in range(signature)) + bytes((i * 7 + 3) % 256 for i in range(size)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', required=True)
    args = parser.parse_args()
    actual = subprocess.check_output([args.driver, '--vectors'], text=True)
    expected = '0 ' + record(1, 8, 1).hex() + '\n1 ' + record(1024, 72, 31).hex() + '\n'
    assert actual == expected, 'Container bytes disagree with independent Python LE oracle'
    print('PASS2 independent metadata bundles98/1215 bytes; SHA256=' + hashlib.sha256(actual.encode()).hexdigest())
