# TV4-CNC — правила разработки Codex

Правила действуют на весь репозиторий. Вложенные AGENTS.md уточняют область,
но не ослабляют безопасность. Продолжать существующий проект, не создавать
новый каркас вместо работающей реализации.

## Перед изменениями

Прочитать README.md, docs/ARCHITECTURE.md, docs/ROADMAP.md,
docs/IMPLEMENTATION.md, docs/HARDWARE_STATUS.md, docs/OPEN_QUESTIONS.md,
docs/BOARD_ADAPTER.md, docs/RUNTIME_DIAGNOSTICS.md, docs/OTA_ARCHITECTURE.md,
docs/RELEASE_PROCESS.md и правила изменяемого модуля. Проверить main, открытые
PR и Actions; не копировать код уже открытого PR. Данные о наличии оборудования
брать из HARDWARE_STATUS, а не из старых планов или имени USB-устройства.

## Архитектура

- ESP32-S3: Waveshare ESP32-S3-Touch-LCD-7 Rev 1.2, 800×480, LVGL 9,
  ESP-IDF 6.1.1; 5.3.2 — проверяемый резерв. Flash 16 МБ, PSRAM 8 МБ
  согласно текущему аппаратному статусу. HMI, Wi-Fi, будущие microSD и OTA,
  CAN-коммуникации; **никакой генерации STEP на ESP32**.
- STM32F407: DevEBox STM32F4VX_M Ver. 2.0; владелец сообщил STM32F407VGT6;
  owner SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3,252V/
  backup1048576bytes/vector structure PASS подтверждены. Package/RAM/CCM/секторы/
  кварц/распиновка/physical GPIO/CAN ещё не проверены. Только он отвечает за планирование, таймерный STEP/DIR,
  энкодеры, homing, watchdog и состояние реальной машины после commissioning.
- Blue Pill STM32F103C8T6: отдельный стенд протокола/таймеров/загрузчиков;
  не замена F407 и не контроллер станка. Подтверждение владельца: Flash 64 КиБ.
- CAN — не аппаратная безопасность. Реальные GPIO, CAN ID и bitrate назначать
  только отдельным документированным решением с проверкой ревизии/трансиверов.
  Существующий 4-байтный heartbeat и 500 мс — только host-заглушка.

## Неослабляемая безопасность

1. Нормально-замкнутый физический E-stop и независимое снятие опасного
   момента/питания через проверенные контактор/STO. UI STOP не заменяет его.
2. Механическая и электрическая блокировка ручного/CNC переключения;
   конкурирующее включение приводов недопустимо. Сохранить ручной режим.
3. Загрузка, сброс, переподключение, потеря CAN, watchdog, ошибка датчика и OTA
   не запускают оси/шпиндель. Движение по умолчанию запрещено. ACK не является
   ARM/START и не возобновляет движение автоматически.
4. Не активировать приводы до отдельного разрешения на commissioning,
   проверки механики, направлений, концевиков, homing, soft limits, полярности
   ENABLE и аппаратной цепи. Не угадывать «безопасный» уровень GPIO.
5. Не обходить E-stop, ограждения, концевики или блокировки. Силовые цепи,
   заземление, контакторы/STO/VFD требуют квалифицированной аппаратной проверки.
6. Резьба, C-axis и фрезерная приставка — проектные задачи до измерения
   энкодера, люфта, механики, удержания и останова. Компенсация не устраняет люфт.
7. Испытание симуляции не доказывает безопасность или готовность станка.
   Различать SIMULATION, нет связи, ошибка, готовность HMI и готовность машины.
8. Чтение/запись реальных устройств, прошивка, стирание и тесты приводов
   требуют отдельного разрешения владельца на соответствующую операцию.
   Не включать Secure Boot, Flash Encryption, eFuse или STM32 option bytes
   без отдельного разрешения; не выполнять необратимые настройки по умолчанию.

## ПО и интерфейс

- Только владелец LVGL меняет UI; очереди/snapshots передают намерения/данные.
  В callbacks нет блокирующего I/O, файловых операций, ожиданий CAN/сети,
  задержек или тяжёлой обработки. Worker-задачи имеют явные лимиты/тайм-ауты.
- Предпочитать статическое/ограниченное выделение, bounded queues и предсказуемую
  память. Не вводить неограниченную аллокацию в частые пути.
- STEP формируется аппаратными таймерами/DMA/IRQ STM32, не RTOS-задержками.
  Частоты planner, jitter и производительность подтверждать измерениями.
- Явные единицы: мм, мм/мин, мм/об, об/мин, градусы; X radius/diameter,
  машинный/рабочий отсчёт и направления. Геометрию станка не придумывать.
- UI MVP только ru-RU, строки через стабильные ID и fallback на русский.
  kk-KZ/en-US резервируются архитектурно, неполных переводов/переключателя нет.
  Протокольные токены, X/Z/CAN/SSID и технические логи могут быть английскими.
- Шрифты с Ё/ё, разрешённые лицензии, проверка глифов/клиппинга/касания на 800×480.
  Тёмный читаемый UI, контраст и удобные сенсорные цели; отзывчивость как цель,
  не копирование iOS. Не коммитить чужие закрытые бинарники/шрифты.
- Цели: touch feedback <50 мс, 30+ FPS где достижимо, координаты 20–30 Гц,
  отсутствие зависаний при потере связи. Не объявлять их достигнутыми без замеров.
  LVGL refresh rate не равен RGB scanout; event latency не touch-to-display.
- Не добавлять web stack или удалённое управление станком без отдельного задания.

## OTA и релизы — обязательная будущая функциональность

- GitHub Releases → HTTPS → ESP32 OTA Manager; отдельно ESP32 OTA A/B и
  ESP32 → CAN → независимый загрузчик F407. Пока это проект, а не готовый OTA.
- Подписанный манифест, доверенный локальный публичный ключ, SHA-256, hardware/
  protocol compatibility, ограниченные размеры, штатный rollback ESP-IDF.
  Не доверять одному SHA, TLS или имени релиза вместо подписи.
- Разметку мигрировать только отдельной USB-процедурой с бэкапом/проверкой,
  не обычным app OTA. Текущий factory 2 МиБ остаётся до отдельного PR миграции.
- Для STM32 нужны подтверждённая геометрия Flash и состояние без движения,
  шпинделя и программы. Не выбирать адреса/сектора/A-B по предположению.
- Автопроверка разрешена; установка требует явного подтверждения оператора
  для конкретного образа. Сброс/потеря питания не разрешают движение.
- Не публиковать релиз без успешного CI и отдельного явного разрешения
  владельца на конкретный релиз. Push в main не является разрешением.
  Не публиковать фиктивную STM32-прошивку. Приватные ключи/пароли/Flash dumps
  не попадают в Git, логи, PR и CI-артефакты.

## Процесс

1. Отдельная ветка, короткий план и критерии приёмки, небольшой независимый PR.
2. Фиксировать точные toolchain/dependency версии и лицензии; воспроизводимые
   команды и реальные результаты. Не менять подтверждённые GPIO без проверки.
3. Прогонять соответствующие host/fault/timeout/layout тесты и матрицу сборок
   для изменений кода. Для docs-only проверять ссылки, согласованность и scope;
   не объявлять новую сборку, если её не выполняли.
4. PR описывает проблему/результат, файлы, критерии, проверки, ограничения,
   аппаратные предположения, риски и откат. Измерения или отметка «не измерено».
5. Не сливать собственный PR без явного разрешения владельца. Не создавать
   теги, релизы, секреты CI и публикации под видом обычного push.
6. Обновлять актуальный статус; исторические отчёты сохранять с явной пометкой.

## Текущее задание

PR #1–42 слиты, база main `4db99df`, main CI5/5 PASS,
открытых PR перед работой нет.
PR-07k: читать STM32_UPDATE_FRAGMENTS.md и protocol/can/AGENTS.md. Только
opt-in single-owner bounded envelope/reassembly:8-byte maximum, owned286byte
TX/RX buffers, explicit local route/generation, retained counters без wrap,
peek/exact local-queue commit, no silent RX restart, single take после codec.
READY_BYTES_UNCHECKED не verified/auth/install authority; unchanged logical
CRC/session/independent crypto/lease/model/deadlines обязательны. Poll без
traffic, duplicates/peek/reject/BUSY не продлевают сроки. Context/clock/exhaustion
sticky; clear/reinit не сбрасывают counters или lifetime. Local identity не
передаётся на wire и не обеспечивает replay isolation после reset. Production
routing/authentication/IDs/bitrate/metadata-signature transport/runtime/installer
ещё отсутствуют. No-I/O/board/output/hardware gates прежние, устройств нет.
Host45/sanitizers45/standalone19, independent envelope oracle3,263 roundtrips,
DLC256/START32768/offset65536,100000 mutation/drop chains и8 signed RAM chains.
Analyzer13 embedded+1 host; ARM fragments1536text/data0/bss0/frame48 отдельно,
4096text/frame128 ceiling +6 archive guard methods; old archives/codec прежние.
Эти цифры не full stack/MCU RAM/CPU/physical throughput approval.

Слитый PR #42, PR-07j: читать STM32_UPDATE_WIRE.md. Только отдельный
opt-in byte codec draft: existing8 message kinds, exact bounded LE framing,
explicit enum tokens, failure output zeroing. Не отправлять C memory layout/
padding/tails. Shared negotiation shape helper не physical/session/auth gate;
CRC проверяет прежний RX, independent crypto/lease/model/deadlines прежние.
Codec OK не dispatch/install/COMMIT/jump/ARM authority. Нет production CAN
version/IDs/bitrate/fragmentation/metadata transfer/runtime caller или устройств.
Host42/sanitizers42/standalone16, golden12/65536 tokens/200000 mutations и
8 signed RAM chains; strict analyzer12 embedded+1 host, wire budget guard6.
ARM4 libraries43362text/data8/bss27 (+18text), отдельный codec1219text/data0/
bss0/frame32; это не full stack/RAM/performance approval. Existing budgets
не расширять, negative budget/object/import/frame checks сохранять.

Слитый PR #41, PR-05d: читать RELEASE_BUILD_BINDING.md при изменении audit. Связать
unsigned CI candidate с тем же signed cohort/full HMI bytes/ELF digest через
explicit external candidate SHA pin. Pin не CI/source authentication; config/
partition/tree/license hashes остаются unsigned claims. Проверенные fields
возвращать из того же snapshot, не перечитывать paths после проверки.
Старые build/cohort CLI reports прежние. Новый audit всегда BLOCKED/exit3;
layout/profile mismatch, missing Motion build evidence и все authority gates
открыто показывать. Нельзя менять factory/PROJECT_VER/SDK/profile ради PASS,
превращать metadata-only report в manifest/installer/approval или добавлять keys.
CI сохраняет matrix8, пишет self-generated integrity pin каждого candidate;
это не owner approval. Host39/sanitizers39/standalone13 и прежние ARM/static/
source/scope/board/output gates; firmware footprint/config/dependencies прежние.

Слитый PR-05c: читать RELEASE_COHORT_REVIEW.md при изменении signed cohort.
Offline tool проверяет existing signed HMI/Motion formats/full images, explicit
local selection/source/channel, ESP app descriptor и prospective HMI peer range.
Motion frontend вызывает прежний independent Mbed verifier, только HOST; нет
нового firmware parser/crypto owner/clock/resource defaults/runtime caller.
Legacy HMI verify/CLI report прежний; signed metadata возвращается из того же
snapshot, что прошёл signature/image checks. Не перечитывать replaced manifest
для binding и не принимать generic success boolean вместо crypto.

Local context/key/executable paths — explicit trusted operator inputs, не поля
скачанного JSON. Bounded regular snapshots; quiescent owned FS/toolchain, не
sandbox hostile executable. Test private keys только RAM (два разных role keys),
никаких private files/production signer/provisioning. Review JSON metadata-only,
не OTA manifest/bundle/CAN ABI или install token. HMI-only без fake Motion record.
Все authority gates закрыты; protocol SemVer и numeric counters НЕ переводить.
Source signer claim и driver SHA не build/code attestation. Реальные versions/
keys/context/protocol mapping/production bundle/лицензия/publish ещё открыты.

Host/sanitizers38 suites, standalone13, ARM/static8 embedded+1 host/source/
board/output/scope gates; прежняя ESP SDK matrix8. ARM libraries43344text/data8/
bss27/frames984,376,328,64 прежние. Host I/O frontend не входит в ARM targets.
Firmware C/UI/board/config/partition/security/dependency pins прежние. Не менять
PROJECT_VER ради review PASS; current git version не trusted release SemVer,
factory2MiB не A/B. Runtime budget/worker/installer и production context открыты.

PR-07i RAM source — один owner, exclusive caller buffer, no init/discard reset
counters/high-water, no writes/discard while leased. API не DMA/MPU protection,
volatile не durable recovery; Flash1MiB не RAM1MiB. Quarantine держит handle,
DATA deadlines не продлевать. Receipt/history не install/COMMIT/jump/ARM token.
Owner F407 SWD/core/rawID/Flash1MiB/3,252V/backup/vector facts прежние; VGT6 не
full marking/HSE/RAM/CCM/IRQ/erase/protection/GPIO/CAN/restore proof. BOARD_IMAGE=ON
fail, synthetic startup не actual profile, no-I/O inhibit=false/watchdog unavailable
не physical safe. Не читать backup/dump и не повторять device probes.
На ESP32 ordinary8f0d899, owner HMI/touch/Wi-Fi PASS; разрешения исполнены.
Нет devices/SWD/UART/reset/Flash/network probes, keys/releases/eFuse/CAN/motion,
installer/boot activation или самостоятельного merge.
