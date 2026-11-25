#!/usr/bin/env python3
"""Inspect compiler outputs only; never imports serial libraries or opens a device."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


OTA_PARTITIONS=[(1,2,0x9000,0x6000,"nvs"),(1,1,0xf000,0x1000,"phy_init"),(0,0,0x10000,0x200000,"factory"),
            (1,0,0x210000,0x2000,"otadata"),(0,0x10,0x220000,0x300000,"ota_0"),(0,0x11,0x520000,0x300000,"ota_1"),
            (1,0x40,0x820000,0x300000,"resources"),(1,0x41,0xb20000,0x4e0000,"store")]

def require(condition, message):
    if not condition:
        raise ValueError(message)


def install_policy(build):
    """Bind the public context policy to its linked service marker, not a grant."""
    build=Path(build)
    context=json.loads((build/'ota-local/ota_local_context.json').read_text())
    require(type(context) is dict and type(context.get('install_reviewed')) is bool and
            type(context.get('provisioned')) is bool and context.get('private_key_present') is False,
            'explicit public-only install policy required')
    allowed=context['install_reviewed']
    require(not allowed or context['provisioned'],'install review requires provisioned local trust')
    markers=(b'install_policy=check_only',b'install_policy=owner_reviewed')
    elf=(build/'tv4_hmi.elf').read_bytes()
    require(markers[allowed] in elf and markers[not allowed] not in elf,'install policy/context/ELF mismatch')
    return allowed


def verify(build, profile, version, *, write_report=True, memory_diagnostics=False, worker_stack_bench=False, worker_tls_bench=False, signed_check_bench=False, reproducible=None):
    require(profile in {"headless", "rev12", "rev12-memory-bench", "rev12-tls-bench", "rev12-ota"}, "unknown profile")
    require(version in {"5.3.2", "6.1.1"}, "unvalidated SDK")
    build = Path(build)
    config = json.loads((build / "config/sdkconfig.json").read_text())
    description = json.loads((build / "project_description.json").read_text())
    require(description["target"] == "esp32s3", "target must be esp32s3")
    require(description["git_revision"] == "v" + version, "wrong SDK revision")
    for name, value in {"ESPTOOLPY_FLASHSIZE": "16MB", "ESPTOOLPY_FLASHFREQ": "80m",
                        "LV_COLOR_DEPTH": 16, "FREERTOS_HZ": 1000,
                        "ESP_DEFAULT_CPU_FREQ_MHZ": 240}.items():
        require(config[name] == value, f"{name} must equal {value}")
    require(config.get("TV4_BOARD_REV12", False) == (profile != "headless"), "wrong board enable gate")
    require(config.get("TV4_OTA_MEMORY_BENCH", False) == (profile == "rev12-memory-bench"),
            "memory probe must be enabled only for explicit bench profile")
    require(config.get("TV4_UTC_TLS_BENCH", False) == (profile == "rev12-tls-bench"),
            "UTC/TLS probe must be enabled only for explicit TLS bench profile")
    ota = profile == "rev12-ota"
    actual_reproducible = config.get("APP_REPRODUCIBLE_BUILD", False)
    require(type(actual_reproducible) is bool and (not actual_reproducible or ota),
            "reproducible release configuration belongs only to rev12-ota")
    require(reproducible is None or (type(reproducible) is bool and reproducible == actual_reproducible),
            "reproducible configuration must match explicit build selection")
    if actual_reproducible:
        require(not config.get("APP_COMPILE_TIME_DATE", False) and config.get("COMPILER_HIDE_PATHS_MACROS") is True,
                "reproducible build must remove compilation date/time and map file macros")
    require(type(memory_diagnostics) is bool and (not memory_diagnostics or ota), "memory diagnostics require explicit OTA selection")
    require(config.get("TV4_OTA_MEMORY_DIAGNOSTICS", False)==memory_diagnostics,
            "memory tracing must match explicit build selection")
    require(type(worker_stack_bench) is bool and (not worker_stack_bench or (ota and memory_diagnostics)),
            "worker stack bench requires explicit OTA memory diagnostics")
    require(config.get("TV4_OTA_WORKER_STACK_BENCH", False)==worker_stack_bench,
            "worker stack bench must match explicit build selection")
    require(type(worker_tls_bench) is bool and (not worker_tls_bench or (ota and memory_diagnostics and worker_stack_bench)),
            "worker TLS bench requires explicit OTA stack/trace diagnostics")
    require(config.get("TV4_OTA_WORKER_TLS_BENCH", False)==worker_tls_bench,
            "worker TLS bench must match explicit build selection")
    if worker_stack_bench:
        context=json.loads((build/'ota-local/ota_local_context.json').read_text())
        require(type(context) is dict and context.get('provisioned') is False and context.get('private_key_present') is False,
                "worker stack bench must remain unprovisioned")
    require(type(signed_check_bench) is bool and (not signed_check_bench or (ota and memory_diagnostics and not worker_stack_bench and not worker_tls_bench)),"signed CHECK bench requires exclusive OTA trace selection")
    require(config.get("TV4_OTA_SIGNED_CHECK_BENCH",False)==signed_check_bench,"signed CHECK bench must match explicit build selection")
    if signed_check_bench:
        context=json.loads((build/'ota-local/ota_local_context.json').read_text())
        require(type(context) is dict and context.get('provisioned') is True and context.get('private_key_present') is False,'signed CHECK bench requires public-only configured context')
    require(config.get("TV4_OTA_ENABLED", False) == ota, "OTA requires explicit migration profile")
    require(config.get("LV_USE_CUSTOM_MALLOC",False)==ota and config.get("LV_USE_CLIB_MALLOC",False)==(not ota),
            "OTA requires the explicit LVGL PSRAM allocator; ordinary profiles require CLIB")
    for name in ("SPIRAM_XIP_FROM_PSRAM","SPIRAM_FETCH_INSTRUCTIONS","SPIRAM_RODATA"):
        require(config.get(name,False)==ota, "PSRAM XiP is required only in the explicit OTA profile")
    if ota:
        require(config.get("LCD_RGB_ISR_IRAM_SAFE",False), "OTA RGB/GDMA interrupts must remain IRAM-safe during Flash writes")
        require(config.get("BOOTLOADER_APP_ROLLBACK_ENABLE", False), "OTA rollback required")
        require(all(config.get(n,False) for n in ("ESP_TASK_WDT_EN","ESP_TASK_WDT_INIT","ESP_TASK_WDT_PANIC","ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0","ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1","ESP_INT_WDT","ESP_SYSTEM_PANIC_PRINT_REBOOT")) and config.get("ESP_TASK_WDT_TIMEOUT_S")==5, "bounded watchdog/reset required")
        require(not any(config.get(n,False) for n in ("BOOTLOADER_APP_ANTI_ROLLBACK","SECURE_BOOT","SECURE_FLASH_ENC_ENABLED")), "irreversible security options forbidden")
        require(config.get("PARTITION_TABLE_CUSTOM_FILENAME") == "ota/partitions.ab.csv", "reviewed migration table required")
    tls_bench = profile in {"rev12-tls-bench", "rev12-ota"}
    allocation_modes = {name for name in ("INTERNAL", "EXTERNAL", "DEFAULT", "CUSTOM", "IRAM_8BIT")
                        if config.get(f"MBEDTLS_{name}_MEM_ALLOC", False)}
    expected_allocation = "EXTERNAL" if tls_bench else "INTERNAL"
    require(allocation_modes == {expected_allocation},
            f"mbedTLS allocator must be {expected_allocation} for {profile}")
    if tls_bench:
        require(config.get("MBEDTLS_CERTIFICATE_BUNDLE", False), "TLS bench requires CA bundle")
        require(config.get("MBEDTLS_HAVE_TIME", False) and config.get("MBEDTLS_HAVE_TIME_DATE", False),
                "TLS bench requires certificate validity-date checks, not only a local time anchor")
        require(not config.get("ESP_TLS_INSECURE", False), "TLS bench forbids insecure TLS options")
        require(config.get("MBEDTLS_ASYMMETRIC_CONTENT_LEN", False), "TLS bench requires explicit record sizes")
        require(config.get("MBEDTLS_SSL_IN_CONTENT_LEN") == 16384 and
                config.get("MBEDTLS_SSL_OUT_CONTENT_LEN") == 4096,
                "TLS bench must retain 16384/4096-byte record payloads")
    if profile != "headless":
        for name, value in {"SPIRAM": True, "SPIRAM_MODE_OCT": True, "SPIRAM_SPEED": 80,
                            "SPIRAM_USE_MALLOC": True, "SPIRAM_MALLOC_ALWAYSINTERNAL": 16384,
                            "SPIRAM_MALLOC_RESERVE_INTERNAL": 65536}.items():
            require(config[name] == value, f"{name} must equal {value}")
    else:
        require(not config.get("SPIRAM", False), "headless must not initialize PSRAM")
    partitions = []
    raw = (build / "partition_table/partition-table.bin").read_bytes()
    require(len(raw) % 32 == 0, "truncated partition table")
    for offset in range(0, len(raw), 32):
        magic, = struct.unpack_from("<H", raw, offset)
        if magic != 0x50AA:
            continue
        _, kind, subtype, address, size, label, flags = struct.unpack_from("<HBBII16sI", raw, offset)
        require(flags == 0, "partition encryption/unknown flags forbidden")
        require(address + size <= 16 * 1024 * 1024, "partition exceeds 16 MB")
        partitions.append(dict(kind=kind, subtype=subtype, address=address, size=size,
                               label=label.rstrip(b"\0").decode()))
    apps = [p for p in partitions if p["kind"] == 0 and p["subtype"] == 0]
    require(len(apps) == 1 and apps[0]["address"] == 0x10000, "expected single factory app at 0x10000")
    if ota:
        require([(p['kind'],p['subtype'],p['address'],p['size'],p['label']) for p in partitions]==OTA_PARTITIONS,"OTA layout mismatch")
    else:
        require([(p["kind"],p["subtype"],p["address"],p["size"],p["label"]) for p in partitions]==[(1,2,0x9000,0x6000,"nvs"),(1,1,0xf000,0x1000,"phy_init"),(0,0,0x10000,0x200000,"factory")], "ordinary profile must retain factory-only layout")
    images = {}
    for rel in ("tv4_hmi.bin", "bootloader/bootloader.bin"):
        image = (build / rel).read_bytes()
        require(len(image) >= 24 and image[0] == 0xE9, f"invalid image header: {rel}")
        require(image[3] >> 4 == 4, f"image flash-size header must specify 16 MB: {rel}")
        require(struct.unpack_from("<H", image, 12)[0] == 9, f"image chip id must be ESP32-S3: {rel}")
        images[rel] = dict(bytes=len(image), sha256=hashlib.sha256(image).hexdigest())
    require(images["tv4_hmi.bin"]["bytes"] <= apps[0]["size"], "application exceeds factory partition")
    if actual_reproducible:
        from esp_app_image import inspect_app
        image = (build / "tv4_hmi.bin").read_bytes()
        app = inspect_app(image, version)
        # The first segment begins with esp_app_desc_t: time[16], date[16].
        require(image[32 + 80:32 + 112] == bytes(32), "compilation date/time retained in app descriptor")
        elf = (build / "tv4_hmi.elf").read_bytes()
        require(len(elf) >= 52 and elf[:6] == b"\x7fELF\x01\x01" and struct.unpack_from('<H', elf, 18)[0] == 94,
                "expected linked Xtensa ELF32")
        require(hashlib.sha256(elf).hexdigest() == app['elf_sha256'], "app/ELF hash mismatch")
    result = dict(profile=profile, sdk=version, target="esp32s3", flash_bytes=16*1024*1024,
                  mbedtls_allocator="spiram-only" if tls_bench else "internal-only",
                  mbedtls_certificate_dates=config.get("MBEDTLS_HAVE_TIME_DATE", False),
                  tls_record_in=config.get("MBEDTLS_SSL_IN_CONTENT_LEN"),
                  tls_record_out=config.get("MBEDTLS_SSL_OUT_CONTENT_LEN"),
                  memory_bench=config.get("TV4_OTA_MEMORY_BENCH", False), tls_bench=config.get("TV4_UTC_TLS_BENCH", False), psram="octal 80MHz" if profile != "headless" else "disabled", images=images, partitions=partitions,
                  device_access=False, physical_performance="not measured")
    if ota:
        result['ota_memory_diagnostics']=memory_diagnostics
        result['ota_install_reviewed']=install_policy(build)
    if actual_reproducible:result['reproducible_build']=True
    if worker_stack_bench:result['ota_worker_stack_bench']=True
    if worker_tls_bench:result['ota_worker_tls_bench']=True
    if signed_check_bench:result['ota_signed_check_bench']=True
    if write_report:
        (build / "validation.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build');parser.add_argument('profile');parser.add_argument('sdk')
    parser.add_argument('--memory-diagnostics',action='store_true')
    parser.add_argument('--worker-stack-bench',action='store_true')
    parser.add_argument('--worker-tls-bench',action='store_true')
    parser.add_argument('--signed-check-bench',action='store_true')
    parser.add_argument('--reproducible',action='store_true')
    args=parser.parse_args()
    try:
        verify(args.build,args.profile,args.sdk,memory_diagnostics=args.memory_diagnostics,worker_stack_bench=args.worker_stack_bench,worker_tls_bench=args.worker_tls_bench,signed_check_bench=args.signed_check_bench,reproducible=args.reproducible)
    except (ValueError, KeyError, OSError, struct.error) as error:
        raise SystemExit(f"Build validation failed: {error}")
