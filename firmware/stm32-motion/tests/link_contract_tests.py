"""Cross-link/ELF fault tests. No ARM execution, emulator, SWD or board memory map."""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import check_motion_link as checker

args = None


class LinkContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tv4-motion-link-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.memory = args.memory.read_text()
        self.inputs = args.objects.read_text().splitlines()
        self.output = self.root / 'synthetic.elf'

    def link(self, memory, extra=()):
        command = [args.compiler, '-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard',
                   '-nostdlib', '-Wl,--gc-sections,--no-undefined,--fatal-warnings,--orphan-handling=error,--build-id=none',
                   '-Wl,-z,noexecstack', '-Wl,--undefined=tv4_probe_data,--undefined=tv4_probe_bss']
        if memory is not None:
            path = self.root / 'memory.ld'
            path.write_text(memory)
            command.append('-Wl,-T,' + str(path))
        command += ['-Wl,-T,' + str(args.sections), *self.inputs, *extra, '-o', str(self.output)]
        return subprocess.run(command, capture_output=True, text=True, timeout=15)

    def rejected(self, memory, cause, extra=()):
        result = self.link(memory, extra)
        self.assertNotEqual(result.returncode, 0)
        self.assertRegex(result.stderr, cause)
        self.assertFalse(self.output.exists(), 'failed link must not leave a usable ELF')

    def test_valid_probe(self):
        result = checker.inspect(args.elf)
        self.assertFalse(result['deployable'])
        self.assertFalse(result['physical_memory_verified'])
        self.assertEqual(result['vectors_bytes'], 64)
        compiler = Path(args.compiler)
        objdump = str(compiler.with_name(compiler.name.replace('gcc', 'objdump')))
        checker.check_reset(args.elf, objdump)
        link = self.link(self.memory)
        self.assertEqual(link.returncode, 0, link.stderr)
        self.assertEqual(checker.inspect(self.output), result)

    def test_missing_memory_or_stack(self):
        self.rejected(None, r'ROM|RAM|stack')
        self.rejected(self.memory.replace('__stack_bytes = 2048;', ''), r'stack')

    def test_rom_overflow(self):
        self.rejected(self.memory.replace('LENGTH = 64K', 'LENGTH = 64'), r'ROM|overflow')

    def test_ram_stack_overflow(self):
        self.rejected(self.memory.replace('LENGTH = 16K', 'LENGTH = 512'), r'RAM|stack')

    def test_data_bss_collide_with_stack(self):
        self.rejected(self.memory.replace('2048', '16384'), r'collide with stack')

    def test_zero_and_unaligned_stack(self):
        self.rejected(self.memory.replace('2048', '0'), r'stack')
        self.rejected(self.memory.replace('2048', '2047'), r'stack alignment')

    def test_unaligned_ram(self):
        self.rejected(self.memory.replace('0x30000000', '0x30000004'), r'stack alignment')

    def test_unaligned_rom(self):
        self.rejected(self.memory.replace('0x01000000', '0x01000004'), r'vector alignment')

    def test_overlapping_memory(self):
        self.rejected(self.memory.replace('0x01000000', '0x30000000'), r'overlap')

    def test_constructor_not_silently_ignored(self):
        source = self.root / 'ctor.c'
        source.write_text('static void callback(void) {}\nvoid (*p)(void) __attribute__((section(".init_array"))) = callback;\n')
        obj = self.root / 'ctor.o'
        subprocess.run([args.compiler, '-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard',
                        '-ffreestanding', '-c', str(source), '-o', str(obj)], check=True, capture_output=True)
        self.rejected(self.memory, r'constructors/destructors', [str(obj)])

    def test_mutated_vectors_entry_and_truncation(self):
        raw = args.elf.read_bytes()
        shoff = struct.unpack_from('<I', raw, 32)[0]
        shsize, shnum = struct.unpack_from('<HH', raw, 46)
        vector_offset = None
        for i in range(shnum):
            section = struct.unpack_from('<IIIIIIIIII', raw, shoff + i * shsize)
            if section[3] == checker.ROM and section[5] == 64:
                vector_offset = section[4]
                vector_section_offset = shoff + i * shsize
        self.assertIsNotNone(vector_offset)
        for offset, value in ((vector_offset, checker.RAM),
                              (vector_offset + 4, checker.ROM), (24, checker.ROM),
                              (vector_section_offset + 12, 0x08000000)):
            with self.subTest(offset=offset):
                changed = bytearray(raw)
                struct.pack_into('<I', changed, offset, value)
                path = self.root / 'bad.elf'
                path.write_bytes(changed)
                with self.assertRaises(ValueError):
                    checker.inspect(path)
        path = self.root / 'short.elf'
        path.write_bytes(raw[:100])
        with self.assertRaises(ValueError):
            checker.inspect(path)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('elf', 'objects', 'sections', 'memory'):
        p.add_argument('--' + name, required=True, type=Path)
    p.add_argument('--compiler', required=True)
    args, remaining = p.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
