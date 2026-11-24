# Bench.4: actual CANCEL after a partial network write

[Plan](../../../plans/P1_PARTIAL_CANCEL.md) · [Software/tests](../../build/esp32-ota-partial-cancel/README.md)
· [Numeric physical results](measurements.json).

Qualification base main `5cacc6f`, immutable runtime source `6c7b1d5`,
Waveshare ESP32-S3 Rev 1.2 / ESP-IDF 6.1.1. Session closed **PASS in
965.072 s / 1800 s**.

| Physical case | Result |
|---|---|
| Current checkpoint and controls | Both signed slots, factory and bootloader/table verified; healthy bench.4 30 s, then receiving `.3` 90 s |
| Operator CHECK | Exactly one authenticated bench.4 candidate; state 6 / error 0, 9 resource checks |
| Operator INSTALL → CANCEL | Owner confirmed cancellation after the KiB counter started; state 9 CANCELLED / error 6, HTTP 11, 41 resource checks; bounded cleanup and 45 s progressing diagnostics |
| Actual partial image | Entire inactive OTA1 3 MiB read and SDK verified: 61440 matching candidate-prefix bytes, remaining bytes erased; image size 1600464 bytes |
| No activation | Native metadata remains seq1 VALID plus erased second record; running/active OTA0 and every protected region unchanged |
| Restore known-good release | Whole original OTA1 3 MiB restored and verified before original dual-VALID metadata; bench.4 90 s PASS |
| Final whole-device check | All 16 MiB actually read and device-verified; every non-NVS byte equals the pre-test checkpoint; two new physical NVMe copies SHA/fsync PASS |
| Final board state | bench.4 30 s PASS; **left running bench.4 OTA1 VALID** |

The matching prefix length is a readback property, not an exact network-byte
counter: erased 0xFF bytes can also match the expected image. It proves a proper
nonempty candidate prefix and an incomplete erased suffix; no complete candidate
was activated. Full image verification/commit is intentionally not claimed for
the cancelled operation.

Three normal host writes: select existing receiving `.3` via 8 KiB otadata,
restore the known-good 3 MiB OTA1 image, then restore original 8 KiB otadata.
The first selection removes the old inactive-slot record before partially
replacing its image, so it cannot refer to a damaged replacement. No active OTA0,
factory, bootloader, table, PHY, gaps, resources or store writes. Full recovery 0.
Temporary `.3` is the separately approved receiving fixture of source `6c7b1d5`,
not the different immutable published bench.3 binary.

TLS CA/hostname/date, local public key, signature/compatibility/size validation,
UI double confirmation, 64 KiB internal / 32 KiB largest / 1 MiB PSRAM /
128 KiB largest / 3072-byte stack floors and serialized monitor ownership were
preserved. Both operations include all cleanup windows. Cancellation is reported
by the existing UI as the generic update-error state (service error 10); the
numeric session record separately identifies CANCELLED. The accompanying
[source/UI fix](../../build/esp32-ota-partial-cancel/README.md#follow-up-from-the-real-cancellation-clear-ui-status)
displays «Обновление отменено» and passes actual LVGL/backend tests; it is not
installed in this immutable bench.4. No production budget,
physical Wi-Fi loss, power-cut, FPS or touch-to-display result is inferred.

Before writes, the previous actual complete read plus fresh NVS 24 KiB was
revalidated against all current Flash and saved on two different physical NVMe
devices. That is complete snapshot revalidation, not a fresh full pre-test UART
read. The final 16 MiB was actually read. Private hashes, dumps, NVS, UART,
contexts and keys remain outside Git. Standing isolated-board permission applies;
no STM32, CAN, motion, eFuse, option bytes or router changes. Published release
and context expiry (25 November 2026, 08:00 Asia/Almaty) are unchanged.

After session closure the owner confirmed the final bench.4 main screen, touch
and saved Wi-Fi all work. No later device session was opened.
