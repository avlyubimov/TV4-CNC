# P1 ESP32 OTA: программная проверка

База main d1317a8, unprovisioned profile по решению владельца. Это evidence
software integration; устройства не открывались, Flash/reset не выполнялись.

Host79/79 PASS, actual signed session и SDK-adapter mock faults, boot window/
accept/rollback,11 trust/context/migration negative tests и LVGL32/RGB565 layout/
confirmation. Первый ASan/UBSan/LSan79/79 PASS; финальные журналы/clean-source
build и bundle identity будут добавлены после фиксации исполняемых исходников.
Программные проверки не подтверждают USB migration, configured OTA, power loss,
реальные heap/stack/FPS/touch margins. Test keys только в host temporary fixtures.

Процедура/ограничения: [ESP32_OTA_INSTALLATION](../../../ESP32_OTA_INSTALLATION.md).
CI:5profiles×2pinnedSDK; ordinary profiles factory2MiB, explicit rev12-ota factory+A/B.
Публичные CI artifacts только text/metadata, без firmware/private keys/Flash dumps.
