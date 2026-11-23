# P1: один автоматический read-only CHECK на стенде

Владелец поручил самостоятельно проверять подключённые ESP32 и F407 до08:00
Asia/Almaty, без ожидания нажатий. Не расширять это на движение, необратимые
защитные настройки, merge или новые публикации. База — слитый владельцем PR79,
mainc3b82f0; измеренные HTTP header/request-line несовместимости исправлены.

Одна интеграция: default-off TV4_OTA_SIGNED_CHECK_BENCH только configured
rev12-ota+trace, с локальным public-only trust. В том же worker после self-test
ждать здоровое подключение Wi-Fi максимум45s и его устойчивость2s, затем ровно
один operation(false). Никакого Flash sink, INSTALL/retry/extra task. API CHECK/
INSTALL/REBOOT в таком image закрыт, отмена сохраняется. Неверный trust или
Wi-Fi/health timeout закрывает тест. Ordinary/unprovisioned режимы прежние.

Приёмка: actual worker SDK doubles для success/offline/unhealthy/memory/SNTP/
session/cancel failures, отказ установки/повторного CHECK, build/source/ELF
guards, host/sanitizers/оба pinned SDK и CI. Новый exact пакет содержит control
с flag0 и автоматический CHECK с flag1: fresh16MiB/two-NVMe → control PASS →
app-only CHECK/verify/UART → возврат всего factory fecc05d. Все прежние trust/
CA/hostname/date/time/resource/size/timeout/monitor gates сохраняются. Последний
допустимый старт каждого1800s сеанса учитывает reserve до разрешённого08:00.
Physical UI после возврата владельцем не наблюдается, это отдельное ограничение.

Обычный operator CHECK сохраняет metadata-only запрос. Только opt-in стенд
задаёт `verify_image=true` существующему guarded read-only session runner,
поэтому PASS требует IMAGE_VERIFIED и SHA-256 всего BIN, а не CANDIDATE_READY.

Программные проверки перед реальными SDK: 231/231 host, actual service SDK
variants0–5, fault cases,9 build/compile guard methods и GCC `-fanalyzer` PASS.
Host session/SNTP wrappers проверяют orchestration/gates, не имитируют аппаратное
доказательство TLS/криптографии; независимые existing crypto/session tests остаются.
Локальные owner-configured control/auto ELF обоих SDK и runtime stack/resource
результаты будут записаны отдельно после выполнения, не объявляются заранее.
