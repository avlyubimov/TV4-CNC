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


def verify(build, profile, version):
    require(profile in {"headless", "rev12"}, "unknown profile")
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
    require(config.get("TV4_BOARD_REV12", False) == (profile == "rev12"), "wrong board enable gate")
    if profile == "rev12":
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
                  psram="octal 80MHz" if profile == "rev12" else "disabled", images=images, partitions=partitions,
                  device_access=False, physical_performance="not measured")
    (build / "validation.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("Usage: check_idf_build.py BUILD_DIR headless|rev12 5.3.2|6.1.1")
    try:
        verify(*sys.argv[1:])
    except (ValueError, KeyError, OSError, struct.error) as error:
        raise SystemExit(f"Build validation failed: {error}")
