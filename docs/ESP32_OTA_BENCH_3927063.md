# P1: повтор touch_fresh → UTC/HTTPS на main 3927063

## Готовый программный пакет

Источник: `392706355193b7544132e2296f7ef8572e1f3c10`, PR #70 слит.
Все 15 jobs пяти workflows main завершились успешно, включая оба SDK.
Новые четыре локальные сборки сделаны из чистого checkout этого коммита.
[Evidence](evidence/build/esp32-ota-repeat-3927063/README.md).

Исправление actual app_main копирует touch snapshot **до** чтения clock в
health и diagnostics paths. Сохранены пороги 100/500 мс, polling 20 мс,
проверка каждого sample и fault latch. Нет clamp отрицательного возраста,
исключения sample или обхода touch_fresh. Clock/report race и настоящий stale
snapshot проверяются тремя actual-source host tests из PR #70.

Профиль `rev12-ota`, trace + worker8192, provisioning отсутствует.
Контроль и TLS различаются только `TV4_OTA_WORKER_TLS_BENCH=false/true`.
Версия приложения `3927063`; существующая разметка `tv4-16m-ab-v1` сохранена.

| SDK | Вариант | BIN, байт | SHA-256 приложения |
|---|---|---:|---|
| 6.1.1 | контроль | 1600640 | `6ecae5f0e4cce3ed29c880fe308be288ae6b4c2fe2e2ee60782de3504697adc5` |
| 6.1.1 | TLS | 1603456 | `cbfb26497c1b09e5b345df61de43f278a8ef7ee951a1a5a63ad5b03635c4ecfd` |
| 5.3.2 | контроль | 1553184 | `0e723a7498acdc6972495e7a597535d98cf808f7fb3a509e41ecad0db9f6845c` |
| 5.3.2 | TLS | 1556016 | `873251c6120a42159055b61d3c227b6e6d0825e2d4185c12ff57e0f5ec060735` |

Для следующего стенда выбран **SDK 6.1.1**; 5.3.2 — software compatibility,
а не второй аппаратный сеанс. ELF digests и compiler frames сохранены в evidence.
Бинарники, private scripts и полный sealed plan хранятся локально вне Git.
SHA-256 plan: `eaf753199d207e237d4d19d11ff1306b4875b58373e352ca0a0742185b02d246`.

## Процедура для отдельного разрешения

Предварительное разрешение владельца на подключение не заменяет согласование
записи этих образов. Перед запуском требуется exact-image разрешение на весь
сценарий ниже и изоляция: питание/USB, без CAN, приводов и иных подключений.
Пакет одноразовый; старый a5f8b7e и его использованное разрешение не повторяются.

1. Проверить чип, защиту и Flash 16 МиБ. Считать свежие 16 МиБ штатным RAM-stub,
   сверить device MD5/SHA-256, сохранить приватные копии на двух разных NVMe.
   Проверить bootloader/table, factory fecc05d и otadata до первой записи.
2. Записать только контрольное factory app, verify; до запуска сравнить Flash
   вне factory. Reset и UART 90 с. Требуются все checks, включая каждый
   touch_age <500 мс, touch_errors, startup/BOOT/self-test/PSRAM XiP/RGB/GT911.
   Любой FAIL запрещает запись TLS app и переводит процедуру к возврату.
3. Только после PASS записать TLS app, verify/outside-factory; три reset/UART
   по 90 с без повторной записи приложения. На boot — одна SNTP-синхронизация
   через pool.ntp.org и один HTTPS HEAD к `https://github.com/avlyubimov/TV4-CNC`
   в **том же OTA worker8192**, existing native backend и serialized min-monitor.
   SNTP неаутентифицирован; HTTPS проверяет CA/hostname/date. Нет body/auth/
   redirects, подписи не подменяются TLS. FAIL останавливает оставшиеся пробы.
4. Вернуть **весь factory 2 МиБ** fecc05d из нового снимка, verify/outside-factory,
   reset/UART 30 с. При ошибке и доступном USB — не более одной попытки полного
   восстановления свежих 16 МиБ, только при запасе не менее 420 с.

Общий предел 1800 с; planned captures 90/90/90/90/30 с. Bootloader/table/otadata
записываются только при условном полном восстановлении. UART/RTS может вызвать
дополнительный reset; SNTP меняет UTC, TLS-память PSRAM не зашифрована. Обрыв
питания при записи может потребовать BOOT/RESET. Испытаний отключением питания,
изменений eFuse, STM32, CAN или motion нет. После возврата нужен owner UI/touch/
saved-Wi-Fi/OTA SETUP check. Новых runtime результатов этот пакет не утверждает.

Host-only процедура прошла 39 тестов: новый единичный control sample с возрастом
500, 501 или UINT32_MAX блокирует TLS write и возвращает factory; 499 принимается
по существующему правилу. Проверены отказ сертификата/HEAD, deadline, backup/
write/restore faults, запрет повторного recovery и exact authorization binding.
Strict analyzer совпадает с использованным в предыдущем стенде; фильтрации нет.

## После успешного HTTPS

1. Сопоставить control/TLS startup, SNTP3/TLS_PROBE6 boundaries, free/largest,
   per-heap minima, stack watermark, cleanup, certificate flags и timeouts.
   Это ещё не budget настоящей signed SESSION4; не переносить minima в floors.
2. Владелец задаёт reviewed local public key/key ID, SemVer/channel/hardware/
   protocol context, time policy и ресурсные требования. Сейчас их нет;
   CHECK/INSTALL остаются закрытыми. Не генерировать test keys и guessed floors.
3. Отдельно подготовить реальный **подписанный** read-only CHECK в existing
   worker: HTTPS GET bounded manifest/image, existing independent verifier,
   SHA-256/size/hardware/source binding. Измерить WORKSPACE2/SNTP3/SESSION4/
   CLEANUP5, стек и DMA reserve при живых LVGL/GT911/Wi-Fi, включая отказ сети/
   сертификата/подписи/size/timeout/cancel. Без fake manifests или нового crypto.
4. После review этих ресурсов и нового разрешения — inactive-slot install,
   reboot self-test/confirmation и преднамеренно неуспешный self-test/rollback.
   Wi-Fi loss и отдельно согласуемые power-loss/recovery сценарии — до приёмки P1.
5. Затем P2: SemVer/source-bound reproducible artifacts и signed manifest,
   prerelease/stable, ручное утверждение exact release владельцем. Push/merge
   не публикуют release. Фиктивный STM32 BIN не добавляется.

NEMA23/STEP-DIR — независимый software-only проект до точной модели привода;
он не включает аппаратные выходы и не снимает ни одного gate P1/P3–P5.
