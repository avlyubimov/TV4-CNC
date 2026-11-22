# P1 — интеграция ESP32 OTA

База main d1317a8, PR #1–58 слиты; P0 ee7d179 hardware report принят владельцем.

1. Один explicit rev12-ota: reviewed 16MiB factory/двух OTA slots, SDK rollback,
   boot watchdog/self-test, без изменения ordinary профилей и неподтверждённых pins.
2. Existing signed session → bounded SDK inactive writer → full digest/EOF/SDK end
   → boot selection последней операцией. Связать одобрение UI с полным образом,
   re-fetch metadata без потери binding; cancel/error cleanup без boot activation.
3. Один worker, очередь1, короткие snapshots, нет I/O в LVGL callback. В отсутствие
   owner public key/versions/resource/time context сборка работоспособна, install
   fail-closed. Владелец явно выбрал настройку без ключа; тестовые ключи только host.
4. Prepare/independent verify USB bundle с clean source/app/ELF/hash/layout/config
   binding. Initial migration factory+erased otadata, сохранение NVS/PHY; отдельные
   backup/recovery/permission/power-loss gates. Никакого device access в инструментах.
5. Fault-injection actual session/SDK adapter, bootstrap window/rollback, local
   trust/bundle negative tests, UI states/layout/confirmation и полный host/sanitizer
   набор. Собрать новый и прежние профили6.1.1, CI5profiles×2pinnedSDK, только text
   evidence upload. Размеры/compiler frames не выдавать за runtime budget.
6. Документация/evidence/отдельный PR, дождаться CI; без self-merge/release/устройств.
   Hardware migration, configured install/rollback/network/power loss и RAM/stack
   измерения остаются следующим отдельно разрешаемым испытанием P1; P2–P5 не начинать.
