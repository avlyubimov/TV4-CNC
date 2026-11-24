# STA disconnect during INSTALL and same-process retry

[Plan](../../../plans/P1_WIFI_DISCONNECT_RETRY.md) · [Software/tests](../../build/esp32-ota-wifi-retry/README.md)
· [Final measurements](measurements.json) · [Failed first attempt](attempt1.json)
· [Separate button observation](button-observation.json).

Base main `bf5c325`, immutable runtime `6c7b1d5`, Waveshare ESP32-S3 Rev 1.2 /
ESP-IDF 6.1.1. The owner could use only the panel **Disconnect** button. This
qualifies a real voluntary STA disconnect, not involuntary RF/AP disappearance.

| Physical case | Result |
|---|---|
| Checkpoint and controls | Whole-device MD5 revalidation with fresh NVS; two physical NVMe copies; bench.4 30 s and receiving `.3` 90 s healthy |
| First CHECK | Authenticated bench.4 candidate, state 6/error 0, 9 resource checks |
| INSTALL interrupted after KiB | Actual disconnect and expected transport failure: state 8/error 2/HTTP 9, 124 resource checks; complete bounded cleanup |
| Offline receiver | Strict offline window passed, progressing touch/UI diagnostics; no reset or ROM attachment |
| Reconnect and fresh CHECK | New IP event and authenticated candidate again, state 6/error 0, 9 checks, same receiving process |
| Full retry INSTALL | State 10/error 0, 798 resource checks; signature, SHA-256, size and compatibility before commit |
| Normal operator reboot | Passive 90 s candidate capture; native self-test/confirmation PASS |
| Independent final check | All 16 MiB actually read and device-verified; exact whole OTA1 image/padding and dual-VALID metadata; every non-NVS byte equals checkpoint; two final physical NVMe copies SHA/fsync PASS |
| Final state | Healthy final 30 s capture; **bench.4 left in OTA1 VALID**; owner confirmed screen, touch and Wi-Fi |

Session closed **PASS in 942.657 s / 1800 s**. One normal host write: initial
8 KiB otadata selection of existing receiving `.3`; the two network attempts use
only inactive OTA1. Full recovery 0. Factory/bootloader/table/PHY/resources/store
and protected running OTA0 are unchanged. Temporary `.3` is the approved receiving
fixture of source `6c7b1d5`, not the different published bench.3 binary.

Across all four operations, session-window internal minimum **96967 bytes**
(interrupted INSTALL); completed retry INSTALL minimum **98375 bytes**,
minimum operation stack **7072 / 12288 bytes**, maximum
periodic touch age **19 ms**, touch/queue/display errors 0.
All reviewed floors, TLS CA/hostname/date, local trust/signature, timeouts,
monitor ownership and UI double confirmation are preserved. These are bench
resource/freshness observations, not production budgets, FPS or physical latency.

No ROM attachment occurs between failure and retry: an intermediate Flash read
would reset the process being qualified. The partial image and intermediate
metadata are therefore **not independently read**. The absence of premature boot selection before
retry is inferred from the unchanged native backend running==boot admission gate;
the complete final image, native VALID state and whole Flash are independently
verified after normal reboot. Before writes, the previous actual full read plus
fresh NVS 24 KiB was checked against all current Flash; this is snapshot
revalidation, not a fresh full pre-test UART read. The final 16 MiB is a new read.

## First attempt and supplemental observation

The first finite attempt stopped on **premature reconnection before the offline
checkpoint** after a genuine transport failure. The owner reported pressing only
Disconnect; the current firmware does not log command admission, so the cause is
not established. The whole original OTA1 was restored before original metadata,
with verify/protected-region checks and healthy bench.4. That failed attempt has
no final full-device read or independently read partial image; it remains FAIL.

The following receive-only UART observation without OTA saw 85.075 s offline
and a new IP after manual reconnect; no spontaneous reconnect was observed.
Its final observation phase hit its wall-clock deadline, so no complete procedure
PASS is claimed for that capture. Opening UART showed a boot preamble; no Flash
or serial command writes or intentional reset calls were made. The second full
OTA attempt above retains the original strict reconnection gate and passes it;
no guard was relaxed and no firmware workaround was installed.

Physical RF/AP loss, physical power interruption and production policy/budget
remain open. The prior UI source fix for cancellation text is not in immutable
bench.4. Releases/contexts are unchanged and expire 25 November 2026, 08:00
Asia/Almaty. Raw UART, backups, private hashes, NVS, contexts and keys stay outside
Git. No router/power/eFuse/option bytes/STM32/CAN/motion operations were performed.
