#!/usr/bin/env python3
"""Inspect actual compiler products and profile gates; never opens hardware."""
import argparse
import json
import re
import subprocess
from pathlib import Path
import check_idf_build

ROOT=Path(__file__).resolve().parents[1]

def require(condition,message):
    if not condition:raise ValueError(message)

def writable_bytes(text):
    total=0
    for line in text.splitlines():
        match=re.match(r'\s*\[\s*\d+\]\s+(.+)',line)
        if not match:continue
        fields=match.group(1).split()
        if len(fields)>=7 and 'W' in fields[6]:total+=int(fields[4],16)
    return total

def frames(text):
    result={}
    for line in text.splitlines():
        fields=line.split('\t')
        if len(fields)!=3:continue
        name=fields[0].rsplit(':',1)[-1]
        require(fields[2] in ('static','dynamic,bounded'),'unbounded compiler frame: '+name)
        result[name]=int(fields[1])
    require(result,'missing stack-usage entries')
    require(max(result.values())<=192,'memory diagnostic frame exceeds 192 bytes')
    return result

def source_owner_guard():
    owners=[]
    for p in (ROOT/'firmware/esp32-hmi').rglob('*.c'):
        # Only project sources, never managed SDK/LVGL dependencies.
        if 'managed_components' in p.parts:continue
        if re.search(r'heap_caps_monitor_local_minimum_free_size_(?:start|stop)\s*\(',p.read_text()):
            owners.append(p.relative_to(ROOT).as_posix())
    require(set(owners)=={'firmware/esp32-hmi/ota/memory_diagnostics.c','firmware/esp32-hmi/ota/utc_tls_bench.c'},'unexpected SDK monitor caller')
    kconfig=(ROOT/'firmware/esp32-hmi/main/Kconfig.projbuild').read_text()
    require('depends on TV4_BOARD_REV12 && SPIRAM && !TV4_OTA_MEMORY_BENCH && !TV4_UTC_TLS_BENCH' in kconfig,'OTA/TLS monitor owner exclusion missing')
    require(re.search(r'config TV4_OTA_MEMORY_DIAGNOSTICS\n.*?depends on TV4_OTA_ENABLED\n\s+default n',kconfig,re.S),'trace must default off behind OTA')
    service=(ROOT/'firmware/esp32-hmi/ota/service.c').read_text()
    require('if(!committed)tv4_ota_memory_window_begin(TV4_MEM_CLEANUP);' in service,'monitor allocation added after boot selection')
    return sorted(owners)

def inspect(build,profile,sdk,trace=False):
    validation=check_idf_build.verify(build,profile,sdk,write_report=False,memory_diagnostics=trace)
    build=Path(build);elf=build/'tv4_hmi.elf'
    symbols=subprocess.check_output(['readelf','-Ws',str(elf)],text=True)
    names={p[-1] for line in symbols.splitlines() if len(p:=line.split())>=8 and p[3]=='FUNC' and p[6]!='UND'}
    obj=build/'esp-idf/main/CMakeFiles/__idf_main.dir/__/ota/memory_diagnostics.c.obj'
    sections=subprocess.check_output(['readelf','-SW',str(obj)],text=True)
    writable=writable_bytes(sections);require(writable<=512,'diagnostic data exceeds 512 bytes')
    usage=obj.with_suffix('.su');require(usage.is_file(),'compiler stack-usage file missing')
    stack=frames(usage.read_text())
    monitor={'heap_caps_monitor_local_minimum_free_size_start','heap_caps_monitor_local_minimum_free_size_stop'}
    if trace:
        require(profile=='rev12-ota','trace outside OTA profile')
        require(monitor<=names,'trace does not link both SDK monitor operations')
        require({'tv4_ota_memory_startup','tv4_ota_memory_window_begin','tv4_ota_memory_window_end','tv4_ota_memory_report_startup','tv4_ota_memory_report_window'}<=names,'trace integration discarded')
        require(0<writable<=512,'trace ledger missing')
        require(b'tv4_ota_mem\0' in elf.read_bytes(),'trace logger missing')
    else:
        require(writable==0,'trace state retained with flag off')
        if profile!='rev12-tls-bench':require(not(monitor&names),'SDK min monitor retained outside explicit diagnostics/TLS bench')
    if profile=='rev12-ota':
        require({'tv4_ota_memory_scope_revision','tv4_ota_memory_scope_since'}<=names,'periodic scope annotation missing')
        require(b'boot_self_test checks_pass=%u sdk_finish=%u' in elf.read_bytes(),'explicit boot result missing')
    else:
        require(not any(n.startswith('tv4_ota_memory_scope_') for n in names),'OTA scope sampler retained in ordinary image')
    return dict(profile=profile,sdk=sdk,memory_diagnostics=trace,module_writable_bytes=writable,
        module_writable_ceiling=512,compiler_frames=stack,compiler_frame_ceiling=192,
        sdk_monitor_linked=sorted(monitor&names),source_monitor_owners=source_owner_guard(),
        app_bytes=validation['images']['tv4_hmi.bin']['bytes'],device_access=False,
        runtime_stack_peak_coverage=False,hardware_performance='not measured')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('build',type=Path);p.add_argument('profile');p.add_argument('sdk');p.add_argument('--memory-diagnostics',action='store_true')
    a=p.parse_args()
    try:print(json.dumps(inspect(a.build,a.profile,a.sdk,a.memory_diagnostics),indent=2))
    except (ValueError,OSError,KeyError,subprocess.CalledProcessError) as e:raise SystemExit('Memory diagnostics validation failed: '+str(e))
