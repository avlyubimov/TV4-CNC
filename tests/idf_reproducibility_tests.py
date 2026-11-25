"""Offline refusals for output equality; synthetic fixtures are not firmware."""
import copy
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import check_idf_build as validation
import check_idf_reproducibility as repro
import ota_setup_tests as fixtures
import build_candidate_tests as images


class ReproducibilityTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.MigrationTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.first = self.fixture.build
        self.sdk = self.fixture.sdk
        self.source = self.fixture.sha
        elf = bytearray(52)
        elf[:6] = b'\x7fELF\x01\x01'
        struct.pack_into('<H', elf, 18, 94)
        self.elf = bytes(elf) + b'non-executable fixture: install_policy=check_only'
        (self.first / 'tv4_hmi.elf').write_bytes(self.elf)
        (self.first / 'tv4_hmi.bin').write_bytes(images.image(self.fixture.version, self.sdk, self.elf))
        self.config = json.loads((self.first / 'config/sdkconfig.json').read_text())
        self.config.update(APP_REPRODUCIBLE_BUILD=True, COMPILER_HIDE_PATHS_MACROS=True,
                           APP_COMPILE_TIME_DATE=False)
        self.write_config()
        self.second = self.fixture.root / 'other' / 'deeper' / 'build'
        shutil.copytree(self.first, self.second)

    def write_config(self):
        (self.first / 'config/sdkconfig.json').write_text(json.dumps(self.config))

    def check(self):
        return repro.compare(self.first, self.second, self.sdk, self.source)

    def test_equal_outputs_and_inspection_does_not_rewrite(self):
        before = {p: p.read_bytes() for p in self.first.rglob('*') if p.is_file()}
        result = self.check()
        self.assertTrue(result['equal_build_outputs'] and result['compile_date_time_absent'])
        self.assertFalse(result['independent_compilation_verified_by_this_tool'])
        self.assertFalse(result['device_access'] or result['install_authorized'] or result['release_published'])
        self.assertEqual(set(result['artifacts']), set(repro.FILES))
        self.assertEqual(before, {p: p.read_bytes() for p in before})

    def test_same_directory_alias_and_bad_source_claim_refused(self):
        for second in (self.first, self.first / '..' / self.first.name):
            with self.subTest(second=second), self.assertRaises(ValueError):
                repro.compare(self.first, second, self.sdk, self.source)
        for source in ('', 'main', 'a' * 39, 'A' * 40, True):
            with self.subTest(source=source), self.assertRaises(ValueError):
                repro.compare(self.first, self.second, self.sdk, source)

    def test_every_artifact_difference_is_refused(self):
        for name in repro.FILES:
            target = self.second / name
            original = target.read_bytes()
            target.write_bytes(original + b'X')
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.check()
            target.write_bytes(original)

    def test_effective_configuration_difference_refused(self):
        self.config['LOG_DEFAULT_LEVEL'] = 2
        self.write_config()
        with self.assertRaisesRegex(ValueError, 'configurations differ'):
            self.check()

    def test_memory_trace_is_explicit_for_configured_bench_pairs(self):
        for root in (self.first, self.second):
            path = root / 'config/sdkconfig.json'
            config = json.loads(path.read_text())
            config['TV4_OTA_MEMORY_DIAGNOSTICS'] = True
            path.write_text(json.dumps(config))
        with self.assertRaises(ValueError):
            self.check()
        result = repro.compare(self.first, self.second, self.sdk, self.source, memory_diagnostics=True)
        self.assertTrue(result['memory_diagnostics'] and result['equal_build_outputs'])

    def test_reproducible_flags_and_explicit_selection_required(self):
        original = copy.deepcopy(self.config)
        for change in ({'APP_REPRODUCIBLE_BUILD': False}, {'APP_COMPILE_TIME_DATE': True},
                       {'COMPILER_HIDE_PATHS_MACROS': False}, {'APP_REPRODUCIBLE_BUILD': 1}):
            self.config = {**original, **change}
            self.write_config()
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.check()
        self.config = original
        self.write_config()
        with self.assertRaises(ValueError):
            validation.verify(self.first, 'rev12-ota', self.sdk, write_report=False, reproducible=False)

    def test_descriptor_date_cannot_be_hidden_by_configuration(self):
        raw = bytearray((self.first / 'tv4_hmi.bin').read_bytes())
        raw[32 + 80:32 + 88] = b'12:34:56'
        (self.first / 'tv4_hmi.bin').write_bytes(images.rehash(raw))
        with self.assertRaisesRegex(ValueError, 'date/time retained'):
            self.check()

    def test_stale_elf_or_foreign_sdk_refused(self):
        original = (self.first / 'tv4_hmi.elf').read_bytes()
        (self.first / 'tv4_hmi.elf').write_bytes(original + b'changed')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            self.check()
        (self.first / 'tv4_hmi.elf').write_bytes(original)
        with self.assertRaises(ValueError):
            repro.compare(self.first, self.second, '5.3.2', self.source)

    def test_cli_and_tamper_refusal_even_with_python_optimization(self):
        cmd = [sys.executable, '-O', str(ROOT / 'tools/check_idf_reproducibility.py'),
               '--first', str(self.first), '--second', str(self.second), '--sdk', self.sdk,
               '--source-commit', self.source]
        good = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(good.returncode, 0, good.stderr)
        self.assertTrue(json.loads(good.stdout)['equal_build_outputs'])
        (self.second / 'partition_table/partition-table.bin').write_bytes(b'bad')
        bad = subprocess.run(cmd, capture_output=True, text=True)
        self.assertNotEqual(bad.returncode, 0)
        self.assertNotIn('Traceback', bad.stderr)

    def test_shell_gate_and_container_forwarding_without_launch(self):
        bin_dir = self.fixture.root / 'fake-bin'
        bin_dir.mkdir()
        docker = bin_dir / 'docker'
        docker.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
        docker.chmod(0o700)
        env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ['PATH'])
        for key in ['TV4_OTA_CONTEXT', 'TV4_OTA_MEMORY_DIAGNOSTICS', 'TV4_OTA_WORKER_STACK_BENCH',
                    'TV4_OTA_WORKER_TLS_BENCH', 'TV4_OTA_SIGNED_CHECK_BENCH']:
            env.pop(key, None)
        for value, profile in [('2', 'rev12-ota'), ('true', 'rev12-ota'), ('1', 'headless'), ('1', 'rev12')]:
            run = subprocess.run([str(ROOT / 'tools/build_idf.sh'), profile, self.sdk, 'docker'],
                                 env={**env, 'TV4_REPRODUCIBLE_BUILD': value}, capture_output=True, text=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertEqual(run.stdout, '')
        run = subprocess.run([str(ROOT / 'tools/build_idf.sh'), 'rev12-ota', self.sdk, 'docker'],
                             env={**env, 'TV4_REPRODUCIBLE_BUILD': '1'}, capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertIn('TV4_REPRODUCIBLE_BUILD=1', run.stdout.splitlines())
        self.assertNotIn('--device', run.stdout.splitlines())


if __name__ == '__main__':
    unittest.main()
