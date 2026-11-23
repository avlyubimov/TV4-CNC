#!/usr/bin/env python3
"""Inspect actual compiler products and profile gates; never opens hardware."""
import argparse
import json
import re
import subprocess
from pathlib import Path
import check_idf_build
import check_ota_dma_diagnostics
import check_lvgl_memory

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
    require(re.search(r'config TV4_OTA_WORKER_STACK_BENCH\n.*?depends on TV4_OTA_MEMORY_DIAGNOSTICS\n\s+default n',kconfig,re.S),'stack bench must default off behind trace')
    require(re.search(r'config TV4_OTA_WORKER_TLS_BENCH\n.*?depends on TV4_OTA_WORKER_STACK_BENCH\n\s+default n',kconfig,re.S),'worker TLS bench must default off behind stack bench')
    service=(ROOT/'firmware/esp32-hmi/ota/service.c').read_text()
    require('if(!committed)tv4_ota_memory_window_begin(TV4_MEM_CLEANUP);' in service,'monitor allocation added after boot selection')
    return sorted(owners)

def inspect(build,profile,sdk,trace=False,worker_stack_bench=False,worker_tls_bench=False):
    validation=check_idf_build.verify(build,profile,sdk,write_report=False,memory_diagnostics=trace,worker_stack_bench=worker_stack_bench,worker_tls_bench=worker_tls_bench)
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
    requested=0;service_frames={}
    marker=b'worker_stack_bytes=%u stack_bench=1 provisioned=0'
    if profile=='rev12-ota':
        context=json.loads((build/'ota-local/ota_local_context.json').read_text())
        require(type(context) is dict and type(context.get('provisioned')) is bool,'missing provisioning state')
        requested=8192 if context['provisioned'] or worker_stack_bench else 3072
        require((marker in elf.read_bytes())==worker_stack_bench,'worker stack bench marker/selection mismatch')
        service_usage=build/'esp-idf/main/CMakeFiles/__idf_main.dir/__/ota/service.c.su'
        require(service_usage.is_file(),'service compiler stack-usage missing')
        for line in service_usage.read_text().splitlines():
            fields=line.split('\t')
            require(len(fields)==3 and fields[2] in ('static','dynamic,bounded'),'unbounded service compiler frame')
            service_frames[fields[0].rsplit(':',1)[-1]]=int(fields[1])
    require(('tv4_utc_tls_bench_run_in_ota_worker' in names)==worker_tls_bench,'worker TLS entry/selection mismatch')
    require((b'tv4_ota_worker_tls\0' in elf.read_bytes())==worker_tls_bench,'worker TLS marker/selection mismatch')
    tls_frames={};tls_monitor_imports=[]
    if worker_tls_bench:
        tls_obj=build/'esp-idf/main/CMakeFiles/__idf_main.dir/__/ota/utc_tls_bench.c.obj'
        tls_symbols=subprocess.check_output(['readelf','-Ws',str(tls_obj)],text=True)
        imports={p[-1] for line in tls_symbols.splitlines() if len(p:=line.split())>=8 and p[6]=='UND'}
        require(not(monitor&imports),'native worker TLS must not directly own the SDK monitor')
        tls_monitor_imports=sorted(imports&{'tv4_ota_memory_window_begin','tv4_ota_memory_window_end','tv4_ota_memory_report_window'})
        require(len(tls_monitor_imports)==3,'native TLS must use the existing OTA monitor adapter')
        require(not any(n.startswith('xTaskCreate') or n=='vTaskDelete' for n in imports),'worker TLS must not create/delete an RTOS task')
        require({'mbedtls_x509_time_cmp','mbedtls_x509_time_gmtime'}<=names,'TLS certificate dates not linked')
        for line in tls_obj.with_suffix('.su').read_text().splitlines():
            fields=line.split('\t')
            require(len(fields)==3 and fields[2] in ('static','dynamic,bounded'),'unbounded TLS compiler frame')
            tls_frames[fields[0].rsplit(':',1)[-1]]=int(fields[1])
        require(tls_frames,'native TLS compiler frames missing')
    dma=check_ota_dma_diagnostics.inspect(build,worker_tls_bench)
    gui_heap=check_lvgl_memory.inspect(build,profile=='rev12-ota')
    return dict(profile=profile,sdk=sdk,memory_diagnostics=trace,worker_stack_bench=worker_stack_bench,
        dma_diagnostics=dma,
        gui_heap=gui_heap,
        worker_tls_bench=worker_tls_bench,tls_compiler_frames=tls_frames,tls_monitor_adapter_imports=tls_monitor_imports,
        worker_stack_bytes_requested=requested,worker_stack_runtime_qualified=False,service_compiler_frames=service_frames,module_writable_bytes=writable,
        module_writable_ceiling=512,compiler_frames=stack,compiler_frame_ceiling=192,
        sdk_monitor_linked=sorted(monitor&names),source_monitor_owners=source_owner_guard(),
        app_bytes=validation['images']['tv4_hmi.bin']['bytes'],device_access=False,
        runtime_stack_peak_coverage=False,hardware_performance='not measured')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('build',type=Path);p.add_argument('profile');p.add_argument('sdk');p.add_argument('--memory-diagnostics',action='store_true');p.add_argument('--worker-stack-bench',action='store_true')
    p.add_argument('--worker-tls-bench',action='store_true')
    a=p.parse_args()
    try:print(json.dumps(inspect(a.build,a.profile,a.sdk,a.memory_diagnostics,a.worker_stack_bench,a.worker_tls_bench),indent=2))
    except (ValueError,OSError,KeyError,subprocess.CalledProcessError) as e:raise SystemExit('Memory diagnostics validation failed: '+str(e))
