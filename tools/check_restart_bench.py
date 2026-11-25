#!/usr/bin/env python3
"""Inspect linked restart markers, not a device, watchdog waiver or runtime test."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from check_idf_build import require, verify

FUNCTIONS=('shutdown_marker','__wrap_esp_restart_noos','tv4_restart_bench_cold_boot',
           'tv4_restart_bench_prepare','tv4_restart_bench_execute')
FORMATS=('tv4_restart_hook_format','tv4_restart_noos_format')

def inspect(build,profile,sdk,enabled):
    build=Path(build)
    config=json.loads((build/'config/sdkconfig.json').read_text())
    verify(build,profile,sdk,write_report=False,
           memory_diagnostics=config.get('TV4_OTA_MEMORY_DIAGNOSTICS',False),
           worker_stack_bench=config.get('TV4_OTA_WORKER_STACK_BENCH',False),
           worker_tls_bench=config.get('TV4_OTA_WORKER_TLS_BENCH',False),
           signed_check_bench=config.get('TV4_OTA_SIGNED_CHECK_BENCH',False),
           restart_bench=enabled,reproducible=config.get('APP_REPRODUCIBLE_BUILD',False))
    elf=build/'tv4_hmi.elf'
    symbols={}
    table=subprocess.check_output(['readelf','-Ws',str(elf)],text=True)
    for line in table.splitlines():
        parts=line.split()
        if len(parts)==8 and parts[0].endswith(':') and parts[0][:-1].isdigit() and parts[6]!='UND':
            symbols[parts[7]]={'address':int(parts[1],16),'bytes':int(parts[2]),'type':parts[3]}
    present={name:symbols[name] for name in FUNCTIONS+FORMATS if name in symbols}
    if not enabled:
        require(not present,'restart instrumentation retained in an ordinary image')
        return dict(enabled=False,markers_absent=True,device_access=False)
    require(len(present)==len(FUNCTIONS+FORMATS),'linked restart marker missing')
    for name in ('shutdown_marker','__wrap_esp_restart_noos'):
        require(0x40370000<=symbols[name]['address']<0x403e0000 and
                0<symbols[name]['bytes']<=96 and symbols[name]['type']=='FUNC',name+' must be bounded IRAM code')
    for name in FORMATS:
        require(0x3fc88000<=symbols[name]['address']<0x3fd00000 and
                0<symbols[name]['bytes']<=128 and symbols[name]['type']=='OBJECT',name+' must be bounded internal DRAM')
    tool=shutil.which('xtensa-esp32s3-elf-objdump')
    require(tool is not None,'activate the validated SDK to inspect Xtensa instructions')
    disassembly={name:subprocess.check_output([tool,'-d','--disassemble='+name,str(elf)],text=True)
                 for name in ('esp_restart','__wrap_esp_restart_noos','shutdown_marker')}
    require('<__wrap_esp_restart_noos>' in disassembly['esp_restart'],'SDK esp_restart does not call the wrapper')
    require('<esp_restart_noos>' in disassembly['__wrap_esp_restart_noos'],'wrapper does not call original SDK restart')
    require('<tv4_restart_noos_format>' in disassembly['__wrap_esp_restart_noos'],'noos marker lost its DRAM string')
    require('<tv4_restart_hook_format>' in disassembly['shutdown_marker'],'shutdown marker lost its DRAM string')
    for name in ('__wrap_esp_restart_noos','shutdown_marker'):
        text=disassembly[name]
        require('<esp_rom_printf>' in text,'ROM-only marker output required')
        for address in re.findall(r'\b([0-9a-f]{8}) <',text):
            value=int(address,16)
            require(0x40000000<=value<0x40080000 or 0x40370000<=value<0x403e0000 or
                    0x3fc88000<=value<0x3fd00000,'cached/unknown dependency in '+name)
    return dict(enabled=True,profile=profile,sdk=sdk,linked_symbols=present,
                sdk_calls_wrapper=True,wrapper_calls_original_sdk=True,
                marker_dependencies_internal_or_rom=True,
                disassembly_sha256={k:hashlib.sha256(v.encode()).hexdigest() for k,v in disassembly.items()},
                device_access=False,runtime_qualified=False)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('build',type=Path);p.add_argument('profile');p.add_argument('sdk',choices=('5.3.2','6.1.1'))
    p.add_argument('--enabled',action='store_true');a=p.parse_args()
    try:result=inspect(a.build,a.profile,a.sdk,a.enabled)
    except (ValueError,KeyError,OSError,subprocess.SubprocessError) as error:
        raise SystemExit('Restart image rejected: '+str(error)) from None
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
