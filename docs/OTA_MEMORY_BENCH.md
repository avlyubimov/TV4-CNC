# PR-04g — отдельный PSRAM allocation bench

PR #15 дал PSRAM-only allocator, но он ещё не запускался на плате.
Этот PR готовит отдельный **rev12-memory-bench** image для следующего
разрешённого испытания. Плата сейчас остаётся на b93cf3b; запись/reset/serial
в этом PR не выполняются. Это не production TLS budget или готовый OTA Manager.

## Что делает профиль

Kconfig TV4_OTA_MEMORY_BENCH default n, зависит от Rev1.2/PSRAM. Только новый
sdkconfig.memory-bench overlay включает его. build_idf.sh/validator различают
три профиля и отклоняют bench flag в обычном headless/rev12 или отсутствие
flag в bench image. Existing sdkconfig.defaults/rev12/layout не меняются.

Через 15 секунд после UI ready owner один раз пытается создать worker
priority 1, stack 4096 bytes. До этого stack не удерживается во время startup.
UI owner не делает buffer pattern/malloc workspace/serial I/O из callbacks;
он только schedules task. Если internal free <24576 либо largest <8192,
или task creation fails, probe не запускается, HMI продолжает работать.
Это частный opt-in bench guard, не гарантированный production reserve.

Worker выполняет максимум три цикла:

1. Heap/stack snapshot before.
2. PSRAM-only acquire: sizeof(session)=34000 на текущем target. Для этой
   allocation-only пробы floors internal free 16384, largest 8192, spare PSRAM
   1MiB, reviewed=true относится только к исходникам этой явной пробы без TLS.
   Эти числа запрещено использовать как production TLS policy.
3. Volatile 0xA5/0x5A write/read/clear metadata manifest/envelope/HTTP chunk
   buffers (22528 bytes). Control/hash/candidate fields не затрагиваются.
   Это CPU/cache-visible probe, не весь PSRAM memtest, не проверка DMA/cache-off.
4. Release/reset session, snapshot after. Пауза 250ms между циклами; при первом
   отказе остановиться. Hash contexts/network/session_run не открываются.
5. completed/pass summary и task self-delete. Повторный start в этом boot
   отклоняется; каждый новый boot bench image снова допускает одну пробу.

Никаких HTTPS, discovery, Flash writes, boot selection, GPIO/CAN/motion или
production keys. Существующий Wi-Fi worker Rev1.2 продолжает обычную работу.

## Логи и интерпретация

Tag tv4_memory_bench. Phases before/allocated/pattern/released/acquire_failed;
result — enum memory.h, pattern_ok=-1 до проверки, затем 0/1. us — время acquire,
pattern либо release в соответствующей фазе, без длительности printf. Snapshot
включает internal free/min/largest, PSRAM free/largest и worker watermark.
Нельзя объявлять leak по неравенству heap до/после при параллельном Wi-Fi/UI.
Числа admission/size не доказывают успешную операцию: нужен completed=3 pass=1,
allocation region checks, pattern_ok=1, release OK и отсутствие новых ошибок.

## Сборка и приёмка без устройства

TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12-memory-bench 6.1.1 local

CI собирает headless/rev12/rev12-memory-bench на SDK 6.1.1/5.3.2. Проверяет flag,
linked bench symbols только в bench image, target sizeof, bounds/Flash/PSRAM.
Host tests: normal-vs-bench gate, helper lifetime guards, adjacent field canaries,
cleared buffers и прежние allocation/OTA/UI tests. [Evidence](evidence/build/ota-memory-bench/README.md).
Компиляция не является аппаратным PASS; real allocation/latency/stacks ещё UNKNOWN.

## Следующая проверка на плате — отдельное разрешение

После review/успешного CI подготовить точный SHA и artifact hashes для profile
rev12-memory-bench/SDK6.1.1; повторно проверить backup. Согласовать запись,
reset и UART capture на стенде только USB/питание, без CAN/исполнителей.
Прежнее разрешение на b93cf3b эту новую сборку не покрывает.

UART open/close прежде сопровождался reset. Не обещать read-only/no-reset:
открывать порт один раз в согласованной процедуре, записывать startup и минимум
45 секунд после последнего boot; проверить единую последовательность uptime.
Не объединять разные boot в непрерывный soak. Изменение линий/драйвера отдельно
не проверено; случайный reboot не считать автоматически WDT или memory fault.

Критерии: правильный bench SHA/profile, HMI/touch/Wi-Fi продолжают работать,
completed=3 pass=1, pattern/release OK, разумный watermark и отсутствие panic/
allocation/display/touch/queue errors. Сохранить numeric/allowlisted logs без
SSID/IP/MAC/паролей. При отказе не запускать TLS/installer, сохранить причину.

Возврат к обычному rev12 — отдельная согласованная запись. Production defaults
всегда probe off; наличие кода не разрешает auto-flash. Откат PR — revert кода,
само устройство это не меняет. Затем — TLS memory profiling/time provisioning,
remaining soak/physical latency/UART procedure и последующие OTA stages.
