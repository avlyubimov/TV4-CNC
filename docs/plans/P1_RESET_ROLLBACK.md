# P1: interrupted confirmation and factory recovery of installed bench.4

## Concrete result and source

Qualification starts from main `8d1603e`, after owner-merged PR #86. The installed
network image remains the immutable owner-approved `v0.1.0-bench.4`, source
`6c7b1d5`, app SHA-256
`1e3de8c629ceb1800c5ba1f947858a5a54e5cce1587810eaa45f7febf5e27945`.
OTA1 is VALID; OTA0 contains the separately approved receiving `.3` fixture of
the same source; factory contains the previous fecc05d recovery. No application,
bootloader, partition table, key, context or release needs replacement for this
qualification. Test enum values now match the pinned SDK, including ABORTED;
host assertions cover refusal to promote/reinstall NEW/INVALID/ABORTED/UNDEFINED.

The [Espressif rollback contract](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/api-reference/system/ota.html#rollback-process)
and pinned SDK bootloader source are the reference: an interrupted unconfirmed
boot becomes ABORTED and is excluded from selection. The physical observations
must establish the result; compiled SDK logic alone is not acceptance.

## Finite hardware procedure

- Validate the exact chip, protection, 16MiB Flash, signed published image and
  current dual-VALID checkpoint. Revalidate the previous complete read against
  all device Flash with fresh NVS, then save two fsynced private copies on
  different physical NVMe devices. The starting state is not factory-sole/FF.
- Capture healthy bench.4 for 90 s. Write only native otadata 8 KiB: seq1/VALID for
  OTA0 and seq2/NEW for the same, already verified bench.4 in OTA1.
- Keep UART open. After the exact OTA1 partition/version/ELF identity appears,
  before its boot self-test completion, perform one controlled RTS reset. The
  trigger is bounded by 15 s; read 90 s of the ensuing OTA0 fallback. Persist metadata
  before validation and require seq2/ABORTED with native CRC/tails unchanged.
- Set both records to ABORTED and require a healthy 90 s factory boot. Legacy
  factory does not emit the newer self-test marker; its acceptance uses exact
  image identity, SDK PSRAM test, RGB/GT911/UI, progressing diagnostics, fresh
  touch, clear counters and saved Wi-Fi. Do not invent a marker or waive these
  observable checks.
- Restore byte-for-byte the original already-VALID metadata checkpoint. This
  restores a previously qualified image; it does not confirm an unknown new
  candidate. Require bench.4 UART 90 s, native dual-VALID readback, a real complete
  read 16 MiB and equality of all non-NVS bytes, two fresh final copies, then
  bench.4 UART 30 s. **Leave bench.4 active.**

Normal host writes: three exact 8 KiB otadata writes only. Application/factory/
bootloader/table/PHY/gaps/resources/store stay protected. Maximum session 1800 s;
reserve 420 s for recovery; final full read has its own 400 s deadline that preserves
the absolute session timer. On failure, at most one extra original-metadata
restore; if required and enough reserve remains, one verified full-current-
snapshot recovery. No forced-security flags, eFuse, option bytes, STM32, CAN,
motion, external power switching, router changes or fresh network INSTALL.
Existing owner standing board permission and leave-release instruction apply;
each packet is sealed and single use. Operator update buttons stay idle.

## Evidence and limits

Require strict UART identity/health/freshness, exact metadata CRC/state, immutable
apps and final checkpoint, plus host fault/timer/collector/tamper tests. Failures
remain failures; save unexpected metadata before comparison. The PENDING step
is inferred from NEW, the pinned bootloader logic and the subsequent physical
ABORTED record, rather than claimed as a separately read intermediate state.

This qualifies reset-before-confirmation and factory selection for an image
previously installed through the signed network path. It is **not** a new
network INSTALL failure, Wi-Fi loss, physical power-cut test, production budget
or complete P1 acceptance. Those remain subsequent stages.

## Execution

[Hardware qualification](../evidence/hardware/2026-11-24-esp32-reset-rollback/README.md)
closed PASS. Three exact metadata-only writes, native ABORTED/OTA0 and factory
branches observed; whole16MiB read/byte comparison and restored bench.4 left
active. Other P1 network/power/production cases remain open.
