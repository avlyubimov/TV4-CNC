# P1: локальное доверие до подтверждения pending OTA-образа

Первый полный signed read-only CHECK на a29c9ed прошёл; это ещё не INSTALL.
Опубликованный bench.2 — unprovisioned payload только для CHECK, устанавливать
его нельзя. До настоящей установки нужен образ, который сохраняет локальный
доверенный ключ и настройки после загрузки.

Обнаружен конкретный дефект self-test: service передавал healthy=true в native
finish_boot до проверки configured(). Поэтому pending образ без ключа мог
стать VALID, а затем перейти в SETUP. Новый тест с настоящим service.c и
fault-injecting SDK воспроизводит ошибку старого кода; это не аппаратный результат.

Исправление в существующем worker: один раз проверить configured() до native
подтверждения. Для OTA partition передать разрешение только при здоровом UI,
PSRAM/layout и рабочем local context. Native esp_ota_ops по-прежнему управляет
PENDING_VERIFY/VALID/rollback; новых Flash APIs, задач, буферов или криптографии
нет. Тот же результат context используется при публикации IDLE/SETUP, повторный
разбор ключа не добавляется. Factory recovery и ранее VALID образ остаются
доступны без настроенного OTA. Доступность AP/DHCP/SNTP не требуется для boot.

Программная приёмка: pending без provisioning/ключа, повреждённый ключ,
несовпадающий/отсутствующий descriptor, неверная time/resource policy и отсутствие
running partition отвергаются. Valid context допускается без сетевых вызовов.
Factory и ранее VALID без context сохраняются; PSRAM/health/task failures и
прежние default-off read-only bench gates проверяются. SDK boundary теперь
точно сохраняет native factory/VALID semantics; сами native SDK функции отдельно
покрыты существующим installer fault suite.265 host/265 ASan/UBSan/LSan PASS.
CI добавляет анализ actual service с тремя local-policy вариантами. Далее —
реальные configured/unprovisioned ELF/BIN6.1.1/5.3.2 и полный source CI.

Следующий измеримый аппаратный результат — native boot confirmation и отказ
непригодного pending образа. Конечный отдельно запечатанный стенд может через
USB подготовить локальные app/otadata fixtures, подтвердить configured pending
и rollback unprovisioned pending, затем вернуть исходные factory/otadata/оба
OTA slots из свежего снимка. Это не сетевой installer и не установка published
bench.2. Перед выполнением обязательны точные hashes, оба SDK/CI, проверка
формата metadata по pinned официальному SDK, свежие16MiB/device MD5/SHA/two-NVMe,
allowlist изменяемых разделов, strict UART/otadata assertions и тесты отказов
самой процедуры. Начальная действующая A/B-разметка и bootloader сохраняются.
Один сеанс≤1800s с recovery reserve до08:00 Asia/Almaty, один conditional full
restore при доступном USB; любой отказ отменяет остальные пробы. Действует
явное разрешение владельца на автономные обратимые проверки подключённых плат.
Без eFuse/option bytes/CAN/приводов/потери питания, публикации и self-merge.

Hardware PASS этого изменения пока отсутствует. Native slot tests отдельно
проверяют bootloader/confirmation; настоящие HTTPS INSTALL/Flash peaks/отказ
Wi-Fi и питания требуют configured signed installable payload и отдельного
результата. Не выдавать USB slot fixture за скачанную OTA-установку.
