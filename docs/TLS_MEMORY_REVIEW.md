# PR-04k — память mbedTLS в диагностическом профиле

## Причина и пределы вывода

[Аппаратная проба b085abb](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md)
закончилась в mbedtls_ssl_setup с -0x008D (PSA_ERROR_INSUFFICIENT_MEMORY).
Фактический sdkconfig6.1.1: MBEDTLS_INTERNAL_MEM_ALLOC=y, входной TLS record
16384bytes, выходной4096bytes. При tls_window_start largest internal block7680,
internalfree20175bytes. В SDK ssl_tls.c mbedtls_ssl_setup выделяет input/output
buffers через mbedtls_calloc; input buffer ещё больше payload за счёт overhead.
Такой input allocation не может поместиться в наблюдаемый largest block.
Это объясняет несостоятельность прежней стратегии, но не трассирует конкретную
allocation: исходный лог не даёт имени/размера отказавшего запроса.
HTTP client RX4096/TX512 — **другие буферы**, они не уменьшают TLS record buffers.

## Исправление

Только sdkconfig.tls-bench выбирает штатный MBEDTLS_EXTERNAL_MEM_ALLOC=y.
В закреплённых ESP-IDF6.1.1 и5.3.2 components/mbedtls/port/esp_mem.c одинаков:
esp_mbedtls_mem_calloc вызывает heap_caps_calloc(n,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),
free — heap_caps_free. **Нет internal fallback.** Не вводим собственный allocator,
не меняем process-global callbacks во время работы и не трогаем generic malloc.

Это выбор для **всех mbedTLS allocations данного image**, включая возможных
SDK/Wi-Fi пользователей, не per-client hook и не только TLS RX. Не все расходы
HTTP/TCP/crypto/SDK используют этот allocator: internal RAM всё ещё нужна.
Все crypto/RGB/PSRAM clock/cache/DMA параметры, firmware locks и board pins прежние.
64KiB SPIRAM_MALLOC_RESERVE_INTERNAL и16384-byte ALWAYSINTERNAL сохраняются.
Входной TLS payload16384/выходной4096 явно сохранены; MFL не используется как
основание для уменьшения record capacity. HTTP buffers/timeouts/URL/HEAD,
CA bundle/hostname/time-anchor/no redirects/auth/body/installer не меняются.
Обычные headless/rev12/rev12-memory-bench сохраняют INTERNAL_MEM_ALLOC, bench off.

## Дополнительный дефект проверки сертификатов

SDK Kconfig по умолчанию MBEDTLS_HAVE_TIME_DATE=n. На исходном b085abb он также
был выключен: fresh UTC anchor проверялся приложением, но сам mbedTLS не проверял
notBefore/notAfter. Поэтому прежние слова «time-checked» описывали anchor gate,
а не полноценную X.509 date validation. HTTP/handshake тогда не состоялись;
успешная проверка сертификата и ранее не заявлялась.

В TLS overlay теперь HAVE_TIME=y/HAVE_TIME_DATE=y. SDK esp_config.h включает
MBEDTLS_HAVE_TIME_DATE, а x509_crt.c проверяет valid_from/valid_to всех сертификатов
и ставит BADCERT_FUTURE/EXPIRED при отказе. В ELF обоих SDK проверяется наличие
mbedtls_x509_time_cmp/gmtime; runtime bad-cert/expiry/clock cases ещё не испытаны.
Private2026 SNTP policy остаётся unauthenticated, production trust не утверждён.

## Защита от неверной сборки

Native bench имеет compile-time guards PSRAM/external allocator/CA/date checks.
check_idf_build.py проверяет единственную allocator choice по профилю, cert dates,
отсутствие insecure TLS, исходные record sizes, PSRAM/internal reserves. JSON
артефакт явно содержит mbedtls_allocator, mbedtls_certificate_dates и record sizes.
CI матрица остаётся четыре профиля × два SDK; добавлен ELF date-symbol check.
Negative fixtures отвергают stale/internal/fallback/custom/IRAM choice, отсутствие
CA/date/time, insecure TLS, урезание records/резерва и утечку choice в ordinary.
Worker логирует только allocator/размеры/cert_dates, не URL headers/секреты.

IDF сохраняет существующий sdkconfig и он имеет приоритет над defaults.
Для уже собранного **TLS bench** перед новой сборкой сохранить старую конфигурацию:

```sh
mv build-idf-6.1.1-rev12-tls-bench/sdkconfig \
   build-idf-6.1.1-rev12-tls-bench/sdkconfig.before-tls-psram
./tools/build_idf.sh rev12-tls-bench 6.1.1 local
```

Только если файл существует; для5.3.2 аналогичный отдельный build directory.
Обычные профили не требуют изменения конфигурации. Ошибку guard/validator нельзя
обходить: нужно заново получить правильный sdkconfig из reviewed overlay.
Build script по-прежнему не прошивает/не открывает serial.

## Критерии и границы

[Результаты software checks](evidence/build/tls-psram-allocator/README.md): оба SDK
× четыре профиля, host/negative/sanitizer tests, неизменность dependencies/layout,
проверки retained symbols и native PSRAM allocation code. Это не результат на плате.

На момент software review PR-04k на плате был **b085abb ordinary rev12**,
новый image ещё не испытывался. Последующая разрешённая проба — ниже.
Для каждого нового теста после review/зелёного CI нужны exact SHA,
новая сборка/хеши, отдельное разрешение на новую пробу и возврат ordinary. Проверить
SNTP, verified HEAD/HTTP200, cleanup, heap windows/stack/error counters и HMI/Wi-Fi.
OOM/expiry/CA/timeout — результат, не повод ослаблять проверки/резервы.

PSRAM медленнее internal и на этом стенде не шифруется: TLS context/session
material может находиться в external RAM. SDK рекомендует internal для защиты
от физического доступа. Это public HEAD diagnostic profile, не production secret
storage policy; eFuse/Flash Encryption не включаются. Production adoption требует
отдельного security/performance review. Ни отсутствие задержек UI, ни TLS peak/
достаточный headroom/production reserve пока не измерены. PSRAM free, compile
или ELF symbols не доказывают успешный handshake или отсутствие регрессии.
Rollback software PR — revert кода; сам software PR не меняет запись платы.

## Последующая аппаратная проба — 19 ноября 2026

[1bfc7e2: SNTP и verified HEAD200 PASS](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md)
по отдельному разрешению; старое состояние b085abb выше относится к моменту
software PR-04k. После пробы возвращён ordinary1bfc7e2,owner HMI/touch/Wi-Fi PASS.
Реальный TLS window internal min3123bytes,historical min2408,stack watermark912:
production memory/stack budget НЕ утверждён, следующий gate — review запаса.
