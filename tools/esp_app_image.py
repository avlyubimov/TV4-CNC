"""Bounded offline inspection of the current unsigned ESP32-S3 app format.

This checks build outputs, not signatures, hardware readiness or OTA eligibility.
No serial, SDK imports, network, subprocesses or image mutation.
"""
import hashlib
import struct

APP_MAX = 0x200000  # Current factory partition; no implied A/B migration.


def require(condition, message):
    if not condition:
        raise ValueError(message)


def cstring(data, name):
    end = data.find(b'\0')
    require(end > 0, 'invalid app descriptor ' + name)
    text = data[:end].decode('ascii')
    require(all(32 <= ord(c) <= 126 for c in text), 'invalid descriptor text')
    return text


def inspect_app(raw, sdk):
    require(sdk in ('5.3.2', '6.1.1'), 'unsupported SDK')
    require(336 <= len(raw) <= APP_MAX, 'app size outside factory bounds')
    h = raw[:24]
    require(h[0] == 0xe9 and 1 <= h[1] <= 16, 'invalid app header/segment count')
    require(h[2:4] == b'\x02\x4f', 'expected DIO/80MHz/16MB app')
    require(struct.unpack_from('<H', h, 12)[0] == 9 and h[23] == 1,
            'expected ESP32-S3 image with appended digest')
    minimum = struct.unpack_from('<H', h, 15)[0] or h[14] * 100
    maximum = struct.unpack_from('<H', h, 17)[0]
    require(minimum <= maximum, 'invalid chip revision range')
    offset, checksum, descriptor = 24, 0xef, None
    for index in range(h[1]):
        require(offset + 8 <= len(raw) - 33, 'truncated segment header')
        length = struct.unpack_from('<I', raw, offset + 4)[0]
        offset += 8
        require(length > 0 and length % 4 == 0 and length <= len(raw) - 33 - offset,
                'invalid/truncated segment length')
        data = raw[offset:offset + length]
        if index == 0:
            require(length >= 256, 'missing app descriptor')
            descriptor = data[:256]
        for byte in data:
            checksum ^= byte
        offset += length
    padding = 15 - offset % 16
    end = offset + padding + 1
    require(end + 32 == len(raw), 'unexpected tail/truncated image')
    require(not any(raw[offset:end - 1]) and raw[end - 1] == checksum,
            'invalid app padding/checksum')
    require(hashlib.sha256(raw[:end]).digest() == raw[end:], 'app appended SHA-256 mismatch')
    require(struct.unpack_from('<II', descriptor) == (0xabcd5432, 0),
            'wrong descriptor magic/security version')
    project = cstring(descriptor[48:80], 'project')
    version = cstring(descriptor[16:48], 'version')
    idf = cstring(descriptor[112:144], 'SDK')
    require(project == 'tv4_hmi' and idf == 'v' + sdk, 'app project/SDK mismatch')
    require('dirty' not in version, 'dirty build version cannot be a review candidate')
    return dict(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest(),
                project=project, version=version, idf_version=idf,
                elf_sha256=descriptor[144:176].hex(), segments=h[1],
                chip_revision_min=minimum, chip_revision_max=maximum,
                flash_bytes=16 * 1024 * 1024, flash_mode='dio', flash_frequency_mhz=80)
