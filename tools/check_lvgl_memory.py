#!/usr/bin/env python3
"""Check actual LVGL CUSTOM allocator linkage; no SDK or device execution."""
from pathlib import Path
from check_ota_dma_diagnostics import require,sections,symbols

ENTRIES={'lv_malloc_core','lv_realloc_core','lv_free_core','lv_mem_init','lv_mem_deinit',
         'lv_mem_add_pool','lv_mem_remove_pool','lv_mem_monitor_core','lv_mem_test_core'}

def inspect(build,ota):
    build=Path(build);obj=build/'esp-idf/main/CMakeFiles/__idf_main.dir/__/ui/lvgl_psram.c.obj'
    table=symbols(obj);writable=sum(x['bytes'] for x in sections(obj).values() if 'W' in x['flags'])
    exports={x['name'] for x in table if x['kind']=='FUNC' and x['index']!='UND'}
    imports={x['name'] for x in table if x['index']=='UND'}
    require(writable==0,'LVGL allocator gained mutable state')
    if not ota:
        require(not(exports or imports),'PSRAM GUI allocator retained outside OTA')
        return dict(backend='unchanged CLIB',custom_state_bytes=0,device_access=False)
    require(exports==ENTRIES,'missing/extra LVGL CUSTOM API implementation')
    expected={'heap_caps_malloc','heap_caps_realloc','heap_caps_free','heap_caps_check_integrity'}
    require(expected<=imports and not(imports-expected-{'memset'}),'unexpected GUI allocator side effects/fallback')
    libraries=list((build/'esp-idf').glob('*lvgl*/lib*lvgl*.a'))
    require(len(libraries)==1,'ambiguous/missing LVGL archive')
    require(not any(x['name'] in ENTRIES and x['index']!='UND' for x in symbols(libraries[0])),
            'another LVGL memory core linked with the custom implementation')
    linked={x['name'] for x in symbols(build/'tv4_hmi.elf') if x['kind']=='FUNC' and x['index']!='UND'}
    require({'lv_mem_init','lv_malloc_core','lv_realloc_core','lv_free_core'}<=linked,'GUI heap entry discarded')
    frames={}
    for line in obj.with_suffix('.su').read_text().splitlines():
        f=line.split('\t');require(len(f)==3 and f[2] in {'static','dynamic,bounded'},'unbounded GUI allocator frame')
        frames[f[0].rsplit(':',1)[-1]]=int(f[1])
    require(frames and max(frames.values())<=128,'GUI allocator frame exceeds128bytes')
    return dict(backend='LVGL9 CUSTOM / SPIRAM|8BIT',custom_state_bytes=writable,
                imports=sorted(imports),compiler_frames=frames,frame_ceiling=128,
                internal_fallback=False,runtime_stack_peak_qualified=False,device_access=False)
