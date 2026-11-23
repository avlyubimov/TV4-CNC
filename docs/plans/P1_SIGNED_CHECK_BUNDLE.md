# P1: локальный подписанный payload настоящего CHECK

База main be8f3f4, PR #76 слит владельцем. Три verified HEAD на38f70b9
прошли; next step — полный manifest/image stream и подпись. По просьбе
владельца создан локальный ключ P-256 вне Git, затем перенесён в домашнюю
папку секретов. Закрытый ключ/абсолютные пути/локальные context files не
публикуются. Владелец утвердил только стендовые версии HMI
0.1.0-bench.1 → 0.1.0-bench.2, prerelease. Метка bootloader0.1.0-bench.1
привязана к проверенному fecc05d; protocol1.0.0 обозначает OTA metadata/image,
не физический CAN. Это локальные согласованные метки, не переименование fecc05d.

## Результат этой интеграции

Одна локальная команда `tools/prepare_ota_release.py` создаёт из настоящей
rev12-ota сборки app-only BIN, manifest.json, manifest.sig, checksums.sha256,
build-evidence.json и release notes. Подписываются точные raw bytes манифеста
через существующий OpenSSL ECDSA-P256-SHA256, затем существующий
verify_ota_candidate повторно проверяет подпись/политику/SHA/длину/образ.
Новой криптографии, HTTPS или firmware task нет.

Входы: отдельный reviewed local plan, canonical DER SPKI и его SHA-256 pin,
private key0600 в owner directory0700, exact clean source SHA, actual SDK build.
Никаких версий, ключей, floors или доверия из скачанного пакета. Упаковка
сверяет actual ELF hash с app descriptor, проверяет A/B/PSRAM/config через
check_idf_build, явно заданный SemVer и dependency lock. Старые metadata-only
review bundles сохраняют прежний scope и не превращаются в release authority.

Пакет только `signed-check-bench`, канал prerelease. В нём нет ключей,
bootloader/table/NVS/otadata или STM32 файла. Он не утверждает CI provenance,
production resource budget или install readiness. Unprovisioned app может
служить реальным image body для read-only CHECK: его `candidate_provisioned`
явно false, будущая установка такого payload этим этапом не разрешается.
Для install/rollback нужен следующий настроенный и отдельно принятый образ.

## Проверки и границы

Host tests используют только ephemeral keys и nonexecutable synthetic ESP
fixtures. Проверяются оба SDK formats, ключ/его pin/права/symlinks, source dirty/
untracked, app version/ELF/table/config, чужой trust, повреждения/усечённый image,
подпись при пересчитанном inventory, внешняя local policy, ошибки signing и
изменение plan. Проверки включены в существующий host/sanitizer CI через CTest.
Реальные ELF/BIN двух pinned SDK и локально подписанные payloads фиксируются
отдельно; ephemeral test images никогда не публикуются как firmware.

Обычные firmware paths/LVGL/GT911/Wi-Fi/stack/DMA/TLS records/CA/time untouched.
Упаковка не открывает сеть, serial/SWD или устройства, не создаёт Git tag/release,
не запускает flash/monitor. Private key не копируется в output/build/CI.
Подписи ECDSA могут различаться, raw manifest и binary digests привязаны точно;
это не доказательство воспроизводимости binary или CI identity.

## После этого пакета

Начальная resource/time policy для CHECK-worker8192 уже одобрена владельцем
только на23ноября; production policy этим не принята. Нужны ресурсные
замеры SESSION4 и заранее утверждённые exact assets/публикация стендового
кандидата. Отсутствующие значения не назначаются по HEAD minimum. Пока их нет,
прочие неутверждённые context поля не заполняются автоматически; на
установленном fecc05d SETUP остаётся закрыт.
План полного стенда — [P1_SIGNED_SESSION_RESOURCE](P1_SIGNED_SESSION_RESOURCE.md).
Публикация или provisioning не следуют из генерации ключа, подписи или merge.
