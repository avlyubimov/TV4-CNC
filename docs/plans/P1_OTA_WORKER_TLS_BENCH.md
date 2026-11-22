# P1 — UTC/HEAD в существующем OTA-worker

База: main `53b0715`, PR #68 слит владельцем. Программный этап; на ESP32
остаётся factory fecc05d, разрешение стенда dfa9f28 закрыто.

1. Добавить default-off `TV4_OTA_WORKER_TLS_BENCH` только к unprovisioned
   rev12-ota с memory diagnostics и проверенным worker8192. Повторно использовать
   native UTC/HEAD backend и единственный OTA minimum-monitor adapter.
2. Выполнить один проход после self-test: settling, Wi-Fi, SNTP, проверяемый
   HTTPS HEAD. Сохранять SETUP, закрытые CHECK/INSTALL, существующие SDK TLS
   проверки и HMI health cancellation. Никаких новых задач/ключей/Flash writes.
3. Проверить actual C service/backend на SDK doubles, отказах и двух версиях
   API; full host/sanitizers/analyzer, 16 реальных сборок на pinned 6.1.1/5.3.2,
   ELF/config/frame/monitor-owner gates и CI. Сохранить software evidence.
4. Отдельный PR без merge. Будущий аппаратный стенд требует точных артефактов,
   приватного fresh backup и нового разрешения. Не объявлять software tests
   измерением памяти/стека/TLS на ESP32 или разрешением установки OTA.

Приёмка: реальные проверенные ELF/BIN двух SDK и воспроизводимый opt-in build;
обычные профили и прежний standalone TLS bench сохранены, сертификаты/date/CA
не ослаблены, нет второго владельца SDK monitor или production trust defaults.

Программная приёмка выполнена: [evidence](../evidence/build/esp32-ota-worker-tls-bench/README.md),
код d27142e, 16/16 SDK builds, host156/sanitizers156/analyzer7 PASS.
Аппаратный запуск и runtime budget не выполнялись; PR не сливать самостоятельно.
