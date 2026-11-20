#!/usr/bin/env python3
"""Fault tests for the dependency integrity gate, in a disposable copy only."""
import argparse
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('source_gate', ROOT / 'tools/check_boot_crypto_source.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name)
        for pattern in ('include/**/*.h', 'library/*.h', 'library/*.c', 'LICENSE'):
            for source in SOURCE.glob(pattern):
                target = self.directory / source.relative_to(SOURCE)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)

    def tearDown(self): self.temp.cleanup()

    def test_exact_source(self):
        self.assertEqual(gate.verify(self.directory, LOCK)['version'], '3.6.7')

    def test_changed_code(self):
        path = self.directory / 'library/ecdsa.c'
        path.write_bytes(path.read_bytes() + b'\n/* changed */\n')
        with self.assertRaises(ValueError): gate.verify(self.directory, LOCK)

    def test_missing_header(self):
        (self.directory / 'include/mbedtls/ecp.h').unlink()
        with self.assertRaises(ValueError): gate.verify(self.directory, LOCK)

    def test_added_header(self):
        (self.directory / 'include/mbedtls/unreviewed.h').write_text('/* injected */\n')
        with self.assertRaises(ValueError): gate.verify(self.directory, LOCK)

    def test_symlink(self):
        path = self.directory / 'library/ecdsa.c'
        path.unlink()
        path.symlink_to(SOURCE / 'library/ecdsa.c')
        with self.assertRaises(ValueError): gate.verify(self.directory, LOCK)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    args = parser.parse_args()
    SOURCE = Path(args.source)
    LOCK = ROOT / 'third_party/mbedtls/origin.json'
    unittest.main(argv=['source_tests'], verbosity=2)
