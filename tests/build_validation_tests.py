"""Negative tests for artifact inspection. Fixtures are not executable firmware."""
import contextlib
import copy
import importlib.util
import io
import json
import struct
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("checker", Path(__file__).resolve().parents[1] / "tools/check_idf_build.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class BuildValidationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ["config", "partition_table", "bootloader"]:
            (self.root / name).mkdir()
        self.config = dict(ESPTOOLPY_FLASHSIZE="16MB", ESPTOOLPY_FLASHFREQ="80m", LV_COLOR_DEPTH=16,
                           FREERTOS_HZ=1000, ESP_DEFAULT_CPU_FREQ_MHZ=240, TV4_BOARD_REV12=False,
                           MBEDTLS_INTERNAL_MEM_ALLOC=True)
        (self.root / "project_description.json").write_text(json.dumps(dict(target="esp32s3", git_revision="v6.1.1")))
        header = bytearray(24)
        header[0] = 0xE9
        header[3] = 0x40
        struct.pack_into("<H", header, 12, 9)
        for name in ["tv4_hmi.bin", "bootloader/bootloader.bin"]:
            (self.root / name).write_bytes(header)
        self.partition()

    def partition(self, address=0x10000, size=0x200000):
        data = struct.pack("<HBBII16sI", 0x50AA, 1, 2, 0x9000, 0x6000, b"nvs", 0) + struct.pack("<HBBII16sI", 0x50AA, 1, 1, 0xf000, 0x1000, b"phy_init", 0) + struct.pack("<HBBII16sI", 0x50AA, 0, 0, address, size, b"factory", 0)
        (self.root / "partition_table/partition-table.bin").write_bytes(data)

    def check(self, profile="headless"):
        (self.root / "config/sdkconfig.json").write_text(json.dumps(self.config))
        with contextlib.redirect_stdout(io.StringIO()):
            return checker.verify(self.root, profile, "6.1.1")

    def test_valid_headless(self):
        self.assertEqual(self.check()["flash_bytes"], 16777216)

    def test_wrong_flash_or_board_gate(self):
        for key, value in [("ESPTOOLPY_FLASHSIZE", "2MB"), ("TV4_BOARD_REV12", True), ("SPIRAM", True)]:
            original = copy.deepcopy(self.config)
            self.config[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.check()
            self.config = original

    def test_rev12_requires_correct_psram(self):
        self.config.update(TV4_BOARD_REV12=True, SPIRAM=True, SPIRAM_MODE_OCT=True, SPIRAM_SPEED=80,
                           SPIRAM_USE_MALLOC=True, SPIRAM_MALLOC_ALWAYSINTERNAL=16384, SPIRAM_MALLOC_RESERVE_INTERNAL=65536)
        self.check("rev12")
        self.config["SPIRAM_SPEED"] = 40
        with self.assertRaises(ValueError):
            self.check("rev12")

    def test_memory_bench_gate(self):
        self.config["TV4_OTA_MEMORY_BENCH"] = True
        with self.assertRaises(ValueError):
            self.check("headless")
        self.config.update(TV4_BOARD_REV12=True, SPIRAM=True, SPIRAM_MODE_OCT=True, SPIRAM_SPEED=80,
                           SPIRAM_USE_MALLOC=True, SPIRAM_MALLOC_ALWAYSINTERNAL=16384, SPIRAM_MALLOC_RESERVE_INTERNAL=65536)
        with self.assertRaises(ValueError):
            self.check("rev12")
        self.assertTrue(self.check("rev12-memory-bench")["memory_bench"])
        self.config["TV4_OTA_MEMORY_BENCH"] = False
        with self.assertRaises(ValueError):
            self.check("rev12-memory-bench")

    def test_tls_bench_gate(self):
        self.config["TV4_UTC_TLS_BENCH"] = True
        with self.assertRaises(ValueError):
            self.check("headless")
        self.config.update(TV4_BOARD_REV12=True, SPIRAM=True, SPIRAM_MODE_OCT=True, SPIRAM_SPEED=80,
                           SPIRAM_USE_MALLOC=True, SPIRAM_MALLOC_ALWAYSINTERNAL=16384, SPIRAM_MALLOC_RESERVE_INTERNAL=65536)
        with self.assertRaises(ValueError):
            self.check("rev12")
        with self.assertRaises(ValueError):
            self.check("rev12-tls-bench")
        self.config["MBEDTLS_CERTIFICATE_BUNDLE"] = True
        self.config.update(MBEDTLS_INTERNAL_MEM_ALLOC=False, MBEDTLS_EXTERNAL_MEM_ALLOC=True,
                           MBEDTLS_HAVE_TIME=True, MBEDTLS_HAVE_TIME_DATE=True,
                           MBEDTLS_ASYMMETRIC_CONTENT_LEN=True,
                           MBEDTLS_SSL_IN_CONTENT_LEN=16384, MBEDTLS_SSL_OUT_CONTENT_LEN=4096)
        self.assertTrue(self.check("rev12-tls-bench")["tls_bench"])
        self.config["TV4_OTA_MEMORY_BENCH"] = True
        with self.assertRaises(ValueError):
            self.check("rev12-tls-bench")
        with self.assertRaises(ValueError):
            self.check("rev12-memory-bench")

    def tls_config(self):
        self.config.update(TV4_BOARD_REV12=True, TV4_UTC_TLS_BENCH=True,
                           SPIRAM=True, SPIRAM_MODE_OCT=True, SPIRAM_SPEED=80,
                           SPIRAM_USE_MALLOC=True, SPIRAM_MALLOC_ALWAYSINTERNAL=16384,
                           SPIRAM_MALLOC_RESERVE_INTERNAL=65536,
                           MBEDTLS_INTERNAL_MEM_ALLOC=False, MBEDTLS_EXTERNAL_MEM_ALLOC=True,
                           MBEDTLS_HAVE_TIME=True, MBEDTLS_HAVE_TIME_DATE=True,
                           MBEDTLS_CERTIFICATE_BUNDLE=True, MBEDTLS_ASYMMETRIC_CONTENT_LEN=True,
                           MBEDTLS_SSL_IN_CONTENT_LEN=16384, MBEDTLS_SSL_OUT_CONTENT_LEN=4096)

    def test_tls_allocator_and_security_regressions(self):
        self.tls_config()
        good = copy.deepcopy(self.config)
        for key, value in [("MBEDTLS_EXTERNAL_MEM_ALLOC", False), ("MBEDTLS_INTERNAL_MEM_ALLOC", True),
                           ("MBEDTLS_DEFAULT_MEM_ALLOC", True), ("MBEDTLS_CUSTOM_MEM_ALLOC", True),
                           ("MBEDTLS_IRAM_8BIT_MEM_ALLOC", True), ("MBEDTLS_CERTIFICATE_BUNDLE", False),
                           ("MBEDTLS_HAVE_TIME", False), ("MBEDTLS_HAVE_TIME_DATE", False),
                           ("ESP_TLS_INSECURE", True), ("MBEDTLS_ASYMMETRIC_CONTENT_LEN", False),
                           ("MBEDTLS_SSL_IN_CONTENT_LEN", 4096), ("MBEDTLS_SSL_OUT_CONTENT_LEN", 512),
                           ("SPIRAM", False), ("SPIRAM_MALLOC_RESERVE_INTERNAL", 32768)]:
            self.config = copy.deepcopy(good)
            self.config[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.check("rev12-tls-bench")
        self.config = good
        del self.config["MBEDTLS_HAVE_TIME_DATE"]
        with self.assertRaises(ValueError):
            self.check("rev12-tls-bench")

    def test_allocator_cannot_leak_into_ordinary_profiles(self):
        for profile in ("headless", "rev12", "rev12-memory-bench"):
            if profile != "headless":
                self.config.update(TV4_BOARD_REV12=True, SPIRAM=True, SPIRAM_MODE_OCT=True, SPIRAM_SPEED=80,
                                   SPIRAM_USE_MALLOC=True, SPIRAM_MALLOC_ALWAYSINTERNAL=16384,
                                   SPIRAM_MALLOC_RESERVE_INTERNAL=65536,
                                   TV4_OTA_MEMORY_BENCH=profile == "rev12-memory-bench")
            self.config.update(MBEDTLS_INTERNAL_MEM_ALLOC=False, MBEDTLS_EXTERNAL_MEM_ALLOC=True)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                self.check(profile)

    def test_bad_image_headers(self):
        original = (self.root / "tv4_hmi.bin").read_bytes()
        for offset, value in [(0, 0), (3, 0x10), (12, 0)]:
            data = bytearray(original)
            data[offset] = value
            (self.root / "tv4_hmi.bin").write_bytes(data)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                self.check()

    def test_partition_overflow(self):
        self.partition(address=0x10000, size=16 * 1024 * 1024)
        with self.assertRaises(ValueError):
            self.check()

    def test_no_assert_optimization_bypass(self):
        with self.assertRaises(ValueError):
            checker.require(False, "reject even under python -O")


if __name__ == "__main__":
    unittest.main()
