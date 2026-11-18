# PR-01 — Architecture & Codex Rules: отчёт для review

Дата: 18 ноября 2026. Ветка: `docs/pr01-architecture-rules`.
База main: `323ab364bde301582f1b9ec8c8bc14e53396cc0c`.

## План и критерии приёмки

1. Сверить main, открытые PR, Actions, текущий код и аппаратный отчёт владельца.
2. Обновить корневые правила и четыре scoped AGENTS, архитектуру и roadmap.
3. Описать hardware status, OTA, release approval и открытые блокирующие решения.
4. Проверить ссылки, согласованность и строго документационный diff.
5. Создать отдельный PR и остановиться для проверки владельцем.

Приёмка: только Markdown, сохранение всех остальных файлов побайтово;
backup завершён, F407 получена, привод ожидается, CAN трансиверов нет;
будущие OTA/motion/SD не объявлены реализованными. Нет подключения к платам,
прошивки, релиза, секретов, смены GPIO/Flash settings или автоматического merge.

## Проверенный снимок GitHub

| Ветка / SHA | Docs | Host / sanitizers | ESP-IDF 6.1.1/5.3.2 × headless/rev12 |
|---|---|---|---|
| main `323ab364` | [success](https://github.com/avlyubimov/TV4-CNC/actions/runs/43960949299) | [success](https://github.com/avlyubimov/TV4-CNC/actions/runs/43960949184) | [все 4 success](https://github.com/avlyubimov/TV4-CNC/actions/runs/43960949071) |
| Открытый PR #5 `b92c559c` | [success](https://github.com/avlyubimov/TV4-CNC/actions/runs/44322100475) | [success](https://github.com/avlyubimov/TV4-CNC/actions/runs/44322100321) | [все 4 success](https://github.com/avlyubimov/TV4-CNC/actions/runs/44322100795) |

Это аудит существующих Actions, а не новая локальная сборка PR-01.
PR #5 не слит и не скопирован в эту ветку. Публичных Releases на момент аудита нет.
Новые push/PR проверки отображаются в Actions/Checks соответствующего коммита;
результаты этого снимка не подменяют их.

## Затронутые файлы

- Правила: `AGENTS.md`, `firmware/esp32-hmi/AGENTS.md`,
  `firmware/stm32-motion/AGENTS.md`, `firmware/bluepill-test/AGENTS.md`,
  `protocol/can/AGENTS.md`.
- Новые проекты/статус: `docs/HARDWARE_STATUS.md`, `docs/OTA_ARCHITECTURE.md`,
  `docs/RELEASE_PROCESS.md` и этот отчёт.
- Актуализация: `README.md`, `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`,
  `docs/IMPLEMENTATION.md`, `docs/OPEN_QUESTIONS.md`,
  `docs/RUNTIME_DIAGNOSTICS.md`, `docs/VALIDATION.md`, `docs/BACKUP_STATUS.md`,
  `docs/BOARD_ADAPTER.md`, `docs/BOARD_VERIFICATION.md`, `docs/CODEX_TASK_001.md`,
  `docs/IDF_611_COMPATIBILITY.md`,
  README трёх firmware модулей и `protocol/can/README.md`.

## Проверки PR-01

Проверяются расширения всех изменённых/новых файлов, Git blob hashes всех
не-Markdown файлов относительно указанной базы, относительные Markdown ссылки,
арифметика/непересечение/выравнивание предложенной карты 16 МиБ и `git diff --check`.
Локальные результаты: 25 изменённых/новых Markdown-файлов; 138 остальных
Git blobs побайтово совпадают с базой; 75 относительных ссылок разрешаются.
10 областей предложенной карты непрерывны, не пересекаются, заканчиваются
на 16 777 216 байтах; app offsets выровнены на 64 КиБ. `git diff --check` — PASS.
Обязательные документы из существующего docs workflow присутствуют и непусты.

Firmware/host tests локально повторно не запускались: PR меняет только документы.
Текущие workflows сохранены; новые source/build/partition/config файлы отсутствуют.
Предложенная OTA карта проверяется как проект, не как собранная таблица разделов.
Аппаратные измерения, restore, OTA, CAN, microSD и motion в PR-01 не выполнялись.

## Ограничения, риски и откат

- Точные MCU/Flash/RAM/сектора/HSE/pinout F407 неизвестны: target linker/startup
  и CAN Flash writer нельзя утверждать по generic названию платы.
- OTA layout/signature envelope/trust provisioning — предложения для review;
  миграция только отдельным USB этапом. Никаких ключей или eFuse сейчас.
- Фото подтверждает изображение HMI, не полный GT911/performance/Wi-Fi тест.
  Код исправления 263 и крупной клавиатуры находится в отдельном PR #5.
- Пересечение документации с PR #5 потребует согласования при merge, без потери
  его изменений; результаты hardware/CI необходимо обновлять с точным SHA.
- Откат: revert документационного коммита. Устройство и Flash не изменяются.

После создания PR-01 работа остановлена для owner review. Следующие PR,
merge, прошивка и публикация не входят в это задание.
