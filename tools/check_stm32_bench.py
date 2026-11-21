#!/usr/bin/env python3
"""Check the actual STM32F407VG bench ELF/BIN/map, never execute or flash it."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / 'firmware/stm32-motion/board/stm32f407vg'
FLASH, FLASH_END = 0x08000000, 0x08100000
SRAM, SRAM_END, CCM, CCM_END = 0x20000000, 0x20020000, 0x10000000, 0x10010000
PROFILE = 'stm32f407vg-hsi-no-motion-v1'
RESERVED = {7, 8, 9, 10, 13, 95}


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def references():
    origin = json.loads((ROOT / 'third_party/stm32f4-reference/origin.json').read_text())
    expected = origin['vendored_sha256']
    require(set(expected) == {'startup_stm32f407xx.s', 'LICENSE.md'}, 'reference allowlist')
    for name, sha in expected.items():
        require(digest((ROOT / 'third_party/stm32f4-reference' / name).read_bytes()) == sha,
                'pinned ST reference mismatch: ' + name)
    require(origin['cmsis_device_f4']['commit'] == '0fa0e489e053fa1ca7790bb40b4d76458f64c55d' and
            origin['runtime_dependency'] is False, 'ST version/reference scope')
    raw = (ROOT / 'third_party/stm32f4-reference/startup_stm32f407xx.s').read_text()
    table = raw.split('g_pfnVectors:', 1)[1].split('.size  g_pfnVectors', 1)[0]
    names = re.findall(r'^\s*\.word\s+(\w+)', table, re.M)
    p = json.loads((BOARD / 'profile.json').read_text())
    require(p['schema'] == 'tv4-stm32-bench-profile-1' and p['profile'] == PROFILE, 'unreviewed profile')
    require(p['vectors'] == names and len(names) == 98 and
            {i for i, n in enumerate(names) if n == '0'} == RESERVED, 'full F407 vectors/reserved IRQ79')
    require(p['flash'] == dict(origin=FLASH, bytes=1024 * 1024) and
            p['sram1'] == dict(origin=SRAM, bytes=112 * 1024) and
            p['sram2'] == dict(origin=0x2001c000, bytes=16 * 1024), 'Flash/SRAM profile')
    require(p['ccm'] == dict(origin=CCM, bytes=64 * 1024, data_only=True, dma_access=False), 'CCM profile')
    require(p['vector_alignment'] == 512 and p['hsi_hz'] == 16000000 and
            p['flash_budget_bytes'] == 16384 and p['main_stack_bytes'] == 4096 and
            p['fault_stack_bytes'] == 1024 and p['stack_guard_bytes'] == 32 and
            p['bootloader_reservation'] is None and p['motion_outputs'] is False and
            p['hardware_tested'] is False, 'bench scope/budgets')
    require(p['iwdg'] == dict(prescaler_divider=64, reload=1023, feed_interval_ms=100,
                             debug_halt_freeze=True), 'watchdog contract')
    address = FLASH
    for i, size in enumerate([16384] * 4 + [65536] + [131072] * 7):
        require(p['flash_sectors'][i] == dict(number=i, origin=address, bytes=size), 'sector map')
        address += size
    require(len(p['flash_sectors']) == 12 and address == FLASH_END, 'Flash geometry')
    cfg = (BOARD / 'openocd.cfg').read_text()
    require('reset_config none' in cfg and 'adapter speed 1000' in cfg and
            'configure -event reset-init { adapter speed 1000 }' in cfg and
            'configure -event pre-enable' in cfg and 'error "Pin trace is forbidden' in cfg,
            'pinless HSI OpenOCD overrides required')
    commands = '\n'.join(x.split('#', 1)[0] for x in cfg.splitlines())
    require(not re.search(r'\b(?:init|program|reset|shutdown|exit)\s*$', commands, re.M) and
            not re.search(r'\b(?:mww|mwh|mwb|flash|mass_erase|unlock|options_write)\b', commands),
            'OpenOCD config must not itself operate/program/change protection')
    return p


class Elf:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        b = self.raw
        require(52 <= len(b) <= 4 * 1024 * 1024 and b[:7] == b'\x7fELF\x01\x01\x01', 'bounded ELF32 little endian required')
        h = struct.unpack_from('<HHIIIIIHHHHHH', b, 16)
        kind, machine, _, self.entry, phoff, shoff, flags, hsize, phsize, phcount, shsize, shcount, strings_index = h
        require(kind == 2 and machine == 40 and flags & 0x400 and hsize == 52, 'ARM executable/hard-float ABI required')
        require(phsize == 32 and 0 < phcount <= 32 and phoff + phcount * phsize <= len(b), 'program table bounds')
        require(shsize == 40 and 0 < strings_index < shcount <= 128 and shoff + shcount * shsize <= len(b), 'section table bounds')
        self.segments = [struct.unpack_from('<IIIIIIII', b, phoff + i * phsize) for i in range(phcount)]
        sections = [struct.unpack_from('<IIIIIIIIII', b, shoff + i * shsize) for i in range(shcount)]
        names = sections[strings_index]
        require(names[4] + names[5] <= len(b), 'section strings bounds')
        strings = b[names[4]:names[4] + names[5]]
        self.sections = {}
        for i, s in enumerate(sections):
            n = self.string(strings, s[0])
            require(n not in self.sections and (s[1] == 8 or s[4] + s[5] <= len(b)), 'duplicate/truncated section')
            self.sections[n] = dict(index=i, header_offset=shoff+i*40, name=n, type=s[1], flags=s[2],
                                    address=s[3], offset=s[4], size=s[5], link=s[6], align=s[8], entsize=s[9])
        table = self.section('.symtab')
        require(table['entsize'] == 16 and table['size'] % 16 == 0 and table['link'] < len(sections), 'symbol table')
        st = sections[table['link']]
        require(st[4]+st[5] <= len(b), 'symbol strings bounds')
        strings = b[st[4]:st[4]+st[5]]
        self.symbols = {}
        for pos in range(table['offset'], table['offset']+table['size'], 16):
            n, value, size, info, _, index = struct.unpack_from('<IIIBBH', b, pos)
            name = self.string(strings, n)
            if name and not name.startswith(('$','.L')) and (info & 15) != 4:
                require(index != 0, 'undefined import: ' + name)
                if name in self.symbols:
                    require(self.symbols[name]['value'] == value, 'conflicting symbol')
                self.symbols[name] = dict(value=value, size=size, kind=info & 15, index=index)

    @staticmethod
    def string(strings, start):
        end = strings.find(b'\0', start)
        require(0 <= start < len(strings) and end >= start, 'invalid ELF string')
        return strings[start:end].decode('ascii')

    def section(self, name):
        require(name in self.sections, 'missing section: ' + name)
        return self.sections[name]

    def symbol(self, name):
        require(name in self.symbols, 'missing symbol: ' + name)
        return self.symbols[name]['value']

    def bytes_at(self, address, length):
        candidates = [s for s in self.sections.values() if s['flags'] & 2 and s['type'] == 1 and
                      s['address'] <= address and address + length <= s['address'] + s['size']]
        require(len(candidates) == 1, 'missing/ambiguous file-backed span')
        s = candidates[0]
        start = s['offset'] + address - s['address']
        return self.raw[start:start+length]


def inspect(elf_path, binary_path, map_path):
    references()
    elf = Elf(elf_path)
    binary = Path(binary_path).read_bytes()
    require(392 < len(binary) <= 16384, 'bench BIN must fit sector0 budget')
    allocated = [s for s in elf.sections.values() if s['flags'] & 2 and s['size']]
    require({s['name'] for s in allocated} == {'.isr_vector', '.text', '.rodata', '.data', '.bss', '.noinit', '.ccm_bss', '.stacks'}, 'unexpected allocated section')
    for s in allocated:
        a, end = s['address'], s['address'] + s['size']
        region = 'flash' if FLASH <= a < FLASH_END else 'sram' if SRAM <= a < SRAM_END else 'ccm' if CCM <= a < CCM_END else None
        require(region and end <= {'flash':FLASH_END, 'sram':SRAM_END, 'ccm':CCM_END}[region], 'allocation out of documented memory')
        require(not(s['flags'] & 1 and s['flags'] & 4) and
                (region == 'flash' or not s['flags'] & 4) and
                (region != 'flash' or not s['flags'] & 1), 'write/execute placement')
        require(s['align'] > 0 and a % s['align'] == 0, 'section alignment')
        for other in allocated:
            if s is not other:
                require(end <= other['address'] or a >= other['address']+other['size'], 'allocated section overlap')
    v = elf.section('.isr_vector')
    require(v['address'] == FLASH and v['size'] == 392 and v['align'] == 512 and v['flags'] == 2, 'vector origin/count/alignment')
    vectors = struct.unpack('<98I', elf.bytes_at(FLASH, 392))
    require(vectors[0] == SRAM_END == elf.symbol('__stack_top__') and vectors[0] % 8 == 0, 'initial MSP')
    require(vectors[1] == elf.entry == elf.symbol('Reset_Handler') and vectors[1] & 1, 'Thumb reset/ELF entry')
    for i, address in enumerate(vectors[1:], 1):
        if i in RESERVED:
            require(address == 0, 'reserved vector must be zero')
        else:
            expected = elf.symbol('Reset_Handler' if i == 1 else 'SysTick_Handler' if i == 15 else 'tv4_bench_fault_entry')
            text = elf.section('.text')
            require(address == expected and address & 1 and text['address'] <= (address & ~1) < text['address']+text['size'], 'vector target/Thumb/executable range')
    for name, size in [('.noinit',84),('.ccm_bss',4),('.stacks',5184)]:
        s = elf.section(name)
        require(s['type'] == 8 and s['flags'] == 3 and s['size'] == size, 'NOLOAD size/flags: ' + name)
    require(elf.section('.ccm_bss')['address'] == CCM, 'CCM placement')
    data, bss, history = elf.section('.data'), elf.section('.bss'), elf.section('.noinit')
    require(data['type'] == 1 and data['address'] == SRAM and data['flags'] == 3 and
            data['address']+data['size'] == elf.symbol('__data_end__') and
            elf.symbol('__data_start__') == SRAM and data['size'] % 4 == 0, 'data VMA/word initialization')
    require(bss['type'] == 8 and bss['flags'] == 3 and bss['address'] == elf.symbol('__bss_start__') and
            bss['address']+bss['size'] == elf.symbol('__bss_end__') and
            data['address']+data['size'] <= bss['address'] <= bss['address']+bss['size'] <= history['address'], 'BSS/data/history separation')
    bottom = elf.symbol('__stack_bottom__')
    fg, fb, ft, mg = (elf.symbol(n) for n in ('__fault_guard_start__','__fault_stack_bottom__','__fault_stack_top__','__main_guard_start__'))
    require(SRAM_END-bottom == 4096 and ft-fb == 1024 and fb-fg == 32 and bottom-mg == 32 and
            ft == mg and ft % 8 == 0 and history['address']+history['size'] <= fg and
            elf.section('.stacks')['address'] == fg, 'stacks/guards/overlap')
    require(elf.symbol('__main_stack_bytes__') == 4096 and elf.symbol('__fault_stack_bytes__') == 1024 and
            elf.symbol('__guard_bytes__') == 32 and elf.symbol('__bench_flash_budget__') == 16384, 'linker budget symbols')
    diag = elf.symbols['tv4_bench_diag']
    require(diag['size'] == 168 and bss['address'] <= diag['value'] and diag['value']+168 <= bss['address']+bss['size'], 'RAM diagnostics placement/ABI')
    require(elf.symbol('tv4_bench_history') == history['address'] and elf.symbols['tv4_bench_history']['size'] == 84, 'persistent record placement')
    require(struct.unpack('<I',elf.bytes_at(elf.symbol('SystemCoreClock'),4))[0] == 16000000 and
            struct.unpack('<I',elf.bytes_at(elf.symbol('data_cookie'),4))[0] == 0x40704701, 'data initialization contents')
    image_end = elf.symbol('__image_end__')
    require(image_end == elf.symbol('__data_load__')+data['size'] and
            FLASH < image_end <= FLASH+16384 and len(binary) == image_end-FLASH, 'Flash LMA/BIN length')
    expected = bytearray(len(binary))
    covered = bytearray(len(binary))
    for seg in elf.segments:
        kind, off, va, pa, filesz, memsz, flags, _ = seg
        if kind != 1:
            continue
        require(filesz <= memsz and off+filesz <= len(elf.raw) and not(flags & 1 and flags & 2), 'load segment bounds/permissions')
        require((FLASH <= va <= va+memsz <= FLASH_END) or (SRAM <= va <= va+memsz <= SRAM_END) or
                (CCM <= va <= va+memsz <= CCM_END), 'load segment outside profile')
        if filesz:
            require(FLASH <= pa < pa+filesz <= image_end, 'all file-backed loads must program Flash only')
            start = pa-FLASH
            require(not any(covered[start:start+filesz]), 'overlapping Flash load segments')
            expected[start:start+filesz] = elf.raw[off:off+filesz]
            covered[start:start+filesz] = b'\1'*filesz
    require(binary == expected, 'ELF load bytes do not match exact BIN')
    stacks = [p for p in elf.segments if p[0] == 0x6474e551]
    require(len(stacks) == 1 and stacks[0][6] == 6, 'non-executable GNU stack')
    build = elf.bytes_at(elf.symbol('tv4_bench_build'),104)
    magic, abi, hz, kib, dirty = struct.unpack_from('<5I',build)
    commit = build[20:61].split(b'\0')[0].decode('ascii')
    profile = build[61:101].split(b'\0')[0].decode('ascii')
    require((magic,abi,hz,kib) == (0x54563442,1,16000000,1024) and dirty in (0,1) and
            re.fullmatch('[0-9a-f]{40}',commit) and build[60] == 0 and profile == PROFILE, 'embedded build identity')
    m = Path(map_path).read_text()
    for name, origin, length in [('FLASH',FLASH,1048576),('SRAM',SRAM,131072),('CCM',CCM,65536)]:
        require(re.search(r'^'+name+r'\s+0x0*'+format(origin,'x')+r'\s+0x0*'+format(length,'x')+r'\b',m,re.M), 'map memory region mismatch')
    for s in allocated:
        match = re.search(r'^'+re.escape(s['name'])+r'\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)',m,re.M)
        require(match and int(match[1],16) == s['address'] and int(match[2],16) == s['size'], 'map section/ELF mismatch: '+s['name'])
    require(not any(n.startswith(('HAL_','LL_','GPIO','CAN_','DMA','TIM','Flash_','malloc','free','__aeabi_')) for n in elf.symbols), 'unexpected runtime/driver/helper symbol')
    return dict(status='software-checked-standalone-bench',profile=PROFILE,source_commit=commit,
                source_dirty=bool(dirty),elf_sha256=digest(elf.raw),bin_sha256=digest(binary),map_sha256=digest(Path(map_path).read_bytes()),
                bin_bytes=len(binary),flash_budget_bytes=16384,vectors=98,reserved_indices=sorted(RESERVED),
                hsi_hz=16000000,main_stack_bytes=4096,fault_stack_bytes=1024,guard_bytes=32,
                sram_static_bytes=data['size']+bss['size']+history['size'],ccm_bytes=4,
                diagnostic_address=diag['value'],diagnostic_bytes=168,history_address=history['address'],
                hardware_tested=False,physical_inhibit_verified=False,motion_outputs=False,
                can=False,flash_writer=False,bootloader_reservation=None,device_access=False,
                install_authorized=False,binary_release_authorized=False)


def code_check(elf_path,objdump='arm-none-eabi-objdump'):
    text = subprocess.check_output([objdump,'-d',str(elf_path)],text=True)
    instructions = re.findall(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2,8}\s+)+([a-z][a-z0-9.]*)\b',text,re.M)
    require(instructions and not any(x.split('.')[0] in ('bkpt','svc','swi','blx') or
                                     x.startswith('v') for x in instructions),
            'semihosting/indirect calls/FPU instructions forbidden')
    def function(name):
        match = re.search(r'^[0-9a-f]+ <'+re.escape(name)+r'>:\n(.*?)(?=\n[0-9a-f]+ <|\Z)',text,re.M|re.S)
        require(match,'missing disassembly: '+name)
        return match[1]
    reset = function('Reset_Handler')
    require('cpsid' in reset and reset.index('cpsid') < reset.index('<tv4_bench_entry>') and
            'msr' in reset and 'CONTROL' in reset and 'cpsie' not in reset, 'reset ordering/control')
    fault = function('tv4_bench_fault_entry')
    require('cpsid' in fault and 'mrs' in fault and 'PSP' in fault and 'MSP' in fault and
            re.search(r'mov\s+sp,',fault) and '<tv4_bench_fault_capture>' in fault, 'emergency fault stack entry')
    for name in ('tv4_bench_fault_entry','tv4_bench_fault_capture','tv4_bench_halt','SysTick_Handler'):
        require('tv4_bench_watchdog_feed' not in function(name),'watchdog must not be fed from fault/ISR')
    require('<tv4_bench_watchdog_feed>' in function('tv4_bench_entry') and
            'tv4_bench_feed_due' in function('tv4_bench_entry'),'main gated watchdog service required')
    require('dmb' in function('tv4_bench_record_save'),'record commit barrier')
    return dict(integer_only=True,semihosting=False,main_only_watchdog_feed=True,emergency_fault_stack=True)


def build_check(build):
    """Compiler frame budgets and output allowlist, not a measured stack test."""
    build = Path(build)
    files = sorted((build / 'CMakeFiles/tv4_stm32f407_bench.dir').rglob('*.su'))
    require(len(files) == 5, 'all five bench C sources must report stack usage')
    rows = [line.split('\t') for path in files for line in path.read_text().splitlines()]
    require(rows and all(len(row) == 3 and row[2] == 'static' and 0 <= int(row[1]) <= 128
                        for row in rows), 'bench compiler frame budget <=128 bytes/static')
    for path in build.rglob('*.bin'):
        require(path == build / 'tv4_stm32f407_bench.bin' or
                (path.name == 'CMakeDetermineCompilerABI_C.bin' and 'CMakeFiles' in path.parts),
                'unexpected firmware BIN: ' + str(path))
    return dict(compiler_frames=len(rows),max_compiler_frame_bytes=max(int(row[1]) for row in rows),
                individual_frame_budget_bytes=128,measured_stack_high_water=False,bin_allowlist=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('elf',nargs='?',type=Path)
    p.add_argument('--bin',dest='binary',type=Path)
    p.add_argument('--map',dest='mapfile',type=Path)
    p.add_argument('--references',action='store_true')
    p.add_argument('--build',type=Path,help='also check compiler stack reports and BIN output allowlist')
    p.add_argument('--output',type=Path)
    a = p.parse_args()
    try:
        if a.references:
            references()
            result = dict(status='PASS',profile=PROFILE,reference_only=True,device_access=False)
        else:
            require(a.elf and a.binary and a.mapfile,'ELF/BIN/map are all required')
            result = inspect(a.elf,a.binary,a.mapfile)
            result['code_check'] = code_check(a.elf)
            if a.build:
                result['build_check'] = build_check(a.build)
    except (ValueError,KeyError,IndexError,UnicodeError,struct.error,OSError,subprocess.SubprocessError) as error:
        raise SystemExit('STM32 bench rejected: '+str(error)) from None
    raw = json.dumps(result,indent=2)+'\n'
    if a.output:
        a.output.write_text(raw)
    print(raw,end='')


if __name__ == '__main__':
    main()
