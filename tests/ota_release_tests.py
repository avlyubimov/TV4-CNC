"""Offline signing fault tests with ephemeral keys and NONEXECUTABLE test images."""
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import prepare_ota_release as release
import build_candidate_tests as image_fixture
import ota_setup_tests as fixtures


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        f = fixtures.MigrationTests('test_bundle_and_no_overwrite')
        self.addCleanup(f.doCleanups); f.setUp()
        self.fixture=f; self.root=f.root; self.repo=f.repo; self.build=f.build
        (self.repo/'LICENSE').write_bytes((ROOT/'LICENSE').read_bytes())
        f.git('add','LICENSE');f.git('-c','user.name=TV4 test','-c','user.email=test@invalid',
                                    '-c','commit.gpgsign=false','commit','-qm','Fixture license')
        f.sha=f.git('rev-parse','HEAD')
        self.output=self.root/'signed-bench'; self.plan_path=self.root/'owner-plan.json'
        self.signer=self.root/'signer'; self.signer.mkdir(mode=0o700)
        self.private=self.signer/'private.pem'; self.public=self.signer/'public.der'
        subprocess.run(['openssl','genpkey','-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-256',
                        '-out',str(self.private)],check=True,capture_output=True)
        self.private.chmod(0o600)
        subprocess.run(['openssl','pkey','-in',str(self.private),'-pubout','-outform','DER',
                        '-out',str(self.public)],check=True,capture_output=True)
        self.plan=dict(reviewed=True,purpose='signed-check-bench',source_commit=f.sha,sdk=f.sdk,
            release_version='0.1.0-bench.2',installed_version='0.1.0-bench.1',
            bootloader_version='0.1.0-bench.1',protocol_version='1.0.0',channel='prerelease',
            key_id='ephemeral-test-only',public_key_sha256=hashlib.sha256(self.public.read_bytes()).hexdigest(),
            release_notes='NONEXECUTABLE synthetic host fixture, never publish or flash.')
        self.write_plan(); self.configure()

    def write_plan(self): self.plan_path.write_text(json.dumps(self.plan))

    def configure(self):
        sdk=self.plan['sdk']; elf=(self.build/'tv4_hmi.elf').read_bytes()
        (self.build/'tv4_hmi.bin').write_bytes(image_fixture.image(self.plan['release_version'],sdk,elf))
        p=self.build/'project_description.json'; d=json.loads(p.read_text())
        d.update(project_version=self.plan['release_version'],git_revision='v'+sdk);p.write_text(json.dumps(d))
        validation=release.check_idf_build.verify(self.build,'rev12-ota',sdk,write_report=False)
        (self.build/'validation.json').write_text(json.dumps(validation))

    def create(self, **kwargs):
        return release.prepare(self.build,self.output,self.plan_path,self.public,
                               kwargs.get('private',self.private),repo=self.repo)

    def verify(self): return release.verify(self.output,self.plan_path,self.public)

    def rehash(self):
        files={p.name:p.read_bytes() for p in self.output.iterdir() if p.name!='checksums.sha256'}
        (self.output/'checksums.sha256').write_bytes(release.inventory(files))

    def test_real_crypto_both_sdk_formats_and_no_keys_in_output(self):
        for sdk in ('5.3.2','6.1.1'):
            with self.subTest(sdk=sdk):
                self.plan['sdk']=sdk;self.write_plan();self.configure()
                self.output=self.root/('signed-'+sdk)
                result=self.create();self.assertEqual(result,self.verify())
                self.assertFalse(result['device_access']);self.assertFalse(result['install_enabled'])
                self.assertFalse(result['release_published']);self.assertFalse(result['production_resource_qualified'])
                evidence=json.loads((self.output/'build-evidence.json').read_text())
                self.assertFalse(evidence['candidate_provisioned'])
                manifest=json.loads((self.output/'manifest.json').read_text())
                self.assertIs(manifest['components'][0]['installable'],False)
                for p in self.output.iterdir():
                    self.assertEqual(p.stat().st_mode & 0o777,0o600)
                    self.assertNotIn(b'PRIVATE KEY',p.read_bytes())
                    self.assertNotIn(str(self.signer).encode(),p.read_bytes())
                self.assertEqual(self.output.stat().st_mode & 0o777,0o700)

    def test_plan_requires_review_exact_scope_and_fields(self):
        good=copy.deepcopy(self.plan)
        for field,value in [('reviewed',False),('purpose','production'),('channel','stable'),
                            ('source_commit','0'*40),('key_id','bad/key'),('public_key_sha256','BAD')]:
            with self.subTest(field=field):
                self.plan=copy.deepcopy(good);self.plan[field]=value;self.write_plan()
                with self.assertRaises(ValueError):self.create()
                self.assertFalse(self.output.exists())
        self.plan=copy.deepcopy(good);self.plan['unknown']=True;self.write_plan()
        with self.assertRaises(ValueError):self.create()

    def test_versions_are_not_inferred(self):
        good=copy.deepcopy(self.plan)
        for value in ('main','0.1','01.0.0','0.1.0-bench.01','0.1.0','0.1.0-a/b','0.1.0+'+'x'*40):
            with self.subTest(version=value):
                self.plan=copy.deepcopy(good);self.plan['release_version']=value;self.write_plan()
                with self.assertRaises(ValueError):self.create()
        self.plan=copy.deepcopy(good);self.plan['installed_version']=self.plan['release_version'];self.write_plan()
        with self.assertRaises(ValueError):self.create()

    def test_source_wrong_dirty_or_untracked(self):
        self.plan['source_commit']='a'*40;self.write_plan()
        with self.assertRaisesRegex(ValueError,'source commit'):self.create()
        self.plan['source_commit']=self.fixture.sha;self.write_plan()
        (self.repo/'untracked').write_text('test')
        with self.assertRaisesRegex(ValueError,'clean'):self.create()

    def test_private_permissions_and_symlinks(self):
        self.private.chmod(0o644)
        with self.assertRaisesRegex(ValueError,'owner-only'):self.create()
        self.private.chmod(0o600);self.signer.chmod(0o755)
        with self.assertRaisesRegex(ValueError,'owner-only'):self.create()
        self.signer.chmod(0o700);link=self.signer/'link.pem';link.symlink_to(self.private)
        with self.assertRaisesRegex(ValueError,'owner-only'):self.create(private=link)

    def test_private_key_in_source_rejected(self):
        p=self.repo/'key.pem';p.write_bytes(self.private.read_bytes());p.chmod(0o600)
        with self.assertRaisesRegex(ValueError,'outside source'):self.create(private=p)

    def test_wrong_private_key_and_wrong_public_pin(self):
        other=self.signer/'other.pem'
        subprocess.run(['openssl','genpkey','-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-256',
                        '-out',str(other)],check=True,capture_output=True);other.chmod(0o600)
        with self.assertRaisesRegex(ValueError,'does not match'):self.create(private=other)
        self.plan['public_key_sha256']='a'*64;self.write_plan()
        with self.assertRaisesRegex(ValueError,'pin mismatch'):self.create()

    def test_app_version_and_elf_identity_checked(self):
        (self.build/'tv4_hmi.bin').write_bytes(image_fixture.image('0.1.0-bench.3'))
        with self.assertRaises(ValueError):self.create()
        self.configure();(self.build/'tv4_hmi.elf').write_bytes(b'changed ELF install_policy=check_only')
        with self.assertRaisesRegex(ValueError,'ELF/app'):self.create()

    def test_image_corruption_and_dump_rejected(self):
        p=self.build/'tv4_hmi.bin';good=p.read_bytes()
        for data in [good[:-1],good[:-1]+bytes([good[-1]^1]),good+b'0',b'\xff'*(16*1024*1024)]:
            with self.subTest(size=len(data)):
                p.write_bytes(data)
                with self.assertRaises(ValueError):self.create()
        self.assertFalse(self.output.exists())

    def test_stale_validation_rejected(self):
        p=self.build/'validation.json';d=json.loads(p.read_text());d['device_access']=True;p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'stale'):self.create()

    def test_app_snapshot_cannot_diverge_from_passed_validation(self):
        real=release.check_idf_build.verify
        def changed(*args,**kwargs):
            result=real(*args,**kwargs)
            p=self.build/'tv4_hmi.bin';raw=bytearray(p.read_bytes());raw[32+200]^=1
            p.write_bytes(image_fixture.rehash(raw))
            return result
        with patch.object(release.check_idf_build,'verify',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'image changed after'):self.create()
        self.assertFalse(self.output.exists())

    def test_actual_table_and_diagnostic_flags_checked(self):
        p=self.build/'partition_table/partition-table.bin';old=p.read_bytes();p.write_bytes(b'invalid')
        with self.assertRaises(ValueError):self.create()
        p.write_bytes(old)
        c=self.build/'config/sdkconfig.json';v=json.loads(c.read_text());v['TV4_OTA_MEMORY_DIAGNOSTICS']=True;c.write_text(json.dumps(v))
        with self.assertRaises(ValueError):self.create()

    def test_candidate_cannot_contain_private_context_or_other_trust(self):
        p=self.build/'ota-local/ota_local_context.json';d=json.loads(p.read_text());d['private_key_present']=True;p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'public-only'):self.create()
        d.update(private_key_present=False,provisioned=True,public_key_sha256='b'*64,
                 installed_version=self.plan['release_version']);p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'trust/version mismatch'):self.create()

    def test_output_no_overwrite_or_in_source(self):
        self.create();before={p.name:p.read_bytes() for p in self.output.iterdir()}
        with self.assertRaisesRegex(ValueError,'new output'):self.create()
        self.assertEqual(before,{p.name:p.read_bytes() for p in self.output.iterdir()})
        self.output=self.repo/'build-output'
        with self.assertRaisesRegex(ValueError,'outside source'):self.create()

    def test_signature_tampering_even_with_rehashed_inventory(self):
        self.create();p=self.output/'manifest.json';p.write_bytes(p.read_bytes()+b'\n');self.rehash()
        with self.assertRaisesRegex(ValueError,'signature rejected'):self.verify()

    def test_signed_installable_cannot_be_relabelled_check_bench(self):
        self.create();p=self.output/'manifest.json';m=json.loads(p.read_text())
        m['components'][0]['installable']=True;p.write_bytes(release.encoded(m))
        sig=subprocess.run(['openssl','dgst','-sha256','-sign',str(self.private),str(p)],
                           check=True,capture_output=True).stdout
        envelope=json.loads((self.output/'manifest.sig').read_text())
        envelope['signature']=release.base64.b64encode(sig).decode('ascii')
        (self.output/'manifest.sig').write_bytes(release.encoded(envelope));self.rehash()
        with self.assertRaisesRegex(ValueError,'CHECK-bench must not authorize'):self.verify()

    def test_image_tampering_even_with_rehashed_inventory(self):
        self.create();p=next(self.output.glob('*.bin'));p.write_bytes(p.read_bytes()+b'x');self.rehash()
        with self.assertRaises(ValueError):self.verify()

    def test_downloaded_bundle_cannot_select_context(self):
        self.create();self.plan['protocol_version']='2.0.0';self.write_plan()
        with self.assertRaisesRegex(ValueError,'protocol incompatible'):self.verify()

    def test_additional_files_and_symlinks_rejected(self):
        self.create();p=self.output/'public.der';p.write_bytes(self.public.read_bytes())
        with self.assertRaisesRegex(ValueError,'unexpected'):self.verify()
        p.unlink();app=next(self.output.glob('*.bin'));data=app.read_bytes();app.unlink();app.symlink_to(self.build/'tv4_hmi.bin')
        with self.assertRaisesRegex(ValueError,'symlink'):self.verify()
        self.assertTrue(data)

    def test_evidence_cannot_claim_ci_or_install_authority(self):
        self.create();p=self.output/'build-evidence.json';good=json.loads(p.read_text())
        for field in ('ci_identity_verified','install_enabled','device_access','production_resource_qualified','release_published'):
            d=copy.deepcopy(good);d[field]=True;p.write_text(json.dumps(d));self.rehash()
            with self.subTest(field=field),self.assertRaisesRegex(ValueError,'authority'):self.verify()
        p.write_text('[]');self.rehash()
        with self.assertRaisesRegex(ValueError,'unsupported'):self.verify()

    def test_changed_plan_during_signing_aborts_without_bundle(self):
        real=subprocess.run
        def changing(args,**kwargs):
            result=real(args,**kwargs)
            if args[:3]==['openssl','dgst','-sha256']:
                self.plan['release_notes']+=' changed';self.write_plan()
            return result
        with patch.object(release.subprocess,'run',side_effect=changing):
            with self.assertRaises(ValueError):self.create()
        self.assertFalse(self.output.exists())
        self.assertFalse(list(self.output.parent.glob('.tv4-signed-check-*')))

    def test_failed_signing_is_cleaned_up(self):
        real=subprocess.run
        def failing(args,**kwargs):
            if args[:3]==['openssl','dgst','-sha256']:return subprocess.CompletedProcess(args,1,b'',b'test failure')
            return real(args,**kwargs)
        with patch.object(release.subprocess,'run',side_effect=failing):
            with self.assertRaisesRegex(ValueError,'signing failed'):self.create()
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
