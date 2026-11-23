# 23 ноября: LVGL PSRAM и три успешных HTTPS-пробы

[Числовые результаты](measurements.json),
[сборки и тесты](../../build/esp32-lvgl-psram/README.md).
Источник 38f70b9 из PR #75, baseline 46cf400, ESP-IDF 6.1.1,
rev12-ota/worker8192 без owner key/context. Source CI 30/30 PASS до стенда;
после слияния владельцем main1682da0 также 15/15 PASS. Сценарий и recovery
проверены 51 host simulation test. Действовало общее разрешение владельца
на необходимые P1 стенды 23 ноября 2026, Asia/Almaty, без повторных вопросов.

## Подтверждено на ESP32

Сеанс 16:10:19–16:28:03 UTC, 1063,692 секунды. Identity/security/Flash16MiB,
свежий полный снимок, device MD5/SHA-256 и две приватные копии на разных
физических NVMe проверены. Snapshot SHA-256:
`ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`.
Каждая запись проверена; MD5 вне factory сравнивалась до и после записи,
до запуска приложения. Это не утверждение о неизменности NVS при работе Wi-Fi.

Baseline → новый control → три TLS reset boots → возврат fecc05d: все шесть
наблюдений PASS. Четыре Flash writes: baseline app, новый control app,
TLS app и весь factory2MiB из свежего снимка. Повторной записи TLS между
тремя boots не было. Bootloader/table/otadata не записывались, полный restore
не понадобился. Шесть UART captures: пять по 90 секунд и возврат 30 секунд.
RTS/open/close могут давать дополнительные reset; это не cold-power испытание.

| Показатель control | Baseline 46cf400 | CUSTOM PSRAM 38f70b9 |
|---|---:|---:|
| Периодический internal free, байт | 16771–16831 | 124703–124755 |
| Internal largest block, байт | 7680 | 63488 |
| PSRAM free, байт | 6090060 | 5982656–5982676 |
| Touch age, мс | 2–21 | 5–6 |
| Touch errors | 0 | 0 |
| Остаток OTA stack из 8192, байт | 6784 | 6784 |

Между TOUCH_READY и DASHBOARD_READY baseline потратил 100516 байт internal
при неизменной PSRAM. Новый control сохранил internal188515 и использовал
100492 байта PSRAM. В steady samples получен прирост около 105 КиБ internal;
это сравнение двух reset boots, не гарантия любого UI/Wi-Fi режима.

| TLS boot | SNTP, мс | HTTPS HEAD, мс | HTTP | TLS-window internal minimum, байт | Остаток стека, байт |
|---|---:|---:|---:|---:|---:|
| 1 | 2229 | 3728 | 200 | 108563 | 3904 |
| 2 | 4513 | 2729 | 200 | 108655 | 3760 |
| 3 | 3244 | 3723 | 200 | 108867 | 3760 |

Все три TLS результата 0, native/tls errors0, cert_flags0, cleanup1, полные
тайм-ауты соблюдены. Проверены CA, hostname и certificate date. SNTP source2
не аутентифицирован; доверие времени не усиливалось. Один HEAD к GitHub на
загрузку, без body/auth/redirects, CHECK/INSTALL не запускались.

Failure trace каждого boot полный: valid1, total/stored/lost/exhausted0;
все пять DMA phase pairs присутствуют. На границах DMA free не ниже 106571,
DMA largest63488. Это boundary samples, не непрерывный минимум DMA. TLS local
min108563–108867 и history43832–44136 — разные per-heap метрики; их нельзя
выдавать за simultaneous free или approved production floors.

Во всех пяти временных boots по 16 periodic samples: touch2–21ms/errors0,
display/queue drops0, watchdog0; BOOT/self-test/PSRAM8MiB/XiP/RGB/GT911 и
завершение единственного monitor owner PASS. TLS touch2–19ms. После возврата
fecc05d четыре samples touch1–11ms, boot/resource PASS. На плате оставлен
fecc05d/factory/tv4-16m-ab-v1; F407 ee7d179 не открывался.

## Пределы результата и следующий этап

Это подтверждение TLS и устранения наблюдавшегося DMA starvation в этих
трёх пробах. Подписанная сессия с manifest/image body, Flash install,
self-test/confirmation/rollback, power-loss и production budget ещё не приняты.
Render/event counts во всех samples0: физические FPS/touch-to-display и
производительность под интерактивной нагрузкой этим стендом не измерены.
Визуальное подтверждение владельца для сегодняшнего образа не получено;
UART touch freshness не означает физические нажатия.

[Следующий точный этап](../../../plans/P1_SIGNED_SESSION_RESOURCE.md) —
настоящий signed CHECK в существующем worker8192. Для него пока отсутствуют
локальный ключ владельца, reviewed SemVer/time/resource context и подписанный
release candidate. Test keys, guessed floors и обходы SETUP запрещены.
TLS records16384/4096, DMA reserve64KiB, stack8192, global malloc policy,
RGB buffers/pins/clocks, touch100/500ms и проверки сертификата сохранены.
eFuse/STM32/CAN/motion не затрагивались; merge/release агент не выполнял.
Raw UART/NVS/dumps/серийные идентификаторы остаются приватными.
