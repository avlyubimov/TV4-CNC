"""Host-only signed review fixtures. Private keys stay in signer process RAM.

No downloads, devices, Flash, release assets or executable MCU firmware.
"""
import argparse
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import review_release_cohort as cohort
from build_candidate_tests import image
from ota_policy_conformance_tests import Harness

DRIVER = SIGNER = None
SOURCE = '0123456789abcdef0123456789abcdef01234567'


def dump(value):
    return json.dumps(value, ensure_ascii=True, separators=(',', ':')).encode()


class Fixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='tv4-cohort-tests-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.signer = Harness(SIGNER, self.root)
        self.addCleanup(self.signer.close)
        self.motion_signer = Harness(SIGNER, self.root)
        self.addCleanup(self.motion_signer.close)
        self.hmi_image = image('2.1.0')
        self.motion_image = bytes((i * 13 + 9) % 256 for i in range(769))
        self.hmi = dict(schema_version=1, release_version='3.0.0', channel='stable', source_commit=SOURCE,
            signing=dict(key_id='test-only', algorithm='ECDSA-P256-SHA256'),
            protocol=dict(min='1.0.0', max='1.0.0'), release_notes='Synthetic review only',
            components=[dict(name='hmi', version='2.1.0', hardware_id='waveshare-esp32s3-touch-lcd-7',
                hardware_revision='1.2', chip='esp32s3', profile='rev12', layout_id='tv4-16m-ab-v1',
                minimum_installed_version='2.0.0', minimum_bootloader_version='1.0.0',
                compatible_peer_versions=dict(motion_min='2.0.0', motion_max='2.9.0'),
                size_bytes=len(self.hmi_image), sha256=hashlib.sha256(self.hmi_image).hexdigest(),
                url='https://github.com/avlyubimov/TV4-CNC/releases/download/v3.0.0/tv4-hmi-esp32s3-v2.1.0.bin')])
        self.motion = dict(schema='tv4-motion-release-draft-1', component='motion', hardware_id='test-f407',
            hardware_revision='synthetic', memory_profile='test-no-addresses', image_counter=3,
            minimum_bootloader_counter=5, protocol_min=6, protocol_max=8, image_size=len(self.motion_image),
            sha256=hashlib.sha256(self.motion_image).hexdigest(), release_version='3.0.0',
            component_version='2.1.0', channel='stable', source_commit=SOURCE)
        self.local = dict(schema='tv4-cohort-local-context-draft-1',
            hmi=dict(key_id='test-only', installed_version='2.0.0', bootloader_version='1.0.0',
                protocol_version='1.0.0', motion_version='2.0.0', layout_id='tv4-16m-ab-v1'),
            motion=dict(key_id='test-only', hardware_id='test-f407', hardware_revision='synthetic',
                memory_profile='test-no-addresses', installed_image_counter=2, minimum_image_counter=3,
                bootloader_counter=5, protocol=7, max_image_bytes=2049, timeout_ms=1000,
                installed_component_version='2.0.0', minimum_component_version='2.0.1'))
        self.args = argparse.Namespace(context=self.root/'context', release_version='3.0.0', source_commit=SOURCE,
            channel='stable', hmi_sdk='6.1.1', hmi_manifest=self.root/'hmi-meta', hmi_signature=self.root/'hmi-signature',
            hmi_public_key=self.root/'hmi-public', hmi_image=self.root/'hmi-image',
            motion_metadata=self.root/'motion-meta', motion_signature=self.root/'motion-signature',
            motion_public_key=self.root/'motion-public', motion_image=self.root/'motion-image', motion_driver=DRIVER)
        self.args.hmi_image.write_bytes(self.hmi_image)
        self.args.motion_image.write_bytes(self.motion_image)
        self.args.hmi_public_key.write_bytes(self.signer.pub)
        self.args.motion_public_key.write_bytes(self.motion_signer.pub)
        self.save_context()
        self.sign_hmi()
        self.sign_motion()

    def save_context(self):
        self.args.context.write_bytes(dump(self.local))

    def sign(self, raw, *, motion=False):
        signer = self.motion_signer if motion else self.signer
        return base64.b64decode(signer.request(op='sign', raw=raw.hex())['signature'], validate=True)

    def sign_hmi(self, raw=None):
        raw = dump(self.hmi) if raw is None else raw
        self.args.hmi_manifest.write_bytes(raw)
        self.args.hmi_signature.write_bytes(dump(dict(key_id='test-only', algorithm='ECDSA-P256-SHA256',
            signature=base64.b64encode(self.sign(raw)).decode())))

    def sign_motion(self, raw=None):
        raw = dump(self.motion) if raw is None else raw
        self.args.motion_metadata.write_bytes(raw)
        self.args.motion_signature.write_bytes(self.sign(raw, motion=True))

    def native(self, *, values=None, paths=None):
        m = self.local['motion'] if values is None else values
        paths = dict(metadata=self.args.motion_metadata, signature=self.args.motion_signature,
            public=self.args.motion_public_key, image=self.args.motion_image) if paths is None else paths
        return subprocess.run([DRIVER, *cohort.motion_arguments(m, paths, self.args)], capture_output=True, timeout=15)

    def hmi_only(self):
        self.local.pop('motion')
        self.save_context()
        for name in cohort.MOTION_INPUTS:
            setattr(self.args, name, None)


class MotionCLI(Fixture):
    def test_actual_independent_crypto_full_image_and_zero_workspace(self):
        r = self.native()
        self.assertEqual(r.returncode, 0, r.stderr)
        v = json.loads(r.stdout)
        self.assertEqual((v['release_version'], v['component_version'], v['source_commit']), ('3.0.0', '2.1.0', SOURCE))
        self.assertEqual(v['sha256'], hashlib.sha256(self.motion_image).hexdigest())
        self.assertEqual(v['metadata_sha256'], hashlib.sha256(self.args.motion_metadata.read_bytes()).hexdigest())
        self.assertTrue(v['workspace_zero'])
        self.assertFalse(v['device_access'] or v['install_enabled'])

    def test_crypto_policy_source_and_document_failures_clear_candidate(self):
        original = copy.deepcopy(self.motion)
        mutations = [dict(source_commit='1'*40), dict(component_version='2.0.0'), dict(image_counter=2),
            dict(protocol_min=8), dict(memory_profile='another'), dict(channel='prerelease'), dict(extra=1)]
        for delta in mutations:
            with self.subTest(delta=delta):
                self.motion = dict(original, **delta)
                self.sign_motion()
                r = self.native()
                self.assertEqual(r.returncode, 1)
                v = json.loads(r.stdout)
                self.assertTrue(v['candidate_zero'] and v['workspace_zero'])
        self.motion = original
        self.sign_motion()
        signature = self.args.motion_signature.read_bytes()
        self.args.motion_signature.write_bytes(signature[:-1] + bytes([signature[-1] ^ 1]))
        self.assertEqual(self.native().returncode, 1)
        self.sign_motion()
        self.args.motion_image.write_bytes(self.motion_image[:-1] + bytes([self.motion_image[-1] ^ 1]))
        self.assertEqual(self.native().returncode, 1)

    def test_frontend_bounds_numbers_and_regular_files(self):
        for field, value in [('timeout_ms', 10001), ('max_image_bytes', 16*1024*1024+1),
                ('protocol', 0), ('installed_image_counter', 4294967296), ('minimum_image_counter', '01'),
                ('hardware_id', 'x'*32), ('installed_component_version', '')]:
            with self.subTest(field=field, value=value):
                local = dict(self.local['motion'], **{field:value})
                self.assertEqual(self.native(values=local).returncode, 2)
        for name, raw in [('motion_metadata', b' ' * 1025), ('motion_signature', b'x'*73), ('motion_public_key', b'x'*92),
                          ('motion_image', b'x'*2050)]:
            with self.subTest(name=name):
                p = getattr(self.args, name); original = p.read_bytes(); p.write_bytes(raw)
                self.assertEqual(self.native().returncode, 2)
                p.write_bytes(original)
        original = self.args.motion_image
        link = self.root/'linked';link.symlink_to(original);self.args.motion_image=link
        self.assertEqual(self.native().returncode, 2)
        fifo = self.root/'pipe';os.mkfifo(fifo);self.args.motion_image=fifo
        self.assertEqual(self.native().returncode, 2)
        self.args.motion_image=original
        self.assertEqual(subprocess.run([DRIVER],capture_output=True,timeout=2).returncode, 2)


class Cohort(Fixture):
    def test_paired_and_hmi_only_reports_all_authority_gates_closed(self):
        r = cohort.review(self.args)
        self.assertEqual([x['name'] for x in r['components']], ['hmi','motion'])
        self.assertTrue(r['prospective_peer_checked'])
        self.assertNotEqual(r['components'][0]['key_sha256'],r['components'][1]['key_sha256'])
        self.assertEqual((r['protocol_semver'],r['protocol_counter']), ('1.0.0',7))
        self.assertFalse(any(r['gates'].values()))
        self.assertFalse(r['install_enabled'] or r['device_access'] or r['release_published'])
        self.assertNotIn(str(self.root), json.dumps(r))
        self.assertEqual(cohort.review(self.args),r)
        self.hmi_only()
        r = cohort.review(self.args)
        self.assertEqual([x['name'] for x in r['components']], ['hmi'])
        self.assertFalse(r['motion_present'] or r['prospective_peer_checked'])
        self.assertNotIn('host_motion_driver_sha256', r)

    def test_prerelease_and_independent_component_versions(self):
        self.args.release_version='3.0.0-rc.1';self.args.channel='prerelease'
        self.hmi['release_version']=self.args.release_version;self.hmi['channel']='prerelease'
        self.hmi['components'][0]['version']='2.1.0-rc.1'
        self.hmi_image=image('2.1.0-rc.1');self.args.hmi_image.write_bytes(self.hmi_image)
        self.hmi['components'][0]['sha256']=hashlib.sha256(self.hmi_image).hexdigest()
        self.hmi['components'][0]['url']='https://github.com/avlyubimov/TV4-CNC/releases/download/v3.0.0-rc.1/tv4-hmi-esp32s3-v2.1.0-rc.1.bin'
        self.motion.update(release_version='3.0.0-rc.1',channel='prerelease',component_version='2.2.0-rc.2')
        self.sign_hmi();self.sign_motion()
        report=cohort.review(self.args)
        self.assertEqual([x['component_version'] for x in report['components']],['2.1.0-rc.1','2.2.0-rc.2'])
        self.assertEqual(report['release_version'],'3.0.0-rc.1')
        self.assertFalse(any(report['gates'].values()))

    def test_signed_mismatched_release_source_and_channel(self):
        original = copy.deepcopy(self.hmi)
        for field, value in [('source_commit','1'*40),('release_version','4.0.0'),('channel','prerelease')]:
            with self.subTest(role='hmi',field=field):
                self.hmi = copy.deepcopy(original);self.hmi[field]=value
                if field=='release_version':
                    self.hmi['components'][0]['url']=self.hmi['components'][0]['url'].replace('/v3.0.0/','/v4.0.0/')
                self.sign_hmi()
                with self.assertRaises(ValueError):cohort.review(self.args)
        self.hmi=original;self.sign_hmi()
        for field, value in [('source_commit','1'*40),('release_version','4.0.0'),('channel','prerelease')]:
            with self.subTest(role='motion',field=field):
                old=self.motion[field];self.motion[field]=value;self.sign_motion()
                with self.assertRaises(ValueError):cohort.review(self.args)
                self.motion[field]=old
        self.sign_motion()
        self.args.source_commit='0'*40
        with self.assertRaises(ValueError):cohort.review(self.args)

    def test_current_and_prospective_peer_context_are_separate_checks(self):
        self.hmi['components'][0]['compatible_peer_versions']['motion_max']='2.0.9';self.sign_hmi()
        # Current2.0.0 is accepted by old HMI policy; prospective2.1.0 must fail.
        with self.assertRaisesRegex(ValueError,'prospective'):cohort.review(self.args)
        self.hmi['components'][0].pop('compatible_peer_versions');self.sign_hmi()
        with self.assertRaisesRegex(ValueError,'peer range'):cohort.review(self.args)
        self.local['hmi']['motion_version']='1.9.0';self.save_context()
        with self.assertRaisesRegex(ValueError,'contexts disagree'):cohort.review(self.args)

    def test_corrupt_images_signatures_app_descriptor_and_sdk(self):
        for field in ('hmi_image','motion_image','hmi_signature','motion_signature','hmi_public_key','motion_public_key'):
            with self.subTest(field=field):
                p=getattr(self.args,field);raw=p.read_bytes();p.write_bytes(raw[:-1]+bytes([raw[-1]^1]))
                with self.assertRaises(ValueError):cohort.review(self.args)
                p.write_bytes(raw)
        self.hmi_image=image('2.2.0');self.args.hmi_image.write_bytes(self.hmi_image)
        self.hmi['components'][0]['sha256']=hashlib.sha256(self.hmi_image).hexdigest();self.sign_hmi()
        with self.assertRaisesRegex(ValueError,'descriptor'):cohort.review(self.args)
        self.args.hmi_image.write_bytes(image('2.1.0'))
        self.hmi['components'][0]['sha256']=hashlib.sha256(self.args.hmi_image.read_bytes()).hexdigest();self.sign_hmi()
        self.args.hmi_sdk='5.3.2'
        with self.assertRaises(ValueError):cohort.review(self.args)

    def test_local_context_unknown_duplicate_partial_and_no_paths_from_manifest(self):
        original=copy.deepcopy(self.local)
        changes=[('motion','protocol',True),('motion','timeout_ms',0),('motion','installed_component_version',''),
            ('motion','unknown',1),('hmi','installed_version',None),('hmi','layout_id','factory-single-2m')]
        for role,field,value in changes:
            with self.subTest(role=role,field=field):
                self.local=copy.deepcopy(original);self.local[role][field]=value;self.save_context()
                with self.assertRaises(ValueError):cohort.review(self.args)
        self.local=original;self.save_context()
        self.args.motion_driver=None
        with self.assertRaisesRegex(ValueError,'together'):cohort.review(self.args)
        self.args.motion_driver=DRIVER
        self.args.context.write_bytes(b'{"schema":1,"schema":2}')
        with self.assertRaises(ValueError):cohort.review(self.args)
        self.save_context()
        self.hmi['motion_driver']='/arbitrary/downloaded-command';self.sign_hmi()
        with self.assertRaises(ValueError):cohort.review(self.args)

    def test_signed_metadata_snapshot_is_not_reopened_for_binding(self):
        original_digest=hashlib.sha256(self.args.hmi_manifest.read_bytes()).hexdigest()
        original=cohort.ota.read_metadata
        calls=[]
        def changed_after_authentication(request):
            signed=original(request);calls.append(signed['source_commit'])
            request.manifest.write_bytes(b'{"source_commit":"unverified replacement"}')
            self.args.hmi_manifest.write_bytes(b'untrusted changed original')
            self.args.hmi_image.write_bytes(b'untrusted changed original image')
            return signed
        with mock.patch.object(cohort.ota,'read_metadata',side_effect=changed_after_authentication):
            r=cohort.review(self.args)
        self.assertEqual(calls,[SOURCE])
        self.assertEqual(r['source_commit'],SOURCE)
        self.assertEqual(r['components'][0]['metadata_sha256'],original_digest)

    def test_nonregular_inputs_and_bounds_before_any_driver_or_crypto(self):
        original=self.args.context
        for kind in ('link','fifo'):
            p=self.root/kind
            if kind=='link':p.symlink_to(original)
            else:os.mkfifo(p)
            self.args.context=p
            with self.assertRaises(ValueError):cohort.review(self.args)
        self.args.context=original
        self.args.hmi_manifest.write_bytes(b' '*16385)
        with self.assertRaises(ValueError):cohort.review(self.args)

    def test_actual_cli_stdout_report_and_failure_has_no_partial_report(self):
        command=[sys.executable,str(ROOT/'tools/review_release_cohort.py')]
        values=vars(self.args)
        for name,value in values.items():
            if value is not None:command+=['--'+name.replace('_','-'),str(value)]
        r=subprocess.run(command,capture_output=True,timeout=20)
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(json.loads(r.stdout)['status'],'consistent-offline-review-only')
        self.args.motion_image.write_bytes(b'bad')
        r=subprocess.run(command,capture_output=True,timeout=20)
        self.assertNotEqual(r.returncode,0)
        self.assertEqual(r.stdout,b'')


if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--driver',required=True);p.add_argument('--signer',required=True)
    p.add_argument('--group',choices=('cli','cohort'),required=True)
    args=p.parse_args();DRIVER=str(Path(args.driver).absolute());SIGNER=str(Path(args.signer).absolute())
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(MotionCLI if args.group=='cli' else Cohort)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():raise SystemExit(1)
    print('PASS '+args.group+' signed host review; no private key files, network, devices, install or publication')
