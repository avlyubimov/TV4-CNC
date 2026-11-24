# INSTALL policy: программные проверки

Exact clean source `3b09bba97ff94b56c0be529c9e9cbc88ba55247c`, PR #82 поверх
PR #81.274/274 host и274/274 ASan/UBSan/LSan с detect_leaks=1 PASS. Source/code
и signed-session regressions отделены от аппаратных измерений. Actual service
проверяет ordinary operator CHECK→READY→INSTALL с двумя local policies и signed
readonly candidate. Native adapter без capability gate воспроизводит отказ
теста; signed session missing/false останавливается до sink.begin/Flash, при
этом read-only full image pass успешен. Independent Python/C согласуют
missing/false/true, type/duplicate/peer policy. CHECK release tool подписывает
false и отвергает действительную installable подпись как неверный CHECK scope.

GCC -fanalyzer actual JSON/manifest/session/native installer и шесть service
policy variants PASS. Real LVGL RGB565 render проверяет «Только проверка»,
disabled callbacks, отзыв stale confirmation и committed RESTART. Это host
render, не измерение physical UI/касания/FPS. Новых задач, сетевых/криптографических
библиотек, Flash layout или security settings нет.

Четыре actual control/auto ELF/BIN обоих pinned ESP-IDF6.1.1/5.3.2 проверены:
Flash16MiB, PSRAM8MiB/XiP, A/B layout, diagnostics/frame ceilings, app checksum/
appended SHA-256, descriptor version и полный ELF SHA binding. Generated local
context/ELF marker явно CHECK-only; owner context не переписан. Worker12288,
own worker frame80bytes; own operation frame464bytes, ранее480. Это compiler
frames, не runtime peaks или гарантия производительности.

Дополнительно два actual compiler-only ELF/BIN с install_reviewed=true и
одноразовым **другим** test public key проверяют положительную native ветку
обоих SDK: esp_ota_begin/write/end/set_boot_partition и SDK adapter связаны.
Own operation frame480bytes. Private test key жил только в памяти subprocess,
не сохранялся. Эти образы не разрешены для платы/публикации, не входят в bench
inventory, не являются согласованным installable payload. CI проверяет default
firmware matrix и actual host service policy variants; эти два configured
compiler fixtures — локальная проверка, не утверждение о CI provisioning.

Все app/ELF hashes, SDK commits, compiler frames и назначения в results.json.
Owner key/settings, BIN/ELF и raw UART/Flash snapshots остаются вне Git/CI.
Стенд ограничен полным read-only CHECK published bench.2; его исходные assets
не изменялись, живые metadata digests всех11 совпали с прежними downloads.
Network INSTALL, Flash write peaks при RGB/TLS, Wi-Fi/power loss, production
budget, physical UI и runtime5.3.2 остаются отдельными проверками.

Source CI30/30 и unchanged main15/15 PASS до USB;22 finite procedure и9 sealed-loader tests PASS. См. ci.json и аппаратный отчёт.

[Закрытый hardware CHECK](../../hardware/2026-11-24-esp32-install-policy-3b09bba/README.md) — отдельное измерение нового source.
