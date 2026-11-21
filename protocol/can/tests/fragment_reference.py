"""Independent stdlib envelope oracle, no driver/C headers used to encode."""
import argparse
import hashlib
import struct
import subprocess


def packets():
    identity = struct.pack('<II', 0x11223344, 0xa1b2c3d4)
    ack = identity + struct.pack('<II', 0x01020305, 0x05060808)
    yield b'TU\x01\x06' + struct.pack('<H', len(ack)) + b'\0\0' + ack
    for length in (1, 256):
        data = identity + struct.pack('<IIHI', 0x01020304, 0x05060708, length, 0xdeadbeef)
        data += bytes((i * 7 + 3) % 256 for i in range(length))
        yield b'TU\x01\x05' + struct.pack('<H', len(data)) + b'\0\0' + data


def envelope(raw):
    frames = []
    for offset in range(0, len(raw), 4):
        field = (0x8000 | len(raw)) if offset == 0 else offset
        frames.append(struct.pack('<HH', 38, field) + raw[offset:offset + 4])
    assert all(5 <= len(frame) <= 8 for frame in frames)
    return frames


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', required=True)
    args = parser.parse_args()
    actual = subprocess.check_output([args.driver, '--vectors'], text=True)
    expected = ''.join(f'{i} ' + ','.join(f.hex() for f in envelope(raw)) + '\n'
                       for i, raw in enumerate(packets()))
    assert actual == expected, 'C fragment vectors differ from independent Python LE envelope'
    print('PASS3 independent vectors: ACK24/6, DATA31/8, DATA286/72 frames; partial DLC7/6; SHA256=' +
          hashlib.sha256(actual.encode()).hexdigest())
