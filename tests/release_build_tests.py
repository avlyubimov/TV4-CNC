"""Offline build/cohort binding faults; unsigned fixtures are never releases."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import build_candidate as build
from build_candidate_tests import CandidateTests, image
import release_cohort_tests as fixtures
import review_release_build as binding
from esp_app_image import inspect_app


class BuildBinding(fixtures.Fixture):
    def setUp(self):
        super().setUp()
        self.cf = CandidateTests()
        self.addCleanup(self.cf.doCleanups)
        self.cf.setUp()
        self.args.source_commit = self.cf.sha
        self.hmi['source_commit'] = self.cf.sha
        self.motion['source_commit'] = self.cf.sha
        self.sign_hmi()
        self.sign_motion()
        self.args.hmi_build_profile = 'rev12'
        self.args.hmi_elf = self.cf.build / 'tv4_hmi.elf'
        self.make_candidate()

    def make_candidate(self, sdk='6.1.1', profile='rev12'):
        self.cf.sdk, self.cf.profile = sdk, profile
        self.cf.configure()
        self.cf.output = self.cf.root / (sdk + '-' + profile)
        self.cf.create()
        self.args.hmi_build_candidate = self.cf.output
        self.args.hmi_build_profile = profile
        self.args.hmi_sdk = sdk
        self.hmi_image = image('2.1.0', sdk)
        self.args.hmi_image.write_bytes(self.hmi_image)
        self.hmi['components'][0]['sha256'] = fixtures.cohort.sha(self.hmi_image)
        self.sign_hmi()
        # Explicitly synthetic unsigned claims, not a SemVer-tagged CI build.
        # Start with the existing actual create/verify fixture, then replace
        # its git-derived app descriptor claim with our signed fixture bytes.
        app = inspect_app(self.hmi_image, sdk)
        validation = copy.deepcopy(self.cf.validation)
        validation['images']['tv4_hmi.bin'] = {k: app[k] for k in ('bytes', 'sha256')}
        raw = build.encoded(validation)
        (self.cf.output / 'validation.json').write_bytes(raw)
        def change(c):
            c['application'] = app
            c['files']['validation.json'] = build.digest(raw)
        self.cf.mutate(change)
        self.pin()

    def pin(self):
        self.args.hmi_candidate_sha256 = hashlib.sha256(
            (self.cf.output / 'candidate.json').read_bytes()).hexdigest()

    def mutate_candidate(self, change):
        self.cf.mutate(change)
        self.pin()

    def command(self, tool='review_release_build.py'):
        result = [sys.executable, str(ROOT / 'tools' / tool)]
        for name, value in vars(self.args).items():
            if tool == 'review_release_cohort.py' and name in (
                    'hmi_build_candidate', 'hmi_candidate_sha256', 'hmi_build_profile', 'hmi_elf'):
                continue
            if value is not None:
                result += ['--' + name.replace('_', '-'), str(value)]
        return result

    def test_paired_full_bytes_link_remains_blocked_and_has_no_private_paths(self):
        r = binding.audit(self.args)
        self.assertEqual(r['status'], 'blocked-build-review-only')
        self.assertTrue(r['candidate_pin_checked'] and r['image_checked'] and r['elf_digest_checked'])
        self.assertFalse(r['elf_format_checked'] or r['ci_identity_verified'] or r['build_provenance_verified'])
        self.assertTrue(r['profile_match'])
        self.assertFalse(r['layout_match'])
        self.assertEqual(r['hmi_build']['layout']['id'], 'factory-single-2m')
        self.assertEqual(r['hmi_build']['signed_layout_id'], 'tv4-16m-ab-v1')
        self.assertIn('hmi-build-layout-mismatch', r['blockers'])
        self.assertIn('motion-build-evidence-unavailable', r['blockers'])
        self.assertFalse(any(r['gates'].values()) or any(r['signed_cohort']['gates'].values()))
        self.assertFalse(r['install_enabled'] or r['device_access'] or r['release_published'])
        self.assertEqual([c['name'] for c in r['signed_cohort']['components']], ['hmi', 'motion'])
        self.assertNotIn(str(self.root), json.dumps(r))
        self.assertNotIn(str(self.cf.root), json.dumps(r))
        self.assertEqual(binding.audit(self.args), r)

    def test_hmi_only_all_sdk_profiles_are_audited_without_relabelling(self):
        self.hmi_only()
        # The setup already created this one; use it rather than overwrite it.
        for sdk in build.SDK_IMAGES:
            for profile in build.PROFILES:
                with self.subTest(sdk=sdk, profile=profile):
                    if (sdk, profile) != ('6.1.1', 'rev12'):
                        self.make_candidate(sdk, profile)
                    else:
                        self.args.hmi_sdk = sdk
                        self.args.hmi_build_candidate = self.cf.root / (sdk + '-' + profile)
                        self.cf.output = self.args.hmi_build_candidate
                        self.args.hmi_build_profile = profile
                        self.hmi_image = image('2.1.0', sdk)
                        self.args.hmi_image.write_bytes(self.hmi_image)
                        self.hmi['components'][0]['sha256'] = fixtures.cohort.sha(self.hmi_image)
                        self.sign_hmi(); self.pin()
                    r = binding.audit(self.args)
                    self.assertEqual(r['hmi_build']['sdk'], sdk)
                    self.assertEqual(r['hmi_build']['profile'], profile)
                    self.assertEqual(r['profile_match'], profile == 'rev12')
                    self.assertEqual('hmi-build-profile-mismatch' in r['blockers'], profile != 'rev12')
                    self.assertNotIn('motion-build-evidence-unavailable', r['blockers'])
                    self.assertEqual(len(r['signed_cohort']['components']), 1)
                    self.assertFalse(r['layout_match'] or any(r['gates'].values()))

    def test_external_pin_is_mandatory_and_cannot_be_taken_from_replaced_metadata(self):
        good = self.args.hmi_candidate_sha256
        for value in (None, '', '0' * 64, '1' * 64, good.upper(), good[:-1], True):
            with self.subTest(value=value):
                self.args.hmi_candidate_sha256 = value
                with mock.patch.object(binding.cohort, 'review_with_details', side_effect=AssertionError('must not reach crypto')):
                    with self.assertRaises(ValueError): binding.audit(self.args)
        self.args.hmi_candidate_sha256 = good
        self.cf.mutate(lambda c: c.update(source_tree='1' * 40))
        with self.assertRaisesRegex(ValueError, 'pin mismatch'): binding.audit(self.args)

    def test_rehashed_unsigned_source_sdk_schema_scope_layout_and_image_claims_rejected(self):
        original = (self.cf.output / 'candidate.json').read_bytes()
        for change in (lambda c: c.update(source_commit='1' * 40), lambda c: c.update(sdk='5.3.2'),
                       lambda c: c.update(profile='headless'), lambda c: c.update(extra='downloaded command'),
                       lambda c: c['policy'].update(install_enabled=True),
                       lambda c: c['layout'].update(id='tv4-16m-ab-v1'),
                       lambda c: c['application'].update(version='2.2.0'),
                       lambda c: c['partition_table'].update(bytes=True)):
            with self.subTest(change=change):
                (self.cf.output / 'candidate.json').write_bytes(original)
                self.mutate_candidate(change)
                with self.assertRaises(ValueError): binding.audit(self.args)

    def test_local_elf_digest_and_length_checked_independently(self):
        original = self.args.hmi_elf.read_bytes()
        for raw in (original + b'x', original[:-1] + bytes([original[-1] ^ 1])):
            with self.subTest(size=len(raw)):
                self.args.hmi_elf.write_bytes(raw)
                with self.assertRaisesRegex(ValueError, 'ELF digest/length'): binding.audit(self.args)
        self.args.hmi_elf.write_bytes(original)

    def test_cohort_signature_peer_and_motion_checks_cannot_be_bypassed_by_build_pin(self):
        for field in ('hmi_signature', 'motion_signature', 'motion_image'):
            with self.subTest(field=field):
                p = getattr(self.args, field); raw = p.read_bytes()
                p.write_bytes(raw[:-1] + bytes([raw[-1] ^ 1]))
                with self.assertRaises(ValueError): binding.audit(self.args)
                p.write_bytes(raw)
        self.hmi['components'][0]['compatible_peer_versions']['motion_max'] = '2.0.9'
        self.sign_hmi()
        with self.assertRaisesRegex(ValueError, 'prospective'): binding.audit(self.args)

    def test_valid_but_substituted_hmi_image_cannot_match_ci_candidate(self):
        raw = image('2.2.0')
        self.args.hmi_image.write_bytes(raw)
        self.hmi['components'][0].update(version='2.2.0', sha256=fixtures.cohort.sha(raw),
            url='https://github.com/avlyubimov/TV4-CNC/releases/download/v3.0.0/tv4-hmi-esp32s3-v2.2.0.bin')
        self.sign_hmi()
        with self.assertRaisesRegex(ValueError, 'local app does not match review'): binding.audit(self.args)

    def test_nonregular_empty_oversized_and_extra_inputs_fail_before_crypto(self):
        original = self.args.hmi_elf
        symlink = self.root / 'elf-link'; symlink.symlink_to(original)
        fifo = self.root / 'elf-pipe'; os.mkfifo(fifo)
        empty = self.root / 'elf-empty'; empty.write_bytes(b'')
        big = self.root / 'elf-big'
        with big.open('wb') as stream: stream.truncate(binding.ELF_MAX + 1)
        parent_link = self.root / 'parent-link'; parent_link.symlink_to(self.cf.build, target_is_directory=True)
        for path in (symlink, fifo, empty, big, parent_link / original.name):
            with self.subTest(path=path.name):
                self.args.hmi_elf = path
                with mock.patch.object(binding.cohort, 'review_with_details', side_effect=AssertionError('must not reach crypto')):
                    with self.assertRaises(ValueError): binding.audit(self.args)
        self.args.hmi_elf = original
        (self.cf.output / 'unexpected.bin').write_bytes(b'not executable')
        with self.assertRaisesRegex(ValueError, 'review files'): binding.audit(self.args)
        (self.cf.output / 'unexpected.bin').unlink()
        path = self.cf.output / 'dependencies.lock'; path.unlink(); os.mkfifo(path)
        with self.assertRaises(ValueError): binding.audit(self.args)

    def test_captured_candidate_not_reopened_after_validation(self):
        verify = build.verify_with_metadata
        def replace_after_verify(*args, **kwargs):
            result = verify(*args, **kwargs)
            (Path(args[0]) / 'candidate.json').write_bytes(b'not verified replacement')
            (self.cf.output / 'candidate.json').write_bytes(b'not verified original')
            return result
        with mock.patch.object(build, 'verify_with_metadata', side_effect=replace_after_verify):
            r = binding.audit(self.args)
        self.assertEqual(r['hmi_build']['source_commit'], self.args.source_commit)
        self.assertEqual(r['hmi_build']['candidate_sha256'], self.args.hmi_candidate_sha256)

    def test_original_images_and_elf_not_reopened_after_snapshot(self):
        verify = build.verify_with_metadata
        def replace_originals(*args, **kwargs):
            result = verify(*args, **kwargs)
            self.args.hmi_image.write_bytes(b'changed original image')
            self.args.hmi_elf.write_bytes(b'changed original ELF')
            return result
        with mock.patch.object(build, 'verify_with_metadata', side_effect=replace_originals):
            r = binding.audit(self.args)
        self.assertEqual(r['hmi_build']['application']['sha256'], fixtures.cohort.sha(self.hmi_image))
        self.assertTrue(r['elf_digest_checked'])

    def test_different_valid_image_between_verifiers_is_rejected(self):
        verify = build.verify_with_metadata
        def change_private_image(*args, **kwargs):
            result = verify(*args, **kwargs)
            changed = image('2.2.0')
            Path(args[4]).write_bytes(changed)
            self.hmi['components'][0].update(version='2.2.0', sha256=fixtures.cohort.sha(changed),
                url='https://github.com/avlyubimov/TV4-CNC/releases/download/v3.0.0/tv4-hmi-esp32s3-v2.2.0.bin')
            self.sign_hmi()
            return result
        with mock.patch.object(build, 'verify_with_metadata', side_effect=change_private_image):
            with self.assertRaisesRegex(ValueError, 'application disagree'): binding.audit(self.args)

    def test_cli_blocked_exit3_invalid_no_report_and_legacy_cli_unchanged(self):
        command = self.command()
        r = subprocess.run(command, capture_output=True, timeout=20)
        self.assertEqual(r.returncode, 3, r.stderr)
        self.assertFalse(r.stderr)
        self.assertEqual(json.loads(r.stdout)['status'], 'blocked-build-review-only')
        old = subprocess.run(self.command('review_release_cohort.py'), capture_output=True, timeout=20)
        self.assertEqual(old.returncode, 0, old.stderr)
        self.assertEqual(json.loads(old.stdout)['schema'], 'tv4-release-cohort-review-draft-1')
        self.args.hmi_elf.write_bytes(b'wrong ELF')
        r = subprocess.run(command, capture_output=True, timeout=20)
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout, b'')
        self.assertIn(b'rejected', r.stderr)
        missing = subprocess.run([sys.executable, str(ROOT / 'tools/review_release_build.py')],
                                 capture_output=True, timeout=5)
        self.assertEqual(missing.returncode, 2)
        self.assertEqual(missing.stdout, b'')

    def test_pin_api_preserves_legacy_report_and_captures_original_snapshot(self):
        request = (self.cf.output, self.args.source_commit, self.args.hmi_sdk, self.args.hmi_build_profile,
                   self.args.hmi_image)
        legacy = build.verify(*request)
        checked, captured = build.verify_with_metadata(*request, self.args.hmi_candidate_sha256)
        self.assertEqual(legacy, checked)
        self.assertEqual(build.verify(*request, self.args.hmi_candidate_sha256), legacy)
        self.assertEqual(captured['candidate_sha256'], self.args.hmi_candidate_sha256)
        self.assertNotIn('candidate_sha256', legacy)
        with self.assertRaisesRegex(ValueError, 'pin mismatch'): build.verify(*request, '1' * 64)
        with self.assertRaisesRegex(ValueError, 'nonzero'): build.verify(*request, '0' * 64)
        cli = [sys.executable, str(ROOT / 'tools/build_candidate.py'), 'verify',
               '--candidate', str(self.cf.output), '--image', str(self.args.hmi_image),
               '--source-commit', self.args.source_commit, '--sdk', self.args.hmi_sdk,
               '--profile', self.args.hmi_build_profile, '--candidate-sha256']
        good = subprocess.run([*cli, self.args.hmi_candidate_sha256], capture_output=True, timeout=5)
        self.assertEqual(good.returncode, 0, good.stderr)
        self.assertEqual(json.loads(good.stdout), legacy)
        bad = subprocess.run([*cli, '1' * 64], capture_output=True, timeout=5)
        self.assertEqual(bad.returncode, 1)
        self.assertEqual(bad.stdout, b'')


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--driver', required=True)
    p.add_argument('--signer', required=True)
    args = p.parse_args()
    fixtures.DRIVER = str(Path(args.driver).absolute())
    fixtures.SIGNER = str(Path(args.signer).absolute())
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(BuildBinding)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful(): raise SystemExit(1)
    print('PASS offline build binding: actual crypto/bytes, external pin, blocked layouts/profiles, no install or provenance authority')
