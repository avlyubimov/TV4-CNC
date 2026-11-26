"""Actual build/compile entry points: no SDK launch, network or devices."""
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import check_idf_build as checker
import ota_setup_tests as fixtures
import check_restart_bench as linked

def ci_trace_fixture(output):
    """Disposable SOFTWARE-ONLY SDK fixture; never an owner key or board image."""
    output=Path(output);output.mkdir(mode=0o700)
    private=subprocess.check_output(['openssl','genpkey','-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-256'],stderr=subprocess.DEVNULL)
    public=subprocess.check_output(['openssl','pkey','-pubout'],input=private,stderr=subprocess.DEVNULL)
    del private  # Only the public half is written; no signer is retained.
    (output/'public.pem').write_bytes(public)
    c=dict(reviewed=True,install_reviewed=True,key_id='ci-restart-trace-only',public_key='public.pem',
           installed_version='0.0.0-ci-trace',bootloader_version='0.0.0-ci-trace',protocol_version='1.0.0',
           manifest_url='https://github.com/avlyubimov/TV4-CNC/releases/download/v0.0.0-ci-trace/manifest.json',
           prerelease=True,resources=dict(internal_free=65536,internal_largest=32768,psram_free=1048576,psram_largest=131072,stack=3072),
           time=dict(allow_unauthenticated_sntp=True,earliest_epoch=1700000000,latest_epoch=1900000000,maximum_age_ms=600000,maximum_skew_ms=5000))
    (output/'context.json').write_text(json.dumps(c)+'\n')
    (output/'DO_NOT_FLASH.txt').write_text('Disposable CI public-key fixture. Never flash, sign or release this build.\n')

class RestartGuards(unittest.TestCase):
    def trace_fixture(self):
        f=self.fixture();p=f.build/'config/sdkconfig.json';c=json.loads(p.read_text())
        c.update(TV4_OTA_RESTART_BENCH=False,TV4_OTA_RESTART_TRACE=True);p.write_text(json.dumps(c))
        p=f.build/'ota-local/ota_local_context.json';c=json.loads(p.read_text())
        c.update(provisioned=True,install_reviewed=True);p.write_text(json.dumps(c))
        (f.build/'tv4_hmi.elf').write_bytes(b'host synthetic ELF marker: install_policy=owner_reviewed')
        return f
    def test_operator_trace_requires_exclusive_reviewed_context(self):
        f=self.trace_fixture();args=dict(memory_diagnostics=True,restart_trace=True)
        self.assertTrue(checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**args)['ota_restart_trace'])
        for changed in ({'restart_trace':False},{'restart_trace':1},{'restart_bench':True},
                        {'memory_diagnostics':False},{'worker_stack_bench':True},{'signed_check_bench':True}):
            with self.subTest(changed=changed),self.assertRaises(ValueError):
                checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**{**args,**changed})
        with self.assertRaises(ValueError):f.prepare_migration()
        p=f.build/'ota-local/ota_local_context.json';good=json.loads(p.read_text())
        for changed in ({'provisioned':False},{'install_reviewed':False},{'private_key_present':True}):
            p.write_text(json.dumps({**good,**changed}))
            with self.subTest(changed=changed),self.assertRaises(ValueError):
                checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**args)
    def test_operator_shell_refuses_before_reading_context(self):
        good={'TV4_OTA_RESTART_TRACE':'1','TV4_OTA_MEMORY_DIAGNOSTICS':'1','TV4_OTA_CONTEXT':'UNREAD-OWNER-CONTEXT'}
        for profile,mode,changed in [('headless','local',{}),('rev12','local',{}),('rev12-ota','docker',{}),
            ('rev12-ota','local',{'TV4_OTA_CONTEXT':''}),('rev12-ota','local',{'TV4_OTA_RESTART_TRACE':'yes'}),
            *[('rev12-ota','local',{k:'1'}) for k in ('TV4_OTA_RESTART_BENCH','TV4_OTA_WORKER_STACK_BENCH','TV4_OTA_WORKER_TLS_BENCH','TV4_OTA_SIGNED_CHECK_BENCH')],
            ('rev12-ota','local',{'TV4_OTA_MEMORY_DIAGNOSTICS':'0'})]:
            run=subprocess.run([str(ROOT/'tools/build_idf.sh'),profile,'6.1.1',mode],
                env={**self.env(),**good,**changed},text=True,capture_output=True)
            with self.subTest(profile=profile,mode=mode,changed=changed):
                self.assertEqual(run.returncode,2);self.assertEqual(run.stdout,'');self.assertNotIn('UNREAD-OWNER',run.stderr)
    def test_operator_actual_c_guards(self):
        base=[os.environ.get('CC','cc'),'-std=c11','-fsyntax-only','-DESP_PLATFORM=1',
              '-I'+str(ROOT/'tests/ota_service_sdk'),'-I'+str(ROOT/'tests'),
              '-I'+str(ROOT/'firmware/esp32-hmi/ota'),'-I'+str(ROOT/'firmware/esp32-hmi/network')]
        good=['-DSERVICE_TEST_RESTART_TRACE=1','-DSERVICE_TEST_PROVISIONED=1','-DSERVICE_TEST_INSTALL_REVIEWED=1']
        source=str(ROOT/'firmware/esp32-hmi/ota/service.c')
        result=subprocess.run(base+good+[source],capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr)
        for extra in ('PROVISIONED=0','INSTALL_REVIEWED=0','DIAGNOSTICS=0','OTA=0','RESTART_BENCH=1','BENCH=1','TLS_BENCH=1','SIGNED_BENCH=1'):
            result=subprocess.run(base+good+['-DSERVICE_TEST_'+extra,source],capture_output=True,text=True)
            with self.subTest(extra=extra):self.assertNotEqual(result.returncode,0)
    def test_ci_fixture_has_no_private_key_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'fixture';ci_trace_fixture(p)
            self.assertEqual({f.name for f in p.iterdir()},{'context.json','public.pem','DO_NOT_FLASH.txt'})
            self.assertTrue((p/'public.pem').read_bytes().startswith(b'-----BEGIN PUBLIC KEY-----'))
            self.assertFalse(any(b'PRIVATE KEY' in f.read_bytes() for f in p.iterdir()))
            with self.assertRaises(FileExistsError):ci_trace_fixture(p)
    def test_linked_operator_path_cannot_bypass_registration_or_sdk(self):
        f=self.trace_fixture();elf=f.build/'tv4_hmi.elf'
        elf.write_bytes(elf.read_bytes()+b' stage=armed ready=%u operator=1')
        symbols={n:[0x40378000+i*64,32,'FUNC'] for i,n in enumerate(linked.TRACE_FUNCTIONS)}
        symbols.update({n:[0x3fc90000+i*128,64,'OBJECT'] for i,n in enumerate(linked.FORMATS)})
        dis={'esp_restart':'<esp_restart>:\n<__wrap_esp_restart_noos>',
             '__wrap_esp_restart_noos':'<__wrap_esp_restart_noos>:\n<esp_restart_noos> <tv4_restart_noos_format> <esp_rom_printf>',
             'shutdown_marker':'<shutdown_marker>:\n<tv4_restart_hook_format> <esp_rom_printf>',
             'worker':'<worker>:\n<tv4_restart_trace_boot> <tv4_restart_bench_prepare> <tv4_restart_bench_execute>',
             'tv4_restart_bench_execute':'<tv4_restart_bench_execute>:\n<esp_restart>'}
        def run():
            def output(args,**unused):
                if args[0]=='readelf':return '\n'.join(f'{i}: {v[0]:08x} {v[1]} {v[2]} LOCAL DEFAULT 7 {k}' for i,(k,v) in enumerate(symbols.items(),1))
                return dis[args[2].removeprefix('--disassemble=')]
            with patch.object(linked.shutil,'which',return_value='SDK-DOUBLE-ONLY'),patch.object(linked.subprocess,'check_output',side_effect=output):
                return linked.inspect(f.build,'rev12-ota',f.sdk,True,post_install=True)
        self.assertTrue(run()['post_install'])
        for marker in ('tv4_restart_trace_boot','tv4_restart_bench_prepare','tv4_restart_bench_execute'):
            saved=dis['worker'];dis['worker']=saved.replace('<'+marker+'>','')
            with self.subTest(marker=marker),self.assertRaises(ValueError):run()
            dis['worker']=saved
        saved=dis['tv4_restart_bench_execute'];dis['tv4_restart_bench_execute']=saved.replace('<esp_restart>','')
        with self.assertRaises(ValueError):run()
        dis['tv4_restart_bench_execute']=saved
        symbols['tv4_restart_bench_cold_boot']=[0x40379000,32,'FUNC']
        with self.assertRaises(ValueError):run()
    def test_analysis_copy_preserves_every_loaded_byte_and_memory_map(self):
        # Header-only synthetic ELF; payload is text, not executable MCU code.
        raw=bytearray(384);raw[:6]=b'\x7fELF\x01\x01'
        struct.pack_into('<III',raw,24,0x40378000,52,128)
        struct.pack_into('<HHHHHH',raw,40,52,32,1,40,3,1)
        struct.pack_into('<8I',raw,52,1,352,0x40378000,0x40378000,16,16,5,4)
        names=b'\0.shstrtab\0.xt.prop\0';raw[256:256+len(names)]=names
        struct.pack_into('<10I',raw,168,1,3,0,0,256,len(names),0,0,1,0)
        struct.pack_into('<10I',raw,208,11,1,0,0,320,12,0,0,4,0)
        raw[352:368]=b'NOT MCU CODE!!!!'
        copy=linked.property_analysis_copy(bytes(raw))
        self.assertEqual(linked.loaded_bytes(raw),linked.loaded_bytes(copy))
        self.assertEqual(copy[:228],raw[:228]);self.assertEqual(copy[232:],raw[232:])
        raw[216]=2 # SHF_ALLOC forbids changing even a property header.
        with self.assertRaises(ValueError):linked.property_analysis_copy(bytes(raw))
    def test_linked_checker_refuses_cached_markers_or_sdk_bypass(self):
        # Synthetic symbol/disassembly transcript only: never MCU instructions.
        f=self.fixture()
        symbols={name:[0x40378000+i*64,32,'FUNC'] for i,name in enumerate(linked.FUNCTIONS)}
        symbols.update({name:[0x3fc90000+i*128,64,'OBJECT'] for i,name in enumerate(linked.FORMATS)})
        dis={'esp_restart':'<esp_restart>:\n<__wrap_esp_restart_noos>',
             '__wrap_esp_restart_noos':'<__wrap_esp_restart_noos>:\n<esp_restart_noos> <tv4_restart_noos_format> <esp_rom_printf>',
             'shutdown_marker':'<shutdown_marker>:\n<tv4_restart_hook_format> <esp_rom_printf>'}
        def run():
            def output(args,**unused):
                if args[0]=='readelf':
                    return '\n'.join(f'{i}: {v[0]:08x} {v[1]} {v[2]} LOCAL DEFAULT 7 {k}'
                                     for i,(k,v) in enumerate(symbols.items(),1))
                return dis[args[2].removeprefix('--disassemble=')]
            with patch.object(linked.shutil,'which',return_value='SDK-DOUBLE-ONLY'),patch.object(linked.subprocess,'check_output',side_effect=output):
                return linked.inspect(f.build,'rev12-ota',f.sdk,True)
        self.assertTrue(run()['sdk_calls_wrapper'])
        for name in ('__wrap_esp_restart_noos','shutdown_marker',*linked.FORMATS):
            old=symbols[name][0];symbols[name][0]=0x3c100000
            with self.subTest(name=name),self.assertRaises(ValueError):run()
            symbols[name][0]=old
        for name in dis:
            old=dis[name];dis[name]='400005d0 <esp_rom_printf>'
            with self.subTest(name=name),self.assertRaises(ValueError):run()
            dis[name]=old
        dis['shutdown_marker']+=' 42000000 <cached_logger>'
        with self.assertRaises(ValueError):run()
    def fixture(self):
        f=fixtures.MigrationTests('test_bundle_and_no_overwrite');f.setUp();self.addCleanup(f.doCleanups)
        p=f.build/'config/sdkconfig.json';c=json.loads(p.read_text())
        c.update(TV4_OTA_MEMORY_DIAGNOSTICS=True,TV4_OTA_RESTART_BENCH=True)
        p.write_text(json.dumps(c));return f
    def test_explicit_exclusive_unprovisioned_only(self):
        f=self.fixture();args=dict(memory_diagnostics=True,restart_bench=True)
        self.assertTrue(checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**args)['ota_restart_bench'])
        for changed in ({'restart_bench':False},{'restart_bench':1},{'memory_diagnostics':False},
                        {'worker_stack_bench':True},{'worker_tls_bench':True},{'signed_check_bench':True}):
            with self.subTest(changed=changed),self.assertRaises(ValueError):
                checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**{**args,**changed})
        with self.assertRaises(ValueError):f.prepare_migration()
        self.assertFalse(f.output.exists())
        p=f.build/'ota-local/ota_local_context.json';c=json.loads(p.read_text())
        for key in ('provisioned','private_key_present'):
            p.write_text(json.dumps({**c,key:True}))
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'unprovisioned'):
                checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**args)
    def env(self):
        return {k:v for k,v in os.environ.items() if not k.startswith('TV4_OTA_')}
    def test_shell_rejects_before_sdk_or_context(self):
        good={'TV4_OTA_RESTART_BENCH':'1','TV4_OTA_MEMORY_DIAGNOSTICS':'1'}
        for profile,change in [('headless',{}),('rev12',{}),('rev12-tls-bench',{}),
            ('rev12-ota',{'TV4_OTA_RESTART_BENCH':'true'}),
            ('rev12-ota',{'TV4_OTA_MEMORY_DIAGNOSTICS':'0'}),
            ('rev12-ota',{'TV4_OTA_WORKER_STACK_BENCH':'1'}),
            ('rev12-ota',{'TV4_OTA_WORKER_TLS_BENCH':'1'}),
            ('rev12-ota',{'TV4_OTA_SIGNED_CHECK_BENCH':'1'}),
            ('rev12-ota',{'TV4_OTA_CONTEXT':'PRIVATE-CONTEXT-MUST-NOT-BE-READ'})]:
            run=subprocess.run([str(ROOT/'tools/build_idf.sh'),profile,'6.1.1','docker'],
                env={**self.env(),**good,**change},capture_output=True,text=True)
            with self.subTest(profile=profile,change=change):
                self.assertEqual(run.returncode,2);self.assertEqual(run.stdout,'')
                self.assertNotIn('PRIVATE-CONTEXT',run.stderr)
    def test_docker_forwarding_and_default_off(self):
        with tempfile.TemporaryDirectory() as directory:
            docker=Path(directory)/'docker';docker.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n');docker.chmod(0o700)
            for value in ('0','1'):
                run=subprocess.run([str(ROOT/'tools/build_idf.sh'),'rev12-ota','6.1.1','docker'],
                    env={**self.env(),'PATH':directory+os.pathsep+os.environ['PATH'],
                         'TV4_OTA_MEMORY_DIAGNOSTICS':value,'TV4_OTA_RESTART_BENCH':value},capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                self.assertIn('TV4_OTA_RESTART_BENCH='+value,run.stdout.splitlines())
                self.assertNotIn('--device',run.stdout.splitlines())
    def test_actual_c_profile_guards(self):
        base=[os.environ.get('CC','cc'),'-std=c11','-fsyntax-only','-DESP_PLATFORM=1',
              '-I'+str(ROOT/'tests/ota_service_sdk'),'-I'+str(ROOT/'tests'),
              '-I'+str(ROOT/'firmware/esp32-hmi/ota'),'-I'+str(ROOT/'firmware/esp32-hmi/network')]
        source=str(ROOT/'firmware/esp32-hmi/ota/service.c')
        good=['-DSERVICE_TEST_RESTART_BENCH=1']
        run=subprocess.run(base+good+[source],capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr)
        for extra in ('DIAGNOSTICS=0','OTA=0','PROVISIONED=1','BENCH=1','TLS_BENCH=1','SIGNED_BENCH=1'):
            run=subprocess.run(base+good+['-DSERVICE_TEST_'+extra,source],capture_output=True,text=True)
            with self.subTest(extra=extra):self.assertNotEqual(run.returncode,0)
        marker=str(ROOT/'firmware/esp32-hmi/ota/restart_bench.c')
        run=subprocess.run(base+good+[marker],capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr)
        run=subprocess.run(base+[marker],capture_output=True,text=True)
        self.assertNotEqual(run.returncode,0)

if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--ci-trace-fixture':ci_trace_fixture(sys.argv[2])
    else:unittest.main()
