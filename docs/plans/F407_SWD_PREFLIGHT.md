# F407: первый аппаратный запуск и исправление найденного отказа

База main4fe84f1, PR #56 слит. Это одна интеграция первого boot, не новый слой.

1. Выполненные отдельно разрешённые preflight/backup/independent copy и сверку
   marking сохранить с фактами/пределами. Не скрывать partial первый сеанс.
2. Сохранить c09 write/verify PASS, потерю SWD и FAULT/error8; сохранить успешный
   полный snapshot restore/verify/reset отдельно от общего failed bench exit1.
3. Исправить start-before-sync IWDG по официальному ST reference, добавить
   actual-source модель stalled synchronization и truthful started/no-feed errors.
   Исправить DBG_SLEEP в custom OpenOCD, сохранить HSI/no-trace/no-motion scope.
4. Проверить actual ELF/BIN/map/frames/vectors, negative profile/debug guards,
   повторную сборку, host/sanitizers и CI. Старый порядок должен падать на новых тестах.
5. Обновить статусы и создать отдельный PR. Не сливать самостоятельно.
6. После review/CI собрать clean exact commit, подготовить конкретные SHA/
   runbook/fallback и запросить новое разрешение на запись/стенд. До ответа
   никаких новых SWD/read/reset/Flash/capture операций.

Приёмка software: программно проверенный ELF/BIN с исправлением, воспроизводимость
и регрессия старого дефекта. Hardware приёмка ещё впереди: RUNNING/progress,
CCM/guards, watchdog reset, UsageFault capture. No option bytes/OTP/GPIO/CAN/motion.
Snapshot не является доказательством factory provenance. CAN loader addresses не назначены.
