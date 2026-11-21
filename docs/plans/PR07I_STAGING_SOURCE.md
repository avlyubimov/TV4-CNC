# PR-07i — volatile RAM staging source

База main2ea51b6, PR #1–38 слиты; открытых PR нет. Main ESP matrix ещё
завершалась при старте, четыре остальных workflow PASS; итог сохранить в evidence.

1. Caller-owned buffer, explicit capacity/boot/object, один owner; без heap,
   MCU addresses/default sizes или runtime caller. Init не сбрасывает live state.
2. Consume только через существующий pending RX API, ACK после RAM copy.
   Capture descriptor/limits/cursors/time identity; deadlines не продлевать.
3. Seal только по completed RX; acquire/inspect/read/release с monotonic tokens,
   запрет writes/discard/reinit while leased, idempotent release без stale unlock.
   Discard сохраняет counters/high-water, exhaustion не wrap. Volatile не durable.
4. Unit/fault/alias/limit/replay tests и реальные signed legacy/release chains
   через backend вместо mock storage; retain старые suites/cleanup/quarantine.
5. Host/sanitizers/standalone, ARM/static/source/import/frame/board gates,
   прежние8 SDK CI profiles; версии/evidence/docs/status и отдельный PR без merge.

Не менять core crypto/handoff/completion/negotiation алгоритмы, actual keys/
versions/context, Flash map/writer, CAN/wire/IDs, physical inhibit или install.
Backend обеспечивает API ownership, не защиту от raw C writes/DMA/других threads.
Нет device/SWD/UART/reset/Flash/backup reads и аппаратных измерений.
