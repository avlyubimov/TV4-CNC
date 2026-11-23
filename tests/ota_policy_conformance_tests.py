"""Independent offline/native signed metadata and bounded SemVer conformance.

No firmware execution, devices, network, private-key files or release outputs.
The host C helper holds a fresh P-256 key in memory; Python only sees public
DER/signatures and invokes the real offline OpenSSL verification path.
"""
import argparse
import base64
import copy
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('offline', ROOT / 'tools/verify_ota_candidate.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


def dump(value):
    return json.dumps(value, ensure_ascii=True, separators=(',', ':')).encode()


def noncanonical(text):
    alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    if not text.endswith('='):
        return text + '===='
    index = len(text.rstrip('=')) - 1
    # Last sextet's unused bit was zero in the canonical encoding.
    return text[:index] + alphabet[alphabet.index(text[index]) | 1] + text[index + 1:]


class Harness:
    def __init__(self, driver, root):
        self.p = subprocess.Popen([driver], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True, bufsize=1)
        self.pub = bytes.fromhex(json.loads(self.p.stdout.readline())['public_key'])
        self.root = root
        self.args = argparse.Namespace(manifest=root / 'manifest.json', signature=root / 'manifest.sig',
            public_key=root / 'public.der', trusted_key_id='test-only', image=root / 'image.bin',
            installed_version='0.1.0', bootloader_version='0.1.0', protocol_version='1.0.0',
            layout_id='tv4-16m-ab-v1', motion_version=None, channel='stable')
        self.count = self.accepted = self.versions = self.encodings = 0

    def request(self, **request):
        self.p.stdin.write(dump(request).decode() + '\n')
        self.p.stdin.flush()
        response = self.p.stdout.readline()
        assert response, 'native test driver exited; inspect sanitizer/CTest log'
        return json.loads(response)

    def close(self):
        self.p.stdin.close()
        try:
            code = self.p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
            raise
        errors = self.p.stderr.read()
        self.p.stdout.close()
        self.p.stderr.close()
        assert code == 0 and not errors, f'native driver failure ({code}): {errors}'

    def check(self, name, manifest, expected, *, context=None, raw=None,
              envelope=None, mutate_signature=None, tamper=False, pub=None):
        local = dict(key_id='test-only', installed_version='0.1.0', bootloader_version='0.1.0',
                     protocol_version='1.0.0', motion_version=None, layout_id='tv4-16m-ab-v1',
                     prerelease=False, board_rev12=True)
        local.update(context or {})
        raw = dump(manifest) if raw is None else raw
        signature = self.request(op='sign', raw=raw.hex())['signature']
        if mutate_signature is noncanonical:
            # DER ECDSA lengths vary. Exercise unused pad bits on a real valid
            # signature, not merely an encoding that was already invalid.
            for _ in range(64):
                if signature.endswith('='):
                    break
                signature = self.request(op='sign', raw=raw.hex())['signature']
            assert signature.endswith('='), 'bounded test signature generation exhausted'
        if mutate_signature:
            signature = mutate_signature(signature)
        en = dump(dict(key_id='test-only', algorithm='ECDSA-P256-SHA256', signature=signature))
        if envelope:
            en = envelope(en)
        if tamper:
            raw += b' '
        public = self.pub if pub is None else pub
        native = self.request(op='verify', raw=raw.hex(), envelope=en.hex(), context=local,
                              public_key=public.hex())
        self.args.manifest.write_bytes(raw)
        self.args.signature.write_bytes(en)
        self.args.public_key.write_bytes(public)
        self.args.trusted_key_id = local['key_id']
        self.args.channel = 'prerelease' if local['prerelease'] else 'stable'
        for field in ('installed_version', 'bootloader_version', 'protocol_version',
                      'motion_version', 'layout_id'):
            setattr(self.args, field, local[field])
        try:
            candidate = ota.read_metadata(self.args)
        except (ValueError, TypeError, KeyError, RecursionError, OSError, subprocess.SubprocessError):
            candidate = None
        assert (native['result'] == 0) == expected, (name, 'native', native)
        assert (candidate is not None) == expected, (name, 'offline', candidate)
        if expected:
            for field in ('release', 'version', 'url', 'notes', 'size_bytes', 'sha256', 'installable'):
                assert native[field] == candidate[field], (name, field)
            # CLI still performs the extra complete image/hash/header layer.
            # Cases with intentionally varied size/SHA below are metadata-only.
            if candidate['size_bytes'] == 512 and candidate['sha256'] == IMAGE_SHA:
                assert ota.verify(self.args)['install_enabled'] is False
            self.accepted += 1
        else:
            assert native['output_zero'] is True
        self.count += 1

    def semver(self):
        # Official precedence chain, then generated independently compared pairs.
        ordered = ['1.0.0-alpha', '1.0.0-alpha.1', '1.0.0-alpha.beta', '1.0.0-beta',
                   '1.0.0-beta.2', '1.0.0-beta.11', '1.0.0-rc.1', '1.0.0']
        for i, a in enumerate(ordered):
            for j, b in enumerate(ordered):
                order = (i > j) - (i < j)
                native = self.request(op='compare', a=a, b=b)
                assert native == dict(ok=True, order=order)
                assert (ota.version(a) > ota.version(b)) - (ota.version(a) < ota.version(b)) == order
                self.versions += 1
        versions = set(ordered + ['0.0.0', '1.9.0', '1.10.0', '4294967295.0.0',
            '0.4294967295.0', '0.0.4294967295', '1.0.0-' + '9' * 58, '1.0.0-' + 'a' * 58])
        for core, pre in itertools.product(['0.0.0', '1.0.0', '2.1.3'],
                ['0', '2', '11', 'A', 'a', '-', '--', '01a', '0.1', 'a.0', 'a.b']):
            versions.add(core + '-' + pre)
        for a, b in itertools.product(sorted(versions), repeat=2):
            av, bv = ota.version(a), ota.version(b)
            expected = (av > bv) - (av < bv)
            assert self.request(op='compare', a=a, b=b) == dict(ok=True, order=expected), (a, b)
            self.versions += 1
        invalid = [None, '', '1', '1.0', '1.0.0.0', '01.0.0', '1.01.0', '1.0.01',
            '4294967296.0.0', '0.4294967296.0', '0.0.4294967296', '1.0.0-',
            '1.0.0-01', '1.0.0-a..b', '1.0.0-a.', '1.0.0+a', '1.0.0-a+b',
            '1.0.0-' + 'a' * 59, '1.0.0-ё', '1.0.0-１２', ' 1.0.0', '1.0.0\n']
        invalid += ['1.0.0-' + chr(c) for c in range(1, 128)
                    if not (chr(c).isascii() and (chr(c).isalnum() or chr(c) == '-'))]
        for bad, other in itertools.product(invalid, ['0.0.0', '1.0.0-a']):
            for a, b in ((bad, other), (other, bad)):
                assert not self.request(op='compare', a=a, b=b)['ok'], (a, b)
                try:
                    ota.version(bad)
                except ota.Rejected:
                    pass
                else:
                    raise AssertionError(('invalid version accepted offline', bad))
                self.versions += 1

    def base64(self):
        for size in range(8, 73):
            raw = bytes(range(size))
            good = base64.b64encode(raw).decode()
            assert ota.decode_signature(good) == raw
            bad = noncanonical(good)
            if good.endswith('='):
                assert base64.b64decode(bad, validate=True) == raw  # stdlib allows nonzero pad bits
            try:
                ota.decode_signature(bad)
            except ValueError:
                pass
            else:
                raise AssertionError(('noncanonical signature', size))
            self.encodings += 2


IMAGE = bytearray(512)
IMAGE[0], IMAGE[3], IMAGE[12] = 0xe9, 0x40, 9
IMAGE_SHA = hashlib.sha256(IMAGE).hexdigest()


def corpus(h):
    base = dict(schema_version=1, release_version='0.2.0', channel='stable', source_commit='a' * 40,
        signing=dict(key_id='test-only', algorithm='ECDSA-P256-SHA256'),
        protocol=dict(min='1.0.0', max='1.0.0'), release_notes='Проверка ё Ё\n🚀',
        components=[dict(name='hmi', version='0.2.0', hardware_id='waveshare-esp32s3-touch-lcd-7',
            hardware_revision='1.2', chip='esp32s3', profile='rev12', layout_id='tv4-16m-ab-v1',
            minimum_installed_version='0.1.0', minimum_bootloader_version='0.1.0',
            size_bytes=512, sha256=IMAGE_SHA)])

    def candidate(release='0.2.0', app='0.2.0', channel='stable'):
        m = copy.deepcopy(base)
        m.update(release_version=release, channel=channel)
        m['components'][0].update(version=app,
            url=f'https://github.com/avlyubimov/TV4-CNC/releases/download/v{release}/tv4-hmi-esp32s3-v{app}.bin')
        return m

    h.check('valid-stable', candidate(), True)
    for value in (False, True, 0, 1, 'true', None, [], {}):
        m = candidate(); m['components'][0]['installable'] = value
        h.check('installable-' + repr(value), m, type(value) is bool)
    m = candidate(); m['components'][0].update(installable=True,
        compatible_peer_versions=dict(motion_min='1.0.0', motion_max='2.0.0'))
    h.check('installable-and-peer', m, True, context=dict(motion_version='1.0.0'))
    raw = dump(candidate()).replace(b'"size_bytes":512', b'"installable":true,"installable":false,"size_bytes":512')
    h.check('duplicate-installable', candidate(), False, raw=raw)

    h.check('independent-release-and-component', candidate('4.0.0', '0.2.0'), True)
    h.check('prerelease', candidate('0.2.0-rc.1', '0.2.0-rc.1', 'prerelease'), True,
            context=dict(prerelease=True))
    h.check('stable-component-in-prerelease-bundle', candidate('4.0.0-rc.1', '0.2.0', 'prerelease'),
            True, context=dict(prerelease=True))
    h.check('stable-rejects-prerelease-component', candidate(app='0.2.0-rc.1'), False)
    h.check('release-channel-mismatch', candidate(channel='prerelease'), False, context=dict(prerelease=True))
    h.check('local-channel-mismatch', candidate(), False, context=dict(prerelease=True))
    h.check('valid-signature-changed-raw', candidate(), False, tamper=True)
    h.check('noncanonical-signed-envelope', candidate(), False, mutate_signature=noncanonical)
    h.check('changed-signature', candidate(), False,
            mutate_signature=lambda s: ('A' if s[0] != 'A' else 'B') + s[1:])
    for field in ('key_id', 'installed_version', 'bootloader_version', 'protocol_version', 'layout_id'):
        for missing in (None, '', 'unknown'):
            h.check(f'local-{field}-{missing}', candidate(), False, context={field: missing})
    for current in ('0.2.0', '0.3.0', '4294967295.0.0'):
        h.check('replay-downgrade-' + current, candidate(), False, context=dict(installed_version=current))
    for name, change in [('schema', dict(schema_version=2)), ('channel', dict(channel='nightly')),
            ('source-commit', dict(source_commit='A' * 40)), ('unknown-root', dict(extra=1)),
            ('missing-components', dict(components=[])),
            ('component-not-object', dict(components=['hmi'])), ('notes-type', dict(release_notes=1)),
            ('notes-UTF8-limit', dict(release_notes='ё' * 2049)), ('notes-NUL', dict(release_notes='ok\x00hidden'))]:
        m = candidate(); m.update(change); h.check(name, m, False)
    for key, value in [('name', 'motion'), ('hardware_revision', '1.1'), ('chip', 'stm32f407'),
            ('hardware_id', 'generic-f407'), ('profile', 'headless'), ('layout_id', 'factory'),
            ('minimum_installed_version', '0.3.0'), ('minimum_bootloader_version', '0.2.0'),
            ('size_bytes', 0), ('size_bytes', 0x300001), ('size_bytes', True),
            ('sha256', 'A' * 64), ('url', 'https://github.com.evil.invalid/asset.bin'),
            ('url', 'https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/../app.bin'),
            ('url', candidate()['components'][0]['url'] + '?token=x')]:
        m = candidate(); m['components'][0][key] = value; h.check(f'component-{key}-{value}', m, False)
    for version in ('1.0.0-' + 'a' * 25, '1.0.0-' + 'a' * 26, '1.0.0+build', '01.2.0', '1.0.0-01'):
        h.check('app-version-bound-' + version, candidate(app=version, release='2.0.0-rc', channel='prerelease'),
                len(version) == 31, context=dict(prerelease=True))
    for values, current, expected in [({'min':'1.0.0','max':'2.0.0'}, '1.0.0', True),
            ({'min':'1.0.0','max':'2.0.0'}, '2.0.0', True),
            ({'min':'2.0.0','max':'1.0.0'}, '1.5.0', False),
            ({'min':'1.0.0','max':'2.0.0'}, '1.0.0-rc', False),
            ({'min':'1.0.0','max':'unknown'}, '1.0.0', False)]:
        m = candidate(); m['protocol'] = values
        h.check('protocol-range-' + current + str(values), m, expected, context=dict(protocol_version=current))
    m = candidate()
    m['components'][0]['compatible_peer_versions'] = dict(motion_min='1.0.0', motion_max='2.0.0')
    for peer, expected in ((None, False), ('unknown', False), ('0.9.0', False), ('1.0.0', True),
                            ('2.0.0', True), ('2.0.1', False)):
        h.check('peer-' + str(peer), m, expected, context=dict(motion_version=peer))
    m = candidate(); m['components'].append(copy.deepcopy(m['components'][0]))
    h.check('duplicate-component', m, False)
    h.check('motion-draft-domain', dict(schema='tv4-motion-admission-draft-1', component='motion'), False)
    for size in (1, 0x300000):
        m = candidate(); m['components'][0]['size_bytes'] = size
        h.check('metadata-size-bound-' + str(size), m, True)
    raw = dump(candidate())
    for slot, char in itertools.product(range(4), ['g', 'Z', '/', '-', ' ']):
        escape = list('1234'); escape[slot] = char
        changed = raw.replace(b'"chip":"esp32s3"', b'"chip":"esp32s3\\u' + ''.join(escape).encode() + b'hidden"')
        assert changed != raw
        h.check(f'invalid-escape-{slot}-{char}', candidate(), False, raw=changed)
    for old, new, expected in [(b'"schema_version":1', b'"schema_version":1,"schema_versi\\u006fn":1', False),
            (b'"schema_version":1', b'"schema_version":1.0', False),
            (b'"schema_version":1', b'"schema_version":1e0', False),
            (b'"chip":"esp32s3"', b'"chip":"esp32s3\\u0000hidden"', False),
            (b'"chip":"esp32s3"', b'"chip":"esp32s\\u0033"', True)]:
        h.check('raw-json-' + str(new), candidate(), expected, raw=raw.replace(old, new))
    h.check('valid-UTF8', candidate(), True, raw=json.dumps(candidate(), ensure_ascii=False).encode())
    h.check('trailing-whitespace', candidate(), True, raw=raw + b'\r\n\t ')
    h.check('trailing-document', candidate(), False, raw=raw + b'{}')
    h.check('invalid-UTF8', candidate(), False, raw=raw.replace(b'"chip":"esp32s3"', b'"chip":"\xc0\x80"'))
    for n, expected in ((16384, True), (16385, False)):
        h.check('manifest-bound-' + str(n), candidate(), expected, raw=raw + b' ' * (n - len(raw)))
    for name, mutate in [('unknown-key', lambda e: e.replace(b'test-only', b'untrusted')),
            ('wrong-algorithm', lambda e: e.replace(b'ECDSA-P256-SHA256', b'none')),
            ('duplicate-key', lambda e: e.replace(b'{', b'{"key_id":"test-only",', 1)),
            ('unknown-envelope-field', lambda e: e.replace(b'{', b'{"public_key":"untrusted",', 1)),
            ('invalid-escape-envelope', lambda e: e.replace(b'test-only', b'test-only\\u00g0hidden'))]:
        h.check('envelope-' + name, candidate(), False, envelope=mutate)
    for n, expected in ((2048, True), (2049, False)):
        h.check('envelope-bound-' + str(n), candidate(), expected,
                envelope=lambda e: e + b' ' * (n - len(e)))
    for public in (b'', h.pub[:-1], h.pub + b'\x00', b'\x00' * 91,
                   h.pub[:27] + b'\x00' * 64):
        h.check('invalid-key-' + str(len(public)), candidate(), False, pub=public)


def main():
    p = argparse.ArgumentParser(); p.add_argument('--driver', required=True)
    args = p.parse_args()
    with tempfile.TemporaryDirectory(prefix='tv4-policy-conformance-') as directory:
        root = Path(directory)
        h = Harness(args.driver, root)
        (root / 'image.bin').write_bytes(IMAGE)
        try:
            h.semver()
            h.base64()
            corpus(h)
        finally:
            h.close()
        print(f'Conformance PASS: signed metadata={h.count}, accepted={h.accepted}, '
              f'rejected={h.count-h.accepted}; SemVer comparisons={h.versions}; Base64 cases={h.encodings}')
        print('Native candidate fields equal offline policy; rejected native output entirely zero. '
              'Ephemeral private key stayed in C helper memory; no device/install/release.')


if __name__ == '__main__':
    main()
