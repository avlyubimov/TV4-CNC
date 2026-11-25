# ESP32 time-policy negative qualification — 25 November 2026

Exact source `104fb065c43093936713e52bd0d1521fa3c70d30`, base main `10cd8cd`;
ESP-IDF 6.1.1, isolated Waveshare ESP32-S3 Rev1.2. Software evidence and both
SDK builds are [separate](../../build/esp32-ota-time-refusal/README.md).
The original policy ended on 25 November at 08:00 Asia/Almaty and is unchanged.
The temporary diagnostic is **unpublished** receiving `.3`, not the immutable
published bench.3 binary. Installed bench.4 remains unchanged.

## Attempt 1: intended range refusal not qualified

[Closed result](attempt1.json): 396.821 seconds / 1800 seconds. Control bench.4,
PSRAM/XiP, RGB, GT911, self-test, fresh touch, queues, stack, resource floors and
cleanup passed. The one automatic read-only CHECK produced `clock result=7,
time=0, cleanup=1`: **SNTP timeout**, with service stage9/error3. The qualification
required result10/time5/cleanup1 and stage9/error24. Those exact two checks failed;
they were not weakened or substituted with a generic clock failure. No session
or TLS probe was entered according to the observed trace and source flow; this
is not an independent network capture or TLS qualification.

Preflight revalidated a previous actual full 16 MiB read with fresh NVS 24 KiB
and whole-device MD5, then verified two copies on different physical NVMe.
This was not a fresh full preflight UART read. Six normal writes were verified
and checked for changes outside each approved span: park first otadata4KiB,
replace OTA0, select diagnostic NEW seq3; park again, restore whole original
OTA0, restore original first sector. The second VALID sector and OTA1 bench.4
were never written. Failure restoration verified original dual-VALID metadata
and a healthy bench.4 boot; full-snapshot recovery attempts: **0**.

After the failed qualification there was **no** new final whole 16 MiB read,
no final full-snapshot copy pair and no independent diagnostic seq3 VALID/full
slot readback. Per-write verify/preservation and scoped recovery passed; do not
report the skipped stronger checks as completed. Owner confirmed the restored
screen, touch and Wi-Fi work. UART session closed, bench.4 left in OTA1 VALID.

A launch initially rejected by automatic approval review did not open a device.
The executed session followed the owner's fresh exact approval. Its one-session
permission was consumed; any repeat is separately sealed and authorized.

## Separately approved attempt 2: exact refusal and final recovery PASS

[Successful repeat](measurements.json): 792.933 seconds / 1800 seconds, same
source 104fb06/app hash/SDK/context/key/floors and 10-second SNTP timeout. One
automatic read-only CHECK reported result10/time5/cleanup1 and stage9/error24.
All **45** boot, sensor, resource, timing, queue and cleanup guards passed. The
original first timeout remains a separate failure; this repeat does not identify
its cause. No third attempt was performed.

Actual native metadata read showed seq3/VALID in OTA0 and preserved seq2/VALID
in OTA1. The whole diagnostic slot was verified by device digest against exact
expected bytes; it was not independently transferred to the host. After the six
verified/preservation-checked host writes, original OTA0 and first metadata
sector were restored; OTA1 and the second VALID sector were never host-written.
A new actual full 16 MiB read, whole-device MD5, equality outside NVS and two
final copies on different physical NVMe passed. Final bench.4 boot/capture30s
passed, UART closed, owner confirmed screen/touch/Wi-Fi. Full recovery: **0**.

Across workspace/SNTP/cleanup windows the minimum local internal free was
117343 bytes; minimum boundary largest block63488 and worker watermark9856 of
12288 bytes. Periodic touch age was at most4ms, with touch/display/queue errors0.
These are sampled software ages/window observations, not physical latency,
production memory qualification or TLS/INSTALL peaks. The clock gate prevented
session entry; absence of TLS follows trace/source flow, not a wire capture.
The new Russian label is verified in real LVGL simulator renders; the temporary
on-device worker error24 was measured, its page text was not visually observed.

The unchanged **bench.4, OTA1 VALID** remains installed. The new error label and
previous cancellation-label fix are not part of that immutable release. No new
tag/release/signing operation or extension of the expired policy was performed.

## Independent PC network observation

[One host-only NTP request](host-ntp-reachability.json) received a valid correlated
mode4/stratum3 reply from `pool.ntp.org` over UDP/123 in 0.037615 seconds. No PC
or device clock/settings changed. NTP was unauthenticated. This is a different
network path and does **not** prove ESP32 reachability or establish its timeout
cause. Timeout, policy and resource limits remain unchanged.

Raw UART, Flash/NVS data, backup hashes/locations, owner contexts and keys remain
private outside Git. F407/CAN/motion/eFuse/option bytes and physical power were
not accessed. RF/AP/power-loss behavior, runtime SDK 5.3.2, production budgets,
physical FPS and touch-to-display latency remain unqualified.
