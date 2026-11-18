# План последовательной разработки TV4-CNC

Актуально на 18 ноября 2026; состояние main после слияния PR #5 —
[IMPLEMENTATION.md](IMPLEMENTATION.md). Рабочий HMI не переписывать заново.
Все новые этапы — отдельные ветки/небольшие PR, merge только с разрешения
владельца. PR-01 слит; по заданию «дальше» выполняется PR-02. Таблица не является разрешением
на автоматическое выполнение/прошивку всех следующих этапов.

| PR | Содержание | Приёмка / зависимости | Статус |
|---|---|---|---|
| PR-01 Architecture & Codex Rules | Root/scoped AGENTS, hardware status, OTA/release design, open questions, актуализация документов | Docs-only diff; нет firmware/settings/partition changes; отдельный PR и отчёт | Завершён, PR #6 слит |
| PR-02 STM32 Firmware Foundation | CMake/Ninja/точный ARM GCC, CMSIS/HAL/LL, startup/linker, clock/GPIO/timer/DMA/watchdog abstractions, faults/state machine и host tests | Target/startup/linker только после идентификации MCU, Flash/RAM/кварца/секторов; иначе host foundation + явный blocker. Никаких приводов. CI size/static analysis без SWD | Ядро/host tests/ARM compile-only в PR-02; board image блокирован HSE/pinout/memory verification |
| PR-03 ESP32 Performance Diagnostics | Дополнить существующие метрики, reset/watchdog/reconnect/queue counters, CPU/PSRAM/stacks, export | Не дублировать dashboard/main logs. Измерить физическую touch-to-display отдельно, собрать baseline и soak/fault results | Частично есть instrumentation; аппаратных замеров нет |
| PR-04 ESP32 OTA Foundation | Плановая USB-миграция, otadata, A/B, HTTPS, подпись/хеш/compatibility, self-test и rollback | Проверенный backup, trusted key provisioning, migration/recovery review; power-loss/bad-signature/wrong-hardware tests. Разметку не менять app OTA. Firmware install только отдельно разрешён | Не реализован |
| PR-05 GitHub Release Pipeline | Build matrix, versioned artifacts, signed manifest, checksums, release notes, prerelease/stable | Публикация только после CI и явного owner approval на SHA/tag/канал/артефакты; secrets/protected environment design; никакой фиктивной STM32 bin | Не реализован; обычный build CI уже есть |
| PR-06 microSD Support | FAT32, detect/mount/read/write/free-space/eject/error handling; G-code files, logs/settings export, OTA cache | Подтверждённая разводка Rev 1.2 и схема USB/CAN/SD; отсутствие/извлечение/повреждение не блокирует HMI; file I/O только worker | Не реализован |
| PR-07 STM32 CAN Bootloader | Независимый bootloader, sequence/chunks/ACK/retry, SHA/signature, hardware/protocol checks, recovery | Flash geometry и A/B либо staging/recovery решение; сначала host transport. SWD initial install и CAN hardware только по отдельному разрешению | Не реализован |
| PR-08 Unified Firmware Update UI | Параметры → Обновления: версии HMI/Motion/protocol, compatibility, changelog, размер, progress/result/history, check/download/install/cancel | Worker APIs PR-04/05/07; auto-check можно, auto-install нельзя. STM32 install только после подтверждённого quiescent state | Не реализован |

PR-04 не обязан ждать физического motion: ESP32 обновляется на изолированном
стенде. Но неподтверждённая механика никогда не компенсируется OTA/CI. PR-07
может проектировать и тестировать transport в host до получения трансиверов;
реальное распределение CAN ID/bitrate и GPIO требует отдельного решения.
UI-каркас обновлений допустим в своём PR, но не должен изображать несуществующие
версии motion, проверенные подписи или завершённую установку.

## Сохранённые инженерные milestones

- M0: база документов/backup. ESP32 backup завершён; Blue Pill backup сообщён;
  restore обоих, идентификация F407 и аппаратные unknowns остаются открыты.
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
