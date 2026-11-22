"""Local configuration/migration fault tests. Test keys never enter firmware."""
import contextlib
import copy
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import prepare_ota_context as context
import prepare_ota_migration as migration
from build_candidate_tests import CandidateTests,image

class ContextTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.output=self.root/'generated';self.config=self.root/'local.json'
        # Ephemeral host-only fixture. Neither private nor public test keys are
        # copied into a firmware tree, checked in or used for an SDK build.
        subprocess.run(['openssl','genpkey','-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-256','-out',str(self.root/'private.pem')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        subprocess.run(['openssl','pkey','-in',str(self.root/'private.pem'),'-pubout','-out',str(self.root/'public.pem')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        self.value=dict(reviewed=True,key_id='host-fixture-only',public_key='public.pem',installed_version='0.1.0',bootloader_version='0.1.0',protocol_version='1.0.0',manifest_url='https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json',prerelease=False,resources=dict(internal_free=32768,internal_largest=8192,psram_free=65536,psram_largest=32768,stack=2048),time=dict(allow_unauthenticated_sntp=True,earliest_epoch=1700000000,latest_epoch=1900000000,maximum_age_ms=300000,maximum_skew_ms=1000))
    def run_context(self):
        self.config.write_text(json.dumps(self.value));return context.prepare(self.config,self.output)
    def test_unconfigured_default_is_deterministic_and_keyless(self):
        result=context.prepare(None,self.output);header=(self.output/'ota_local_context.h').read_bytes()
        self.assertFalse(result['provisioned']);self.assertIsNone(result['public_key_sha256'])
        self.assertIn(b'TV4_OTA_PROVISIONED 0',header);self.assertIn(b'OTA_FLOOR_STACK 0u',header)
        self.assertIn(b'OTA_INSTALLED_VERSION ""',header);self.assertNotIn(b'host-fixture',header)
        context.prepare(None,self.output);self.assertEqual(header,(self.output/'ota_local_context.h').read_bytes())
    def test_explicit_public_context_and_no_private_key(self):
        result=self.run_context();self.assertTrue(result['provisioned']);self.assertFalse(result['private_key_present'])
        self.assertEqual(result['installed_version'],'0.1.0')
        header=(self.output/'ota_local_context.h').read_bytes()
        self.assertNotIn(b'PRIVATE',header);self.assertNotIn(str(self.root).encode(),header)
    def test_missing_unreviewed_and_unknown_settings_fail(self):
        good=copy.deepcopy(self.value)
        for field in good:
            self.value=copy.deepcopy(good);del self.value[field]
            with self.subTest(field=field),self.assertRaises((ValueError,KeyError)):self.run_context()
        for change in ({'reviewed':False},{'extra':1},{'public_key':'private.pem'}, {'prerelease':1},{'manifest_url':'http://example.org/manifest.json'}):
            self.value=copy.deepcopy(good);self.value.update(change)
            with self.subTest(change=change),self.assertRaises(ValueError):self.run_context()
    def test_resource_time_and_versions_are_not_inferred(self):
        good=copy.deepcopy(self.value)
        for section in ('resources','time'):
            for key in good[section]:
                self.value=copy.deepcopy(good);self.value[section][key]=0
                with self.subTest(section=section,key=key),self.assertRaises(ValueError):self.run_context()
        for version in ('','main','1.0','01.0.0','1.0.0-01','4294967296.0.0','1.0.0+build'):
            self.value=copy.deepcopy(good);self.value['installed_version']=version
            with self.subTest(version=version),self.assertRaises(ValueError):self.run_context()
    def test_duplicate_or_non_finite_json_rejected(self):
        for raw in ('{"reviewed":true,"reviewed":false}','{"reviewed":NaN}'):
            self.config.write_text(raw)
            with self.assertRaises(ValueError):context.prepare(self.config,self.output)
    def test_other_curve_refused(self):
        subprocess.run(['openssl','genpkey','-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-384','-out',str(self.root/'other.pem')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        subprocess.run(['openssl','pkey','-in',str(self.root/'other.pem'),'-pubout','-out',str(self.root/'public.pem')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        with self.assertRaises(ValueError):self.run_context()

# Reuse the existing bounded synthetic image/source fixture, not device files.
class MigrationTests(CandidateTests):
    # Only migration-specific tests run here; inherited candidate tests are
    # already registered independently by CTest.
    def configure(self):
        self.profile='rev12-tls-bench';super().configure();self.profile='rev12-ota'
        p=self.build/'config/sdkconfig.json';c=json.loads(p.read_text());c.update(TV4_UTC_TLS_BENCH=False,TV4_OTA_ENABLED=True,BOOTLOADER_APP_ROLLBACK_ENABLE=True,PARTITION_TABLE_CUSTOM_FILENAME='ota/partitions.ab.csv',ESP_TASK_WDT_EN=True,ESP_TASK_WDT_INIT=True,ESP_TASK_WDT_PANIC=True,ESP_TASK_WDT_TIMEOUT_S=5,ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=True,ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=True,ESP_INT_WDT=True,ESP_SYSTEM_PANIC_PRINT_REBOOT=True);p.write_text(json.dumps(c))
        table=b''.join(struct.pack('<HBBII16sI',0x50aa,k,s,a,n,label.encode(),0) for k,s,a,n,label in migration.check_idf_build.OTA_PARTITIONS)
        (self.build/'partition_table/partition-table.bin').write_bytes(table)
        (self.build/'ota-local').mkdir();context.prepare(None,self.build/'ota-local')
    def prepare_migration(self):return migration.prepare(self.build,self.sdk,self.output,repo=self.repo)
    def test_bundle_and_no_overwrite(self):
        result=self.prepare_migration();self.assertEqual(result,migration.verify(self.output))
        self.assertFalse(result['provisioned']);self.assertFalse(result['device_access'])
        self.assertEqual([x['offset'] for x in result['writes']],[0,0x8000,0x10000,0x210000])
        self.assertEqual((self.output/'otadata-erased.bin').read_bytes(),b'\xff'*8192)
        with self.assertRaises(ValueError):self.prepare_migration()
    def test_corrupt_payload_and_unexpected_inventory(self):
        self.prepare_migration()
        for name,_,_ in migration.WRITES:
            p=self.output/name;raw=p.read_bytes();p.write_bytes(raw[:-1]+bytes([raw[-1]^1]))
            with self.subTest(name=name),self.assertRaises(ValueError):migration.verify(self.output)
            p.write_bytes(raw)
        (self.output/'extra.bin').write_bytes(b'no')
        with self.assertRaises(ValueError):migration.verify(self.output)
    def test_unsafe_address_or_claim_rejected(self):
        self.prepare_migration();p=self.output/'plan.json';good=p.read_bytes()
        for field in ('device_access','hardware_migration_authorized','release_authorized','provisioned','erases_all_flash'):
            plan=json.loads(good);plan[field]=True;p.write_bytes(migration.encoded(plan))
            with self.subTest(field=field),self.assertRaises(ValueError):migration.verify(self.output)
        plan=json.loads(good);plan['writes'][0]['offset']=0x9000;p.write_bytes(migration.encoded(plan))
        with self.assertRaisesRegex(ValueError,'address'):migration.verify(self.output)
    def test_stale_source_elf_or_provisioning_refused(self):
        for name,data in [('tv4_hmi.elf',b'stale ELF'),('tv4_hmi.bin',image('older-build')),('ota-local/ota_local_context.json',b'{"provisioned":true}')]:
            p=self.build/name;good=p.read_bytes();p.write_bytes(data)
            with self.subTest(name=name),self.assertRaises(ValueError):self.prepare_migration()
            self.assertFalse(self.output.exists());p.write_bytes(good)
        (self.repo/'dirty.txt').write_text('unreviewed source')
        with self.assertRaisesRegex(ValueError,'clean'):self.prepare_migration()
    def test_rollback_security_and_wrong_layout(self):
        p=self.build/'config/sdkconfig.json';good=json.loads(p.read_text())
        for change in ({'ESP_TASK_WDT_PANIC':False},{'ESP_SYSTEM_PANIC_PRINT_REBOOT':False},{'BOOTLOADER_APP_ROLLBACK_ENABLE':False},{'BOOTLOADER_APP_ANTI_ROLLBACK':True},{'SECURE_BOOT':True},{'SECURE_FLASH_ENC_ENABLED':True},{'TV4_OTA_ENABLED':False},{'MBEDTLS_INTERNAL_MEM_ALLOC':True}):
            c=copy.deepcopy(good);c.update(change);p.write_text(json.dumps(c))
            with self.subTest(change=change),self.assertRaises(ValueError):self.prepare_migration()
        p.write_text(json.dumps(good));p=self.build/'partition_table/partition-table.bin';raw=bytearray(p.read_bytes());struct.pack_into('<I',raw,4*32+4,0x10000);p.write_bytes(raw)
        with self.assertRaisesRegex(ValueError,'layout'):self.prepare_migration()

if __name__=='__main__':
    suite=unittest.TestSuite()
    for cls in (ContextTests,MigrationTests):
        for name in cls.__dict__:
            if name.startswith('test_'):suite.addTest(cls(name))
    result=unittest.TextTestRunner().run(suite);raise SystemExit(not result.wasSuccessful())
