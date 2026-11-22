# Actual halted snapshots

| Snapshot | Boot count | Nominal ticks | Heartbeat | Feeds | Reset flags | Previous reason |
|---|---:|---:|---:|---:|---|---|
| INITIAL | 1 | 1499 | 14 | 14 | 0x1e000000 | 0x00000000 |
| PROGRESS | 1 | 2997 | 29 | 29 | 0x1e000000 | 0x00000000 |
| WATCHDOG | 2 | 3050 | 30 | 30 | 0x24000000 | 0x57444731 |
| FAULT | 3 | 3005 | 30 | 30 | 0x24000000 | 0x0000000b |
| FINAL | 3 | 4501 | 45 | 45 | 0x24000000 | 0x0000000b |

All five snapshots: state4/error0, watchdog_started1, data/BSS/CCM/guards1, motion_outputs0.
Ticks are firmware counters, not calibrated wall-clock measurements.
