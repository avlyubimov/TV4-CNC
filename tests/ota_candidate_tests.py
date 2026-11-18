import argparse
import base64
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('candidate', ROOT / 'tools/verify_ota_candidate.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


class CandidateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tv4-ota-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.private = self.root / 'ephemeral-private.pem'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'EC', '-pkeyopt',
                        'ec_paramgen_curve:prime256v1', '-out', str(self.private)],
                       check=True, capture_output=True)
        self.pub = self.root / 'public.der'
        subprocess.run(['openssl', 'pkey', '-in', str(self.private), '-pubout', '-outform',
                        'DER', '-out', str(self.pub)], check=True, capture_output=True)
        image = bytearray(512)
        image[0] = 0xe9
        image[3] = 0x40
        image[12] = 9
        self.image = self.root / 'image.bin'
        self.image.write_bytes(image)
        self.manifest = {
            'schema_version': 1, 'release_version': '0.2.0', 'channel': 'stable',
            'source_commit': 'a' * 40, 'release_notes': 'Test only, not a release',
            'signing': {'key_id': 'ephemeral-test', 'algorithm': 'ECDSA-P256-SHA256'},
            'protocol': {'min': '1.0.0', 'max': '1.0.0'},
            'components': [{'name': 'hmi', 'version': '0.2.0',
                'hardware_id': 'waveshare-esp32s3-touch-lcd-7', 'hardware_revision': '1.2',
                'chip': 'esp32s3', 'profile': 'rev12', 'layout_id': 'tv4-16m-ab-v1',
                'minimum_installed_version': '0.1.0', 'minimum_bootloader_version': '0.1.0',
                'size_bytes': len(image), 'sha256': hashlib.sha256(image).hexdigest(),
                'url': 'https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/tv4-hmi-esp32s3-v0.2.0.bin'}]}
        self.args = argparse.Namespace(manifest=self.root / 'manifest.json',
            signature=self.root / 'manifest.sig', public_key=self.pub,
            trusted_key_id='ephemeral-test', image=self.image, installed_version='0.1.0',
            bootloader_version='0.1.0', protocol_version='1.0.0', layout_id='tv4-16m-ab-v1',
            motion_version=None, channel='stable')

    def sign(self, raw=None):
        if raw is None:
            raw = json.dumps(self.manifest, separators=(',', ':')).encode()
        self.args.manifest.write_bytes(raw)
        signature = subprocess.check_output(['openssl', 'dgst', '-sha256', '-sign',
                                             str(self.private), str(self.args.manifest)])
        self.args.signature.write_text(json.dumps({'key_id': 'ephemeral-test',
            'algorithm': 'ECDSA-P256-SHA256', 'signature': base64.b64encode(signature).decode()}))

    def test_valid_offline_only(self):
        self.sign()
        result = ota.verify(self.args)
        self.assertFalse(result['install_enabled'])
        self.assertFalse(result['device_access'])

    def test_signature_changed_bytes_and_unknown_key(self):
        self.sign()
        self.args.manifest.write_bytes(self.args.manifest.read_bytes() + b'\n')
        with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.sign()
        self.args.trusted_key_id = 'unprovisioned'
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_signed_schema_and_compatibility_rejections(self):
        original = copy.deepcopy(self.manifest)
        changes = [('hardware_revision', '1.1'), ('profile', 'headless'), ('layout_id', 'factory'),
                   ('size_bytes', True), ('size_bytes', 0x300001), ('sha256', 'BAD'),
                   ('url', 'https://github.com.evil.invalid/asset.bin'),
                   ('version', '0.1.0'), ('version', '0.01.3'),
                   ('minimum_installed_version', '2.0.0'), ('minimum_bootloader_version', '2.0.0')]
        for key, value in changes:
            with self.subTest(key=key, value=value):
                self.manifest = copy.deepcopy(original)
                self.manifest['components'][0][key] = value
                self.sign()
                with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.manifest = copy.deepcopy(original)
        self.manifest['components'][0]['compatible_peer_versions'] = {'motion_min': '1.0.0', 'motion_max': '2.0.0'}
        self.sign()
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_protocol_schema_and_json(self):
        self.manifest['protocol']['min'] = '2.0.0'
        self.sign()
        with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.manifest['protocol']['min'] = '1.0.0'
        self.sign(b'{"schema_version":1,"schema_version":1}')
        with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.sign(b'{"schema_version":NaN}')
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_image_corrupt_truncated_oversized(self):
        self.sign()
        image = self.image.read_bytes()
        for changed in (image[:-1], image + b'x', image[:-1] + b'x'):
            with self.subTest(length=len(changed)):
                self.image.write_bytes(changed)
                with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.image.write_bytes(b'x' * 512)
        self.manifest['components'][0]['sha256'] = hashlib.sha256(self.image.read_bytes()).hexdigest()
        self.sign()
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_limits_current_layout_channel(self):
        self.sign()
        self.args.layout_id = 'factory'
        with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.args.layout_id = 'tv4-16m-ab-v1'
        self.args.channel = 'prerelease'
        with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.args.manifest.write_bytes(b' ' * (ota.MANIFEST_MAX + 1))
        with self.assertRaises(ota.Rejected): ota.verify(self.args)
        self.args.signature.write_bytes(b' ' * (ota.ENVELOPE_MAX + 1))
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_envelope_and_missing_fields(self):
        self.sign()
        original = json.loads(self.args.signature.read_text())
        for envelope in [dict(original, algorithm='none'), dict(original, signature='!bad!'),
                         dict(original, public_key='downloaded-key'), {'key_id': 'ephemeral-test'}]:
            with self.subTest(envelope=sorted(envelope)):
                self.args.signature.write_text(json.dumps(envelope))
                with self.assertRaises(ValueError): ota.verify(self.args)
        self.manifest['unsupported'] = True
        self.sign()
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_valid_prerelease_and_stable_rejects_prerelease_app(self):
        self.manifest['release_version'] = '0.2.0-rc.1'
        self.manifest['channel'] = 'prerelease'
        self.manifest['components'][0]['version'] = '0.2.0-rc.1'
        self.manifest['components'][0]['url'] = 'https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0-rc.1/tv4-hmi-esp32s3-v0.2.0-rc.1.bin'
        self.args.channel = 'prerelease'
        self.sign()
        self.assertFalse(ota.verify(self.args)['install_enabled'])
        self.manifest['release_version'] = '0.2.0'
        self.manifest['channel'] = 'stable'
        self.args.channel = 'stable'
        self.sign()
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_native_bounds_nul_and_local_key_id(self):
        self.manifest['release_notes'] = '\x00'
        self.sign()
        with self.assertRaises(ValueError): ota.verify(self.args)
        self.manifest['release_notes'] = 'test'
        key_id = 'т' * 33  # 66 UTF-8 bytes, above native 64-byte bound.
        self.manifest['signing']['key_id'] = key_id
        self.args.trusted_key_id = key_id
        self.sign()
        envelope = json.loads(self.args.signature.read_text())
        envelope['key_id'] = key_id
        self.args.signature.write_text(json.dumps(envelope))
        with self.assertRaises(ota.Rejected): ota.verify(self.args)

    def test_semver(self):
        for a, b in [('1.0.0-alpha', '1.0.0-alpha.1'), ('1.0.0-2', '1.0.0-11'),
                     ('1.0.0-rc.1', '1.0.0'), ('1.9.0', '1.10.0')]:
            self.assertLess(ota.version(a), ota.version(b))
        for bad in ['1.0', '1.0.0-01', '1.0.0+unreviewed', '01.0.0', '4294967296.0.0']:
            with self.assertRaises(ota.Rejected): ota.version(bad)


if __name__ == '__main__':
    unittest.main()
