"""Compile real host ELF/archive negatives for the callback safety inspector."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import check_ota_dma_diagnostics as check


class Artifacts(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
    def compile(self,name,source,link=False):
        c=self.root/(name+'.c');c.write_text(source)
        out=self.root/(name+('.elf' if link else '.obj'))
        cmd=[os.environ.get('CC','cc'),'-std=c11','-ffunction-sections',str(c),'-o',str(out)]
        if not link:cmd.insert(1,'-c')
        subprocess.run(cmd,check=True,capture_output=True);return out
    def archive(self,name,objects):
        p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(['ar','rcs',str(p),*map(str,objects)],check=True,capture_output=True);return p
    def test_actual_archive_other_registration_refused(self):
        caller='extern int heap_caps_register_failed_alloc_callback(void *); int setup(void*p){return heap_caps_register_failed_alloc_callback(p);}'
        obj=self.compile('dma_diagnostics.c',caller)
        main=self.archive('esp-idf/main/libmain.a',[obj])
        owners=check.registration_owners([main]);check.require_owners(owners,True)
        with self.assertRaises(ValueError):check.require_owners(owners,False)
        other=self.compile('foreign',caller.replace('setup','other_setup'))
        foreign=self.archive('esp-idf/other/libother.a',[other])
        with self.assertRaises(ValueError):check.require_owners(check.registration_owners([main,foreign]),True)
        with self.assertRaises(ValueError):check.require_owners([],True)
    def test_actual_callback_unsafe_import_refused(self):
        declarations='extern void xPortEnterCriticalTimeout(void), xPortInIsrContext(void), xTaskGetCurrentTaskHandle(void), vPortExitCritical(void);'
        body='xPortEnterCriticalTimeout();xPortInIsrContext();xTaskGetCurrentTaskHandle();vPortExitCritical();'
        good=self.compile('good',declarations+'__attribute__((section(".iram1"))) void callback(void){'+body+'}')
        self.assertEqual(check.callback_relocations(good),check.RTOS-{'vPortEnterCritical'})
        bad=self.compile('bad',declarations+'extern void *heap_caps_malloc(int);__attribute__((section(".iram1"))) void callback(void){'+body+'(void)heap_caps_malloc(1);}')
        with self.assertRaisesRegex(ValueError,'heap_caps_malloc'):check.callback_relocations(bad)
    def test_actual_flash_callee_refused(self):
        names={check.CALLBACK,'xPortEnterCriticalTimeout','xPortInIsrContext','xTaskGetCurrentTaskHandle','vPortExitCritical'}
        for wrong in [False,True]:
            source='\n'.join('__attribute__((section("'+('.flash.text' if wrong and n=='xTaskGetCurrentTaskHandle' else '.iram0.text')+'"))) void '+n+'(void){}' for n in sorted(names))+'\nint main(void){return 0;}'
            elf=self.compile('placement'+str(wrong),source,True)
            if wrong:
                with self.assertRaisesRegex(ValueError,'xTaskGetCurrentTaskHandle'):check.require_iram(check.symbols(elf),check.sections(elf),names)
            else:self.assertEqual(set(check.require_iram(check.symbols(elf),check.sections(elf),names)),names)


if __name__=='__main__':unittest.main()
