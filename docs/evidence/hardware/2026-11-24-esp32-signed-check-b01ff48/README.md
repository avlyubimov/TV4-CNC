# 24 ноября: один signed CHECK — отказ framing, возврат PASS

Exact source `b01ff48608d4d1480ea4b427de51a6705ba3eb99`, SDK6.1.1,
configured rev12-ota, worker12288. Source CI30/main c3b82f0 CI15 PASS до USB.
Владелец разрешил автономные обратимые проверки обеих изолированных плат до08:00
Asia/Almaty; этот finite пакет закрыт за1094,580s и не исполняется повторно.

Свежие16MiB/device MD5/SHA-256/две независимые NVMe PASS. Контроль90s PASS:
16 samples, touch15–16ms, internal free минимум120587bytes, largest63488,
worker remaining10880/12288. Затем app-only auto image, ровно один CHECK,
UART420s. Три normal writes/control+auto+весь factory; full restore0.

CHECK FAIL: native open_failed step5, status302, headers5704, bad_headers1.
Session state8/error2/http4, resource0/checks2, remaining stack6896bytes.
Это отказ до manifest/image verification, не полный resource budget PASS.
Автоматический итог attempted1/pass0/read_only1; повторного запроса/INSTALL нет.
Весь factory fecc05d возвращён/verify/сохранность вне factory/UART30 PASS;
bootloader/table/otadata/inactive/resources/store сохранены. На плате fecc05d.
Владелец спит: physical UI/touch/FPS не подтверждались, UART не подменяет их.

Проверенный прямой GET того же GitHub URL:302, Content-Length0, без Transfer-
Encoding/Content-Encoding (только форма заголовков в http-header-shape.json,
Location/token values не сохраняются). В обоих exact SDK fetch_headers помечает
content_length<=0 как is_chunked=true, включая явный нулевой Content-Length.
Прежняя проверка равенства этому SDK-флагу отвергает такой ответ. Actual-source
SDK double с этой семантикой воспроизвёл отказ старого адаптера. Это конкретная
software несовместимость; положительный аппаратный результат исправления ещё
предстоит получить. Floors, сертификаты и framing-guards не снижались.

Новый код различает wire headers и этот SDK-флаг только для однозначного
Content-Length0 без Transfer-Encoding. Empty200/unknown framing/CL+TE/
duplicates/compression/прочие SDK contradictions по-прежнему отвергаются.
Следующий exact пакет — после host/sanitizers/двух SDK/CI, с новым backup/возвратом.
Подпись/полный image stream/install/rollback/production budget пока не приняты.

[Software evidence](../../build/esp32-ota-autonomous-check/README.md),
[план](../../../plans/P1_AUTONOMOUS_SIGNED_CHECK.md). Приватные UART/dumps/keys/
context остаются вне Git; measurements.json содержит только numeric evidence.
