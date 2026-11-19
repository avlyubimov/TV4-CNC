"""Offline candidate pipeline fault tests. Synthetic images cannot run on a board."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_candidate as candidate
from esp_app_image import inspect_app


def image(version, sdk='6.1.1', elf=b'non-executable test ELF'):
    header = bytearray(24)
    header[:4] = b'\xe9\x01\x02\x4f'
    struct.pack_into('<H', header, 12, 9)
    struct.pack_into('<H', header, 17, 0xffff)
    header[23] = 1
    descriptor = bytearray(256)
    struct.pack_into('<I', descriptor, 0, 0xabcd5432)
    for offset, text in ((16, version), (48, 'tv4_hmi'), (112, 'v' + sdk)):
        descriptor[offset:offset + len(text)] = text.encode()
    descriptor[144:176] = hashlib.sha256(elf).digest()
    body = header + struct.pack('<II', 0x3c000020, len(descriptor)) + descriptor
    checksum = 0xef
    for byte in descriptor:
        checksum ^= byte
    body += b'\0' * (15 - len(body) % 16) + bytes([checksum])
    return bytes(body) + hashlib.sha256(body).digest()


def rehash(raw, checksum=True):
    data = bytearray(raw[:-32])
    if checksum:
        value = 0xef
        for byte in data[32:288]:
            value ^= byte
        data[-1] = value
    return bytes(data) + hashlib.sha256(data).digest()


class CandidateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tv4-build-review-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        (self.repo / '.gitignore').write_text('build*/\n')
        (self.repo / 'LICENSE-NOTICE.md').write_bytes((ROOT / 'LICENSE-NOTICE.md').read_bytes())
        locks = self.repo / 'firmware/esp32-hmi/locks'
        locks.mkdir(parents=True)
        for sdk in candidate.SDK_IMAGES:
            (locks / ('idf-' + sdk + '.lock')).write_bytes(
                (ROOT / 'firmware/esp32-hmi/locks' / ('idf-' + sdk + '.lock')).read_bytes())
        self.git('init', '-q')
        self.git('add', '.')
        self.git('-c', 'user.name=TV4 test', '-c', 'user.email=test@invalid',
                 '-c', 'commit.gpgsign=false', 'commit', '-qm', 'Synthetic source only')
        self.sha = self.git('rev-parse', 'HEAD')
        self.version = self.git('describe', '--always', '--dirty')
        self.build = self.repo / 'build-fixture'
        for directory in ('config', 'partition_table', 'bootloader'):
            (self.build / directory).mkdir(parents=True, exist_ok=True)
        self.output = self.root / 'review'
        self.sdk, self.profile = '6.1.1', 'headless'
        self.configure()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args],
                                       text=True, stderr=subprocess.PIPE).strip()

    def configure(self):
        tls = self.profile == 'rev12-tls-bench'
        config = dict(ESPTOOLPY_FLASHSIZE='16MB', ESPTOOLPY_FLASHFREQ='80m', LV_COLOR_DEPTH=16,
                      FREERTOS_HZ=1000, ESP_DEFAULT_CPU_FREQ_MHZ=240,
                      TV4_BOARD_REV12=self.profile != 'headless',
                      TV4_OTA_MEMORY_BENCH=self.profile == 'rev12-memory-bench', TV4_UTC_TLS_BENCH=tls,
                      MBEDTLS_INTERNAL_MEM_ALLOC=not tls, MBEDTLS_EXTERNAL_MEM_ALLOC=tls)
        if self.profile != 'headless':
            config.update(SPIRAM=True, SPIRAM_MODE_OCT=True, SPIRAM_SPEED=80, SPIRAM_USE_MALLOC=True,
                          SPIRAM_MALLOC_ALWAYSINTERNAL=16384, SPIRAM_MALLOC_RESERVE_INTERNAL=65536)
        if tls:
            config.update(MBEDTLS_CERTIFICATE_BUNDLE=True, MBEDTLS_HAVE_TIME=True, MBEDTLS_HAVE_TIME_DATE=True,
                          MBEDTLS_ASYMMETRIC_CONTENT_LEN=True, MBEDTLS_SSL_IN_CONTENT_LEN=16384,
                          MBEDTLS_SSL_OUT_CONTENT_LEN=4096)
        (self.build / 'config/sdkconfig.json').write_text(json.dumps(config))
        (self.build / 'sdkconfig').write_text('# Synthetic config, not board settings\n')
        (self.build / 'project_description.json').write_text(json.dumps(dict(project_name='tv4_hmi',
            project_version=self.version, target='esp32s3', git_revision='v' + self.sdk,
            project_path='/private/path/never-export-this', build_dir='/private/build')))
        elf = b'non-executable test ELF'
        (self.build / 'tv4_hmi.elf').write_bytes(elf)
        self.app = image(self.version, self.sdk, elf)
        for name in ('tv4_hmi.bin', 'bootloader/bootloader.bin'):
            (self.build / name).write_bytes(self.app)
        partitions = b''.join(struct.pack('<HBBII16sI', 0x50aa, kind, subtype, address, size, label, 0)
            for kind, subtype, address, size, label in ((1, 2, 0x9000, 0x6000, b'nvs'),
                (1, 1, 0xf000, 0x1000, b'phy_init'), (0, 0, 0x10000, 0x200000, b'factory')))
        (self.build / 'partition_table/partition-table.bin').write_bytes(partitions)
        self.validation = candidate.check_idf_build.verify(self.build, self.profile, self.sdk, write_report=False)
        (self.build / 'validation.json').write_bytes(candidate.encoded(self.validation))

    def create(self, output=None):
        return candidate.create(self.repo, self.build, output or self.output, self.sdk, self.profile, self.sha)

    def verify(self, image_path=None):
        return candidate.verify(self.output, self.sha, self.sdk, self.profile, image_path)

    def mutate(self, change):
        record = json.loads((self.output / 'candidate.json').read_bytes())
        change(record)
        (self.output / 'candidate.json').write_bytes(candidate.encoded(record))
        files = {p.name: p.read_bytes() for p in self.output.iterdir() if p.name != 'checksums.sha256'}
        (self.output / 'checksums.sha256').write_bytes(candidate.inventory(files))

    def test_full_matrix_deterministic_bundle_no_binary_or_private_paths(self):
        for sdk in candidate.SDK_IMAGES:
            for profile in candidate.PROFILES:
                with self.subTest(sdk=sdk, profile=profile):
                    self.sdk, self.profile = sdk, profile
                    self.configure()
                    self.output = self.root / (sdk + '-' + profile)
                    self.create()
                    result = self.verify(self.build / 'tv4_hmi.bin')
                    self.assertTrue(result['image_checked'])
                    self.assertFalse(result['install_enabled'])
                    self.assertFalse(result['authenticity_verified'])
                    self.assertEqual({p.name for p in self.output.iterdir()}, candidate.FILES)
                    other = self.root / (sdk + '-' + profile + '-repeat')
                    self.create(other)
                    for name in candidate.FILES:
                        self.assertEqual((other / name).read_bytes(), (self.output / name).read_bytes())
                        self.assertNotIn(b'/private/', (other / name).read_bytes())
                    self.assertFalse(self.verify()['image_checked'])

    def test_source_mismatch_dirty_and_untracked(self):
        with self.assertRaisesRegex(ValueError, 'source commit mismatch'):
            candidate.create(self.repo, self.build, self.output, self.sdk, self.profile, 'a' * 40)
        (self.repo / 'LICENSE-NOTICE.md').write_text('dirty')
        with self.assertRaisesRegex(ValueError, 'must be clean'):
            self.create()
        self.git('restore', 'LICENSE-NOTICE.md')
        (self.repo / 'not-reviewed.txt').write_text('untracked')
        with self.assertRaisesRegex(ValueError, 'must be clean'):
            self.create()
        self.assertFalse(self.output.exists())

    def test_stale_description_validation_elf_or_version(self):
        for name, replacement, message in (
                ('tv4_hmi.elf', b'another ELF', 'app/ELF'),
                ('validation.json', b'{}', 'stale build'),
                ('project_description.json', b'{"target":"esp32s3","git_revision":"v6.1.1","project_name":"other"}', 'project version')):
            with self.subTest(name=name):
                original = (self.build / name).read_bytes()
                (self.build / name).write_bytes(replacement)
                with self.assertRaisesRegex(ValueError, message):
                    self.create()
                (self.build / name).write_bytes(original)
        (self.build / 'tv4_hmi.bin').write_bytes(image('older-build'))
        self.validation['images']['tv4_hmi.bin'] = candidate.digest((self.build / 'tv4_hmi.bin').read_bytes())
        (self.build / 'validation.json').write_bytes(candidate.encoded(self.validation))
        with self.assertRaisesRegex(ValueError, 'stale app'):
            self.create()
        self.assertFalse(self.output.exists())

    def test_wrong_profile_and_layout(self):
        with self.assertRaises(ValueError):
            candidate.create(self.repo, self.build, self.output, self.sdk, 'rev12', self.sha)
        raw = bytearray((self.build / 'partition_table/partition-table.bin').read_bytes())
        struct.pack_into('<I', raw, 64 + 8, 0x100000)
        (self.build / 'partition_table/partition-table.bin').write_bytes(raw)
        report = candidate.check_idf_build.verify(self.build, self.profile, self.sdk, write_report=False)
        (self.build / 'validation.json').write_bytes(candidate.encoded(report))
        with self.assertRaisesRegex(ValueError, 'factory layout'):
            self.create()

    def test_repeat_output_preserves_existing_bundle(self):
        self.create()
        before = (self.output / 'candidate.json').read_bytes()
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.create()
        self.assertEqual(before, (self.output / 'candidate.json').read_bytes())

    def test_inventory_damage_and_unexpected_files(self):
        self.create()
        path = self.output / 'LICENSE-NOTICE.md'
        original = path.read_bytes()
        path.write_bytes(original + b'!')
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            self.verify()
        path.write_bytes(original)
        for name in ('app.bin', 'private.key', 'nested'):
            with self.subTest(name=name):
                (self.output / name).write_text('must not be uploaded')
                with self.assertRaisesRegex(ValueError, 'unexpected/missing'):
                    self.verify()
                (self.output / name).unlink()

    def test_verify_expected_source_profile_and_immutable_gates(self):
        self.create()
        for sha, sdk, profile in (('b' * 40, self.sdk, self.profile),
                                   (self.sha, '5.3.2', self.profile), (self.sha, self.sdk, 'rev12')):
            with self.subTest(sha=sha, sdk=sdk, profile=profile), self.assertRaises(ValueError):
                candidate.verify(self.output, sha, sdk, profile)
        original = (self.output / 'candidate.json').read_bytes()
        mutations = [lambda r: r['policy'].update(install_enabled=True),
                     lambda r: r['policy'].update(device_access=0),
                     lambda r: r['layout'].update(ota_slots=2),
                     lambda r: r.update(kind='signed-release'),
                     lambda r: r.update(release_gates=[]),
                     lambda r: r['application'].update(elf_sha256='f' * 64),
                     lambda r: r.update(source_tree='short'),
                     lambda r: r.update(files={'../../private': {'bytes': 1, 'sha256': 'a' * 64}})]
        for mutation in mutations:
            (self.output / 'candidate.json').write_bytes(original)
            self.mutate(mutation)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.verify()

    def test_strict_json_bounds_and_nonregular_inputs(self):
        self.create()
        self.assertRaises(ValueError, candidate.strict_json, b'{"x":1,"x":2}')
        self.assertRaises(ValueError, candidate.strict_json, b'{"x":NaN}')
        self.assertRaises(ValueError, candidate.strict_json, b'{"x": Infinity}')
        self.assertRaises(ValueError, candidate.strict_json, b'\xef\xbb\xbf{}')
        path = self.output / 'validation.json'
        path.write_bytes(b' ' * (candidate.META_MAX + 1))
        with self.assertRaisesRegex(ValueError, 'oversized'):
            self.verify()
        path.unlink()
        path.symlink_to(self.build / 'validation.json')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.verify()
        path.unlink()
        os.mkfifo(path)
        with self.assertRaisesRegex(ValueError, 'regular'):
            self.verify()
        path.unlink()
        path.mkdir()
        with self.assertRaisesRegex(ValueError, 'regular'):
            self.verify()

    def test_image_corruption_truncation_extra_bytes_and_wrong_sdk(self):
        self.create()
        path = self.build / 'other.bin'
        for raw in (self.app[:-1], self.app + b'x', self.app[:-1] + b'x',
                    image('other-version'), image(self.version, '5.3.2')):
            with self.subTest(length=len(raw)):
                path.write_bytes(raw)
                with self.assertRaises(ValueError):
                    self.verify(path)

    def test_structural_image_errors_even_with_recomputed_digest(self):
        for offset, byte in ((0, 0), (1, 0), (1, 17), (2, 0), (3, 0x3f), (12, 0),
                             (23, 0), (32, 0), (36, 1), (48, 0), (80, ord('X')),
                             (144, ord('X')), (288, 1)):
            with self.subTest(offset=offset, byte=byte):
                raw = bytearray(self.app)
                raw[offset] = byte
                with self.assertRaises(ValueError):
                    inspect_app(rehash(raw), self.sdk)
        for length in (0, 255, 0xffffffff):
            raw = bytearray(self.app)
            struct.pack_into('<I', raw, 28, length)
            with self.subTest(length=length), self.assertRaises(ValueError):
                inspect_app(rehash(raw), self.sdk)
        raw = bytearray(self.app)
        raw[-33] ^= 1
        with self.assertRaisesRegex(ValueError, 'checksum'):
            inspect_app(rehash(raw, checksum=False), self.sdk)
        with self.assertRaisesRegex(ValueError, 'dirty'):
            inspect_app(image('abc123-dirty'), self.sdk)

    def test_cli_create_verify_and_reject_no_extra_files(self):
        base = [sys.executable, str(ROOT / 'tools/build_candidate.py')]
        common = ['--source-commit', self.sha, '--sdk', self.sdk, '--profile', self.profile]
        create = subprocess.run(base + ['create', '--repo', str(self.repo), '--build', str(self.build),
                                       '--output', str(self.output)] + common, capture_output=True, text=True)
        self.assertEqual(create.returncode, 0, create.stderr)
        verify = subprocess.run(base + ['verify', '--candidate', str(self.output),
                                        '--image', str(self.build / 'tv4_hmi.bin')] + common,
                                capture_output=True, text=True)
        self.assertEqual(verify.returncode, 0, verify.stderr)
        self.assertTrue(json.loads(verify.stdout)['image_checked'])
        (self.output / 'private.key').write_text('test')
        bad = subprocess.run(base + ['verify', '--candidate', str(self.output)] + common,
                             capture_output=True, text=True)
        self.assertNotEqual(bad.returncode, 0)
        self.assertIn('rejected', bad.stderr)
        self.assertNotIn('Traceback', bad.stderr)

    def test_container_version_forwarding_without_running_container(self):
        bin_dir = self.root / 'fake-bin'
        bin_dir.mkdir()
        docker = bin_dir / 'docker'
        docker.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
        docker.chmod(0o700)
        env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ['PATH'])
        env.pop('TV4_BUILD_PROJECT_VERSION', None)
        run = subprocess.run(['bash', str(ROOT / 'tools/build_idf.sh'), 'rev12', '5.3.2', 'docker'],
                             env=env, capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        args = run.stdout.splitlines()
        expected = subprocess.check_output(['git', '-C', str(ROOT), 'describe', '--always', '--dirty'],
                                           text=True).strip()
        self.assertIn('TV4_BUILD_PROJECT_VERSION=' + expected, args)
        self.assertIn(candidate.SDK_IMAGES['5.3.2'], args)
        self.assertNotIn('--device', args)
        for bad_version in ('x;other', 'x' * 32, '$(not-a-command)', 'v1/escape'):
            env['TV4_BUILD_PROJECT_VERSION'] = bad_version
            run = subprocess.run(['bash', str(ROOT / 'tools/build_idf.sh'), 'rev12', '5.3.2', 'docker'],
                                 env=env, capture_output=True, text=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertEqual(run.stdout, '')

    def test_validation_api_does_not_rewrite_inputs(self):
        path = self.build / 'validation.json'
        before = path.read_bytes()
        candidate.check_idf_build.verify(self.build, self.profile, self.sdk, write_report=False)
        self.assertEqual(before, path.read_bytes())
        path.unlink()
        candidate.check_idf_build.verify(self.build, self.profile, self.sdk, write_report=False)
        self.assertFalse(path.exists())
        with self.assertRaises(FileNotFoundError):
            self.create()
        self.assertFalse(self.output.exists())

    def test_large_idf_description_is_bounded_but_never_exported(self):
        path = self.build / 'project_description.json'
        data = json.loads(path.read_bytes())
        data['build_component_paths'] = '/private/source/' * 25000
        path.write_text(json.dumps(data))
        self.create()
        self.assertFalse(any(b'/private/source/' in p.read_bytes() for p in self.output.iterdir()))
        path.write_bytes(b'x' * (candidate.DESCRIPTION_MAX + 1))
        with self.assertRaisesRegex(ValueError, 'oversized'):
            self.create(self.root / 'second')

    def test_rehashed_payload_still_must_match_expected_profile(self):
        self.create()
        report = json.loads((self.output / 'validation.json').read_bytes())
        report['profile'] = 'rev12-tls-bench'
        raw = candidate.encoded(report)
        (self.output / 'validation.json').write_bytes(raw)
        self.mutate(lambda r: r['files'].update({'validation.json': candidate.digest(raw)}))
        with self.assertRaisesRegex(ValueError, 'profile/SDK mismatch'):
            self.verify()


if __name__ == '__main__':
    unittest.main()
