# Bench.4: reset-before-confirmation rollback and factory recovery

[Plan](../../../plans/P1_RESET_ROLLBACK.md), [software/tests](../../build/esp32-ota-reset-rollback/README.md),
[physical numeric results](measurements.json). Qualification base main `8d1603e`;
unchanged signed network image bench.4/source `6c7b1d5`, ESP-IDF 6.1.1, Waveshare Rev 1.2.
This finite session closed **PASS in 924.525 s / 1800 s**.

## Measured hardware results

| Case | Physical result |
|---|---|
| Existing dual-VALID checkpoint | seq1/VALID2 OTA0 and seq2/VALID2 OTA1; exact apps/ELF/native records verified |
| Healthy bench.4 control | 90 s progressing boot/PSRAM/RGB/GT911/UI/saved Wi-Fi PASS |
| Interrupted first confirmation | Write seq2/NEW0 for the same verified image; observe exact OTA1/version/ELF; one deliberate RTS reset before self-test completion |
| Native rollback | Subsequent actual read: seq1/VALID2 + seq2/ABORTED4, exact native CRC/tails; healthy OTA0 `.3` fallback 90 s |
| Both OTA records rejected | Write two native ABORTED4 records; factory fecc05d boots and remains healthy 90 s; metadata remains [4,4] |
| Restore known-good selection | Byte-exact original already-VALID metadata restored; bench.4 boot 90 s and dual-VALID readback PASS |
| Whole Flash | Actual read 16 MiB/device MD5; **every non-NVS byte matches the pre-test checkpoint**, two new physical NVMe copies with SHA/fsync PASS |
| Final state | bench.4 UART 30 s PASS; **left running bench.4 OTA1 VALID** |

Three normal host writes, all 8192 bytes at otadata 0x210000. **No application,
bootloader, partition table or factory image was written**; full recovery 0.
Only bootloader-native state changes and runtime NVS updates are expected.
All raw metadata was saved privately **before** validation, including failures
if any. Current image equals the previously signed/downloaded release by exact
SHA; OTA0 `.3` is the separately approved receiving fixture of the same source,
not the different immutable published bench.3 binary.

The controlled interruption occurred 4.024 s after the host's initial reset,
after exact descriptor/partition identity and before any self-test completion
marker. This is host timing, not physical reset latency. NEW0 and ABORTED4 were
read directly; intermediate PENDING_VERIFY1 is inferred from the pinned SDK
bootloader transition and subsequent ABORTED read, not presented as an
independently read intermediate state.

All boot observers require exact identity, no fatal/unexpected network operations,
progressing uptime/valid resource diagnostics/fresh touch/error counters/saved IP.
Touch sample age ≤19 ms across the captured boots is freshness, not latency.
Legacy factory emits no newer boot-self-test marker; its actual SDK PSRAM memory
test, RGB/GT911/UI and 16 periodic samples are verified without inventing one.
Factory is the previous unprovisioned recovery; its smaller legacy idle heap is
not a qualified TLS/INSTALL production budget. The configured `.3`/`.4` traces
retain their normal self-test and local trust policy requirements.

## Scope, recovery and retention

The owner provided standing permission for isolated reversible board checks and
required the good release to remain installed. Packet is sealed/single-use;
maximum 1800 s, recovery reserve 420 s, phase deadline 400 s for the final full UART
read without extending the absolute process deadline. Software faults/timers/
collector/tamper tests passed before opening serial. Source firmware/dependencies/
pins/clock/framebuffer/TLS/floors and published releases are unchanged. eFuse/
option bytes, STM32, CAN and motion were not accessed.

Before writes, previous actual complete snapshot + fresh NVS 24 KiB was revalidated
against **all** current device Flash and saved twice on different NVMe devices.
That is complete snapshot revalidation, not a fresh 16 MiB pre-test UART read.
The final 16 MiB **was actually read again** and fully compared outside NVS; hashes,
raw dumps/NVS/UART, keys, local paths and contexts remain private.

Restoring the original VALID records restores a previously qualified, unchanged
image checkpoint; it is not promotion of an unknown candidate or a bypass of the
normal OTA self-test/confirmation path. Factory and OTA0 launches were temporary
qualification steps; the final active release is bench.4, as requested.

This is reset recovery for an image previously installed through signed HTTPS.
It is **not** a fresh network INSTALL failure, Wi-Fi interruption, physical
power-cut test or full P1 acceptance. Download/cancel/fault tests, production
policy/budgets, runtime 5.3.2 and numerical FPS/touch-to-display remain open.
Bench contexts retain their 25 November 2026 08:00 Asia/Almaty limit.

After the UART session closed, the owner confirmed the final bench.4 main screen,
touch input and saved Wi-Fi all work. No further device access was performed.
