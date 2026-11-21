"""Negative checks of actual ARM archive/frame guard. Nothing is executed/flashed."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools'))
import check_update_wire_build as guard

BUILD = None


class Budget(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tv4-wire-budget-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copyfile(BUILD / 'libtv4_update_wire.a', self.root / 'libtv4_update_wire.a')
        self.frames = self.root / 'CMakeFiles/tv4_update_wire.dir/check.su'
        self.frames.parent.mkdir(parents=True)
        self.frames.write_text('fixture.c:1:1:check\t32\tstatic\n')

    def archive(self, extra):
        # Throwaway Cortex-M4 objects, not board ELFs or firmware images.
        source = self.root / 'fixture.c'
        source.write_text('void tv4_update_wire_encode(void){}\nvoid tv4_update_wire_decode(void){}\n' + extra)
        subprocess.run(['arm-none-eabi-gcc', '-mcpu=cortex-m4', '-mthumb', '-Os', '-c', str(source),
                        '-o', str(self.root / 'fixture.o')], check=True, capture_output=True)
        (self.root / 'libtv4_update_wire.a').unlink()
        subprocess.run(['arm-none-eabi-ar', 'rcs', str(self.root / 'libtv4_update_wire.a'),
                        str(self.root / 'fixture.o')], check=True, capture_output=True)

    def test_real_archive(self):
        self.assertFalse(guard.inspect(BUILD)['device_access'])

    def test_missing_frame_or_dynamic_frame(self):
        for text in ('', 'fixture.c:1:1:check\t32\tdynamic,bounded\n', 'fixture.c:1:1:check\t-1\tstatic\n'):
            self.frames.write_text(text)
            with self.assertRaises(ValueError): guard.inspect(self.root)

    def test_excessive_frame(self):
        self.frames.write_text('fixture.c:1:1:check\t129\tstatic\n')
        with self.assertRaisesRegex(ValueError, 'frame budget'): guard.inspect(self.root)

    def test_mutable_globals(self):
        self.archive('int tv4_test_state=1;\n')
        with self.assertRaisesRegex(ValueError, 'object budget'): guard.inspect(self.root)

    def test_excessive_code_or_rodata(self):
        self.archive('const unsigned char tv4_test_rodata[4097]={1};\n')
        with self.assertRaisesRegex(ValueError, 'object budget'): guard.inspect(self.root)

    def test_unreviewed_import(self):
        self.archive('extern void tv4_unapproved(void);\nvoid tv4_test_bad_import(void){tv4_unapproved();}\n')
        with self.assertRaisesRegex(ValueError, 'unreviewed codec import'): guard.inspect(self.root)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True, type=Path)
    args = parser.parse_args()
    BUILD = args.build.absolute()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Budget))
    if not result.wasSuccessful(): raise SystemExit(1)
    print('PASS6 ARM budget guard cases; temporary negative objects removed, no board image or execution')
