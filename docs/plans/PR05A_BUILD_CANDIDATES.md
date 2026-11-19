# PR-05a — проверяемые CI-кандидаты

База: main18b2bf5, PR #26 слит, четыре workflow main PASS. Дата: 19 ноября 2026.

## Один этап, пять связанных результатов

1. Offline проверка полного ESP32-S3 app image: segments, descriptor, checksum,
   appended SHA-256, Flash16МБ/DIO80МГц, project/SDK и связь с ELF.
2. Детерминированный review bundle: exact clean source commit/tree, SDK digest,
   profile, app/ELF/config/partition hashes, config/size report и per-SDK lock.
3. Независимая offline перепроверка bundle и, по запросу, локального app image;
   bounded input/strict JSON, allowlist файлов, отказ при corruption/stale inputs.
4. Встроить сборку/перепроверку metadata-only кандидатов в существующую матрицу,
   закрепить все GitHub Actions полными SHA, сохранить read-only permissions.
5. Негативные тесты, host/sanitizers, SDK6.1.1/5.3.2 ×4 profiles, evidence/docs/PR.

## Приёмка

- Нельзя выдать bench/headless за rev12, грязный source за exact commit или
  повреждённый/чужой binary за проверенный app. Проверить stale config/ELF/
  descriptor, длины, checksum/hash, unsafe files/links и повторную упаковку.
- Public CI artifact содержит только фиксированный набор review metadata;
  никаких app/ELF/bootloader/Flash/NVS dumps, raw sdkconfig/project_description,
  credentials, private keys, signing или release/install commands.
- SHA-256 inventory проверяет целостность, но не подпись/авторство. Bundle
  не OTA manifest; действующая factory2MiB layout честно остаётся без OTA.
- Owner ещё не выбрал лицензию для внешнего распространения приложения
  (LICENSE-NOTICE.md). Публикация binary/release и ключей — отдельный этап.
- Native firmware/UI/board settings/locks/layout не меняются; на плате
  ordinary8f0d899. Device/flash/reset/serial/network probes не выполняются.

Откат: revert PR; ранее установленные образы не меняются.
