# P1: реальные подписанные CHECK payloads двух SDK

[Лицензированный c013ddf и опубликованный prerelease](publication.md) —
актуальный внешний пакет. Следующий ниже результат3a2e03d сохранён как история.

[Протокол](results.json): первый clean source3a2e03d, CI15/15,
184/184 host,184/184 ASan/UBSan/LSan,21 signing fault tests. Настоящие
rev12-ota ELF/BIN0.1.0-bench.2 собраны на ESP-IDF5.3.2 и6.1.1 и подписаны
локальным ключом владельца. Это не ephemeral fixtures; ключи/пути приватны.

Python/OpenSSL проверил signature,size/SHA,hardware/layout/version и полный
image checksum/digest/descriptor. Existing native ota_policy_driver независимо
принял оба manifest и отверг изменённые raw bytes с output_zero.
[Evidence caller](native_stream_check.c) проверил оба полных BIN существующим
C image_stream по4096bytes: VERIFIED/error0/exact size. Изменение одного байта
отвергнуто с error8; после image_close hash contexts закрыты. Clock
синтетический: это не runtime deadlines/resources или hardware evidence.

```bash
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/esp32-hmi/ota \
  docs/evidence/build/ota-signed-check/native_stream_check.c \
  build-host/libtv4_ota_verification.a build-host/libtv4_json.a -lcrypto \
  -o /tmp/tv4-native-check
/tmp/tv4-native-check manifest.json manifest.sig /absolute/trusted/public.der \
  app.bin local-key-id 0.1.0-bench.1 0.1.0-bench.1 1.0.0
```

Configured BASE0.1.0-bench.1 также реально собран на6.1.1: public-only trust,
temporary CHECK policy,memory diagnostics,worker8192,operation frame432
(ceiling512),LVGL heap/ELF guards PASS. Не прошит; runtime qualification false.

Владелец утвердил начальные стендовые guards: internal64KiB/largest32KiB,
PSRAM1MiB/largest128KiB,remaining stack3072/8192. Это допуск первого замера,
не production floors из HEAD. SNTP unauthenticated, только23ноября2026
Asia/Almaty,anchor10min/skew5s. Пороги не снижаются автоматически; после дня
context не повторяется как новый стенд.

После первого прогона владелец выбрал MIT. Новый bundle содержит LICENSE и
attribution; source3a2e03d выше сохранён как история. Лицензированный payload
собирается/подписывается заново от clean commit с решением владельца, hashes
закрепляются новым локальным пакетом. Старый семифайловый payload не является
утверждённой внешней публикацией.

Hardware CHECK/SESSION4,FPS/latency,install/confirm/rollback/power-loss не приняты.
Exact prerelease требует отдельного owner approval; подпись/лицензия/merge
его не заменяют. На ESP32 fecc05d/A-B,F407 ee7d179 не открывался. Hardware I/O нет.
