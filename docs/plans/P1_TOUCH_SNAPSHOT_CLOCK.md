# P1 — touch snapshot до часов потребителя

База main a5f8b7e, PR #69 слит. Разрешённый стенд завершён: контроль8192 дал
один touch_age_ms=4294967295 из16 samples, дальнейшая диагностика остановлена,
factory2MiB fecc05d возвращён/verify/UART30s/owner HMI-touch-Wi-Fi-SETUP PASS.
TLS image не записывался, SNTP/HEAD и full recovery не выполнялись. Разрешение
использовано; новых hardware операций нет.

1. Зафиксировать исходный контроль/бэкап/возврат как hardware result с FAIL
   touch_fresh и TLS not run. Не заменять его будущим software PASS.
2. В actual app_main воспроизвести публикацию touch snapshot на1ms новее
   ранее прочитанных consumer clocks. Проверить freshness и diagnostic paths.
3. Скопировать snapshot прежде чтения соответствующего clock; сохранять
   <100ms freshness и >=500ms fault/OTA-health, polling20ms, queues и GPIO.
4. Full host/sanitizers, real LVGL negative-stale coverage, обе pinned SDK
   матрицы и ELF/config/frame/dependency checks. Настоящие исправленные BIN/ELF,
   отдельный PR без merge. Новый аппаратный тест только отдельно разрешённый.

Критерий: обе regression injections падают на исходном app_main, проходят
после исправления; действительно устаревшие данные продолжают fault/health
refusal. Ни clamp, ни исключение плохого hardware sample из результата.

Software приёмка выполнена: [evidence](../evidence/build/esp32-touch-snapshot-clock/README.md),
host159/sanitizers159/SDK16 PASS; actual app_main analyzer PASS. На устройстве
исправление не запускалось, повторного hardware разрешения нет.
