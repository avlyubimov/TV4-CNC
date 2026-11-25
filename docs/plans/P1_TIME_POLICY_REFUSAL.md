# P1: explain a rejected OTA time window and verify refusal

Base main `10cd8cd`, after owner-merged PR #89. Installed release remains the
immutable bench.4 (`6c7b1d5`). The approved bench time window ended on
25 November 2026 at 08:00 Asia/Almaty. Do not extend it automatically or change
the original contexts, public release assets, key, resource floors or TLS checks.

The actual OTA service currently maps every SNTP failure to service error 3;
the screen then suggests checking the network. A successful clock acquisition
rejected by the locally reviewed date range needs a separate explanation.

1. Give only `TIME_REJECTED + TIME_RANGE + successful cleanup` a dedicated
   service error and Russian UI label. Earlier/later dates both mean outside
   the allowed period; do not assert that unauthenticated SNTP proves expiry.
2. Log one bounded numeric clock result per operation, with no epoch, address,
   credentials, key material or new task. Keep existing state/error values,
   command gates, cleanup, watchdog, memory monitor and 5 Hz progress behavior.
3. Exercise the real SDK-bound worker for repeated rejected CHECK operations:
   dates before/after policy, invalid clock, timeout and failed cleanup, in both
   local CHECK-only and install-reviewed configurations. No session/TLS/Flash
   path may start; INSTALL/REBOOT remain refused. Also authenticate a candidate
   first, then expire time before INSTALL: no writer/session may start and a
   subsequent CHECK must refuse again. Test real LVGL in both formats.
4. Build with both pinned SDKs, run host/sanitizers and Actions, then prepare a
   new finite negative hardware packet. A temporary **unpublished** diagnostic
   receiving `.3` uses the unchanged, already expired local fixture context.
   No new release/version policy or signing operation is introduced. Require
   the exact new source/ELF/BIN/config hashes before any write.
5. In a separately sealed single-use session, verify chip/security/16 MiB,
   revalidate the complete checkpoint with fresh NVS and device MD5, save two
   physical NVMe copies. Preserve current bench.4 in OTA1. Write only temporary
   OTA0 and native metadata selecting NEW sequence 3 with OTA1 VALID fallback.
   After healthy self-test the existing opt-in one-shot read-only CHECK must
   report the real time-range rejection, successful cleanup and continuing
   health, without a session. External CHECK/INSTALL/REBOOT stay blocked in this
   diagnostic profile; no new automation API is added. Verify exact diagnostic
   bytes and native VALID metadata, with no unexpected non-NVS change; the
   first boot's legitimate NEW/PENDING/VALID metadata writes are expected.
   Restore the original whole OTA0
   **before** original metadata, verify preserved partitions and leave bench.4
   OTA1 VALID. Maximum 1800 s, recovery reserve 420 s, at most one conditional
   full-snapshot recovery. Both actual sessions followed separate exact owner approvals; see the closed evidence.

Report software and physical observations separately. Absence of HTTPS after
clock refusal is a source/control-flow inference, not a wire capture. Do not
turn this negative test into network INSTALL, RF/AP loss, power-cut or production
acceptance. No eFuse/option bytes/STM32/CAN/motion operations or self-merge.

## Completed result

Exact source 104fb06 builds on SDK 6.1.1/5.3.2, 290 host/290 sanitizer tests,
16 added worker invocations and source CI 15 PASS. [Hardware evidence](../evidence/hardware/2026-11-25-esp32-time-policy/README.md)
preserves attempt1 SNTP timeout FAIL and one separately approved repeat PASS
without changing policy or timeout. Exact range rejection/error24, all 45 guards,
native seq3 VALID, diagnostic device digest, original OTA0/metadata restoration,
actual final 16MiB/two-NVMe and retained bench.4 healthy boot passed. Owner UI
confirmation passed. No new publication or automatic extension of old contexts.
The next positive OTA requires a separately reviewed current policy; the closed
single-use packets are not reusable. SDK 5.3.2 was built, not run on hardware.
