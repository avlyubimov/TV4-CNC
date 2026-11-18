# Когда понадобится проверка на плате

Состояние 18 ноября 2026, PR-04e. Плата не требуется для завершения текущего
read-only session PR. Host tests/SDK compile не заменяют следующие испытания.
Перед подключением runtime HTTPS worker к HMI потребуется изолированный стенд.
Агент должен явно сообщить владельцу: «нужна проверка на плате», указать reviewed
SHA/build/profile, существующий layout и точную разрешаемую операцию. Этот
документ сам не разрешает serial access, reset, прошивку или запись устройства.

## Gate 1 — HMI/Wi-Fi baseline до runtime HTTPS

На согласованной версии rev12, без приводов/CAN/управления станком:

- Записать installed SHA/version, SDK, reset reason и startup logs; не считать
  main версией уже установленной на плате. Проверить сохранность backup отдельно.
- RGB/PSRAM startup: стабильное изображение, отсутствие reset/allocation errors,
  доступная internal RAM/PSRAM, min heap и task stacks из существующей диагностики.
- GT911: края/углы, ввод на увеличенной клавиатуре, показать/скрыть пароль;
  пароль не попадает в отчёт, serial logs или screenshots.
- Wi-Fi: список/RSSI, верный пароль → IP/DHCP, неверный пароль → понятная ошибка,
  disconnect/reconnect и сохранённая сеть после согласованного reboot.
- Отзывчивость меню/касания при scan/reconnect, базовые counters/reset, короткий
  soak с длительностью в отчёте. FPS и физическую latency отмечать отдельно;
  программная event latency не доказывает touch-to-display.

Результат PASS только с реальным наблюдением и версией; неизвестное оставить
UNKNOWN. Сейчас эти пункты ещё не подтверждены. Если Wi-Fi/DHCP не работают,
runtime HTTPS откладывается и сначала исправляется baseline.

## Gate 2 — после отдельного runtime worker PR

Нужны reviewed trust/version/layout context, реальный источник TLS time,
тестовый подписанный asset и bounded worker snapshots/cancel. Затем отдельно
проверить TLS CA/hostname/clock failure, реальную цепочку GitHub redirects,
Wi-Fi loss/cancel/deadlines/retry, peak TLS heap/PSRAM/stack и UI responsiveness.
Текущий PR не добавляет trigger для этого теста; выпускать production релиз
ради теста нельзя. Read-only image verification не пишет Flash.

## Gate 3 — установка, отдельный этап

Только после review миграции factory → A/B, локального backup/restore plan,
self-test/rollback/recovery и явного разрешения на конкретную запись. Power-loss
проверки и boot partition activation не входят в Gate 1/2. Не менять eFuse,
Secure Boot, Flash Encryption или STM32 option bytes. F407 SWD/таймеры/CAN
планируются отдельно после подтверждения HSE/геометрии/распиновки.

Evidence складывать с датой/версией/методом/ограничениями в docs/evidence/;
не публиковать дампы Flash, Wi-Fi пароли, ключи или лишние идентификаторы.
