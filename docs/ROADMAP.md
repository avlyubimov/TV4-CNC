# План реализации TV4-CNC: функциональные приоритеты P0–P5

## 25 ноября: bench.5 опубликован, RTC watchdog остановил приёмку

[Сборка и публикация](evidence/build/esp32-ota-bench5/README.md): source `0dbc1e9`,
шесть сборок с утверждёнными настройками, две равные пары кандидатов, 12 файлов
проверены обратным скачиванием; CI source/main/tag PASS.
[Аппаратный результат](evidence/hardware/2026-11-25-esp32-bench5/README.md):
контроль 37/37, подписанные CHECK/INSTALL и ресурсы PASS; после штатного
перезапуска RTCWDT/reset_reason=7/watchdog_boot=1 дали общий FAIL. Встроенный
self-test не заменяет независимую проверку watchdog или чтение Flash.
Один полный возврат свежего снимка 16 МиБ, verify и загрузка PASS. Оставлен
bench.4 OTA1 с исходными VALID-метаданными; владелец подтвердил экран, касания и
Wi-Fi. Сеанс закрыт, повторять его пакет нельзя. F407 ee7d179 не подключался.
Следующий шаг — диагностика одного software restart без INSTALL. Причина
неизвестна; production policy, потеря RF/AP/питания, runtime 5.3.2 и FPS/latency
ещё открыты. [Текущий план](NEXT_STAGE.md).

## 25 ноября: отказ по периоду OTA проверен, bench.4 сохранён

От main 10cd8cd / source 104fb06 завершён [отрицательный стенд истёкшего
контекста](evidence/hardware/2026-11-25-esp32-time-policy/README.md). Первый
сеанс 396.821 с остановлен на SNTP timeout; FAIL сохранён. Один отдельно одобренный
повтор 792.933 с на том же образе/политике/таймауте получил точные
TIME_REJECTED/TIME_RANGE/cleanup OK и service error24; все 45 boot/GT911/RGB/
PSRAM/resource/queue/touch guards PASS. Причина первого таймаута не установлена.

Исходные OTA0 и metadata восстановлены; второй VALID-сектор и OTA1 не писались.
Все 16 МиБ заново считаны, MD5/равенство вне NVS/две финальные NVMe-копии PASS;
full restore 0. **Bench.4 оставлен в OTA1 VALID**, UART закрыт, владелец подтвердил
экран/касания/Wi-Fi. Новая строка проверена реальным LVGL simulator; на плате
измерен error24 временного образа, его текст визуально не наблюдался. Новый UI
и прежний source fix отмены ещё не входят в оставленный неизменяемый релиз.

[290 host / 290 sanitizers, 16 новых worker-сценариев, два SDK и source CI 15](evidence/build/esp32-ota-time-refusal/README.md)
PASS. Hardware только ESP-IDF6.1.1; runtime 5.3.2/FPS/physical latency и production
budget не квалифицированы. F407 ee7d179/CAN/motion/eFuse/option bytes не трогали.
Исходное окно истекло 25 ноября 08:00 Asia/Almaty и не продлено; положительный OTA
требует нового решения о policy. RF/AP/power loss и production discovery открыты.

## Закрыто 24 ноября: отключение STA и повторная OTA без сброса проверены

От main `bf5c325` (после слитого PR #88) на неизменяемом `6c7b1d5` /
ESP-IDF 6.1.1 выполнен [реальный стенд отключения кнопкой Wi-Fi](evidence/hardware/2026-11-24-esp32-wifi-retry/README.md):
CHECK → INSTALL до КиБ → «Отключить» → transport failure/cleanup → окно без
связи → подключение сохранённой сети → новый CHECK → полный INSTALL.
Обе попытки — в одной загрузке принимающей `.3`, без промежуточного ROM/reset.
После штатного перезапуска self-test/VALID PASS. На плате оставлен **bench.4,
OTA1 VALID**; все 16 МиБ считаны и сверены вне NVS, две финальные копии на
разных NVMe проверены. Владелец подтвердил экран, касания и Wi-Fi.

Полный стенд 942.657 с / 1800 с, одна запись хоста (выбор `.3` через
8 КиБ otadata), full restore 0. Повторный INSTALL: 798 проверок/error0;
Общий SESSION local minimum 96967 байт (прерванная запись);
успешный retry — 98375 байт,
остаток стека ≥7072/12288, touch age ≤19 мс — не физическая задержка.
Промежуточный неполный образ/otadata не считаны; отсутствие ранней активации
выведено из native admission gate, итоговый полный образ/VALID считаны независимо.

Первый сеанс остановлен на раннем переподключении и восстановил bench.4;
причина не установлена, его FAIL сохранён. Без OTA кнопка удержала сеть
отключённой более 85 с; повторный полный стенд прошёл без ослабления guard.
[274 host / 274 sanitizers / 24 проверки процедуры](evidence/build/esp32-ota-wifi-retry/README.md)
PASS; 56 частичных сбоев теперь также проверяют полный retry в том же процессе.
Firmware/assets/context прежние; UI fix «Обновление отменено» ещё не на плате.

Это добровольное отключение STA, не исчезновение RF/AP. Далее P1: независимая
потеря Wi-Fi/питания, production policy/budget/version discovery; P2 — чистая
воспроизводимость. Runtime 5.3.2/FPS/physical latency ещё не измерены. Контексты
только до 25 ноября 2026, 08:00 Asia/Almaty; новые release/tag — отдельное
точное разрешение, self-merge запрещён. F407 ee7d179/CAN/motion/eFuse/option
bytes не затронуты. Закрытые пакеты не переиспользовать; ниже история.

## История: native pending confirmation/refusal/rollback a74bfcd — PASS

[Реальный USB boot стенд](evidence/hardware/2026-11-24-esp32-native-boot-a74bfcd/README.md)
закрыт за1031,352s/1800s: fresh16MiB/device MD5/SHA/two-NVMe → configured
factory-control PASS → ota_0 NEW/VALID → unprovisioned ota_1 NEW/INVALID/native
rollback в healthy ota_0 → полные исходные factory2MiB/otadata8KiB/два OTA slots
по3MiB/verify/UART30 PASS. Native4KiB otadata writes ожидаемы; bootloader/table
не записаны.9 обычных write calls/4 captures/full recovery0; на ESP32 fecc05d.

Exact source a74bfcd, открытый PR81, оба pinned SDK/4 ELF и source CI30/main15,
265 host/265 sanitizers/analyzer PASS до USB. Hardware только6.1.1. Это прямой
USB fixture, не signed network INSTALL или SemVer upgrade. Published bench.2
не использовался и остаётся read-only. Physical UI/FPS/latency ночью не наблюдались.
Далее отдельный gate install_reviewed для CHECK-only context и настоящий
configured signed installable payload/HTTPS INSTALL/resource check. Production
budget, потеря Wi-Fi/питания и app Flash writes при активном RGB ещё не приняты.

## История: a29c9ed — полный signed CHECK и три reset-повтора PASS

[Полный подписанный CHECK](evidence/hardware/2026-11-24-esp32-signed-check-a29c9ed/README.md)
на ESP-IDF6.1.1 закрыт за1095,492s: свежие16MiB/device MD5/SHA/two-NVMe,
configured control PASS → ровно один автоматический CHECK → весь factory2MiB
fecc05d/verify/preservation/UART30 PASS. IMAGE_VERIFIED(7), все ошибки0,
406 resource checks; полный payload bench.2 проверен без Flash sink/INSTALL.
SESSION local minimum internal97399bytes, сетевой largest63488, остаток стека
6896/12288bytes; touch age0–20ms/errors0. Cleanup/окна/CA/hostname/date/timeouts
и прежние floors сохранены. Physical UI/FPS/latency ночью не наблюдались.

[Software evidence](evidence/build/esp32-ota-zero-length-check/README.md):237
host/237 ASan/UBSan/LSan, actual-source analyzer, четыре configured ELF/BIN
ESP-IDF6.1.1/5.3.2 PASS. Source a29c9ed CI30/30 и mainc3b82f0 CI15/15 прошли
до стенда. Runtime5.3.2 не испытывался. PR #80 открыт, самостоятельно не сливать.

[Повторяемость](evidence/hardware/2026-11-24-esp32-signed-check-repeat-a29c9ed/README.md)
также PASS: новый свежий снимок/two-NVMe, контроль90s, три reset CHECK без
повторной записи auto-app, весь factory fecc05d/verify/preservation/UART30.
Сеанс закрыт за1148,571s;3×IMAGE_VERIFIED/error0/406checks, SESSION local minima
internal97783–99427bytes, largest63488, stack6896/12288, touch1–18ms/errors0.

Далее отдельный [PR #81](https://github.com/avlyubimov/TV4-CNC/pull/81): рабочий
local context до pending confirmation и native boot/rollback проверка. Затем
configured installable payload и сетевой INSTALL. Опубликованный unprovisioned
bench.2 остаётся только read-only: его нельзя устанавливать или менять assets/tag.
Production resource budget, INSTALL и испытания потери питания не приняты.

[SWD readback F407](evidence/hardware/2026-11-24-f407-readback/README.md) PASS:
current1MiB/ee7d179 exact bytes, self-tests/guards/progress/resume; без прошивки,
reset и RAM test requests. Две приватные current copies на разных NVMe проверены.
Владелец разрешил автономные обратимые проверки изолированных плат до24ноября
08:00 Asia/Almaty: каждый сеанс≤1800s и recovery reserve до срока; без eFuse,
option bytes/CAN/motion/merge/новых релизов. Ключи/context/бэкапы вне Git/CI.

## История: b01ff48 контроль PASS, CHECK остановлен на нулевой длине HTTP302

[Один автономный CHECK](evidence/hardware/2026-11-24-esp32-signed-check-b01ff48/README.md)
закрыт: control PASS; auto attempted1/pass0, HTTP302/header bytes5704/bad_headers1.
Resource guard0/checks2/stack6896 — ещё не full session budget. Весь factory
fecc05d возвращён/verify/UART30 PASS, full restore0; owner UI ночью не наблюдался.
[SWD readback F407](evidence/hardware/2026-11-24-f407-readback/README.md) PASS:
current1MiB/ee7d179 exact bytes, self-tests/guards/progress, resume; без прошивки/
reset/RAM test requests. Две приватные current copies на разных NVMe проверены.

[Следующий exact пакет](plans/P1_AUTONOMOUS_SIGNED_CHECK.md) исправляет
семантику ESP-IDF5/6: fetch_headers выставляет is_chunked для Content-Length0,
даже без Transfer-Encoding. Narrow adaptation только для explicit zero length
без TE; empty200, unknown framing, CL+TE, duplicates/compression и прочие
contradictions отвергаются. Сначала237 host/sanitizers/оба SDK/CI, затем новый
fresh16MiB/two-NVMe → control flag0 PASS → один full CHECK flag1 → весь fecc05d.
Стенд b01ff48 закрыт, не повторять. Подпись/full stream/install/rollback ещё не
приняты. Пороги/CA/hostname/date/timeouts и обычные профили сохраняются.

Действует разрешение владельца на автономные обратимые проверки изолированных
плат до24ноября08:00 Asia/Almaty: finite≤1800s/recovery reserve, без eFuse/
option bytes/CAN/motion/merge/новых релизов. Published bench.2 остаётся read-only.

## После PR #77: опубликованный payload и следующий configured CHECK

[Prerelease v0.1.0-bench.2](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.2)
опубликован по отдельному разрешению на c013ddf:11 downloaded assets/SHA,
подпись и полный BIN Python+C PASS; CI source30/main15/tag15 PASS.
[Протокол публикации](evidence/build/ota-signed-check/publication.md).
[Первый configured стенд](evidence/hardware/2026-11-23-esp32-signed-check-c013ddf/README.md)
закрыт: boot/touch PASS, два ранних owner CHECK во время контроля дали
SESSION_RESOURCE/stack3024 ниже floor3072. Planned CHECK420s не запускался;
factory fecc05d возвращён/verify/UART/owner PASS, full restore0.

Следующий результат — [configured worker headroom](plans/P1_CONFIGURED_WORKER_HEADROOM.md):
12288 вместо8192 только при provisioned context; ordinary3072 и bench8192
прежние. Floors/CA/time/touch не снижать. Новый exact BASE после двух SDK/CI:
чистый контроль, один CHECK по команде, возврат. Время проверки явно продлено
владельцем; finite1800s и recovery reserve сохраняются. SESSION4/production
budget/install/confirmation/rollback ещё не приняты; F407/CAN/motion не менялись.


## 23 ноября: LVGL PSRAM и HTTPS — PASS, далее signed CHECK

PR #75 слит владельцем, main1682da0 CI15/15 PASS. [Стенд38f70b9](evidence/hardware/2026-11-23-esp32-lvgl-38f70b9/README.md)
завершён за1063,692s: fresh16MiB/device MD5/SHA/two-NVMe, baseline46cf400 →
новый control → три TLS reset boots → весь factory2MiB fecc05d/verify/UART30 PASS.
Четыре writes/шесть captures; full restore не понадобился. На плате fecc05d,
F407 ee7d179 не открывался. Владелец заранее разрешил необходимые P1 стенды
23ноября2026 Asia/Almaty без повторных вопросов; этот exact пакет уже закрыт.

Свободная internal RAM control16771–16831 →124703–124755bytes, largest7680→63488.
Три verified HEAD HTTP200, cleanup1, полные DMA traces без allocation failures;
TLS local minimum108563–108867, remaining stack3760–3904/8192, touch2–19ms/errors0.
Возврат fecc05d touch1–11ms PASS. Это UART evidence, не physical touch/FPS:
render/event samples0, визуального owner подтверждения сегодня нет.

[Software evidence](evidence/build/esp32-lvgl-psram/README.md):16 clean ELF/BIN
двух SDK,183 host/183 sanitizer tests, actual allocator analyzer/ELF/config PASS.
Global malloc policy/stack/DMA64KiB/TLS16384/4096/RGB/pins/clocks/touch/CA/time
не ослаблены. [Следующий этап](plans/P1_SIGNED_SESSION_RESOURCE.md) — настоящая
signed CHECK/session4 с полным manifest/image stream в существующем worker8192,
затем install/confirm/rollback и manual release acceptance. Локальный ключ уже создан; стендовые SemVer/метки согласованы.
Начальная time/resource policy для CHECK23ноября одобрена владельцем;
exact candidate теперь опубликован отдельно, см. актуальный раздел выше;
SETUP/CHECK/INSTALL gates сохраняются, test keys/floors из HEAD не назначать.

## История до стенда38f70b9

## 23 ноября: следующий P1 результат — LVGL heap в PSRAM

[Стенд46cf400](evidence/hardware/2026-11-23-esp32-dma-46cf400/README.md)
дал control PASS/SNTP PASS и TLS handshake FAIL с9 allocation failures
DMA|INTERNAL|8BIT,4 retained/5 lost. Полный fresh16MiB restore fecc05d
реально выполнен/verify/reset PASS после touch clock race старого baseline;
последующий UART30s PASS. Physical UI/FPS/latency и signed OTA не приняты.

Dashboard занял100516bytes internal при свободной PSRAM. Следующий отдельный
PR — официальный CUSTOM allocator LVGL только OTA, оба SDK/host/fault/ELF,
затем измеренное сравнение control → новый control → TLS. Действует заранее
данное разрешение23ноября; пакет46cf400 одноразовый и уже закрыт. P2–P5 и
motion/irreversible/merge/release сюда не входят.

## История P1 после main502a73d

PR #71–73 слиты владельцем, main CI15/15 PASS. [Стенд3927063](evidence/hardware/2026-11-22-esp32-worker-tls-3927063/README.md)
реально выполнен по отдельному разрешению: control touch_fresh PASS, SNTP PASS,
первый HTTPS HEAD FAIL на hardware-AES allocation. Остальные TLS boots отменены.
Весь factory fecc05d возвращён, verify/UART/owner UI-touch-Wi-Fi-SETUP PASS;
сеанс743,183s закрыт. На F407 ee7d179 изменений нет.

Следующая [одна интеграция](plans/P1_DMA_ALLOCATION_TRACE.md): прошиваемый
diagnostic image обоих SDK с точными allocation bytes/caps/phase и DMA heaps →
ограниченный exact-image стенд → обоснованная экономия/размещение памяти.
Владелец заранее разрешил необходимые P1 подключения и стенды на23ноября2026
(Asia/Almaty), без повторных запросов. Использовать новый одноразовый пакет,
fresh16MiB/two-NVMe и автоматический возврат fecc05d; старый3927063 закрыт.
[Software evidence](evidence/build/esp32-ota-dma-trace/README.md): код46cf400,
16 clean ELF/BIN,14 regression matches, host181/sanitizers181/analyzer9 PASS.
Не отключать hardware AES,
CA/hostname/date, не снижать TLS16384/4096/DMA64KiB/display/clock/touch требования.
После HTTPS — owner settings и реальная signed SESSION4/resource CHECK,
затем install/self-test/confirm/rollback и ручная release acceptance. Пока
ключ/context/floors отсутствуют, CHECK/INSTALL закрыты. Не угадывать настройки.

[NEMA23/STEP-DIR design](NEMA23_BENCH_PLAN.md) выполнен в PR #72: host160/
sanitizer160/20000 cases/ARM archive и запрет physical outputs. Следующий
аппаратный этап motor/driver ждёт exact моделей и отдельного разрешения;
питание/ток/ENA/pins не назначены, independent E-stop обязателен.

## История перед PR #70

## 22 ноября: контроль a5f8b7e остановлен, fecc05d возвращён

[Стенд](evidence/hardware/2026-11-22-esp32-worker-tls-control/README.md) завершён
за608.667s: fresh16MiB/device MD5/SHA-256/two-NVMe PASS, control8192 app-only
write/verify/UART90.101s. Одно touch_age_ms=4294967295 на uptime26335ms из16
samples при touch_errors0; остальные0–19ms. touch_fresh FAIL сохранён,
TLS image не записывался, SNTP/HEAD не запускались. Весь factory2MiB fecc05d
возвращён из свежего снимка/verify/UART30.046s/owner HMI-touch-Wi-Fi-SETUP PASS.
Две writes/два captures, полный16MiB restore не понадобился. На ESP32 fecc05d,
F407 ee7d179 не открывался; разрешение использовано, новых аппаратных действий нет.

В actual app_main найдены два clock-before-snapshot paths. Host injections
воспроизводят underflow/false-health, исправление копирует snapshot прежде clock.
Пороги100/500ms, polling20ms и строгий hardware parser сохраняются. Исправление
ещё не испытывалось на плате; TLS/crypto/install/floors/rollback остаются открыты.

После main `53b0715`/слитого PR #68 следующий software результат P1 —
[opt-in UTC/verified HEAD image в OTA-worker8192](ESP32_OTA_WORKER_TLS_BENCH.md).
Он не даёт ключа/context/install authority; TLS runtime measurements и затем
configured crypto/install/confirmation/rollback остаются отдельной приёмкой.
На ESP32 fecc05d; новых hardware действий, merge или release не разрешено.

База main `dfa9f28`, PR #1–67 слиты. [Стенд3072/8192](evidence/hardware/2026-11-22-esp32-worker-stack/README.md)
завершён22ноября за928.998s по отдельному разрешению: fresh16MiB/device MD5/
SHA-256/two-NVMe, контроль3072 и три reset-запуска8192, три app/factory writes,
пять UART captures и возврат всего factory2MiB fecc05d/verify PASS.
Владелец подтвердил HMI/касания/Wi-Fi/OTA SETUP временного8192 и возвращённого
fecc05d. На ESP32 fecc05d/factory/tv4-16m-ab-v1; F407 ee7d179 не открывался.
Полный16MiB restore не понадобился; сеанс и разрешение закрыты.

Worker8192 watermark6608–6784bytes, BOOT local15767–15863; periodic internal
free14291–16863/largest7680 против control21795–21799/largest12288 после IP event.
Первый8192 включал UI-проверку: нагрузка различалась. Это unprovisioned reset
boot evidence, не configured TLS/install/production floor или cold-power/soak.
Ключ/context не заданы, CHECK/INSTALL закрыты. FPS/latency/slot confirm/rollback/
power-loss/full recovery не приняты. Новых hardware разрешений нет.

Следующий P1 результат — TLS/crypto/install budget в существующем worker8192,
затем reviewed настройки и реальная OTA A/B проверка; не новая цепь абстракций.

## Исторический итог P0 и программной подготовки

22 ноября 2026, база main `07fb406e37ebd1f1e019dbee0b54e8bb5571be1a`, PR #1–57 слиты.
После отдельного разрешения один стенд ee7d179 завершён за16,196s, exit0:
program/verify/exact identity, RUNNING, data/BSS/один CCM word/guards, progress,
watchdog reset и UsageFault capture/reset **PASS**. На F407 оставлен работающим
clean `ee7d179435f3f9e7598c7646b85ff64f4d5e9a32`, BIN2540bytes. Условный restore
не понадобился. Предыдущий c09 FAIL8 и проверенный полный snapshot restore — история.

Плата изолирована, только питание/SWD; option bytes/OTP/CAN/motion/ESP32 не менялись.
Этот разрешённый сеанс завершён. Полный RAM/CCM stress, stack high-water,
точные HSI/LSI/HSE частоты, physical GPIO/CAN/inhibit и power-loss остаются открытыми.
P0 выполнен на базовом стенде; следующий приоритет — P1 ESP32 OTA с existing
HTTPS/crypto и review16MiB factory+двух OTA slots/USB migration. Новых hardware
операций или будущих CAN loader addresses это не разрешает.

[Успешный стенд ee7d179](evidence/hardware/2026-11-22-f407-ee7d179-bench/README.md). [Следующий этап](NEXT_STAGE.md), [приоритеты](IMPLEMENTATION_PRIORITIES.md).
Базовый P0 hardware acceptance выполнен; PR #58 слит, база того этапа main d1317a8.
P1 интегрирован программно: [installer/rollback/migration](ESP32_OTA_INSTALLATION.md);
ключ/context не настроены по решению владельца; аппаратная migration/factory
проверка описана выше, configured OTA ещё не испытан.
Приводы, CAN и independent bootloader остаются отдельными этапами.

## Порядок и критерии результата

| Приоритет | Законченный результат | Зависимости и приёмка | Текущий статус |
|---|---|---|---|
| P0 — STM32F407 firmware | Настоящий ELF/BIN на HSI с RAM/SWD diagnostics, exception/watchdog handling, без активации motion outputs | Официальная карта VGT6/SRAM/CCM/Flash sectors/vectors; real startup/linker; map/size/hash/source и memory/vector CI; ST-Link install/restore runbook; запись только отдельно разрешена | **PASS базового стенда ee7d179**: program/verify/identity/RUNNING/progress, data-BSS/CCM word/guards, watchdog reset и UsageFault. Образ оставлен running; стресс/частоты/physical outputs не подтверждены |
| P1 — ESP32 OTA | Установка проверенного образа в неактивный слот, boot self-test, подтверждение или rollback с работающими LVGL/GT911/Wi-Fi | 16 МиБ factory recovery+otadata+два OTA-слота; подпись/local key/SHA/size/hardware; Wi-Fi/power-loss и resource acceptance | Migration/factory, native rollback, signed network INSTALL/self-test/VALID, CANCEL и STA Disconnect→полный retry проверены на bench.4. Отказ по истёкшему периоду/error24 проверен на 104fb06; все 16 МиБ/две NVMe-копии, bench.4 OTA1 VALID оставлен. RF/AP/power loss, production policy/budget/discovery открыты; прежний context истёк 25 ноября 08:00 |
| P2 — Releases | Настоящие воспроизводимые бинарники, SemVer/source binding, signed manifest/SHA-256, prerelease/stable, экран «ПО и обновления» | Рабочий OTA P1; проверка воспроизводимости, trust/signer/licensing; ручное утверждение владельцем exact SHA/tag/channel/assets; push/merge не выпускают release | Экспериментальные signed bench.2/.3/.4 опубликованы по exact owner approvals; bench.4 реально установлен. Source 6b84584: две clean builds каждого pinned SDK совпали по app BIN/ELF/bootloader/table, CI 17 PASS. Configured .5 payload требует собственного proof/review. Production signer lifecycle, stable/discovery остаются открытыми; автоматических релизов нет |
| P3 — physical CAN | Минимальный проверенный обмен ESP32-S3↔STM32F407 с ошибками/тайм-аутами/восстановлением | Получить трансиверы; проверить уровни/терминаторы/pinout/mux/IDs/bitrate на изолированном стенде; отдельное hardware permission | Физический CAN не проверен. Существующий протокол сохраняется; новые слои только для конкретной интеграции |
| P4 — STM32 CAN OTA | Независимый bootloader, проверенная запись Flash/recovery после прерывания, existing independent crypto и ESP32 OTA Manager | Реальная карта/профиль P0 и physical CAN P3; reviewed bootloader/app/staging layout; fresh quiescent/operator/recovery gates; обновления при движении запрещены | Host transport/verifier подготовлены. Адреса загрузчика сейчас не резервируются, Flash writer/boot activation ещё нет |
| P5 — microSD/G-code/CNC | microSD/tools/parser/planner/timer STEP-DIR/encoders/homing/thread sync/coordinates/manual-CNC | Стабилизированные базовые контроллеры, проверенная механика/электрика/защита; каждое испытание движения отдельно разрешено | PR #72: pinless timer register design/host160/sanitizer160/20000 cases/ARM archive PASS; physical outputs и motor/driver ещё не проверены. Существующие simulation/tools не выдавать за управление станком |

## Разрешения и доказательства

Подготовка образа/CI не разрешает доступ к устройствам, Flash/reset/SWD/UART,
движение, merge или release. NC E-stop/STO/manual-CNC interlocks сохраняются.
Не назначать неизвестные motion pins/полярности, не менять eFuse/option bytes
без отдельного разрешения. Boot/reset/reconnect/OTA не разрешают движение.

[HARDWARE_STATUS.md](HARDWARE_STATUS.md) остаётся источником owner facts.
Официальный device memory/vector contract проверен программно внутри P0; измерения на
экземпляре, physical GPIO/CAN и backup restore отмечаются отдельно. Неизвестный
HSE не препятствует HSI-only сборке. Новую аппаратную проверку или размер памяти
не объявлять по успешной компиляции/host test.

Исторические номера PR-01…PR-08 ниже — имена подсистем, не порядок дальнейшей
работы. Сохранённые evidence и limitations не требуют нового подготовительного
PR вместо P0 и не являются разрешением аппаратных действий.

## Сохранённые инженерные milestones

### Слитый PR #54, PR-07v — от main5e76626

[Threaded signed HOST link](STM32_THREADED_LINK.md): dedicated driver actor/
concurrent RX/full independent signed chain, original deadlines/ownership/ACK+
actual drain. Host75/sanitizers75/standalone49/TSan8+1 PASS, eleven ARM archives
и старые4 неизменны. Target scheduler/backend/clock/identity/resources/runtime/
installer/physical gates открыты. [Evidence](evidence/build/threaded-link/README.md).

### Слитый PR #53, PR-07u — acknowledged RX owner

[Owner](STM32_INGRESS_OWNER.md): exact driver pause ACK+actual ring/staged/held/
RX drain/epoch cutover и bounded lower-timeout recovery. Исторические host74/
sanitizers74/standalone48/TSan8; software barrier не physical drain.
[Evidence](evidence/build/ingress-owner/README.md).

### Слитый PR #52, PR-07t — owned RX ingress

[Ingress](STM32_FRAME_INGRESS.md): bounded SPSC owned copies/original capture age/
sticky loss, host71/sanitizers71/standalone45/TSan6; no target runtime.
[Evidence](evidence/build/frame-ingress/README.md).

### PR-07s — owned driver dispatch от maindb50840

[Dispatch](STM32_DRIVER_DISPATCH.md): bounded actual frame publication/take/
fresh check/exact report, retained pending/claim, slot reuse after producer return
and actual consumer cleanup. Host68/sanitizers68/standalone42/TSan4;8 signed
positives+3 independent+2 lifetime refusals. ARM1324text/data0/bss0/frame72/14 DMB,
old budgets unchanged. Target backend/ISR/RTOS/cache/DMA/clock/truth/physical/
install open. [Evidence](evidence/build/driver-dispatch/README.md).

### PR-07r — SPSC completion mailbox от main2c6fd1c

[Mailbox](STM32_COMPLETION_MAILBOX.md): single producer/consumer atomic owned
reports, actual bound consumer/current-time service/retained claims. Host65/
sanitizers65/standalone39/TSan2,1200000 ordered reports +8448 actual completions;
8 signed positives+3 independent+2 lifetime refusals. Separate ARM1038text/data0/
bss0/frame64/5 DMB; old budgets unchanged. Target ISR/RTOS/cache/DMA/driver truth/
reset/identity/runtime/physical/install open. [Evidence](evidence/build/completion-mailbox/README.md).

### PR-07q — driver event inbox от mainefa2727

[Event inbox](STM32_DRIVER_EVENTS.md): owned actual egress/FIFO reports, full
backpressure, bounded current-time service, retained ownership/IDs/phase/deadlines.
Host61/sanitizers61/standalone35;8 signed positive+3 independent+2 delayed event
refusals; ARM768text/data0/bss0/frame56 separately. Actual synchronized adapter/
ISR/RTOS/reset/provenance/runtime/physical/install остаются open.
[Evidence](evidence/build/driver-events/README.md).

### PR-07p — bound TX egress от main555007d

[Egress](STM32_UPDATE_EGRESS.md): fixed custody queue/exact local driver tickets,
phase/begin wait for actual returns, held ownership after fault/cancel/uncertain.
Host59/sanitizers59/standalone33,8 signed positives+3 independent+2 driver refusals;
separate ARM1972text/data0/bss0/frame88, old code/budgets unchanged. Actual driver/
reset/trust/persistence/runtime/physical/install ещё open.
[Evidence](evidence/build/update-egress/README.md).

### PR-07o — shared update channel от main389de77

[Channel](STM32_UPDATE_CHANNEL.md): one owned outbox/strict TU/MS codec selection,
shared counters/local phases/role filtering. Host56/sanitizers56/standalone30,
512 cycles/7168 messages, actual8 signed positive+3 refusals. Separate ARM1940text/
data0/bss0/frame56, old code/budgets unchanged. Local traffic policy не upper
success/auth/remote queue flush proof; runtime/physical/install open.
[Evidence](evidence/build/update-channel/README.md).

### PR-07n — metadata setup от main d18cf93

[Setup/shared namespace](STM32_METADATA_SETUP.md): paired metadata ID из original
CLOSED pool, bounded MS exchange/raw fragments и pool ownership до concrete
metadata endpoint close. Original image windows/independent verification прежние.
Host54/sanitizers54/standalone28, setup separate3324text/frame48, old ceilings
не расширены. Authentication/persistence/routing/runtime/hardware/install open.
[Evidence](evidence/build/metadata-setup/README.md).

### PR-07m — bounded metadata transport от main7640f43

[Metadata](STM32_METADATA_TRANSPORT.md): distinct DATA identity/raw exact
container98..1215bytes, copy-before-ACK/one owned UNVERIFIED snapshot, retained
high-water/deadlines; real delivered metadata feeds independent verifier.
Host50/sanitizers50/standalone24, oracle2/1024 lengths/100000 mutations/8 positive
signed chains+3 refusals. Separate ARM1568text/data0/bss0/frame48; old libs прежние.
Authenticated setup/identity/runtime/physical/installer gates остаются открытыми.

### Слитый PR #44, PR-07l — shared-clock host composition от main277801e

[Timing contract](STM32_UPDATE_TIMING.md): one-frame queue per direction,
shared caller clock, losses/delays/duplicates/whole-message retries без reset
logical deadlines.34 DATA cases,8 signed positive chains/2 refusals; host47/
sanitizers47/standalone21, ARM footprint прежний. Measurements/runtime driver/
identity/metadata transport/install не добавлены; board gates прежние.

### Слитый PR #43, PR-07k — bounded fragments от main4db99df

[Fragment draft](STM32_UPDATE_FRAGMENTS.md):286byte owned buffers, <=8byte
frames, explicit route/generation и retained counters, queue peek/commit,
exact duplicates/order, bounded deadlines/quota/READY backpressure/single take.
Host45/sanitizers45/standalone19, oracle3,100000 fault chains и8 signed RAM
chains. ARM separate1536text/data0/bss0/frame48, old libraries/codec прежние;
не full RAM/stack/throughput proof. Production identity/routing/authentication/
metadata transport/runtime/install и аппаратные gates остаются открытыми.

### Слитый PR #42, PR-07j — bounded update byte codec от maindeace1c

[Codec draft](STM32_UPDATE_WIRE.md):8 message kinds, exact length/header/reserved,
canonical LE без C ABI/padding/tail, zero failure output. Independent golden12,
65536 outcome pairs,200000 mutations,8 signed RAM integration chains; host42/
sanitizers42/standalone16, analyzer12+1 и budget guard6. ARM codec1219text,
data/bss0/frame32, old4 archives43362text (+18); это не board RAM/CPU budget.
Codec не authentication, production CAN/fragmentation/runtime или installer.

### Слитый PR #41, PR-05d — blocked build binding audit от mainaa06833

[Build binding](RELEASE_BUILD_BINDING.md) сопоставляет external-pinned unsigned
candidate/full image/ELF digest с existing signed cohort. Factory/A-B layout и
build/signed profile mismatch явные, Motion build provenance не выдумана.
Report metadata-only/blocked exit3, все authority gates false. Host39/sanitizers39/
standalone13, новая suite13methods и прежние ARM gates; SDK matrix8 сохранена с
integrity pin check. Production bundle/context/provenance и hardware blockers
не закрыты, firmware/settings/keys/devices/publication не менялись.

### Слитый PR #40, PR-05c — offline release cohort review от main2c06cdc

[Review](RELEASE_COHORT_REVIEW.md): independent HMI/Motion verifiers, полные
images/ESP descriptor, signed source/release/channel и prospective peer range.
Host38/sanitizers38/standalone13, новые CLI3/cohort9 methods, analyzer8 embedded
+1 host/source/board/output/ARM-scope PASS. Firmware libraries/frames прежние.
Report не production bundle, context provider, protocol mapping или installer.

### PR-07i — RAM staging source от main2ea51b6

[Staging source](STM32_STAGING_SOURCE.md): explicit caller buffer, pending DATA
copy перед ACK, SEALED/LEASED/discard и monotonic tokens/high-water без reset.
Host36/sanitizers36/standalone13,45 variants/1536 cycles/8 signed backend chains,
ARM/static8/source/board/output PASS. State160 bytes плюс buffer; consume frame328
при прежнем лимите512. Не runtime caller, persistent recovery или RAM/map proof.

### PR-07h — release-aware handoff от mainc17e89c

[Release handoff](STM32_RELEASE_HANDOFF.md) связывает signed15-field document
с прежними lease/RX/owner/deadline/vector guards и completion/outcome; raw metadata
SHA сверяется с negotiation. Host34/sanitizers34/standalone11 и ARM/static7/source/
board/output gates PASS,6 новых full chains. Legacy core/receipt/completion/
workspace прежние; adapter528bytes caller-owned. Production linkage/local context,
runtime/CAN/backend/Flash/recovery/physical gates ещё открыты, устройства не трогали.

### PR-07g — независимый signed release document от main3381236

[Binding](STM32_RELEASE_BINDING.md): новый opt-in API и strict15-field draft,
release selection/source/channel/component SemVer + old target/counter/full image
checks через тот же engine/arena. Host33/sanitizers33/standalone10,2745 SemVer
comparisons, ARM/static/source/image/output PASS. Legacy API, workspace17664 и
max individual admission frame984 прежние. Нет production bundle/version/key
context или аппаратных действий. Это исторический результат PR #37;
последующее live coordinator соединение описано выше в PR-07h.

### PR-07f — свежий logical update-сеанс от mainf0bd866

[Negotiation](STM32_UPDATE_NEGOTIATION.md): четыре memory-model сообщения,
monotonic pools/high-water, bounded replies/deadline и one-time take.1024 sessions,
6 new signed full chains/3 negative bindings; host31/sanitizers31/standalone8,
ARM/static/source/image/output PASS. No authentication/boot identity provider/
persistence/wire/runtime caller; actual F407/Flash/hardware gates прежние.

### PR-08a — локальные сведения о ПО от mainb56986f

[Экран](FIRMWARE_INFO_UI.md): parameters menu + build/SDK/profile, explicit
simulator/unknown provenance, no runtime OTA caller. Lazy-create/delete panels,
64 host navigation cycles с одинаковым pool free и object count, persistent
STOP/fault/keyboard cleanup. Host30/sanitizers30 и local SDK6.1.1 headless/rev12
PASS; CI matrix8/STM32 gates сохранены. Не закрывает production context,
resource/soak/installer или physical F407/CAN gates.

### PR-05b — release metadata policy review от main455ccc6

[Review](RELEASE_POLICY_REVIEW.md): malformed Unicode hex gate и canonical Base64
offline parity;115 signed C/Python scenarios,2809 SemVer comparisons,65536 BMP
escapes и130 Base64 cases. Host29/sanitizers29/native analyzer3units и local
ESP-IDF6.1.1 headless/rev12 PASS; CI matrix8 и STM32 gates сохранены.
Actual local versions/keys, production Motion schema, layout migration/installer
и hardware commissioning остаются открыты. Owner повторил VGT6, HSE не сообщил.

### Исторический снимок PR-07b от main74a7af9 — 20 ноября2026

[Независимый image admission](STM32_IMAGE_ADMISSION.md) в этой ветке:
подпись P-256/raw metadata, strict draft parser/local compatibility/counters,
SHA/exact EOF, bounded arena/deadline/cancel, cleanup и ARM compile checks.
На момент подготовки PR #29 transport был открыт; код admission не зависел от него. Теперь оба PR слиты. Есть только
VERIFIED_CANDIDATE, без board image/Flash/CAN/commit/jump/runtime.
Дальше нужны утверждённый release metadata/keys/version context, интеграционный
session/quiescent contract и verified erase/vector/recovery перед writer.
Аппаратные milestones ниже этим software результатом не закрываются.

- M0: база документов/backup. ESP32 backup завершён; Blue Pill и F407 backups
  сообщены; F407 SWD/core/Flash1024KiB подтверждены владельцем. Restore/SHA F407,
  маркировка/RAM/HSE/секторы и аппаратные unknowns остаются открыты.
- M1: работающий HMI/software simulation и матрица SDK; display наблюдался,
  Wi-Fi input/connect fix — PR #5; commissioning сенсора/метрик ещё не завершён.
- M2: CAN bench и fault injection; сейчас только inert host envelope,
  трансиверов нет. Никаких физических CAN испытаний не заявлять.
- M3: STM32 foundation и bench timer/DMA measurements без приводов.
- M4: квалифицированная safety wiring review, interlocks, homing/limits,
  подтверждённые направления и поосевое commissioning с отдельным разрешением.
- M5: энкодер шпинделя/servo/thread synchronization после механических измерений.
- M6: indexing/C-axis/removable milling — последующие отдельно проверяемые задачи.

OTA, microSD и CI/CD поддерживают эти этапы, но не являются доказательством
безопасности реального станка. Исторический CODEX_TASK_001 больше не активный план.

## Hardware baseline b93cf3b

[18 ноября: разрешённая USB-запись и результаты](evidence/hardware/2026-11-18-b93cf3b/README.md).
Basic HMI/input/Wi-Fi DHCP PASS; internal largest block 12288 bytes блокирует
простое добавление HTTPS/session в internal RAM. PR-04f/04g слиты; [isolated allocation bench 1c38d1e прошёл три цикла](evidence/hardware/2026-11-18-1c38d1e-psram/README.md). Production TLS memory budget ещё не утверждён; Gate 1 soak/negative tests,
UART capture/reset procedure и Gate 2 ещё не завершены.

## PR-04h — время для TLS

[Local provenance/age/skew gate](TLS_TIME_GATE.md) и guard вокруг каждого
HTTP operation, SDK clock sampler. Источник UTC/SNTP service, production policy,
UI и runtime TLS profiling ещё не включены; аппаратных операций нет.

## PR-04i — SNTP source

[Bounded source](SNTP_SOURCE.md) готовится отдельным PR после #18. Нет
startup/UI invocation или production time policy. Следующий шаг — opt-in
UTC/TLS memory profiling с отдельным разрешением на точный image и network test.

## PR-04j — явный UTC/TLS memory bench

[Отдельный профиль](UTC_TLS_BENCH.md), две heap windows, один HEAD без body.
PR #20 слит; [19 ноября b085abb испытан](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md):
SNTP PASS, TLS context allocation FAIL (-141 insufficient memory). Возвращён
ordinary rev12 b085abb. Следующий шаг — review allocator/PSRAM/TLS memory
с host/build проверками; новое аппаратное испытание требует нового разрешения. Production TLS budget не объявлять по одному
снимку/endpoint: нужны ошибки/redirect-hosts/soak и ownership/performance review.

## PR-04k — TLS PSRAM allocator / X.509 dates

[Review и профиль](TLS_MEMORY_REVIEW.md): исправление причины b085abb allocation
failure через штатный SDK PSRAM-only allocator. Только TLS bench; ordinary
profiles/reserves/record sizes не меняются. Включена SDK certificate-date check,
которую local anchor не заменяет. Software matrix/negative/ELF checks; новый
hardware gate отдельно, до него runtime TLS/production budget остаются блокированы.

## Hardware gate PR-04k — 1bfc7e2

[Разрешённый TLS/PSRAM test](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md)
дал SNTP/verified HEAD200/cleanup PASS; возвращён ordinary1bfc7e2,owner HMI PASS.
Production reserve не утверждён: internal min3123/window,2408/history и stack
watermark912bytes. Следующий программный шаг — bounded memory/stack review
перед runtime session/body. Не подключать installer или снижать CA/time/DMA
reserve ради progress; новые hardware/fault/soak tests только отдельно согласовать.

## PR-04l — TLS worker headroom

[Изолированное исправление](TLS_HEADROOM.md): compact Wi-Fi online accessor
вместо UI snapshot в стеке, HTTP RX2048, finite stage diagnostics. Stack6144,
TLS16384/4096 и DMA reserve64KiB прежние. Software checks не закрывают runtime
memory/stack/CPU gate; следующее exact-image испытание отдельно согласовать.

## Hardware gate PR-04l — 8f0d899

[Разрешённый test](evidence/hardware/2026-11-19-8f0d899-headroom/README.md):
SNTP/HEAD200/cleanup PASS,window internal min5307/stack watermark1840bytes,
против3123/912 у1bfc7e2. Вернули ordinary8f0d899,owner HMI/touch/Wi-Fi PASS.
Следующий программный шаг — review remaining handshake/internal allocations/
call stack и bounded resource admission/failure перед runtime OTA session/body.
Не превращать один положительный test в production reserve/soak approval.

## PR-04m — bounded resource admission

[Resource guard](OTA_RESOURCE_GUARD.md): reviewed local policy/current heap/stack,
обязательные inputs нового guarded session entry, sticky failure/candidate revoke
на phase/chunk boundaries. Legacy API и target workspace34000 сохранены.
Нет runtime worker/body/install activation; native budget/load/fault gates открыты.

## PR-05a — сразу несколько частей подготовки релизов

[Build candidates](BUILD_CANDIDATES.md): полный offline ESP app parser, связь
app↔ELF/clean source/display version, детерминированный metadata-only bundle,
независимый verifier и negative tests, интеграция в8 CI jobs и SHA-pinned Actions.
Обычная factory2MiB layout остаётся; SemVer/key/license/release approval и runtime
OTA/migration gates не подменяются успешным review artifact. Аппаратных операций нет.

## PR-02b — после owner SWD F407

[Owner evidence](evidence/hardware/stm32f407-owner-swd/README.md) закрывает прежнее
UNKNOWN по SWD/core/device ID/Flash capacity. [Startup foundation](STM32_STARTUP.md)
проверяет portable init/no-I/O и Cortex-M4 reset/link sections на synthetic map,
сохраняя блокировку board image. Следом нужен reviewed RAM/vector/clock/board
contract; HSE можно отложить при отдельно принятом HSI-only решении. CAN bootloader
и OTA sectors не резервируются до geometry/recovery review; motion запрещён.

## PR-07a — две стороны транспорта до Flash и CAN hardware

[Ограниченный logical session](STM32_UPDATE_TRANSPORT.md): stop-and-wait,
двухфазный consumer/ACK, exact duplicate с проверкой payload, CRC-32C,
explicit retries/total/progress deadlines, identity/cursor checks и sticky faults.
Host/sanitizer tests проверяют потери DATA/final ACK, reorder/corruption,
partial consumer/volatile resets и отсутствие automatic resume. Новый модуль
ARM compile-only, независим от motion. Нет runtime caller или installer.
Этот transport gate был продолжен независимым admission в PR #30;
hardware/Flash layout/recovery/quiescent/wire decisions остаются открытыми.

## PR-07c — transport/source lifetime

[Read-only handoff](STM32_UPDATE_HANDOFF.md) от main28fe7bc связывает слитые
PR #29/30: immutable lease, policy/boot/source guards, descriptor↔signature/SHA,
receipt revoke и bounded cleanup/quarantine. Root25/sanitizer25/standalone5,
ARM/static/image/output gates; hardware milestones не закрываются.
Следующий software review — production metadata/context и protocol/source
transition после bounded RX completion. Writer/jump ждут board/erase/vector/
recovery/fresh physical quiescent/operator gates; Flash addresses не резервируются.

## PR-07d — проверка модели образа до board map

[SP/PC/vector checker](STM32_IMAGE_PREFLIGHT.md) от main4b7631e использует только
explicit local model; источник остаётся подписанным immutable snapshot. Новая
profile suite, signed vector/fault tests, ARM/static/import/frame и прежние
8 SDK builds. MODEL_MATCH не закрывает hardware/Flash/recovery milestones и
не разрешает writer/jump. Следующие review: production release/context/profile
и verified board memory/vector/clock/erase/protection без предположения адресов.

## PR-07e — завершение software-проверки и доставка истории

[Completion/outcome](STM32_UPDATE_OUTCOME.md) от maine0bd84e: actual crypto/model
checks → lease close → historical report/ACK, bounded repeats/linger/deadline,
context/reboot/conflict и quarantine. Host27/sanitizers27/standalone7, ARM/static/
source/output gates и8 SDK builds. Next: reviewed production negotiation/codec/
identity и metadata/context, actual board/Flash/recovery; не activation по peer report.

## Слитый PR #52, PR-07t

[Очередь входящих кадров](STM32_FRAME_INGRESS.md) от main25e635c после PR #51:
owned SPSC capture/retry/stage/fresh age check и sticky loss/clock/expiry/exhaustion,
actual signed host chains сохраняют original crypto/lease/model/close deadlines.
Host71/sanitizers71/standalone45/TSan6; ARM1060/frame48 отдельно; no new devices,
Flash map/pins/driver/runtime. Далее — actual producer-stop/phase-cutover, truthful
backend loss/completion/reset/clock и reviewed resource/identity/runtime port.

## Слитый PR #53, PR-07u

[Подтверждённая пауза RX](STM32_INGRESS_OWNER.md) от mainbc35a37 после PR #52:
actual driver ACK + owned ingress/channel drain перед сменой фазы, epoch capture
permits и bounded fragment-timeout recovery без reset clocks/counters/upper
windows. Дальше actual target peripheral/IRQ/DMA/backend stop/route lifecycle,
clock/resources/identity/runtime и physical/install gates; устройств/Flash/pins нет.
