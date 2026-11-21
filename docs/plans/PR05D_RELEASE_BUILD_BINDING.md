# PR-05d — связь build candidate и signed cohort

База main `aa068336635ab2e3b9c83d67a17ee71d8a821dbb`, PR #1–40 слиты,
открытых PR нет. Проверить итоговый main CI перед публикацией новой ветки.

1. Сохранить existing metadata-only build candidate и signed HMI/Motion formats.
   Добавить explicit external SHA-256 pin candidate.json и возврат captured
   verified metadata; старые CLI/JSON reports остаются совместимыми.
2. Новый host-only offline audit сопоставляет source/SDK/profile, весь HMI image,
   descriptor и локальный ELF digest с тем же signed cohort. Использовать bounded
   regular private snapshots, не перечитывать verified inputs для binding.
3. Factory versus signed A/B layout и headless/bench versus signed rev12 явно
   показать как blockers. Не исправлять layout/PROJECT_VER ради положительного
   отчёта. Pin unsigned metadata не authentication/CI attestation.
4. Любой сформированный audit остаётся blocked/review-only; нет success/install/
   publish authority. Paired cohort не получает выдуманную Motion build provenance.
5. Host/fault/CLI/snapshot tests с двумя ephemeral RAM-only role keys; default
   sanitizers, standalone и прежние ARM/source/scope gates. CI matrix8 и exact
   dependencies сохраняются, candidate pin проверяется во всех SDK jobs.
6. Evidence/документация/статусы, отдельный PR, дождаться CI точного head.
   Нет devices/SWD/UART/reset/Flash/backup reread/CAN/motion/release/self-merge.

Приёмка: mismatch pin/source/image/ELF/SDK/metadata отвергается без частичного
stdout. Valid synthetic inputs дают metadata-only blocked report, exit3 и все
authority gates false; factory не становится A/B. Firmware footprint неизменен.
