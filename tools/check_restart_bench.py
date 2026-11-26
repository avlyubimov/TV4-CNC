#!/usr/bin/env python3
"""Inspect linked restart markers, not a device, watchdog waiver or runtime test."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
from check_idf_build import require, verify

FUNCTIONS=('shutdown_marker','__wrap_esp_restart_noos','tv4_restart_bench_cold_boot',
           'tv4_restart_bench_prepare','tv4_restart_bench_execute')
FORMATS=('tv4_restart_hook_format','tv4_restart_noos_format')
TRACE_FUNCTIONS=tuple(n for n in FUNCTIONS if n!='tv4_restart_bench_cold_boot')+('tv4_restart_trace_boot',)

def loaded_bytes(raw):
    """ELF32 load identity; file offsets may change in an analysis copy."""
    require(len(raw)>=52 and raw[:6]==b'\x7fELF\x01\x01','expected ELF32 little endian')
    offset=struct.unpack_from('<I',raw,28)[0]
    size,count=struct.unpack_from('<HH',raw,42)
    require(size==32 and 0<count<=128 and offset+size*count<=len(raw),'invalid load table')
    loads=[]
    for i in range(count):
        kind,start,address,physical,length,memory,flags,align=struct.unpack_from('<8I',raw,offset+size*i)
        if kind==1:
            require(length<=memory and start+length<=len(raw),'invalid load segment')
            loads.append((address,physical,length,memory,flags,align,raw[start:start+length]))
    require(loads,'missing load segments')
    return struct.unpack_from('<I',raw,24)[0],loads

def property_analysis_copy(raw):
    """Only hide a non-loaded .xt.prop table from the disassembler, in a copy."""
    offset=struct.unpack_from('<I',raw,32)[0]
    size,count,names_index=struct.unpack_from('<HHH',raw,46)
    require(size==40 and 0<count<=256 and names_index<count and offset+size*count<=len(raw),
            'invalid analysis section table')
    headers=[struct.unpack_from('<10I',raw,offset+i*size) for i in range(count)]
    strings=headers[names_index]
    require(strings[4]+strings[5]<=len(raw),'invalid section names')
    names=raw[strings[4]:strings[4]+strings[5]]
    properties=[i for i,h in enumerate(headers) if names[h[0]:].split(b'\0',1)[0]==b'.xt.prop']
    require(len(properties)==1,'expected one SDK5 property table')
    i=properties[0];h=headers[i]
    require(h[1]==1 and h[2]==0 and h[3]==0 and 0<h[5]<=1048576 and h[4]+h[5]<=len(raw),
            'property table must be non-loaded metadata')
    copy=bytearray(raw)
    struct.pack_into('<I',copy,offset+i*size+20,0)
    require(loaded_bytes(raw)==loaded_bytes(copy),'analysis changed loaded firmware')
    return copy

def inspect(build,profile,sdk,enabled,*,post_install=False):
    require(type(enabled) is bool and type(post_install) is bool and (not post_install or enabled),
            'explicit restart instrumentation mode required')
    build=Path(build)
    config=json.loads((build/'config/sdkconfig.json').read_text())
    verify(build,profile,sdk,write_report=False,
           memory_diagnostics=config.get('TV4_OTA_MEMORY_DIAGNOSTICS',False),
           worker_stack_bench=config.get('TV4_OTA_WORKER_STACK_BENCH',False),
           worker_tls_bench=config.get('TV4_OTA_WORKER_TLS_BENCH',False),
           signed_check_bench=config.get('TV4_OTA_SIGNED_CHECK_BENCH',False),
           restart_bench=enabled and not post_install,restart_trace=post_install,
           reproducible=config.get('APP_REPRODUCIBLE_BUILD',False))
    elf=build/'tv4_hmi.elf'
    symbols={}
    table=subprocess.check_output(['readelf','-Ws',str(elf)],text=True)
    for line in table.splitlines():
        parts=line.split()
        if len(parts)==8 and parts[0].endswith(':') and parts[0][:-1].isdigit() and parts[6]!='UND':
            symbols[parts[7]]={'address':int(parts[1],16),'bytes':int(parts[2]),'type':parts[3]}
    present={name:symbols[name] for name in set(FUNCTIONS+TRACE_FUNCTIONS+FORMATS) if name in symbols}
    if not enabled:
        require(not present,'restart instrumentation retained in an ordinary image')
        return dict(enabled=False,markers_absent=True,device_access=False)
    expected=set((TRACE_FUNCTIONS if post_install else FUNCTIONS)+FORMATS)
    require(set(present)==expected,'linked restart marker missing or wrong mode retained')
    for name in ('shutdown_marker','__wrap_esp_restart_noos'):
        require(0x40370000<=symbols[name]['address']<0x403e0000 and
                0<symbols[name]['bytes']<=96 and symbols[name]['type']=='FUNC',name+' must be bounded IRAM code')
    for name in FORMATS:
        require(0x3fc88000<=symbols[name]['address']<0x3fd00000 and
                0<symbols[name]['bytes']<=128 and symbols[name]['type']=='OBJECT',name+' must be bounded internal DRAM')
    tool=shutil.which('xtensa-esp32s3-elf-objdump')
    require(tool is not None,'activate the validated SDK to inspect Xtensa instructions')
    targets=('esp_restart','__wrap_esp_restart_noos','shutdown_marker')
    if post_install:targets+=('worker','tv4_restart_bench_execute','tv4_restart_trace_boot')
    disassembly={name:subprocess.check_output([tool,'-d','--disassemble='+name,str(elf)],text=True)
                 for name in targets}
    analysis_copy=False
    # IDF5's sparse nonallocating .xt.prop can classify unlisted SDK code as
    # data. Never accept missing decoded calls. Analyze a temporary copy only,
    # and require identical entry/load addresses, flags and every loaded byte.
    if sdk=='5.3.2' and re.search(r'^\s*[0-9a-f]{8}:\s+[0-9a-f]{8}\s*$',disassembly['esp_restart'],re.M):
        original=elf.read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            copy=Path(directory)/'analysis.elf'
            copy.write_bytes(property_analysis_copy(original))
            disassembly={name:subprocess.check_output([tool,'-d','--disassemble='+name,str(copy)],text=True)
                         for name in disassembly}
        require(elf.read_bytes()==original,'original ELF changed during inspection')
        analysis_copy=True
    require('<__wrap_esp_restart_noos>' in disassembly['esp_restart'],'SDK esp_restart does not call the wrapper')
    require('<esp_restart_noos>' in disassembly['__wrap_esp_restart_noos'],'wrapper does not call original SDK restart')
    require('<tv4_restart_noos_format>' in disassembly['__wrap_esp_restart_noos'],'noos marker lost its DRAM string')
    require('<tv4_restart_hook_format>' in disassembly['shutdown_marker'],'shutdown marker lost its DRAM string')
    if post_install:
        for name in ('tv4_restart_trace_boot','tv4_restart_bench_prepare','tv4_restart_bench_execute'):
            require('<'+name+'>' in disassembly['worker'],'operator worker bypassed restart trace: '+name)
        require('<esp_restart>' in disassembly['tv4_restart_bench_execute'],'operator request bypassed SDK restart')
        boot=disassembly['tv4_restart_trace_boot']
        require(re.search(r'<esp_log(?:_write|_writev)?>',boot) is not None and
                '<esp_rom_printf>' not in boot,'startup trace must use serialized SDK logging')
        require(b'stage=armed ready=%u operator=1' in elf.read_bytes(),'operator trace admission marker missing')
    for name in ('__wrap_esp_restart_noos','shutdown_marker'):
        text=disassembly[name]
        require('<esp_rom_printf>' in text,'ROM-only marker output required')
        for address in re.findall(r'\b([0-9a-f]{8}) <',text):
            value=int(address,16)
            require(0x40000000<=value<0x40080000 or 0x40370000<=value<0x403e0000 or
                    0x3fc88000<=value<0x3fd00000,'cached/unknown dependency in '+name)
    return dict(enabled=True,post_install=post_install,profile=profile,sdk=sdk,linked_symbols=present,
                sdk_calls_wrapper=True,wrapper_calls_original_sdk=True,
                serialized_boot_logging=post_install,
                marker_dependencies_internal_or_rom=True,
                property_table_analysis_copy=analysis_copy,original_load_bytes_preserved=True,
                disassembly_sha256={k:hashlib.sha256(v[v.index('<'+k+'>:'):].encode()).hexdigest() for k,v in disassembly.items()},
                device_access=False,runtime_qualified=False)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('build',type=Path);p.add_argument('profile');p.add_argument('sdk',choices=('5.3.2','6.1.1'))
    mode=p.add_mutually_exclusive_group()
    mode.add_argument('--enabled',action='store_true');mode.add_argument('--post-install',action='store_true');a=p.parse_args()
    try:result=inspect(a.build,a.profile,a.sdk,a.enabled or a.post_install,post_install=a.post_install)
    except (ValueError,KeyError,OSError,struct.error,subprocess.SubprocessError) as error:
        raise SystemExit('Restart image rejected: '+str(error)) from None
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
