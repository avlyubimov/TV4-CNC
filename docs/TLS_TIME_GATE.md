# PR-04h — проверка времени для TLS

После PSRAM bench остаются TLS memory peak и корректное время. Этот PR добавляет
проверку происхождения/свежести и стабильности времени, **не SNTP service**.
Системные часы не устанавливаются, main/UI/сеть не запускают новых операций.
На плате остаётся memory-bench 1c38d1e, разрешение на его тест завершено.

## Локальная anchor и политика

`time_gate.h/c` принимает sample (UTC epoch us + monotonic us + source) только
от будущего local provider. Нельзя копировать provenance/date из HTTP/manifest,
build date или статических часов HMI. Это контракт trusted application context,
не защита от произвольного вредоносного кода внутри той же прошивки.

Policy без reviewed=true не принимается. Нет production даты/TTL/skew defaults:
локально review earliest/latest UTC seconds, age (1..86400000ms), skew
(1..60000ms). Epoch seconds uint32 (до2106), вычисления/monotonic 64-bit;
2040/boundaries тестируются. Policy не означает независимую истинность UTC.

Sources: LOCAL_CONFIRMED — подтверждение будущим local operator/provider
workflow; UNKNOWN не допускается. SNTP_UNAUTHENTICATED допускается только
explicit allow_unauthenticated_sntp в reviewed local policy. Это **не** NTS,
аутентификация SNTP или доказательство доверия удалённому серверу.
SNTP adapter/network worker, ручной ввод UTC, RTC/NVS и policy provisioning
ещё отсутствуют; ни один источник автоматически не объявляется готовым.

Adopt копирует sample/policy, очищает предыдущую anchor при отказе.
Check сравнивает wall clock с anchor UTC + monotonic elapsed. Возраст >=TTL,
неожиданный forward/backward step, обратный monotonic, range/invalid sample
отзывают anchor до новой явной adoption. Возврат часов назад сам по себе не
восстанавливает разрешение. Не заявляется persistent anti-rollback UTC.

## SDK sampler

gettimeofday + esp_timer_get_time, три попытки пары с интервалом <=1000us;
результат source=UNKNOWN. Чтение системных часов само по себе не устанавливает
provenance. Прерывание/preemption может привести к отказу после трёх попыток;
жёсткий deadline внутри SDK не обещается. Нет settimeofday, SNTP/DNS, TZ changes,
threads, callbacks или новых clock logs. Host sampler возвращает UNAVAILABLE.

## Привязка к HTTPS

Сигнатура native tv4_https_read теперь включает worker-owned tv4_time_anchor*.
Флаг request.trusted_time остаётся request intent, но недостаточен без anchor.
`timed_download.c` оборачивает существующий bounded HTTP engine/backend:

- проверка перед первым open и до/после каждого open/read/complete;
- проверка после consumer/close/финального cleanup;
- clock failure → close resources, revoke anchor, UNTRUSTED_TIME даже при
  полном полученном body или apparent backend success;
- data consumer может уже иметь часть/весь body, но не должен публиковать
  candidate/commit до успешного final result; session на ошибке очищает candidate;
- CA bundle/hostname verification, allowlist/redirects/limits и deadlines сохранены.

Один worker владеет anchor и UTC updates весь request. Будущий SNTP/time provider
должен serialize updates и не менять anchor из callback параллельно TLS.
Сэмплирование не обнаружит любой transient clock change между checks и не
заменяет source authentication/CA; доверенный код должен соблюдать ownership.

## Проверки и оставшееся

[Evidence](evidence/build/tls-time/README.md): provenance/policy/range/64-bit/
age/skew/revocation и mock HTTP integration. Включены часы, изменённые в open,
read, consumer, close, expired during open и true hint без anchor: failure с
cleanup, без ложного final success. Native SDK compiled/linked, не исполнялся.

Нет новых аппаратных операций, Flash writes/OTA installer/boot selection,
production keys/release. SDK configs/locks/layout/board/UI/Wi-Fi/STM32/vendor
не меняются. Для runtime TLS ещё нужны источник UTC и reviewed source policy,
TLS peak/budget/time failure tests, bounded worker/UI и разрешённый bench.
Откат — revert PR; persistent/device state не меняется.

## Следующий слой PR-04i

[SNTP source](SNTP_SOURCE.md) даёт explicit worker operation; предыдущие слова
«SNTP service отсутствует» описывают PR-04h до него. В startup/UI он по-прежнему
не вызывается, production policy/UTC readiness не подставляются автоматически.
