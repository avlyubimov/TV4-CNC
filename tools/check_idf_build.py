#!/usr/bin/env python3
"""Inspect compiler outputs only; never imports serial libraries or opens a device."""
import hashlib
import json
import struct
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(build, profile, version, *, write_report=True):
    require(profile in {"headless", "rev12", "rev12-memory-bench", "rev12-tls-bench"}, "unknown profile")
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
    tls_bench = profile == "rev12-tls-bench"
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
        _, kind, subtype, address, size, label, _ = struct.unpack_from("<HBBII16sI", raw, offset)
        require(address + size <= 16 * 1024 * 1024, "partition exceeds 16 MB")
        partitions.append(dict(kind=kind, subtype=subtype, address=address, size=size,
                               label=label.rstrip(b"\0").decode()))
    apps = [p for p in partitions if p["kind"] == 0 and p["subtype"] == 0]
    require(len(apps) == 1 and apps[0]["address"] == 0x10000, "expected single factory app at 0x10000")
    images = {}
    for rel in ("tv4_hmi.bin", "bootloader/bootloader.bin"):
        image = (build / rel).read_bytes()
        require(len(image) >= 24 and image[0] == 0xE9, f"invalid image header: {rel}")
        require(image[3] >> 4 == 4, f"image flash-size header must specify 16 MB: {rel}")
        require(struct.unpack_from("<H", image, 12)[0] == 9, f"image chip id must be ESP32-S3: {rel}")
        images[rel] = dict(bytes=len(image), sha256=hashlib.sha256(image).hexdigest())
    require(images["tv4_hmi.bin"]["bytes"] <= apps[0]["size"], "application exceeds factory partition")
    result = dict(profile=profile, sdk=version, target="esp32s3", flash_bytes=16*1024*1024,
                  mbedtls_allocator="spiram-only" if tls_bench else "internal-only",
                  mbedtls_certificate_dates=config.get("MBEDTLS_HAVE_TIME_DATE", False),
                  tls_record_in=config.get("MBEDTLS_SSL_IN_CONTENT_LEN"),
                  tls_record_out=config.get("MBEDTLS_SSL_OUT_CONTENT_LEN"),
                  memory_bench=config.get("TV4_OTA_MEMORY_BENCH", False), tls_bench=config.get("TV4_UTC_TLS_BENCH", False), psram="octal 80MHz" if profile != "headless" else "disabled", images=images, partitions=partitions,
                  device_access=False, physical_performance="not measured")
    if write_report:
        (build / "validation.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("Usage: check_idf_build.py BUILD_DIR headless|rev12|rev12-memory-bench|rev12-tls-bench 5.3.2|6.1.1")
    try:
        verify(*sys.argv[1:])
    except (ValueError, KeyError, OSError, struct.error) as error:
        raise SystemExit(f"Build validation failed: {error}")
