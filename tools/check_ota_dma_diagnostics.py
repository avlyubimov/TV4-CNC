#!/usr/bin/env python3
"""Verify actual opt-in DMA callback placement, ownership and bounds; no device."""
import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
REGISTER='heap_caps_register_failed_alloc_callback'
CALLBACK='tv4_dma_allocation_failed'
RTOS={'xPortEnterCriticalTimeout','vPortEnterCritical','vPortExitCritical',
      'xPortInIsrContext','xTaskGetCurrentTaskHandle'}


def require(ok,message):
    if not ok:raise ValueError(message)


def sections(path):
    result={}
    for line in subprocess.check_output(['readelf','-SW',str(path)],text=True).splitlines():
        m=re.match(r'\s*\[\s*(\d+)\]\s+(.+)',line)
        if not m:continue
        f=m[2].split()
        if len(f)>=7:result[m[1]]=dict(name=f[0],bytes=int(f[4],16),flags=f[6])
    return result


def symbols(path):
    result=[]
    for line in subprocess.check_output(['readelf','-Ws',str(path)],text=True).splitlines():
        f=line.split()
        if len(f)>=8 and f[0].endswith(':') and f[0][:-1].isdigit():
            result.append(dict(name=f[7],size=int(f[2],0),kind=f[3],index=f[6]))
    return result


def registration_owners(archives):
    require(archives,'missing SDK archives')
    owners=[];current=''
    # Stream symbol tables instead of retaining all SDK debug symbols.
    with subprocess.Popen(['readelf','-Ws',*map(str,archives)],text=True,
                          stdout=subprocess.PIPE,stderr=subprocess.PIPE) as process:
        for line in process.stdout:
            if line.startswith('File: '):current=line[6:].strip()
            f=line.split()
            if len(f)>=8 and f[6]=='UND' and f[7]==REGISTER:owners.append(current)
        error=process.stderr.read();require(process.wait()==0,error)
    return owners


def require_owners(owners,enabled):
    if enabled:
        require(len(owners)==1 and owners[0].endswith('/libmain.a(dma_diagnostics.c.obj)'),
                'SDK allocation failure callback has another/unexpected registration caller')
    else:require(not owners,'allocation failure hook retained outside diagnostic profile')


def callback_relocations(obj):
    active=False;calls=set()
    for line in subprocess.check_output(['readelf','-rW',str(obj)],text=True).splitlines():
        if line.startswith('Relocation section '):
            active=bool(re.search(r"'\.rela?\.iram",line));continue
        f=line.split()
        if not active or len(f)<5 or not re.fullmatch('[0-9a-fA-F]+',f[0]):continue
        target=f[4]
        if target.startswith(('.dram','.iram')):continue
        require(target in RTOS,'unsafe callback relocation: '+target)
        calls.add(target)
    require({'xPortInIsrContext','xTaskGetCurrentTaskHandle','vPortExitCritical'}<=calls,
            'missing expected callback RTOS chain')
    return calls


def require_iram(symbol_table,section_table,names):
    placed={}
    for name in names:
        found=[s for s in symbol_table if s['name']==name and s['kind']=='FUNC']
        require(len(found)==1,'missing/ambiguous callback callee: '+name)
        section=section_table.get(found[0]['index'],{}).get('name','')
        require(section.startswith('.iram'),'callback/callee outside IRAM: '+name)
        placed[name]=dict(section=section,bytes=found[0]['size'])
    return placed


def inspect(build,enabled):
    build=Path(build);obj=build/'esp-idf/main/CMakeFiles/__idf_main.dir/__/ota/dma_diagnostics.c.obj'
    elf=build/'tv4_hmi.elf';obj_sections=sections(obj);obj_symbols=symbols(obj);elf_symbols=symbols(elf)
    writable=sum(s['bytes'] for s in obj_sections.values() if 'W' in s['flags'])
    entries={'tv4_ota_dma_init','tv4_ota_dma_begin','tv4_ota_dma_end','tv4_ota_dma_finish'}
    linked={s['name'] for s in elf_symbols if s['kind']=='FUNC'}
    owners=registration_owners(sorted((build/'esp-idf').glob('*/lib*.a')))
    require_owners(owners,enabled)
    if not enabled:
        require(writable==0 and not(entries&linked) and not owners,
                'DMA hook/state retained outside worker TLS bench')
        return dict(enabled=False,writable_bytes=0,registration_callers=[],device_access=False)
    require(entries<=linked and 0<writable<=192,'missing DMA trace or writable budget')
    calls=callback_relocations(obj);elf_sections=sections(elf)
    placements=require_iram(elf_symbols,elf_sections,{CALLBACK}|calls)
    for name in ['dma_trace','dma_mux']:
        found=[s for s in elf_symbols if s['kind']=='OBJECT' and s['name']==name]
        require(len(found)==1 and elf_sections[found[0]['index']]['name'].startswith('.dram'),
                'callback state outside internal DRAM: '+name)
    frames={}
    for line in obj.with_suffix('.su').read_text().splitlines():
        f=line.split('\t');require(len(f)==3 and f[2] in ('static','dynamic,bounded'),'unbounded DMA diagnostic frame')
        frames[f[0].rsplit(':',1)[-1]]=int(f[1])
    require(frames and max(frames.values())<=192,'DMA frame budget')
    imports={s['name'] for s in obj_symbols if s['index']=='UND'}
    require(not any(x.startswith(('xTaskCreate','heap_caps_malloc','heap_caps_calloc','heap_caps_aligned','heap_caps_monitor'))
                    or x in {'heap_caps_free','vTaskDelete','esp_restart'} for x in imports),
            'DMA diagnostics gained allocation/task/min-monitor side effects')
    return dict(enabled=True,writable_bytes=writable,writable_ceiling=192,frames=frames,
                frame_ceiling=192,iram_callback_chain=placements,
                registration_callers=['main/dma_diagnostics.c.obj'],retained_failures=4,
                callback_heap_log_string_access=False,failure_time_heap_not_sampled=True,
                runtime_stack_peak_qualified=False,device_access=False)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('build',type=Path);p.add_argument('--enabled',action='store_true');a=p.parse_args()
    try:print(json.dumps(inspect(a.build,a.enabled),indent=2))
    except (ValueError,OSError,KeyError,subprocess.CalledProcessError) as e:raise SystemExit('DMA diagnostics validation failed: '+str(e))
