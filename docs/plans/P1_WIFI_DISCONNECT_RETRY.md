# P1: real STA disconnect and retry without a process reset

Base main `bf5c325`, after owner-merged PR #88. Runtime remains the existing
signed source `6c7b1d5` / ESP-IDF 6.1.1, pair receiving `.3` → published bench.4.
The owner can use only the panel's Disconnect button. This qualifies a real
operator-requested client disconnect and reconnection, **not** an involuntary
RF/AP disappearance. Router/hotspot and physical power remain untouched.

Revalidate the current whole-Flash checkpoint with fresh NVS and device MD5,
save two independent NVMe copies, then select the existing OTA0 receiving `.3`
using seq1 VALID plus an erased second record. Capture healthy control.

Keep one UART open throughout both operations and the final normal reboot:

1. Owner CHECK → candidate bench.4 → INSTALL → confirmation. After KiB appear,
   navigate back to Settings → Wi-Fi → Disconnect. Do not press OTA Cancel.
2. Require an actual driver disconnect/IP loss, a failed transport session,
   complete bounded cleanup and continuing healthy diagnostics while offline.
3. Owner reconnects the saved network without RESET, performs a fresh CHECK,
   then INSTALL/confirmation. Require full signed verification, complete SHA/
   size/compatibility, resource guards and commit. Owner presses normal Reboot;
   passively capture the complete bench.4 self-test before any ROM attachment.
4. Read all 16 MiB, verify the real whole-device data, exact candidate/metadata,
   protected active OTA0 and all non-NVS checkpoint bytes. Save two final copies,
   perform final healthy boot and leave bench.4 OTA1 VALID.

No intermediate ROM/Flash read occurs between the failed attempt and retry:
that would reset the process and hide lifecycle/resource recovery failures.
The owner-observed KiB counter and UART establish interruption during a write;
the partial failed image is not separately read before it is overwritten.
Successful retry admission also relies on the unchanged native backend gate
requiring running == selected boot partition. Any claim about no early selection
before retry is that inference, not an independent intermediate otadata read.
The final complete image/metadata/whole-Flash comparison is independent readback.

Normal host writes: only initial 8 KiB selection. Two network write attempts
may touch only inactive OTA1. Failure restores the known-good whole OTA1 before
original metadata; at most one full current-snapshot recovery if necessary.
Maximum 1800 s, recovery reserve 420 s, bounded operator phases and 400 s final
full-read deadline. Existing owner standing isolated-board permission applies.
TLS/owner key/signature/floors/clock policy/UI double gate remain unchanged.
No release/context change, eFuse, option bytes, STM32, CAN, motion or power cut.
Bench contexts expire 25 November 2026 at 08:00 Asia/Almaty.

## Observed outcome

[Hardware evidence](../evidence/hardware/2026-11-24-esp32-wifi-retry/README.md)
preserves both attempts: the first stopped on premature reconnection and restored
bench.4; the separately sealed second attempt passed the unchanged offline gate,
same-process retry, native confirmation and actual final whole-Flash readback.
The owner's final UI check passed. The cause of the first reconnection remains
unknown. Neither voluntary STA disconnection nor a receive-only button diagnostic
qualifies involuntary RF/AP loss; physical power interruption remains open.
