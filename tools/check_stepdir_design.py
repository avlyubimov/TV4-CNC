#!/usr/bin/env python3
"""Inspect the opt-in ARM arithmetic archive, not firmware or a device."""
import argparse
import json
from pathlib import Path
import subprocess


def require(ok, message):
    if not ok:
        raise ValueError(message)


def inspect(build):
    build = Path(build)
    archive = build / 'libtv4_stepdir_design.a'
    members = subprocess.check_output(['arm-none-eabi-ar', 't', str(archive)], text=True).splitlines()
    require(members == ['timer_plan.c.obj'], 'unexpected archive member')
    rows = subprocess.check_output(['arm-none-eabi-size', str(archive)], text=True).splitlines()[1:]
    require(len(rows) == 1, 'unexpected size rows')
    text, data, bss = (int(v) for v in rows[0].split()[:3])
    require(0 < text <= 1024 and data == 0 and bss == 0, 'archive text/writable budget')
    symbols = subprocess.check_output(['arm-none-eabi-nm', '-g', str(archive)], text=True).splitlines()
    exports, imports = set(), set()
    for row in symbols:
        fields = row.split()
        if len(fields) == 2 and fields[0] == 'U':
            imports.add(fields[1])
        elif len(fields) == 3:
            require(fields[1] == 'T', 'unexpected global symbol kind')
            exports.add(fields[2])
    require(exports == {'tv4_stepdir_compile'}, 'unexpected exports')
    require(imports <= {'__aeabi_uldivmod', 'memset'}, 'unexpected runtime/IO/heap import')
    usage = build / 'CMakeFiles/tv4_stepdir_design.dir/stepdir/timer_plan.c.su'
    frames = {}
    for row in usage.read_text().splitlines():
        name, size, kind = row.split('\t')
        require(kind == 'static', 'unbounded compiler frame')
        frames[name.rsplit(':', 1)[-1]] = int(size)
    require(frames and max(frames.values()) <= 96, 'compiler frame budget')
    require(not list(build.glob('*.elf')) and not list(build.glob('*.bin')),
            'design target must not create firmware')
    return dict(text=text, data=data, bss=bss, text_ceiling=1024, frames=frames,
                frame_ceiling=96, imports=sorted(imports), exports=sorted(exports),
                physical_outputs=False, device_access=False, firmware_image=False,
                runtime_stack_peak_qualified=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.build), indent=2))


if __name__ == '__main__':
    main()
