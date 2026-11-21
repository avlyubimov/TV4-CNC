# PR-07h — signed release в живом software-сеансе

21 ноября2026. База main `c17e89c`, PR #1–37 слиты, открытых PR нет.

1. Явный opt-in adapter связывает release-draft-1 verifier с существующими
   immutable lease/RX/owner-generation/deadline guards. Snapshot всех входов
   до callbacks, один owner, прежний workspace. Legacy API/footprint сохраняются.
2. Общие guard/cleanup/preflight/completion algorithms не дублировать. Explicit
   local mode не выбирается по сетевому schema/успеху HMI; legacy verify/start/
   positive receipt/clear не обходят release policy. Close/abort/poll безопасно
   обслуживают cleanup, без дополнительной попытки release за один вызов.
3. Полные независимые signature/SemVer/selection/SHA/EOF и vector model из того
   же source; raw metadata SHA сверяется с negotiation. Historical outcome
   выдаётся после clean close; retry/quarantine не теряют held lease.
4. Signed negotiation→DATA→release verification→vectors→close→outcome/ACK
   fixtures, losses/replay/domain mismatch, owner/source/RX/clock changes,
   snapshots/aliases/reentry/cleanup faults и сохранение прежних проверок.
5. Host/LVGL/sanitizers/standalone, ARM/frame/import/static/source/image gates,
   прежняя ESP SDK matrix8; evidence/docs/status и отдельный PR без merge.

Нет production bundle linkage/key/installed-context provider, persistent identity,
wire codec/IDs/bitrate, runtime caller/installer, actual MCU/Flash/erase/profile,
physical quiescent proof или device/SWD/UART/reset/flash. Не менять DATA deadlines,
dependency pins/arena/crypto owner или текущую ordinary8f0d899 на плате.
