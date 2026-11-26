# P1: shutdown-маркеры в настоящем пути после INSTALL

База — main `ef3b47d`, PR #1–94 слиты владельцем. Отдельный unprovisioned
factory restart source `f819955` прошёл один раз, но не воспроизвёл
[RTC watchdog после bench.5](../evidence/hardware/2026-11-25-esp32-bench5/README.md).
На начало этапа плата оставалась на bench.4 OTA1 VALID. После
[стенда 26 ноября](../evidence/hardware/2026-11-26-esp32-post-install-trace/README.md)
оставлен bench.5 OTA1 VALID. Причина первоначального RTCWDT неизвестна.

Результат этой интеграции — настоящие configured ELF/BIN обоих pinned SDK и
проверенный конечный пакет для наблюдения signed CHECK → INSTALL → операторский
REBOOT. Это диагностика существующего пути, без нового installer/crypto layer.

## Изменение

`TV4_OTA_RESTART_TRACE=1` выключен по умолчанию. Требует explicit rev12-ota,
memory diagnostics и локальный публичный context с install_reviewed=true.
Несовместим с automatic restart, signed auto-CHECK, worker8192 и TLS bench.
Обычные профили сохраняют прежний код и отсутствие диагностических символов.

Тот же worker 12288 байт. Boot marker сообщает reason/core/operator=1 через
штатный сериализованный SDK logger, ничего не запускает. В первом стенде
source `2cae6c4` ROM printf перемешался с параллельным startup log; строгий
контроль остановил пакет до сети и вернул проверенный bench.4. Повреждённая
строка не восстанавливается догадкой. Финальные IRAM hook/noos сохраняют
ROM-only вывод без logging locks. Callback регистрируется до допуска CHECK/INSTALL, после прежнего
HMI/PSRAM/layout/local-trust self-test. Ошибка регистрации запрещает команды
обновления до любой записи. Shared readiness публикуется под прежним mux.

Загрузка, pending confirmation/refusal/rollback, CHECK, подтверждение кандидата,
session verifier, HTTPS/UTC, resource policy, cancel и inactive-slot backend
сохраняются. После commit нет новой регистрации, heap monitor start, аллокации
или failable admission. Только штатная операторская команда REBOOT вызывает
request marker → esp_restart → shutdown marker → IRAM wrapper → прежний SDK
esp_restart_noos. Обработчики/строки используют прежние IRAM/DRAM/ROM границы.
Пины, часы, DMA, framebuffer, TLS records, watchdog и eFuse не меняются.

## Программная приёмка

- Actual service C с SDK doubles: успешный CHECK/INSTALL/REBOOT, отсутствие
  автоматического сброса, ожидание оператора после commit, registration failure,
  stale health, транспортный/resource/cancel отказ и boot trust/refusal cases.
- Compile/shell/config negative gates, default-off и разные build trees.
  Migration/release authoring по-прежнему не принимает диагностический профиль.
- Оба реальных ELF: worker вызывает boot/prepare/execute, execute вызывает SDK,
  SDK вызывает wrapper, wrapper вызывает исходный noos; final markers только
  IRAM/DRAM/ROM. Доступный SDK5 analysis-copy guard сохраняет все loaded bytes.
- CI сохраняет матрицу и проверяет новый образ с одноразовым public-only
  software fixture, версией 0.0.0-ci-trace и отброшенной приватной частью.
  Этот BIN не публикуется и не прошивается. Реальный стенд использует только
  прежний утверждённый context/ключ владельца; policy автоматически не меняется.

## Критерии конечного стенда

Новый exact source/app/CI packet, не повтор закрытого. Свежие actual16МиБ,
device verify/SHA-256 и две независимые NVMe-копии до записи. Сохранять
known-good bench.4/VALID и bootloader/table. Ограничить области записи,
время, число запусков и резерв восстановления; no CAN/motion/eFuse/option bytes.

Принимающий diagnostic .4 использует существующую policy .4→.5 только до
27 ноября 2026 08:00 Asia/Almaty, без продления. Проверяемый payload — прежний
неизменяемый опубликованный bench.5; новый release/tag для диагностики не нужен.
CHECK и два подтверждения INSTALL остаются штатными действиями владельца.
Непрерывный UART охватывает operator reboot и новый boot без промежуточного
host reset. Строгий reset_reason/watchdog_boot guard сохраняется даже при
работающем экране/self-test. Только PASS допускает независимое чтение слота
и otadata. При FAIL остановить диагностику и выполнить предусмотренный возврат
здорового bench.4. По окончании подтвердить реальный retained образ/метаданные.

Маркеры локализуют последнюю наблюдавшуюся границу, а не причину reset. Их
влияние на timing и разница factory/configured/post-write условий остаются
инженерными гипотезами. Software fixtures, compiler frames, init messages и
touch sample age не доказывают аппаратные FPS/latency или production budget.

## Фактический результат

Исходник `8b3db2f`: 334 host / 334 sanitizers, оба pinned SDK и CI исходника 34/34 PASS;
40+40 procedure groups, включая interleaved/legacy boot marker refusal.
[SDK 6.1.1 hardware](../evidence/hardware/2026-11-26-esp32-post-install-trace/README.md):
control 37/37 → signed CHECK/INSTALL → operator restart →90.160 с passive/SW3/WDT0 →
native VALID/exact published candidate/actual 16 MiB/two NVMe pairs PASS.
Bench.5 оставлен OTA1 VALID, UART закрыт, owner health PASS; recovery 0.
Первый control FAIL source `2cae6c4` и первоначальный RTCWDT FAIL сохранены. Причина
watchdog не установлена; следующий результат — сопоставимый uninstrumented test,
а не повтор закрытых пакетов. [Активная очередь](../NEXT_STAGE.md).
