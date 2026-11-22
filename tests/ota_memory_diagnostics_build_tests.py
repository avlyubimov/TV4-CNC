"""Fault checks for the actual object/frame inspector, no executable images."""
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import check_ota_memory_diagnostics as check

class Guards(unittest.TestCase):
    def test_writable_regions_not_debug_or_rodata(self):
        text='''  [ 1] .text.f PROGBITS 00000000 000034 0000a0 00 AX 0 0 4
  [ 2] .bss.startup NOBITS 00000000 0000d4 0000fc 00 WA 0 0 4
  [ 3] .data.owner PROGBITS 00000000 0000d4 000008 00 WA 0 0 4
  [12] .rodata.str PROGBITS 00000000 0000dc 000040 01 AMS 0 0 1
'''
        self.assertEqual(check.writable_bytes(text),260)
    def test_bounded_static_or_bounded_dynamic_only(self):
        good='/source/memory_diagnostics.c:1:3:capture\t128\tstatic\n/source/memory_diagnostics.c:2:1:report\t192\tdynamic,bounded\n'
        self.assertEqual(check.frames(good),{'capture':128,'report':192})
        for bad in ('',good.replace('192','208'),good.replace('dynamic,bounded','dynamic')):
            with self.subTest(bad=bad),self.assertRaises(ValueError):check.frames(bad)
    def test_existing_profile_owner_guards(self):
        self.assertEqual(len(check.source_owner_guard()),2)

if __name__=='__main__':unittest.main()
