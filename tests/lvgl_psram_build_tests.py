"""Compile actual LVGL adapter guards with the pinned headers, no hardware."""
import os
from pathlib import Path
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
LVGL=Path(sys.argv.pop(1))

class Guards(unittest.TestCase):
    def run_compile(self,flags):
        return subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
            '-fsyntax-only','-I'+str(ROOT/'tests'),'-I'+str(LVGL),*flags,
            str(ROOT/'firmware/esp32-hmi/ui/lvgl_psram.c')],capture_output=True,text=True)
    def test_custom_and_ordinary(self):
        self.assertEqual(self.run_compile([]).returncode,0)
        r=self.run_compile(['-DTV4_LVGL_PSRAM_TEST=1','-DLV_CONF_SKIP=1','-DLV_USE_STDLIB_MALLOC=255'])
        self.assertEqual(r.returncode,0,r.stderr)
    def test_stale_clib_or_builtin_config_refused(self):
        for mode in (0,1):
            r=self.run_compile(['-DTV4_LVGL_PSRAM_TEST=1','-DLV_CONF_SKIP=1','-DLV_USE_STDLIB_MALLOC='+str(mode)])
            self.assertNotEqual(r.returncode,0);self.assertIn('OTA requires LVGL CUSTOM malloc',r.stderr)
    def test_wrong_board_psram_or_cache_config_refused(self):
        for key in ('CONFIG_TV4_BOARD_REV12','CONFIG_SPIRAM','CONFIG_SPIRAM_XIP_FROM_PSRAM'):
            r=self.run_compile(['-DTV4_LVGL_PSRAM_TEST=1','-DLV_CONF_SKIP=1','-DLV_USE_STDLIB_MALLOC=255','-D'+key+'=0'])
            self.assertNotEqual(r.returncode,0);self.assertIn('requires the verified Rev1.2 PSRAM-XiP profile',r.stderr)

if __name__=='__main__':unittest.main()
