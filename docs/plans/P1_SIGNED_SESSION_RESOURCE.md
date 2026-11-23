# P1: ресурсная приёмка настоящей подписанной OTA-сессии

После исправления LVGL heap в PR #75 следующий результат — проверенная на
ESP32 подписанная read-only сессия CHECK в существующем OTA-worker8192.
HTTPS HEAD проверяет транспорт, но не загрузку manifest/image, P-256, SHA-256,
Flash writer или rollback. Не переносить его минимумы в production floors.
Новые криптография, HTTPS-клиент, task или транспортная библиотека не нужны.

## Входные данные, которые ещё отсутствуют

- Утверждённые владельцем key_id и публичный PEM P-256 SPKI с fingerprint.
  Приватный ключ остаётся у владельца/offline signer; test key на плате запрещён.
- Согласованные installed/bootloader/protocol SemVer, candidate version,
  prerelease/stable channel и точный HTTPS URL подписанного manifest.
- Локальная reviewed policy: положительные internal free/largest, PSRAM
  free/largest и stack floors, time bounds/age/skew и явное решение о доверии
  unauthenticated SNTP. Скачать эти настройки из manifest нельзя.
- Настоящий подписанный candidate: exact source commit, app-only rev12-ota,
  hardware Rev1.2/layout tv4-16m-ab-v1, размер и SHA-256, manifest/signature.
  Никаких фиктивных STM32 assets. Публикация на GitHub требует отдельного
  утверждения владельцем точных tag/channel/assets; разрешение стенда её не даёт.

Формат уже реализованного локального контекста —
[ESP32_OTA_INSTALLATION](../ESP32_OTA_INSTALLATION.md#будущая-локальная-настройка-доверия).
Пока данных нет, сохранять «Нужна настройка», CHECK/INSTALL закрыты.
Не генерировать заменяющий ключ, не назначать версии/floors ради продолжения теста.

## Следующий отдельный PR и пакет

1. Применить проверенный CUSTOM PSRAM allocator; подготовить provisioned
   image с `TV4_OTA_CONTEXT` из приватного reviewed файла. Отдельно собрать
   оба pinned SDK, проверить source/config/ELF/BIN/lock hashes, CI и размер
   слотов. Unprovisioned worker-TLS diagnostic flag не включать в configured
   image: его build guard намеренно запрещает такой режим.
2. Использовать `service.c::operation(false)` → существующий
   `tv4_ota_session_run_guarded`. Он получает и проверяет manifest и весь
   app stream, но не имеет Flash sink и не меняет boot selection. Измерения
   брать в том же worker: WORKSPACE2 → SNTP3 → SESSION4 → CLEANUP5 и stack
   watermark; окна SDK minimum monitor строго последовательны.
3. До записи образа запечатать exact hashes, длительность/пределы операций,
   свежий16MiB/device MD5/SHA и две копии на разных NVMe, возврат текущего
   factory и условный единственный full snapshot restore. Проверять область
   вне factory до каждого запуска; не повторять закрытые диагностические пакеты.
4. Выполнить CHECK с настоящей подписью и всем image body, сохранив CA,
   hostname/date, signature, SHA/size/hardware/protocol/version/time/resource
   проверки. Успех — READY с точной identity candidate, resource report PASS,
   закрытыми HTTP/hash/workspace/monitor owners, без изменения otadata/слотов.
5. Сохранить диапазоны current free/largest отдельно от local/history minima,
   PSRAM и stack, длительности/полные тайм-ауты, touch age/errors, display/queue
   drops и Wi-Fi reconnect. Generic INTERNAL|8BIT не выдавать за DMA heap.
   Нынешний failure hook доступен только unprovisioned TLS bench: если нужен
   DMA trace configured сессии, подключить его к существующему SESSION owner
   отдельным проверенным изменением, а не снять compile/ownership guards.
6. Проверить cancel, потерю Wi-Fi, deadline, отказ ресурсов и неверные
   signature/hash/size/hardware/version сначала host tests, затем ограниченными
   стендовыми случаями. При любом FAIL остановиться, закрыть ресурсы и вернуть
   проверенный factory; не ослаблять gates и не снижать floors до PASS.

Три reset HEAD boots не заменяют UI load, cold-power, soak или физические
FPS/touch-to-display/tearing. Принять ресурсный бюджет с запасом можно только
после full signed session и failure/load измерений, с review владельца.

## После принятого CHECK

Отдельный пакет INSTALL повторно проверяет exact candidate identity/подпись/
SHA/size и пишет только неактивный OTA slot. Оператор подтверждает конкретный
образ; reboot остаётся явным. Проверить self-test, mark-valid и rollback при
неподтверждённом/ошибочном запуске, затем разрывы Wi-Fi и питания на отдельно
согласованных границах. До этого rollback и power-loss recovery не приняты.
Factory USB restore — другой механизм, не доказательство OTA rollback.

P2 следует после приёмки: существующая Actions матрица, SemVer/commit binding,
подписанный manifest и checksums, manual owner publication, повторная проверка
скачанных assets и экран «ПО и обновления». Не создавать release на каждый
push, не публиковать автоматически. eFuse/Secure Boot/Flash Encryption, STM32,
CAN, STEP/DIR и аппаратная независимая E-stop цепь этим этапом не меняются.
