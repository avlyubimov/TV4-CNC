"""Actual build-script and ESP service guards; no SDK or device execution."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import check_idf_build as checker
import ota_setup_tests as fixtures

class Guards(unittest.TestCase):
    def fixture(self):
        f=fixtures.MigrationTests('test_bundle_and_no_overwrite');f.setUp();self.addCleanup(f.doCleanups)
        path=f.build/'config/sdkconfig.json';config=json.loads(path.read_text())
        config.update(TV4_OTA_MEMORY_DIAGNOSTICS=True,TV4_OTA_WORKER_STACK_BENCH=True)
        path.write_text(json.dumps(config));return f
    def verify(self,f,**kwargs):
        return checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**kwargs)
    def test_explicit_variant_and_no_normal_migration_bundle(self):
        f=self.fixture()
        result=self.verify(f,memory_diagnostics=True,worker_stack_bench=True)
        self.assertTrue(result['ota_worker_stack_bench'])
        for args in ({},{'memory_diagnostics':True},{'worker_stack_bench':True},
                     {'memory_diagnostics':True,'worker_stack_bench':1}):
            with self.subTest(args=args),self.assertRaises(ValueError):self.verify(f,**args)
        with self.assertRaises(ValueError):f.prepare_migration()
        self.assertFalse(f.output.exists())
    def test_bench_cannot_carry_provisioning_or_private_key_claim(self):
        f=self.fixture();p=f.build/'ota-local/ota_local_context.json';good=json.loads(p.read_text())
        for key in ('provisioned','private_key_present'):
            value=dict(good);value[key]=True;p.write_text(json.dumps(value))
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'unprovisioned'):
                self.verify(f,memory_diagnostics=True,worker_stack_bench=True)
        p.write_text(json.dumps(good))
    def test_flag_cannot_leak_into_ordinary_profile(self):
        f=self.fixture()
        for profile in ('headless','rev12','rev12-memory-bench','rev12-tls-bench'):
            with self.subTest(profile=profile),self.assertRaises(ValueError):
                checker.verify(f.build,profile,f.sdk,write_report=False,memory_diagnostics=True,worker_stack_bench=True)
    def test_shell_rejects_invalid_scope_before_sdk_or_context(self):
        clean=dict(os.environ)
        for key in ('TV4_OTA_CONTEXT','TV4_OTA_WORKER_STACK_BENCH','TV4_OTA_MEMORY_DIAGNOSTICS'):clean.pop(key,None)
        cases=[('headless','1','1',None),('rev12','1','1',None),('rev12-ota','1','0',None),
               ('rev12-ota','yes','1',None),('rev12-ota','2','1',None),
               ('rev12-ota','1','1','PRIVATE-LOCAL-CONTEXT-NEVER-READ')]
        for profile,bench,trace,context in cases:
            env={**clean,'TV4_OTA_WORKER_STACK_BENCH':bench,'TV4_OTA_MEMORY_DIAGNOSTICS':trace}
            if context:env['TV4_OTA_CONTEXT']=context
            run=subprocess.run([str(ROOT/'tools/build_idf.sh'),profile,'6.1.1','docker'],env=env,text=True,capture_output=True)
            with self.subTest(profile=profile,bench=bench,trace=trace):
                self.assertEqual(run.returncode,2);self.assertEqual(run.stdout,'')
                self.assertNotIn('PRIVATE-LOCAL',run.stderr)
    def test_container_receives_both_explicit_flags_and_default_off(self):
        with tempfile.TemporaryDirectory() as directory:
            docker=Path(directory)/'docker';docker.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n');docker.chmod(0o700)
            env={**os.environ,'PATH':directory+os.pathsep+os.environ['PATH']}
            for key in ('TV4_OTA_CONTEXT','TV4_OTA_WORKER_STACK_BENCH','TV4_OTA_MEMORY_DIAGNOSTICS'):env.pop(key,None)
            for enabled in (False,True):
                run=subprocess.run([str(ROOT/'tools/build_idf.sh'),'rev12-ota','6.1.1','docker'],
                    env={**env,'TV4_OTA_WORKER_STACK_BENCH':str(int(enabled)),'TV4_OTA_MEMORY_DIAGNOSTICS':str(int(enabled))},text=True,capture_output=True)
                self.assertEqual(run.returncode,0,run.stderr)
                self.assertIn('TV4_OTA_WORKER_STACK_BENCH='+str(int(enabled)),run.stdout)
                self.assertIn('TV4_OTA_MEMORY_DIAGNOSTICS='+str(int(enabled)),run.stdout)
                self.assertIn('--ulimit\nnofile=4096:4096\n',run.stdout)
                self.assertNotIn('--device',run.stdout)
    def test_actual_service_preprocessor_guards(self):
        base=[os.environ.get('CC','cc'),'-std=c11','-fsyntax-only','-DESP_PLATFORM=1',
              '-I'+str(ROOT/'tests/ota_service_sdk'),'-I'+str(ROOT/'tests'),
              '-I'+str(ROOT/'firmware/esp32-hmi/ota'),'-I'+str(ROOT/'firmware/esp32-hmi/network')]
        source=str(ROOT/'firmware/esp32-hmi/ota/service.c')
        for enabled in (0,1):
            run=subprocess.run(base+['-DSERVICE_TEST_BENCH='+str(enabled),source],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
        run=subprocess.run(base+['-DSERVICE_TEST_PROVISIONED=1',source],capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr)
        cases=[(['-DSERVICE_TEST_DIAGNOSTICS=0'],'requires explicit OTA memory diagnostics'),
               (['-DSERVICE_TEST_OTA=0'],'requires explicit OTA memory diagnostics'),
               (['-DSERVICE_TEST_PROVISIONED=1'],'must remain unprovisioned')]
        for flags,message in cases:
            run=subprocess.run(base+['-DSERVICE_TEST_BENCH=1']+flags+[source],capture_output=True,text=True)
            with self.subTest(flags=flags):
                self.assertNotEqual(run.returncode,0);self.assertIn(message,run.stderr)

if __name__=='__main__':unittest.main()
