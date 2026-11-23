#!/usr/bin/env python3
"""Offline candidate validation only. No HTTP, Flash, signing or key provisioning."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

MANIFEST_MAX = 16384
ENVELOPE_MAX = 2048
SLOT_MAX = 0x300000
P256_PREFIX = bytes.fromhex('3059301306072a8648ce3d020106082a8648ce3d03010703420004')


class Rejected(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Rejected(message)


def bounded_read(path, maximum):
    with Path(path).open('rb') as stream:
        data = stream.read(maximum + 1)
    require(0 < len(data) <= maximum, 'file size limit exceeded or empty file')
    return data


def pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def parse(raw):
    # Strict UTF-8, no BOM/NaN/Infinity or repeated keys, including nested objects.
    value = json.loads(raw.decode('utf-8'), object_pairs_hook=pairs,
                       parse_constant=lambda _: (_ for _ in ()).throw(Rejected('invalid JSON number')))
    def check_strings(node):
        if isinstance(node, str):
            require('\x00' not in node, 'embedded NUL rejected')
            node.encode('utf-8')  # Reject unpaired escaped surrogates too.
        elif isinstance(node, dict):
            for key, item in node.items():
                check_strings(key)
                check_strings(item)
        elif isinstance(node, list):
            for item in node:
                check_strings(item)
    check_strings(value)
    return value


def keys(value, required, optional=()):
    require(isinstance(value, dict), 'expected object')
    require(set(required) <= set(value) <= set(required) | set(optional), 'unsupported/missing schema fields')


def version(text):
    require(isinstance(text, str) and len(text) <= 64, 'invalid version')
    m = re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?', text)
    require(m is not None, 'expected bounded SemVer without build metadata')
    require(all(int(x) <= 0xffffffff for x in m.group(1, 2, 3)), 'version core overflow')
    pre = m.group(4)
    if pre:
        require(all(not (x.isdigit() and len(x) > 1 and x[0] == '0') for x in pre.split('.')), 'invalid prerelease')
    # Numeric identifiers compare below alpha, final releases above prereleases.
    return (tuple(int(x) for x in m.group(1, 2, 3)), 1 if pre is None else 0,
            tuple((0, int(x)) if x.isdigit() else (1, x) for x in pre.split('.')) if pre else ())


def decode_signature(text):
    # Same canonical Base64 bounds as native envelope.c; validate=True alone
    # accepts nonzero pad bits and some surplus padding spellings.
    require(isinstance(text, str) and 12 <= len(text) <= 96 and len(text) % 4 == 0,
            'invalid signature encoding')
    signature = base64.b64decode(text, validate=True)
    require(8 <= len(signature) <= 72, 'invalid signature length')
    require(base64.b64encode(signature).decode('ascii') == text,
            'noncanonical signature encoding')
    return signature


def read_metadata(args):
    """Read-only signed metadata policy; a candidate is not install authority.

    Kept separate from image reads so tests can compare the same layer with
    native manifest_read. The public CLI still requires full image validation.
    """
    require(isinstance(args.trusted_key_id, str) and 0 < len(args.trusted_key_id.encode('utf-8')) <= 64,
            'invalid local key id')
    raw = bounded_read(args.manifest, MANIFEST_MAX)
    envelope = parse(bounded_read(args.signature, ENVELOPE_MAX))
    keys(envelope, ('key_id', 'algorithm', 'signature'))
    require(envelope['key_id'] == args.trusted_key_id and args.trusted_key_id,
            'unknown trusted key id')
    require(envelope['algorithm'] == 'ECDSA-P256-SHA256', 'unsupported signature algorithm')
    signature = decode_signature(envelope['signature'])
    key = bounded_read(args.public_key, 91)
    require(len(key) == 91 and key.startswith(P256_PREFIX), 'expected canonical P-256 DER SPKI')
    with tempfile.TemporaryDirectory(prefix='tv4-verify-') as directory:
        # Verify the exact bounded bytes read, never re-open mutable input paths.
        root = Path(directory)
        for name, data in [('key.der', key), ('manifest', raw), ('signature.der', signature)]:
            (root / name).write_bytes(data)
        result = subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(root / 'key.der'),
                                 '-keyform', 'DER', '-signature', str(root / 'signature.der'),
                                 str(root / 'manifest')], capture_output=True, timeout=10)
        require(result.returncode == 0, 'manifest signature rejected')
    manifest = parse(raw)  # Only authenticated bytes are interpreted as a manifest.
    keys(manifest, ('schema_version', 'release_version', 'channel', 'source_commit',
                    'signing', 'protocol', 'components', 'release_notes'))
    require(type(manifest['schema_version']) is int and manifest['schema_version'] == 1, 'unsupported schema')
    release = version(manifest['release_version'])
    require(manifest['channel'] in ('stable', 'prerelease'), 'invalid release channel')
    require(manifest['channel'] == args.channel, 'channel mismatch')
    require((release[1] == 1) == (args.channel == 'stable'), 'channel/version mismatch')
    require(isinstance(manifest['source_commit'], str) and re.fullmatch('[0-9a-f]{40}', manifest['source_commit']), 'invalid source commit')
    require(isinstance(manifest['release_notes'], str) and len(manifest['release_notes'].encode('utf-8')) <= 4096, 'invalid changelog')
    keys(manifest['signing'], ('key_id', 'algorithm'))
    require(manifest['signing'] == {k: envelope[k] for k in ('key_id', 'algorithm')}, 'signing metadata mismatch')
    keys(manifest['protocol'], ('min', 'max'))
    local_protocol = version(args.protocol_version)
    require(version(manifest['protocol']['min']) <= local_protocol <= version(manifest['protocol']['max']), 'protocol incompatible')
    components = manifest['components']
    require(isinstance(components, list) and len(components) == 1, 'foundation supports exactly one HMI component')
    app = components[0]
    keys(app, ('name', 'version', 'hardware_id', 'hardware_revision', 'chip', 'profile',
               'layout_id', 'minimum_installed_version', 'minimum_bootloader_version',
               'size_bytes', 'sha256', 'url'), ('compatible_peer_versions', 'installable'))
    require((app['name'], app['hardware_id'], app['hardware_revision'], app['chip'], app['profile']) ==
            ('hmi', 'waveshare-esp32s3-touch-lcd-7', '1.2', 'esp32s3', 'rev12'), 'hardware/profile incompatible')
    require(args.layout_id == 'tv4-16m-ab-v1' and app['layout_id'] == args.layout_id,
            'layout incompatible: factory layout cannot use app OTA')
    app_version = version(app['version'])
    require(len(app['version']) <= 31, 'app descriptor version too long')
    require(args.channel != 'stable' or app_version[1] == 1, 'prerelease app in stable channel')
    current = version(args.installed_version)
    require(version(app['minimum_installed_version']) <= current < app_version, 'update version/replay/downgrade rejected')
    require(version(app['minimum_bootloader_version']) <= version(args.bootloader_version), 'bootloader incompatible')
    if 'compatible_peer_versions' in app:
        peer = app['compatible_peer_versions']
        keys(peer, ('motion_min', 'motion_max'))
        require(args.motion_version is not None, 'motion version unavailable')
        require(version(peer['motion_min']) <= version(args.motion_version) <= version(peer['motion_max']), 'motion incompatible')
    require(type(app.get('installable', False)) is bool, 'installable must be a signed boolean')
    require(type(app['size_bytes']) is int and 0 < app['size_bytes'] <= SLOT_MAX, 'image size rejected')
    require(isinstance(app['sha256'], str) and re.fullmatch('[0-9a-f]{64}', app['sha256']), 'invalid SHA-256')
    require(isinstance(app['url'], str) and len(app['url']) <= 512, 'invalid URL')
    prefix = 'https://github.com/avlyubimov/TV4-CNC/releases/download/v' + manifest['release_version'] + '/'
    asset = 'tv4-hmi-esp32s3-v' + app['version'] + '.bin'
    require(app['url'] == prefix + asset, 'unapproved release URL')
    return dict(release=manifest['release_version'], version=app['version'],
                url=app['url'], notes=manifest['release_notes'],
                size_bytes=app['size_bytes'], sha256=app['sha256'], installable=app.get('installable', False),
                key_sha256=hashlib.sha256(key).hexdigest(),
                source_commit=manifest['source_commit'], channel=manifest['channel'],
                profile=app['profile'], layout_id=app['layout_id'],
                metadata_sha256=hashlib.sha256(raw).hexdigest(),
                protocol=dict(manifest['protocol']),
                compatible_peer_versions=dict(app['compatible_peer_versions'])
                if 'compatible_peer_versions' in app else None)


def verify_with_metadata(args):
    """One authenticated metadata snapshot + full image pass, for local review."""
    candidate = read_metadata(args)
    total = 0
    header = b''
    digest = hashlib.sha256()
    with Path(args.image).open('rb') as stream:
        while chunk := stream.read(4096):
            if not total: header = chunk[:24]
            total += len(chunk)
            require(total <= candidate['size_bytes'], 'image exceeds signed size')
            digest.update(chunk)
    require(total == candidate['size_bytes'] and digest.hexdigest() == candidate['sha256'], 'image hash/length rejected')
    require(len(header) == 24 and header[0] == 0xe9 and header[3] >> 4 == 4
            and struct.unpack_from('<H', header, 12)[0] == 9, 'image header incompatible')
    report = {'status': 'verified-offline-only', 'schema_version': 1, 'image_bytes': total,
            'sha256': digest.hexdigest(), 'key_sha256': candidate['key_sha256'],
            'device_access': False, 'install_enabled': False}
    return candidate, report


def verify(args):
    # Preserve the existing public CLI/report; no second metadata verification.
    return verify_with_metadata(args)[1]


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('manifest', 'signature', 'public-key', 'trusted-key-id', 'image',
                 'installed-version', 'bootloader-version', 'protocol-version', 'layout-id'):
        p.add_argument('--' + name, required=True)
    p.add_argument('--channel', choices=('stable', 'prerelease'), default='stable')
    p.add_argument('--motion-version')
    return p


def main():
    args = parser().parse_args()
    try:
        report = verify(args)
    except (Rejected, ValueError, TypeError, KeyError, RecursionError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit('OTA candidate rejected: ' + str(error)) from None
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
