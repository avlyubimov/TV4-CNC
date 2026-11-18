# PR-04d — signature envelope и read-only HTTPS

Следующий независимый шаг после PR #11. Критерии: bounded конверт с локальным
ключом; HTTPS-only allowlist/manual redirects; fault/timeout/cancel tests;
headless/rev12 на ESP-IDF 6.1.1 и 5.3.2. Код не вызывается app_main/UI.

## Подпись

`envelope.c` принимает JSON до 2048 байт, ровно key_id/algorithm/signature.
Алгоритм ECDSA-P256-SHA256, canonical Base64 DER 8..72 байта. Единственный
доверенный ключ приходит из локального context; downloaded key не принимается.
Envelope разбирается первым, освобождается; подпись exact bytes manifest
проверяется до разбора самого manifest. Общий private JSON guard сохраняет
UTF-8/duplicate/depth/token/length checks. Ошибки очищают candidate.

## Транспорт

`download.c` — переносимый синхронный bounded receiver, `https.c` — адаптер
esp_http_client + штатный CA bundle с hostname verification. Вызывать только
из будущего worker, никогда из LVGL callback. Пользователь владеет workspace
на всё время запроса; повторно/параллельно его использовать нельзя.

- Исходные адреса: только https://github.com/avlyubimov/TV4-CNC/releases/download/vSEMVER/
  manifest.json, manifest.sig или tv4-hmi-esp32s3-vVERSION.bin.
- До трёх явных redirects; дополнительные точные hosts:
  release-assets.githubusercontent.com (/github-production-release-asset/),
  objects.githubusercontent.com (/github-production-release-asset-).
  Нет wildcard/HTTP/credentials/custom ports/fragments, malformed escapes/control.
- Новый TLS client на каждом переходе; нет переноса Authorization/Cookie.
  Автоматические redirects отключены, нет insecure fallback.
- Только 200; identity encoding; единственный Content-Length либо chunked.
  Отказ при duplicate/ambiguous headers, неполном EOF, excess/truncated length.
  URL до 2048, headers до 4096, chunk 4096; manifest до 16384, envelope до 2048,
  image до 3 МиБ и с обязательной expected length из проверенного manifest.
- Cancel, idle/total deadlines с wrap-safe monotonic ms, I/O timeout до 10 с.
  Проверки до/после blocking SDK calls и consumer. Они отклоняют опоздавшие
  данные, но не обеспечивают жёсткое прерывание DNS/TLS внутри SDK.
- trusted_time — обязательное предусловие от будущего проверенного поставщика
  времени. Статические часы HMI не являются таким источником.
- Нет логирования URL/query, заголовков, credentials или ключей.

Consumer может подавать bytes в read-only image checker. При любой ошибке
будущий coordinator обязан abort/cleanup checker; этот PR не создаёт coordinator,
сеть/таймеры/задачи, discovery или release. Ничего не сохраняется во Flash.

## Проверки и ограничения

[Evidence](evidence/build/ota-https/README.md): mock transport fault injection,
настоящие ephemeral host signatures и SDK compile/link. Host TLS stub возвращает
UNAVAILABLE. Реальные DNS/TLS/certificate/redirect tests, SDK heap/stack/latency,
часовой источник и production trust provisioning ещё не проверены. Allowlist
нужно подтвердить на реальных GitHub redirects перед включением worker.

Зависимости прежние: LVGL 9.2.2, touch 1.2.1, GT911 1.1.3, private namespaced
cJSON 1.7.19 MIT с SHA-256. esp_http_client/CA bundle входят в pinned SDK.
Active factory 2 МиБ/partitions/sdkconfig/board/CAN/STM32 не меняются.
Production SemVer/ключ/контексты совместимости и USB migration/rollback остаются
блокерами installer. Откат: revert этого PR; persistent state не меняется.
