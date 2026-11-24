# P1: cancellation after a partial network write

Base main `5cacc6f`, after owner-merged PR #87. Runtime firmware and the published
signed `bench.4` remain unchanged. The owner granted isolated board checks and
will use the existing UI confirmation and cancellation buttons.

The finite session revalidates the complete current checkpoint using fresh NVS
and whole-device verification, and saves two private copies on different physical
NVMe devices. It temporarily selects the already verified receiving `.3` in OTA0
using seq1 VALID plus an erased second otadata record. This makes OTA1 inactive
and prevents its old selection record from referring to a partial replacement.
No application is written to prepare the receiving controller.

After a healthy control boot, one operator CHECK must authenticate the exact
published bench.4 manifest. The operator then confirms INSTALL and presses Cancel
when the KiB counter starts. Require a CANCELLED session, bounded cleanup, healthy
resources/touch, unchanged boot selection and active OTA0. Read all 3 MiB of OTA1
and require a nonempty proper prefix of the approved image followed by erased
bytes through the image's allocated erase range. A full install or cancellation
before any write is not a partial-write PASS.

Restore the entire known-good OTA1 partition from the fresh checkpoint, verify,
then restore the original dual-VALID otadata bytes. Capture healthy bench.4,
actually read all 16 MiB, compare every non-NVS byte, save two new final copies,
and perform the final boot. Leave bench.4 OTA1 VALID.

Normal host writes are limited to selection metadata, restoration of the whole
OTA1 partition, and original metadata. The network installer may write only the
inactive OTA1 image. Bootloader/table/factory/OTA0/PHY/gaps/resources/store are
protected. Every host write is followed by SDK verify and an outside-span check.
Session maximum 1800 s, recovery reserve 420 s; bounded operator windows and
90 s partial-slot / 400 s final-full-read deadlines do not extend the absolute
deadline. Failure restores the original OTA1 and metadata, with at most one full
current-snapshot recovery if needed and enough reserve remains. No active-slot
write, eFuse, STM32, CAN, motion, router or physical power-cut operations.

Host tests cover cancellation and transport failure after every partial chunk:
exact bytes, one abort, closed transport/hash contexts, no seal/activation,
cleared candidate. Procedure, UART parser, timeout, tamper and recovery tests
must pass before serial access. Published assets, signature, owner key, policy
floors and context expiry (25 November 2026, 08:00 Asia/Almaty) remain unchanged.
This test alone does not qualify physical Wi-Fi loss or power interruption.

## UI finding

The current release conflates cancellation with the generic update-failure text.
The follow-up source displays «Обновление отменено» for the existing CANCEL
error code, preserving state numbers, gates and progress coalescing. Real LVGL
and backend tests cover it; this source fix is not installed over immutable
bench.4 in the cancellation qualification. A future release remains separate.

## Execution

[Hardware evidence](../evidence/hardware/2026-11-24-esp32-partial-cancel/README.md)
closed PASS. Actual partial candidate was not selected; original bench.4 restored
and left active. Physical Wi-Fi/power interruption remains separate.
