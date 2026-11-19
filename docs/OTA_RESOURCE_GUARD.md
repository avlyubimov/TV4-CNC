# PR-04m — проверяемый допуск read-only OTA-сессии по ресурсам

## Основание и scope

[8f0d899](evidence/hardware/2026-11-19-8f0d899-headroom/README.md) дал HEAD200
и наблюдаемый internal min5307/window,4204/history,worker stack watermark1840bytes.
Это не approved бюджет manifest/image download, crypto/error paths или UI load.
Существующий memory.c проверяет internal/PSRAM floors до/после PSRAM workspace
allocation, но не stack или последующие границы сессии.

Новые resource_guard.c/h и tv4_ota_session_run_guarded добавляют проверяемый
контракт для будущего worker. **Нет caller в startup/UI/network tasks**, нет
нового task/профиля/production policy/runtime HTTPS/OTA body/install. Изменения
native primitives только compile/link; на плате остаётся ordinary8f0d899.

## Политика и samples

Политика — immutable local input с reviewed=true и пятью ненулевыми floors:
internal free/largest, PSRAM free/largest, текущий worker stack watermark.
Largest floor не выше соответствующего free floor. Отсутствующая/непроверенная/
некорректная политика отклоняется **до fetch**, никакого default/автоподбора по
HEAD minima или downloaded metadata. Test floors32KiB/etc только синтетические
fixtures, не разрешение и не параметры firmware.

Reader не аллоцирует, не делает I/O и вызывается тем же worker. Snapshot содержит
текущие capability heap free/largest и lifetime stack watermark в **ESP-IDF bytes**.
Отдельные причины: ARGUMENT/NOT_REVIEWED/UNAVAILABLE/INVALID_SAMPLE/
INTERNAL_FREE/INTERNAL_LARGEST/PSRAM_FREE/PSRAM_LARGEST/STACK. Равенство floor
допускается; inconsistent largest>free отклоняется. Неуспешный reader оставляет
zero sample, а не частично записанные/старые значения. SIZE_MAX floors сравниваются
без сложения/вычитания/overflow. Policy и sample — разные типы/входы.

Native tv4_ota_resource_sample_current использует heap_caps_get_free_size/
get_largest_free_block(INTERNAL|8BIT и SPIRAM|8BIT) и
uxTaskGetStackHighWaterMark(NULL). Только Rev1.2/PSRAM; если external heap
недоступен — false. Host/headless — false/zero output. Нет global heap-min
monitor owner/reset или изменения памяти/stack/приоритетов/проверок TLS.

## Границы guarded run

Новый entry сохраняет legacy signature/behavior tv4_ota_session_run, добавляя
отдельную обязательную policy/reader/context/report комбинацию. Старый API
остаётся portable coordinator для тестов/совместимости, **не production admission**.
Будущий runtime worker обязан выбрать guarded entry и reviewed policy отдельно.

Проверки идут на существующих stop boundaries: до/после fetch, перед принятием
каждого metadata/image chunk, до/после signature/policy/finish и перед success.
Это проверки уровня сессии, **не внутри esp_http_client_open/TLS handshake**.
Transport adapter по-прежнему обязан корректно завершать fetch/close, в том числе
после consumer failure/partial open; guard не может принудительно закрыть чужой
нарушающий контракт callback.

Отказ ресурсов фиксируется до конца операции: позднее восстановление heap,
cancel/deadline при cleanup не превращают его в success и не стирают первую
resource cause. Если cancel/timeout уже обнаружены до resource check, прежняя
причина сохраняется. Raw HTTP/policy/image errors остаются доступны в result.
На отказе — TV4_SESSION_RESOURCE/FAILED, failed_stage, очистка candidate/metadata/
HTTP workspace и закрытие hash contexts через существующий failed path.
Нет CANDIDATE_READY/IMAGE_VERIFIED или доверенного частичного результата.

Report отдельно от workspace: result, saturating checks, последний sample (при
отказе первый отказавший). ResultOK с checks0 не означает измерение ресурсов
(например, запрос отменён до первого sample). Busy/terminal session отвергает
повторный run без изменения прежнего report/result; reset делает владелец вне run.
Policy immutable, context живёт до конца run; report writable только runner и
не пересекается с workspace/другими inputs. Session/report не thread-safe, UI
получает только будущие copied snapshots. Формат session/result не расширен:
RESOURCE добавлен в конец error enum, workspace остаётся target34000bytes.

## Что это не гарантирует

- Snapshot не атомарен и не резервирует heap; concurrent allocations могут
  отнять память сразу после проверки. Несогласованная выборка консервативно fail.
- Транзитный минимум внутри SDK/crypto, восстановившийся до boundary, может
  остаться незамеченным. Current free — не TLS-window minimum.
- Stack watermark отражает жизнь этой задачи, включая прежние вызовы и сам
  sampler. Он не восстанавливается после освобождения heap/reset session;
  guard не предотвращает overflow, случившийся до возврата управления.
- Ни guard, ни deadlines не дают hard preemption, достаточного stack reserve,
  защищённого времени, подписи/CA/hostname, admission нескольких параллельных
  клиентов или production budget автоматически. Остальные security gates прежние.
- Sampling имеет стоимость, native runtime/latency/CPU/fault/soak под полной
  нагрузкой ещё не измерены. Без reviewed margins runtime integration блокирована.

## Проверки и следующий шаг

[Evidence](evidence/build/ota-resource-guard/README.md): policy edge tests и
подписанная end-to-end mock session с инъекцией resource failure на **каждой**
достигнутой boundary, включая активный transport, cleanup и уже проверенный
candidate. Resource failure после cleanup/последнего шага не публикует успех.
Отдельно stack/reader/unreviewed/missing/cancel/deadline/signature tests.
Host/sanitizers и оба SDK ×4profiles, ELF retained symbols/workspace/layout/locks.
Нативные samples/HTTPS/рабочее устройство в этом PR не вызывались.

Для дальнейшей native integration нужны reviewed heap/stack policy с запасом
для workspace/TLS/crypto/SDK/UI, budget/fault/load/soak trials и exact-image owner
approval. Текущий HEAD bench и NVS/board/RGB/GT911/Wi-Fi/config/layout/locks не
меняются, private keys/release/Flash/boot selection/CAN/motion отсутствуют.
Откат — revert PR; установленная ordinary8f0d899 этим PR не меняется.
