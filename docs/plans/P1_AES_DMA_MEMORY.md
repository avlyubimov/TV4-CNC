# P1: устранить измеренный AES/DMA memory blocker

Основание — [стенд3927063](../evidence/hardware/2026-11-22-esp32-worker-tls-3927063/README.md).
Touch control PASS, SNTP PASS; первый HEAD остановлен после SDK hardware-AES
allocation failure. Factory fecc05d возвращён/verify/UART/owner PASS, сеанс
закрыт. PR #71/#72 слиты; не повторять их задачи или использованное разрешение.

## Что известно и чего пока не хватает

Проверенный SDK6.1.1 checkout `051e4c4eb20346e043cdd47bc388fde75ff8af63`:
`components/mbedtls/port/aes/dma/esp_aes_dma_core.c` содержит несколько мест с
наблюдаемым `Failed to allocate memory`. В том числе DMA bounce buffers и
descriptor arrays; один текст ошибки не выбирает конкретный site. В SDK S3
bounce chunk ограничен1600bytes, но **размер именно failed request неизвестен**.
`tls_error132` — generic PSA error; одного кода недостаточно для классификации.

Internal8BIT free/largest не равен heap с требуемыми DMA/alignment caps. Current
snapshots не дают transient largest DMA block в момент отказа. Min2571/history2408
и остаток стека3792из8192 показывают давление памяти, не требуемый reserve/floor
или возможность безопасно уменьшить stack. Изменение clock-before-snapshot
здесь не повторять: его control и TLS touch samples2–19ms прошли без ошибок.

## Следующая одна программная интеграция

Критерий результата — реальный diagnostic ELF/BIN двух SDK с достаточными
bounded числовыми данными для определения failed allocation, не новая модель
OTA/криптографии и не слепой повтор того же стенда.

1. В existing default-off diagnostics добавить capability-specific DMA free/
   largest в фазах HTTP init/open/read/cleanup. Зафиксировать размер/caps/count
   отказа allocator безопасным failure-only SDK hook, если его ownership и
   ISR/cache/heap-lock contract подтверждены на обоих SDK. Hook не выделяет
   память, не логирует/блокирует и не перезаписывает чужого владельца. Emit
   после выхода из callback через существующий worker; никаких credentials.
2. Сохранить native HTTP error и отдельный ESP-TLS layer error при наличии
   API. Positive132 не печатать как errno. Старый SDK event/error path сохранить;
   недоступные сведения явно unknown, не zero/success. Не добавлять fallback.
3. Проверить actual sources: fault injection обеих SDK API, callback ownership/
   failure paths, sanitizers/static analysis, default-off footprint, bounded
   frames и единственный serialized SDK min-monitor owner. Сохранить ELF/BIN/
   SHA/config/locks/partition/source evidence и CI. Никаких ключей или floors.
4. По этим данным выбрать конкретную экономию или размещение **подходящих**
   не-DMA объектов в PSRAM с проверкой lifetime/cache/Flash-write restrictions.
   Не менять глобальную allocator policy, stack location или task stack sizes
   по одному watermark. Сохранить hardware AES, CA/hostname/date, TLS RX16384/
   TX4096, DMA reserve64KiB, framebuffer/pins/clocks/polling/тайм-ауты.
   Производительность и ресурсный эффект подтвердить измерением, не предполагать.
5. Новый exact-image пакет после review/CI и **нового** разрешения: fresh16MiB/
   two-NVMe, control touch_fresh, TLS только PASS, остановка при отказе, возврат
   всего factory fecc05d, ограниченное recovery. Старый пакет не запускать.

## Последующие gates остаются закрытыми

После verified HTTPS — подготовить настоящую signed CHECK SESSION4 в existing
configured worker8192, без нового crypto/HTTP слоя. Для этого нужны owner local
P-256 public key/key ID, reviewed SemVer/channel/hardware/protocol/time/resources
и настоящий подписанный кандидат с источником/размером/SHA. Existing URL gate
принимает TV4 GitHub Releases и ограниченные CDN redirects; не обходить его
file URL, test key, unsigned metadata или произвольным сервером. Первое реальное
release asset требует отдельного решения/публикации владельцем; этого разрешения
пока нет. Новая программа не публикует asset автоматически.

Для resource CHECK использовать `rev12-ota` + memory diagnostics и reviewed
local context; worker8192 выбирается provisioning, не stack/TLS bench override.
Измерить WORKSPACE2/SNTP3/SESSION4/CLEANUP5, verify/SHA/size/compatibility,
callback buffers, stack и cleanup при живых LVGL/GT911/Wi-Fi. Запись inactive
slot не относится к CHECK и согласуется отдельно. Затем install/self-test/
confirm/rollback, Wi-Fi/power-loss/recovery и P2 stable/prerelease acceptance.
Без owner settings CHECK/INSTALL остаются SETUP. eFuse/F407/CAN/motion закрыты.
