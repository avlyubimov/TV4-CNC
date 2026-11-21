"""Independent standard-library golden encoder; no C padding/enum assumptions."""
import argparse
import hashlib
import struct
import subprocess


def expected(kind):
    index = kind
    if kind > 8:
        kind = 7
    request = (0x11223344, 0xa1b2c3d4)
    descriptor = lambda identity: struct.pack('<III', *identity, 0x10203040) + bytes(range(32))
    if 1 <= kind <= 4:
        identity = (0, 0) if kind == 1 else (0x55667788, 0x01020304)
        body = (struct.pack('<II', *request) + descriptor(identity) + struct.pack('<I', 0x55667788)
                + bytes(range(32, 64)) + bytes(range(64, 96)) + struct.pack('<H', 256))
    elif kind == 5:
        body = (struct.pack('<IIIIHI', *request, 0x01020304, 0x05060708, 17, 0xdeadbeef)
                + bytes((i * 7 + 3) & 255 for i in range(17)))
    elif kind == 6:
        body = struct.pack('<IIII', *request, 0x01020305, 0x05060808)
    else:
        tokens = (1, 0) if index <= 8 else ((2, 5), (3, 14), (4, 17), (5, 15))[index - 9]
        body = descriptor(request) + bytes(range(32, 64)) + bytes(tokens)
    return b'TU' + bytes([1, kind]) + struct.pack('<H', len(body)) + b'\0\0' + body


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--driver', required=True)
    args = parser.parse_args()
    result = subprocess.run([args.driver, '--vectors'], check=True, capture_output=True, text=True, timeout=10)
    assert not result.stderr
    lines = result.stdout.splitlines()
    assert len(lines) == 12
    collected = b''
    for number, line in enumerate(lines, 1):
        token, hex_bytes = line.split()
        raw = bytes.fromhex(hex_bytes)
        assert int(token) == number and raw == expected(number), (number, raw.hex(), expected(number).hex())
        collected += raw
    print('PASS12 independent Python LE golden messages,8 kinds/all5 outcome codes; concatenated_sha256=' + hashlib.sha256(collected).hexdigest())
