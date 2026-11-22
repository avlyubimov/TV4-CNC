"""Opt-in image isolation and actual C guards, without SDK/network/devices."""
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
        config.update(TV4_OTA_MEMORY_DIAGNOSTICS=True,TV4_OTA_WORKER_STACK_BENCH=True,TV4_OTA_WORKER_TLS_BENCH=True)
        path.write_text(json.dumps(config));return f
    def verify(self,f,**kwargs):
        return checker.verify(f.build,'rev12-ota',f.sdk,write_report=False,**kwargs)
    def test_all_three_explicit_flags_required(self):
        f=self.fixture();good=dict(memory_diagnostics=True,worker_stack_bench=True,worker_tls_bench=True)
        self.assertTrue(self.verify(f,**good)['ota_worker_tls_bench'])
        for key in good:
            args=dict(good);args[key]=False
            with self.subTest(key=key),self.assertRaises(ValueError):self.verify(f,**args)
        with self.assertRaises(ValueError):self.verify(f,**{**good,'worker_tls_bench':1})
        with self.assertRaises(ValueError):f.prepare_migration()
        self.assertFalse(f.output.exists())
    def test_context_and_other_profiles_rejected(self):
        f=self.fixture();args=dict(memory_diagnostics=True,worker_stack_bench=True,worker_tls_bench=True)
        for profile in ('headless','rev12','rev12-memory-bench','rev12-tls-bench'):
            with self.subTest(profile=profile),self.assertRaises(ValueError):
                checker.verify(f.build,profile,f.sdk,write_report=False,**args)
        path=f.build/'ota-local/ota_local_context.json';context=json.loads(path.read_text())
        for key in ('provisioned','private_key_present'):
            value=dict(context);value[key]=True;path.write_text(json.dumps(value))
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'unprovisioned'):self.verify(f,**args)
    def clean_env(self):
        env=dict(os.environ)
        for key in ('TV4_OTA_CONTEXT','TV4_OTA_MEMORY_DIAGNOSTICS','TV4_OTA_WORKER_STACK_BENCH','TV4_OTA_WORKER_TLS_BENCH'):
            env.pop(key,None)
        return env
    def test_shell_rejects_before_sdk_or_context(self):
        for profile,trace,stack,tls,context in (
            ('headless','1','1','1',None),('rev12','1','1','1',None),
            ('rev12-ota','0','1','1',None),('rev12-ota','1','0','1',None),
            ('rev12-ota','1','1','true',None),('rev12-ota','1','1','2',None),
            ('rev12-ota','1','1','1','PRIVATE-CONTEXT-MUST-NOT-BE-READ')):
            env={**self.clean_env(),'TV4_OTA_MEMORY_DIAGNOSTICS':trace,
                'TV4_OTA_WORKER_STACK_BENCH':stack,'TV4_OTA_WORKER_TLS_BENCH':tls}
            if context:env['TV4_OTA_CONTEXT']=context
            run=subprocess.run([str(ROOT/'tools/build_idf.sh'),profile,'6.1.1','docker'],env=env,text=True,capture_output=True)
            with self.subTest(profile=profile,trace=trace,stack=stack,tls=tls):
                self.assertEqual(run.returncode,2);self.assertEqual(run.stdout,'');self.assertNotIn('PRIVATE-CONTEXT',run.stderr)
    def test_docker_explicit_and_default_off(self):
        with tempfile.TemporaryDirectory() as directory:
            docker=Path(directory)/'docker';docker.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n');docker.chmod(0o700)
            for enabled in (0,1):
                env={**self.clean_env(),'PATH':directory+os.pathsep+os.environ['PATH'],
                    **{k:str(enabled) for k in ('TV4_OTA_MEMORY_DIAGNOSTICS','TV4_OTA_WORKER_STACK_BENCH','TV4_OTA_WORKER_TLS_BENCH')}}
                run=subprocess.run([str(ROOT/'tools/build_idf.sh'),'rev12-ota','6.1.1','docker'],env=env,text=True,capture_output=True)
                self.assertEqual(run.returncode,0,run.stderr)
                self.assertIn('TV4_OTA_WORKER_TLS_BENCH='+str(enabled),run.stdout)
                self.assertIn('--ulimit\nnofile=4096:4096\n',run.stdout);self.assertNotIn('--device',run.stdout)
    def test_actual_c_guards(self):
        base=[os.environ.get('CC','cc'),'-std=c11','-fsyntax-only','-DESP_PLATFORM=1','-DTLS_TEST_SDK=6',
            '-I'+str(ROOT/'tests/ota_tls_sdk'),'-I'+str(ROOT/'tests/ota_service_sdk'),'-I'+str(ROOT/'tests'),
            '-I'+str(ROOT/'firmware/esp32-hmi/ota'),'-I'+str(ROOT/'firmware/esp32-hmi/network')]
        good=['-DSERVICE_TEST_BENCH=1','-DSERVICE_TEST_TLS_BENCH=1']
        for source in ('service','utc_tls_bench'):
            path=str(ROOT/f'firmware/esp32-hmi/ota/{source}.c')
            run=subprocess.run(base+good+[path],capture_output=True,text=True);self.assertEqual(run.returncode,0,run.stderr)
            for flags in (['-DSERVICE_TEST_BENCH=0','-DSERVICE_TEST_TLS_BENCH=1'],
                good+['-DSERVICE_TEST_DIAGNOSTICS=0'],good+['-DSERVICE_TEST_OTA=0'],
                good+['-DTLS_TEST_STANDALONE=1'],good+['-DSERVICE_TEST_PROVISIONED=1']):
                run=subprocess.run(base+flags+[path],capture_output=True,text=True)
                with self.subTest(source=source,flags=flags):self.assertNotEqual(run.returncode,0)

if __name__=='__main__':unittest.main()
