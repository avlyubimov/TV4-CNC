# OTA time-range refusal: software evidence

Base main `10cd8cd`, after PR #89. [Plan](../../../plans/P1_TIME_POLICY_REFUSAL.md).
The original bench contexts expired on 25 November 2026 at 08:00 Asia/Almaty;
they are not extended or overwritten. Existing release assets and keys remain
unchanged. Hardware observations will be recorded separately.

Previously every failed clock acquisition produced service error 3 and the
generic network/settings message. Only successful cleanup with
`TV4_SNTP_TIME_REJECTED / TV4_TIME_RANGE` now produces new service error 24:
**«Дата вне разрешённого периода OTA»**. Earlier and later dates share this
status; unauthenticated SNTP does not prove the real date or expiry. Other
clock/source/timeout/cleanup errors keep their existing behavior and codes.
One fixed numeric `clock result/time/cleanup` record is emitted per operation,
without dates, addresses, credentials or key material. No new task or allocation
is introduced. Signature/TLS/clock/resource checks and all command gates remain.

- [290 host tests](host-tests.log) PASS.
- [290 ASan/UBSan/LeakSanitizer tests](sanitizer-tests.log) PASS.
- Fourteen new repeated-CHECK actual-service tests: before/after policy, invalid clock, timeout,
  forbidden source, failed cleanup and stale range detail with failed cleanup,
  in both CHECK-only and install-reviewed configurations. Each performs two
  failed CHECKs in one worker process with complete lease/monitor cleanup and
  refused INSTALL/REBOOT. These SDK-boundary tests do not use real devices.
- The additional reviewed-install case authenticates a candidate while time
  is valid, then expires time before INSTALL. The installer is never entered;
  the next CHECK refuses again. Cached candidate details never grant permission.
- The existing opt-in one-shot read-only CHECK is also exercised with a range
  refusal; no session starts and the expected diagnostic summary is attempted=1/pass=0.
- Actual LVGL RGB565/XRGB8888 tests verify the new label, layout, disabled
  install/reboot, permitted fresh CHECK and no redraw for unchanged state.
  The Russian string ID is appended; previous IDs and 5 Hz progress coalescing
  remain unchanged. [Real LVGL simulator preview](ota-time-range-preview.png).
- [Exact source CI](source-ci.json): `104fb06`, 15/15 successful jobs, including both pinned
  ESP-IDF SDKs. [Results/source hashes](results.json).

The host total includes sixteen new test executables' invocations: fourteen
repeated-CHECK scenarios, one CHECK-before-expiry/INSTALL-after-expiry case and
one existing one-shot read-only CHECK time-range refusal.
No device was accessed for these software results. Physical observations are
[recorded separately](../../hardware/2026-11-25-esp32-time-policy/README.md).
The first hardware attempt timed out in SNTP; a separately approved single
repeat qualified exact TIME_RANGE/error24 and all 45 guards. Original OTA0/metadata,
actual final 16MiB/two-NVMe and healthy bench.4 retention passed. This is not
production time-policy approval.

Both exact configured diagnostic ELF/BIN builds passed on [SDK 6.1.1 and 5.3.2](configured-builds.json).
[Actual linked Xtensa clock-policy, resource-floor and public-key objects](compiled-policy.json) match
unchanged inputs; this is target ELF inspection, not a host-size inference.
[10 recovery/timer procedure tests](procedure-tests.log), [8 strict observer tests](observer-tests.log),
[6 loader/tamper tests](loader-tests.log) and 10 pinned native metadata oracle
[cases](metadata-proof.json) PASS before serial access. The finite packet preserves seq2/VALID/OTA1;
all six normal writes touch only OTA0 or the **first** 4 KiB of otadata. A park
removes the OTA0 record before each image replacement; original selection is
restored only after the original full image. No second metadata-sector write.

Initial automatic approval review rejected the launch before device access.
After fresh exact owner approval the first hardware attempt ran and closed:
SNTP timeout7, successful cleanup, scoped restoration PASS, bench.4 retained.
The failure is preserved separately. The same exact image/policy/timeout passed
one separately approved repeat: TIME_RANGE/error24 and complete recovery/readback.
Only SDK 6.1.1 ran on hardware; SDK 5.3.2 was built/inspected. Runtime policy was not extended.
