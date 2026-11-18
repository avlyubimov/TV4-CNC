# PR-04i — ограниченная SNTP-синхронизация

После PR #18 добавлен источник для time gate: `tv4_sntp_run` — переносимый
lifecycle, `tv4_sntp_sync` — SDK adapter для Rev1.2. **Не вызывается main/UI**,
нет новых startup/task/timer/network operations при запуске существующего HMI.
Реальная синхронизация и TLS peak на плате ещё не проверены; плата остаётся1c38d1e.

## Policy и lifecycle

Только explicit worker operation, anchor и policy принадлежат одному caller.
Policy копируется до revoke anchor; нет production дат/TTL/default approval.
Нужны reviewed=true, allow_unauthenticated_sntp=true и валидные time gate bounds.
Если policy не допускает источник, ни start, ни DNS/UTC update не выполняются.
Каждая принятая попытка отзывает старую anchor; неуспех не выдаёт новую.

- Перед start проверяются network/cancel и общий deadline1..30000ms.
- Poll slices<=250ms; cancel/loss/timeout проверяются до/после SDK waits.
- Дополнительный cap ceil(timeout/250)+2 polls останавливает WAIT с frozen clock.
- Stop/cleanup вызывается после каждого start attempt, включая partial failure/
  BUSY. Backend закрывает только свою инициализацию, не чужой service.
- После observed sync клиент сначала остановлен; затем system UTC/monotonic
  sample читается, получает source=SNTP_UNAUTHENTICATED и проходит time gate policy.
  Возраст anchor отсчитывается от чтения часов после stop, не от точного
  момента приёма NTP packet; длительность sync доступна в report.
- Late sync/cleanup/sample не дают success; final deadline check отзывает grant.
  Result и time_result отдельно описывают cause, elapsed/polls/cleanup_ok — факты.
  cleanup_ok означает cleanup нашей попытки, не отсутствие чужого SNTP клиента.

Контракт backend/clock/cancel и anchor single-owner, не thread-safe. Не запускать
параллельно TLS или другими UTC setters. Callback не меняет anchor. API calls,
TCP/IP IPC, scheduler и cleanup не имеют жёсткой preemption; logical deadline
отклоняет опоздавший результат, но не гарантирует возврат ровно через30s.

## SDK adapter

Предусловие — существующий project Wi-Fi snapshot NET_ONLINE, затем SDK interface
WIFI_STA_DEF должен быть up с IP. Adapter не создаёт netif и не переподключает сеть.
Используется штатный esp_netif SNTP, статический pool.ntp.org (DNS+UDP123),
без server-from-DHCP/renew hook/custom callback. init start=false сначала
получает storage; immediate sync mode/start — только после successful ownership.

Init/start и deinit целиком выполняются через esp_netif_tcpip_exec. Вложенные
esp-netif IPC выполняются непосредственно в TCP/IP context — source reviewed
на6.1.1 и pinned5.3.2. Это сериализует init/teardown с SNTP callbacks; stack-local
custom callback/context SDK не сохраняет. Wait выполняется из worker, не TCP/IP.

Активный raw SNTP/занятый esp_netif SNTP service → BUSY без stop чужого клиента.
Локальный critical guard исключает вторую операцию adapter. Все внешние владельцы
того же SDK global service должны соблюдать ownership; inactive SDK configuration
не сохраняется как отдельный чужой state. При uncertain cleanup собственный
busy сохраняется, автоматический новый client не запускается.

SDK SNTP **изменяет global UTC до окончательной проверки**. При range/policy/
cancel/timeout после sync новая anchor не выдаётся, но часы назад автоматически
не откатываются. Остальные TLS callers обязаны использовать time gate; источник
и clock changes нельзя считать изолированными от остального приложения.

SNTP_UNAUTHENTICATED не является NTS/криптографическим доказательством UTC.
Explicit policy допускает слабый источник по review, не повышает его provenance.
Нет fallback к build date/static HMI clock/HTTP Date. Публичный сервер не
отправляет credentials, но network metadata обычных DNS/UDP пакетов существуют.
Host/headless возвращают UNAVAILABLE, не запускают сеть и не меняют UTC.

## Проверки и следующие шаги

[Evidence](evidence/build/bounded-sntp/README.md): mock lifecycle/policy/late
response/cancel/network loss/frozen clock/cleanup/range/wrap/retry. Реальных
DNS/UDP/SNTP servers, native clock setting/teardown races, heap/stack/latency
или network attacker tests не было. SDK6.1.1/5.3.2 × три профиля compile/link.

Нет нового bench trigger: память/time policy/TLS handshake/CA/redirect/failure
profiling надо собрать в отдельный explicit image. Перед его записью/reset/
UART/network test получить отдельное разрешение. На плате остаётся1c38d1e,
не повторять прежние physical operations под видом read-only.

Production policy/UTC UX, TLS peak/internal reserve, bounded runtime OTA worker,
install/A-B migration/rollback, physical latency/soak и UART cause остаются открытыми.
Configs/overlays/layout/locks/main/board/UI/Wi-Fi/STM32/vendor не меняются.
Никаких Flash/eFuse/boot selection/keys/release/auto-merge. Откат — revert PR;
текущее состояние устройства этим кодом не меняется, потому что он не вызывался.
