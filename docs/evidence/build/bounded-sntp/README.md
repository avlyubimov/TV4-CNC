# PR-04i — проверки 18 ноября 2026

Root CTest16/16 PASS; ASan/UBSan/LeakSanitizer16/16 PASS вне ptrace sandbox
без suppressions. Прежние HMI/STM32/OTA tests сохранены. SNTP mock-only:
нет реальных DNS/UDP/server/clock setting или device operations.

Проверены policy/source denial до start, offline/cancel before/during sync,
foreign BUSY/partial start, poll error, timeout в start/poll/stop/clock,
frozen progress cap, cleanup failure, bad epoch/clock rejection, wrap и100retry.
Stop вызывается после каждого start attempt; clock читается только после stop;
неуспех не оставляет ready anchor, source успешной anchor SNTP_UNAUTHENTICATED.
Local policy validation используется общая с time gate, без копии правил.

| SDK | Profile | App bytes | SNTP/time/HTTPS linked |
|---|---|---:|---|
| 6.1.1 | headless | 1054976 | PASS |
| 6.1.1 | rev12 | 1573072 | PASS |
| 6.1.1 | rev12-memory-bench | 1574720 | PASS |
| 5.3.2 | headless | 1069376 | PASS |
| 5.3.2 | rev12 | 1528768 | PASS |
| 5.3.2 | rev12-memory-bench | 1530480 | PASS |

6.1.1 — установленный SDK; 5.3.2 — pinned SDK Docker tools/build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
validation JSON: hashes/Flash16MB/PSRAM/factory/profile flags. Workspace34000
на всех6ELF; SNTP/time/native HTTPS entry points linked/non-UND. Bench start
только в bench profile. Native SNTP adapter compiled/linked, не исполнялся.

Reviewed SDK6.1.1 и pinned5.3.2 esp_netif_lwip.c: IPC исполняет nested callback
непосредственно из TCP/IP context. В adapter init/deinit целиком сериализованы
через esp_netif_tcpip_exec; нет borrowed custom callback/context после return.
SDK APIs bundled/pinned, новых dependencies/locks/config changes нет.

Host GCC15.2.0/CMake4.2.3/Ninja1.13.2/OpenSSL3.5.5/LVGL9.2.2.
-fanalyzer lifecycle/host SDK stub, workflow YAML/links/diff PASS.
Mainbe0a57c Actions — все4 workflow success перед публикацией ветки.

Не проверены live packet loss/server spoofing/DNS/UTC setting/cleanup races,
heap/stack/latency/TLS peak на ESP32. Политика production отсутствует. SNTP не
аутентифицирован; разрешение policy не меняет происхождение. SDK sync может
изменить global UTC до ошибки; rollback UTC не выполняется, anchor не выдаётся.
Logical timeout не hard preemption SDK calls; unknown cleanup блокирует adapter
от нового клиента. Нет startup/UI invocation или нового bench trigger.

На плате остаётся memory-bench1c38d1e: новых serial/reset/flash/network probes
не делали. Main/configs/overlays/layout/locks/board/UI/Wi-Fi/STM32/CAN/vendor
сохранены. Нет keys/release/auto-merge. Результаты рабочего дерева до commit,
точный SHA новой CI — на ветке/PR.
