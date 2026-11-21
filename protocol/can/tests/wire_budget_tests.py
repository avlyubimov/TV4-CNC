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
ARCHIVE = 'libtv4_update_wire.a'
TARGET = 'tv4_update_wire'
EXPORTS = ('tv4_update_wire_encode', 'tv4_update_wire_decode')
TEXT_CEILING = 4096


class Budget(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tv4-wire-budget-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copyfile(BUILD / ARCHIVE, self.root / ARCHIVE)
        self.frames = self.root / f'CMakeFiles/{TARGET}.dir/check.su'
        self.frames.parent.mkdir(parents=True)
        self.frames.write_text('fixture.c:1:1:check\t32\tstatic\n')

    def archive(self, extra):
        # Throwaway Cortex-M4 objects, not board ELFs or firmware images.
        source = self.root / 'fixture.c'
        source.write_text(''.join(f'void {name}(void){{}}\n' for name in EXPORTS) + extra)
        subprocess.run(['arm-none-eabi-gcc', '-mcpu=cortex-m4', '-mthumb', '-Os', '-c', str(source),
                        '-o', str(self.root / 'fixture.o')], check=True, capture_output=True)
        (self.root / ARCHIVE).unlink()
        subprocess.run(['arm-none-eabi-ar', 'rcs', str(self.root / ARCHIVE),
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
        self.archive(f'const unsigned char tv4_test_rodata[{TEXT_CEILING + 1}]={{1}};\n')
        with self.assertRaisesRegex(ValueError, 'object budget'): guard.inspect(self.root)

    def test_unreviewed_import(self):
        self.archive('extern void tv4_unapproved(void);\nvoid tv4_test_bad_import(void){tv4_unapproved();}\n')
        with self.assertRaisesRegex(ValueError, 'unreviewed .* import'): guard.inspect(self.root)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True, type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--fragments', action='store_true')
    mode.add_argument('--metadata', action='store_true')
    mode.add_argument('--setup', action='store_true')
    mode.add_argument('--channel', action='store_true')
    mode.add_argument('--egress', action='store_true')
    args = parser.parse_args()
    BUILD = args.build.absolute()
    if args.fragments:
        import check_update_fragment_build as guard
        ARCHIVE = 'libtv4_update_fragments.a'
        TARGET = 'tv4_update_fragments'
        EXPORTS = tuple('tv4_fragment_' + suffix for suffix in (
            'tx_init', 'rx_init', 'tx_begin', 'tx_peek', 'tx_commit', 'rx_receive',
            'rx_take', 'tx_poll', 'rx_poll', 'tx_cancel', 'rx_cancel', 'tx_clear', 'rx_clear',
            'tx_init_raw', 'rx_init_raw', 'tx_begin_raw', 'rx_take_raw'))
    if args.metadata:
        import check_update_metadata_build as guard
        ARCHIVE = 'libtv4_update_metadata.a'
        TARGET = 'tv4_update_metadata'
        EXPORTS = ('tv4_metadata_encode', 'tv4_metadata_decode', 'tv4_metadata_rx_init',
                   'tv4_metadata_rx_begin', 'tv4_metadata_rx_poll', 'tv4_metadata_rx_receive',
                   'tv4_metadata_rx_take', 'tv4_metadata_rx_discard')
    if args.setup:
        import check_metadata_setup_build as guard
        ARCHIVE = 'libtv4_metadata_setup.a'
        TARGET = 'tv4_metadata_setup'
        TEXT_CEILING = 4096
        EXPORTS = tuple('tv4_metadata_setup_' + suffix for suffix in (
            'encode', 'decode', 'init', 'request', 'respond', 'receive', 'next', 'poll',
            'take', 'close_tx', 'close_rx', 'abort', 'clear'))
    if args.channel:
        import check_update_channel_build as guard
        ARCHIVE = 'libtv4_update_channel.a'
        TARGET = 'tv4_update_channel'
        EXPORTS = tuple('tv4_channel_' + suffix for suffix in (
            'tx_init', 'rx_init', 'tx_begin', 'tx_peek', 'tx_commit', 'rx_receive', 'rx_take',
            'tx_poll', 'rx_poll', 'tx_advance', 'rx_advance', 'tx_cancel', 'rx_cancel', 'tx_clear', 'rx_clear'))
    if args.egress:
        import check_update_egress_build as guard
        ARCHIVE = 'libtv4_update_egress.a'
        TARGET = 'tv4_update_egress'
        EXPORTS = tuple('tv4_frame_queue_' + suffix for suffix in (
            'init', 'admit', 'claim', 'complete', 'poll', 'cancel', 'clear', 'quiescent'))
        EXPORTS += tuple('tv4_update_egress_' + suffix for suffix in (
            'init', 'begin', 'pump', 'claim', 'complete', 'poll', 'advance', 'cancel', 'clear'))
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Budget))
    if not result.wasSuccessful(): raise SystemExit(1)
    print('PASS6 ARM budget guard cases; temporary negative objects removed, no board image or execution')
