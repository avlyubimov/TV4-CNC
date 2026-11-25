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
- [Base main CI](source-ci.json): 15/15 successful jobs, including both pinned
  ESP-IDF SDKs. [Results/source hashes](results.json).

The host total includes sixteen new test executables' invocations: fourteen
repeated-CHECK scenarios, one CHECK-before-expiry/INSTALL-after-expiry case and
one existing one-shot read-only CHECK time-range refusal.
No device was accessed for these software results. A new, single-use expired
context test packet is required for physical qualification; the existing bench.4
must be retained afterwards. This is not a production time-policy approval.
