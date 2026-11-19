#!/usr/bin/env python3
"""Inspect a SYNTHETIC ARM link probe, never execute it or access devices."""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess

ROM, ROM_END, RAM, RAM_END = 0x01000000, 0x01010000, 0x30000000, 0x30004000


def require(value, message):
    if not value:
        raise ValueError(message)


def inspect(path):
    with Path(path).open('rb') as source:
        raw = source.read(1024 * 1024 + 1)
    require(52 <= len(raw) <= 1024 * 1024, 'bounded ELF probe required')
    require(raw[:7] == b'\x7fELF\x01\x01\x01', 'expected ELF32 little endian')
    header = struct.unpack_from('<HHIIIIIHHHHHH', raw, 16)
    kind, machine, _, entry, phoff, shoff, flags, _, phsize, phcount, shsize, shcount, names_index = header
    require(kind == 2 and machine == 40 and flags & 0x400, 'expected ARM hard-float ELF')
    require(shsize == 40 and 0 < names_index < shcount <= 128 and
            shoff + shcount * shsize <= len(raw), 'invalid section table')
    require(phsize == 32 and 0 < phcount <= 32 and phoff + phsize * phcount <= len(raw),
            'invalid program headers')
    segments = [struct.unpack_from('<IIIIIIII', raw, phoff + i * phsize) for i in range(phcount)]
    sections = [struct.unpack_from('<IIIIIIIIII', raw, shoff + i * shsize) for i in range(shcount)]
    names = sections[names_index]
    require(names[4] + names[5] <= len(raw), 'invalid section names')
    strings = raw[names[4]:names[4] + names[5]]
    def string(data, start):
        end = data.find(b'\0', start)
        require(0 <= start < len(data) and end >= start, 'invalid ELF string')
        return data[start:end].decode('ascii')
    indexed = {}
    for section in sections:
        name = string(strings, section[0])
        require(name not in indexed, 'duplicate ELF section')
        require(section[1] == 8 or section[4] + section[5] <= len(raw), 'truncated section')
        if section[2] & 2 and section[5]:
            start, end = section[3], section[3] + section[5]
            require((ROM <= start <= end <= ROM_END) or (RAM <= start <= end <= RAM_END),
                    'allocation outside synthetic fixture map')
        indexed[name] = section
    table = indexed.get('.symtab')
    require(table and table[9] == 16 and table[5] % 16 == 0 and table[6] < len(sections),
            'missing symbol table')
    text_table = sections[table[6]]
    symbol_strings = raw[text_table[4]:text_table[4] + text_table[5]]
    symbols = {}
    for offset in range(table[4], table[4] + table[5], 16):
        name_offset, value, _, _, _, section_index = struct.unpack_from('<IIIBBH', raw, offset)
        name = string(symbol_strings, name_offset)
        if name:
            require(section_index != 0, 'unresolved symbol: ' + name)
            symbols[name] = value
    def symbol(name):
        require(name in symbols, 'missing symbol: ' + name)
        return symbols[name]
    vectors = indexed.get('.isr_vector')
    require(vectors and vectors[3] == ROM and vectors[5] == 64 and vectors[2] == 2,
            'core-only fixture vectors required')
    sp, reset = struct.unpack_from('<II', raw, vectors[4])
    require(sp == RAM_END == symbol('__stack_top__') and sp % 8 == 0, 'invalid initial SP')
    require(reset == entry == symbol('Reset_Handler') and reset & 1 and ROM <= reset < ROM_END,
            'invalid Thumb reset/entry')
    require(symbol('__stack_bytes') == 2048 and symbol('__stack_bottom__') == RAM_END - 2048,
            'invalid synthetic stack reserve')
    data, bss = indexed.get('.data'), indexed.get('.bss')
    require(data and data[1] == 1 and data[3] == symbol('__data_start__') and
            data[3] + data[5] == symbol('__data_end__') and data[5] == 4 and data[2] == 3,
            'invalid data run span')
    require(bss and bss[1] == 8 and bss[3] == symbol('__bss_start__') and
            bss[3] + bss[5] == symbol('__bss_end__') and bss[5] == 4 and bss[2] == 3,
            'invalid zero BSS span')
    require(RAM <= data[3] < data[3] + data[5] <= bss[3] < bss[3] + bss[5] <= symbol('__stack_bottom__'),
            'data/BSS/stack overlap')
    require(ROM <= symbol('__text_end__') <= symbol('__data_load__') and
            symbol('__data_load__') + data[5] == symbol('__rom_end__') <= ROM_END,
            'invalid data ROM load span')
    require(struct.unpack_from('<I', raw, data[4])[0] == 0x12345678, 'wrong data initializer')
    loads = [p for p in segments if p[0] == 1 and p[2] <= data[3] and data[3] + data[5] <= p[2] + p[4]]
    require(len(loads) == 1, 'missing/ambiguous data load segment')
    load = loads[0]
    require(load[3] + data[3] - load[2] == symbol('__data_load__') and
            load[1] + data[3] - load[2] == data[4] and load[6] == 6, 'data LMA/file span mismatch')
    stacks = [p for p in segments if p[0] == 0x6474e551]
    require(len(stacks) == 1 and stacks[0][6] == 6, 'non-executable stack metadata required')
    require(symbol('tv4_startup_entry') & 1 and symbol('tv4_startup_halt') & 1,
            'startup targets must be Thumb')
    require(not any(name.startswith(('HAL_', 'LL_', 'GPIO', 'CAN_', 'RCC_')) for name in symbols),
            'physical driver unexpectedly linked')
    return dict(status='synthetic-link-only', vectors_bytes=64, data_bytes=4, bss_bytes=4,
                stack_bytes=2048, rom_load_bytes=symbol('__rom_end__') - ROM,
                deployable=False, device_access=False, physical_memory_verified=False)


def check_reset(path, objdump):
    output = subprocess.check_output([objdump, '-d', '--disassemble=Reset_Handler', str(path)], text=True)
    require(re.search(r'\bcpsid\s+i', output) and 'tv4_startup_entry' in output,
            'reset must mask IRQs before C entry')
    require(output.index('cpsid') < output.index('tv4_startup_entry') and 'cpsie' not in output,
            'reset IRQ ordering')
    require(not re.search(r'\b(?:vldr|vstr|vstm|vldm|vmov|vpush|vpop|vadd|vsub)(?:\.[a-z0-9]+)?\b', output),
            'reset must not require initialized FPU')
    return output


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('elf', type=Path)
    p.add_argument('--objdump', default='arm-none-eabi-objdump')
    a = p.parse_args()
    try:
        result = inspect(a.elf)
        check_reset(a.elf, a.objdump)
    except (ValueError, KeyError, UnicodeError, struct.error, OSError, subprocess.SubprocessError) as error:
        raise SystemExit('Synthetic link rejected: ' + str(error)) from None
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
