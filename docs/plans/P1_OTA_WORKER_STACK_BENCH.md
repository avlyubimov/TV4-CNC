# P1: OTA worker8192 без provisioning

База main3529b8e, отчётный PR #66 ещё открыт при начале работы. Его owner
confirmations и connected-only comparison остаются отдельным изменением.
Последний разрешённый b5f4b38 стенд закрыт; на ESP32 fecc05d, на F407 ee7d179.

Результат этого software PR — реальные ELF/BIN для измерения существующего
OTA-worker со стеком8192 вместо unprovisioned3072, без owner key/context,
с сохранением SETUP и закрытых CHECK/INSTALL. Это не TLS/install acceptance.

1. Добавить default-off `TV4_OTA_WORKER_STACK_BENCH=1` только вместе с
   rev12-ota/memory diagnostics1 и без provisioning. Отдельный build tree,
   Kconfig/CMake/C/source/artifact guards; ordinary/trace-control прежние.
2. Использовать тот же worker/xTaskCreate/boot self-test/rollback deadline.
   Ни новых задач/crypto/network probes/таймеров, ни guessed floors/keys.
   Публиковать requested stack bytes в существующей boot-result строке
   только стендового образа, после закрытия окна. Watermark не равен budget.
3. Исполнить actual ESP service с host SDK boundary: проверить actual requested
   stack3072/8192, self-test, task failure/timeout/restart и закрытые команды.
   Модели SDK не измеряют target RAM/тайминги. Добавить negative build guards.
4. Host/sanitizers и обе закреплённые SDK: прежние5 profiles + trace control
   + worker-stack bench. Проверить clean source/image/ELF/config/Flash16MiB/
   PSRAM/locks/frame evidence и CI без расширения matrix jobs.
5. Отдельный PR без merge/release или устройств. Не менять memory monitor
   ownership, DMA64KiB/TLS16384/4096/framebuffer/clocks/partition/trust gates.
   Следующий аппаратный сеанс требует нового exact-image разрешения.

Программные критерии выполнены: [evidence](../evidence/build/esp32-ota-worker-stack-bench/README.md).
Локальные14/14 clean SDK builds и host/sanitizers108/108 PASS; аппаратных
действий/merge/release нет. Аппаратная квалификация остаётся следующим этапом.

Аппаратное продолжение после owner merge PR #67 выполнено на main dfa9f28:
[контроль3072/три boots8192/fresh snapshot/factory return](../evidence/hardware/2026-11-22-esp32-worker-stack/README.md)
PASS за928.998s, owner temporary/returned UI PASS. Испытание unprovisioned,
без TLS/install/floors; все аппаратные разрешения закрыты. Этот report PR
обновляет только документацию и numeric evidence, исполняемый код не меняет.
