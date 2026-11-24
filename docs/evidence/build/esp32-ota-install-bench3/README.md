# Реальный installable bench.3: сборки, подпись и публикация

Source `08932ee4ef57e585d0101d90cc54800d6e87ae3b`, main CI **15/15 PASS**;
после создания tag его отдельные CI также **15/15 PASS**.
PR #80–83 уже слиты владельцем. Исполняемый код этой интеграцией не менялся.
Четыре ELF/BIN построены из чистого source до изменения документации;
точные результаты и ссылки на CI — [results.json](results.json).

| SDK | Контроль bench.1 с trace | Кандидат bench.3 без trace |
|---|---:|---:|
| ESP-IDF 6.1.1 | 1 602 176 байт | 1 600 192 байта |
| ESP-IDF 5.3.2 | 1 554 688 байт | 1 552 720 байт |

Оба профиля — explicit `rev12-ota`, owner public key, worker 12288 байт,
local `install_reviewed=true`; auto CHECK, stack/TLS bench выключены.
Кандидат не содержит диагностического монитора. GPIO, RGB/GT911/CH422G,
Flash16MiB/PSRAM8MiB/XiP, allocator, TLS records и существующие guards сохранены.

SDK6 commit `051e4c4eb20346e043cdd47bc388fde75ff8af63`, GCC15.2.0
`esp-15.2.0_20251204`; SDK5 commit
`9d7f2d69f50d1288526d4f1027108e314e8c879f`, GCC13.2.0
`esp-13.2.0_20240530`, Docker digest
`5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a`.
LVGL9.2.2, esp_lcd_touch1.2.1, GT9111.1.3 и SDK-specific locks прежние.

## Независимая программная проверка

- Actual build validation, memory/compiler gates, ELF→app descriptor SHA,
  image checksum/appended SHA, размер, аппаратная совместимость и контекст PASS.
- Существующий локальный owner P-256 key подписал новые точные manifest bytes;
  private key не копировался и не передавался в Git/CI/browser.
- Python verifier и заново собранные C manifest policy/stream verifier на обоих
  SDK BIN подтвердили `installable=true`, источник, версию, размер и SHA-256.
  Изменённый manifest и неверный публичный ключ отвергнуты.
- Те же реальные inputs прошли C ASan/UBSan/LSan, leak detection включён.
  Это дополнение к существующей host/sanitizer матрице main, не её повторный запуск.
- Стендовая процедура: 12 методов с fault paths, 13 loader методов и 8 offline
  сравнений otadata с официальным pinned SDK PASS; UART/serial target в них нет.

Исходный CHECK-only context не изменён. Владелец отдельно одобрил новые
стендовые contexts `.1→.3` до25ноября2026 08:00 Asia/Almaty. Floors остаются
internal free64KiB/largest32KiB, PSRAM1MiB/largest128KiB, stack3072bytes;
unauthenticated SNTP разрешён явно, anchor≤10min/skew≤5s. Проверка сертификата,
hostname/date, тайм-ауты и signature authority не ослаблялись. Это не production
политика; bench labels bootloader `.1`/protocol1.0.0 не доказывают physical CAN.

## Публикация по отдельному разрешению

[v0.1.0-bench.3](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.3),
release ID456819407, **Pre-release**, без latest/stable. Tag заранее закреплён
на полном source SHA; GitHub `target_commitish=main` — metadata для уже
существующего tag, фактический tag object и видимый commit проверены отдельно.

Опубликованы 11 точных файлов: configured SDK6 app, manifest/signature, две
checksum inventories, release notes/README, build evidence, MIT license/notice
и third-party notices ZIP. Все 11 скачаны обратно через обычную HTTPS-проверку;
их размеры/SHA совпали с согласованным inventory. Скачанные manifest/signature
и app повторно проверены независимо. 157 оригинальных license/notice files
сверены с текущими pinned sources; ZIP содержит ещё четыре metadata files.

App SHA-256: `c015c025c739ec22937d7434c023686173330b40fe3164caab268f9b9e6d00e5`.
`release-checksums.sha256` SHA-256:
`cc4ef5ef326133d03bacf127d3845b68f66c83858dd9e6842c5fec1cb5aca9a4`.

Опубликованный bench.2 остаётся immutable CHECK-only. В release нет bootloader,
partition table, NVS, контекстов, бэкапов или STM32 binaries. Публикация этого
стендового payload не подтверждает сетевую установку, rollback, Flash-write
ресурсы, power/Wi-Fi-loss, production budget или физические FPS/latency.
Аппаратные измерения оформляются отдельно после закрытия сеанса.

## Предел воспроизводимости

Versions/toolchain/dependencies закреплены, но byte-for-byte reproducibility
этого bench не подтверждена: actual sdkconfig имеет
`CONFIG_APP_REPRODUCIBLE_BUILD` выключенным и `CONFIG_APP_COMPILE_TIME_DATE=y`.
Независимая повторная clean build/hash comparison не выполнялась. Опубликованные
assets неизменяемы; включение reproducible config и сравнение двух независимых
сборок обоих SDK относятся к следующему отдельному P2 после P1 runtime acceptance.
