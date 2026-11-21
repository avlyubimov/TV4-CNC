"""Reject corrupt real bench artifacts and bad links. No ARM/device execution."""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import check_stm32_bench as checker

args = None
FLAGS = ['-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard']


class BenchImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tv4-bench-contract-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.elf = args.build / 'tv4_stm32f407_bench.elf'
        self.binary = args.build / 'tv4_stm32f407_bench.bin'
        self.mapfile = args.build / 'tv4_stm32f407_bench.map'
        self.parsed = checker.Elf(self.elf)

    def changed(self, edits):
        raw = bytearray(self.parsed.raw)
        for offset, value, fmt in edits:
            struct.pack_into(fmt, raw, offset, value)
        path = self.root / 'changed.elf'
        path.write_bytes(raw)
        return path

    def refuse(self, edits):
        with self.assertRaises(ValueError):
            checker.inspect(self.changed(edits), self.binary, self.mapfile)

    def test_real_image(self):
        result = checker.inspect(self.elf, self.binary, self.mapfile)
        self.assertEqual(result['vectors'], 98)
        self.assertEqual(result['reserved_indices'], [7, 8, 9, 10, 13, 95])
        self.assertLessEqual(result['bin_bytes'], 16384)
        self.assertFalse(result['hardware_tested'])
        self.assertFalse(result['motion_outputs'])
        checker.code_check(self.elf, args.compiler.replace('gcc', 'objdump'))
        checker.build_check(args.build)

    def test_elf_header_truncation_and_architecture(self):
        for length in (0, 51, 100, len(self.parsed.raw) - 1):
            with self.subTest(length=length):
                path = self.root / 'short.elf'
                path.write_bytes(self.parsed.raw[:length])
                with self.assertRaises(ValueError):
                    checker.inspect(path, self.binary, self.mapfile)
        for edit in ((18, 3, '<H'), (24, checker.FLASH, '<I'), (28, 0xffffffff, '<I'),
                     (32, 0xffffffff, '<I'), (42, 16, '<H'), (48, 0, '<H')):
            with self.subTest(edit=edit):
                self.refuse([edit])

    def test_stack_and_each_vector(self):
        offset = self.parsed.section('.isr_vector')['offset']
        for index in range(98):
            # Includes every reserved vector, specifically external IRQ79.
            value = checker.FLASH | 1 if index in checker.RESERVED else 0
            with self.subTest(index=index):
                self.refuse([(offset + 4 * index, value, '<I')])
        self.refuse([(offset, checker.SRAM_END - 4, '<I')])

    def test_section_origin_size_alignment_permissions(self):
        for name, field, value in (('.isr_vector', 12, checker.FLASH + 512),
                                  ('.isr_vector', 20, 64), ('.isr_vector', 32, 128),
                                  ('.text', 8, 7), ('.bss', 8, 7),
                                  ('.ccm_bss', 12, checker.CCM_END),
                                  ('.bss', 12, checker.SRAM_END),
                                  ('.noinit', 4, 1), ('.noinit', 20, 80),
                                  ('.stacks', 12, checker.SRAM),
                                  ('.data', 12, checker.CCM)):
            with self.subTest(name=name, field=field):
                self.refuse([(self.parsed.section(name)['header_offset'] + field, value, '<I')])

    def test_programming_range_never_loads_ram(self):
        phoff = struct.unpack_from('<I', self.parsed.raw, 28)[0]
        for index, segment in enumerate(self.parsed.segments):
            if segment[0] == 1 and segment[4]:
                with self.subTest(index=index):
                    self.refuse([(phoff + index * 32 + 12, checker.SRAM, '<I')])
                    self.refuse([(phoff + index * 32 + 16, segment[5] + 1, '<I')])

    def test_bin_and_map_must_match_elf(self):
        raw = self.binary.read_bytes()
        for value in (raw[:-1], raw + b'\0', raw[:392] + bytes([raw[392] ^ 1]) + raw[393:]):
            path = self.root / 'bad.bin'
            path.write_bytes(value)
            with self.assertRaises(ValueError):
                checker.inspect(self.elf, path, self.mapfile)
        path = self.root / 'bad.map'
        path.write_text(self.mapfile.read_text().replace('FLASH', 'wrong', 1))
        with self.assertRaises(ValueError):
            checker.inspect(self.elf, self.binary, path)

    def test_identity_semihosting_and_fpu_refused(self):
        section = self.parsed.section('.rodata')
        offset = section['offset'] + self.parsed.symbol('tv4_bench_build') - section['address']
        self.refuse([(offset + 8, 168000000, '<I')])
        text = self.parsed.section('.text')
        offset = text['offset'] + (self.parsed.symbol('tv4_bench_halt') & ~1) - text['address']
        for instruction in (0xbeab, 0xdf00):  # BKPT semihosting, SVC.
            with self.subTest(instruction=instruction), self.assertRaises(ValueError):
                checker.code_check(self.changed([(offset, instruction, '<H')]),
                                   args.compiler.replace('gcc', 'objdump'))
        # Assemble genuine FP encodings, including an instruction outside the
        # former hand-picked vldr/vstr list. Mutate only a temporary ELF copy.
        for instruction in ('vmrs r0, fpscr', 'vstmia r0!, {s0-s1}'):
            source, obj, binary = (self.root / n for n in ('fp.S', 'fp.o', 'fp.bin'))
            source.write_text('.syntax unified\n.thumb\n.fpu fpv4-sp-d16\n.text\n' + instruction + '\n')
            subprocess.run([args.compiler, *FLAGS, '-c', str(source), '-o', str(obj)],
                           capture_output=True, check=True)
            subprocess.run([args.compiler.replace('gcc', 'objcopy'), '-O', 'binary', '-j', '.text',
                            str(obj), str(binary)], capture_output=True, check=True)
            encoded = binary.read_bytes()
            self.assertEqual(len(encoded), 4)
            with self.subTest(instruction=instruction), self.assertRaisesRegex(ValueError, 'FPU instructions'):
                checker.code_check(self.changed([(offset, int.from_bytes(encoded, 'little'), '<I')]),
                                   args.compiler.replace('gcc', 'objdump'))

    def test_profile_and_pinned_reference_refused(self):
        import json
        import shutil
        board = self.root / 'board'
        board.mkdir()
        shutil.copy(checker.BOARD / 'openocd.cfg', board)
        profile = json.loads((checker.BOARD / 'profile.json').read_text())
        profile['flash']['bytes'] = 2 * 1024 * 1024
        (board / 'profile.json').write_text(json.dumps(profile))
        with patch.object(checker, 'BOARD', board), self.assertRaises(ValueError):
            checker.references()
        # The real board lost its debug path on resume without DBG_SLEEP.
        # Check configuration refusal independently of the bad profile case.
        (board / 'profile.json').write_text((checker.BOARD / 'profile.json').read_text())
        for replacement in ('# missing DBG_SLEEP', '# mmw 0xE0042004 0x00000001 0'):
            (board / 'openocd.cfg').write_text((checker.BOARD / 'openocd.cfg').read_text().replace(
                'mmw 0xE0042004 0x00000001 0', replacement))
            with patch.object(checker, 'BOARD', board), self.assertRaisesRegex(ValueError, 'DBG_SLEEP'):
                checker.references()
        reference = self.root / 'third_party/stm32f4-reference'
        shutil.copytree(ROOT / 'third_party/stm32f4-reference', reference)
        with (reference / 'startup_stm32f407xx.s').open('a') as stream:
            stream.write('\n/* changed upstream */\n')
        with patch.object(checker, 'ROOT', self.root), self.assertRaises(ValueError):
            checker.references()

    def test_compiler_stack_and_bin_allowlist(self):
        import shutil
        build = self.root / 'build'
        objects = Path('CMakeFiles/tv4_stm32f407_bench.dir')
        shutil.copytree(args.build / objects, build / objects)
        checker.build_check(build)
        (build / 'not-the-bench.bin').write_bytes(b'invalid')
        with self.assertRaisesRegex(ValueError, 'unexpected firmware BIN'):
            checker.build_check(build)
        (build / 'not-the-bench.bin').unlink()
        stack = next((build / objects).rglob('*.su'))
        stack.write_text('source:1:1:oversized\t4096\tstatic\n')
        with self.assertRaisesRegex(ValueError, 'compiler frame budget'):
            checker.build_check(build)

    def link(self, memory, extra=()):
        script = self.root / 'memory.ld'
        script.write_text(memory)
        output = self.root / 'linked.elf'
        output.unlink(missing_ok=True)
        result = subprocess.run([args.compiler, *FLAGS, '-nostdlib',
                                 '-Wl,--gc-sections,--no-undefined,--fatal-warnings,--orphan-handling=error,--build-id=none',
                                 '-Wl,-z,noexecstack', '-Wl,-T,' + str(script),
                                 *(args.build / 'bench-inputs.txt').read_text().splitlines(),
                                 *extra, '-o', str(output)], capture_output=True, text=True, timeout=10)
        return result, output

    def reject_link(self, script, cause, extra=()):
        result, output = self.link(script, extra)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertRegex(result.stderr, cause)
        self.assertFalse(output.exists(), 'a failed link must leave no usable image')

    def test_actual_objects_relink(self):
        result, _ = self.link((checker.BOARD / 'memory.ld').read_text())
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_flash_ram_ccm_and_stack_overflow(self):
        script = (checker.BOARD / 'memory.ld').read_text()
        for before, after, cause in (('LENGTH = 1024K', 'LENGTH = 512', 'FLASH|overflow'),
                                     ('LENGTH = 128K', 'LENGTH = 4096', 'SRAM|stack|overflow'),
                                     ('LENGTH = 64K', 'LENGTH = 0', 'CCM|overflow'),
                                     ('__bench_flash_budget__ = 16K', '__bench_flash_budget__ = 512', 'Flash budget'),
                                     ('__main_stack_bytes__ = 4096', '__main_stack_bytes__ = 131072', 'SRAM|stack'),
                                     ('__fault_stack_bytes__ = 1024', '__fault_stack_bytes__ = 1023', 'alignment')):
            with self.subTest(before=before):
                self.assertIn(before, script)
                self.reject_link(script.replace(before, after), cause)

    def test_constructors_orphans_and_runtime_not_discarded(self):
        script = (checker.BOARD / 'memory.ld').read_text()
        for name, cause in (('.init_array', 'runtime/constructors/unwind'),
                            ('.ARM.extab', 'runtime/constructors/unwind'),
                            ('.got', 'runtime/constructors/unwind'),
                            ('.unexpected_driver', 'orphan')):
            with self.subTest(section=name):
                source = self.root / 'extra.S'
                source.write_text('.syntax unified\n.section ' + name + ',"a"\n'
                                  '.global tv4_forbidden_fixture\ntv4_forbidden_fixture: .word 1\n')
                obj = self.root / 'extra.o'
                subprocess.run([args.compiler, *FLAGS, '-c', str(source), '-o', str(obj)],
                               capture_output=True, check=True)
                self.reject_link(script, cause, [str(obj), '-Wl,--undefined=tv4_forbidden_fixture'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--compiler', required=True)
    args, rest = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)
