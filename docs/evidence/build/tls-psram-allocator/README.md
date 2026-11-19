# PR-04k — software evidence, 19 ноября 2026

База main fde5a28 (PR #21 слит, все4 workflows success). Результаты рабочего
дерева **до commit** ветки fix/tls-psram-allocator; локальные images имеют
project_version=fde5a28-dirty в6.1.1 и fallback1 вDocker5.3.2 (см. matrix.json):
worktree Git metadata вне container mount. Это локальный build fallback, не версия
для выпуска; exact source ниже и хеши изменяемого кода сохранены. Точные PR/CI commit artifacts
создаются CI. Эти локальные images не прошивались и не предлагаются как
аппаратно проверенные. [Scope и review](../../../TLS_MEMORY_REVIEW.md).

## Результаты

- Host CTest17/17 PASS, включая LVGL RGB565/XRGB8888, UI/Wi-Fi/OTA/time/SNTP/
  HEAD lifecycle/cleanup/error cases и motion core. host.log.
- ASan/UBSan/LeakSanitizer17/17 PASS, detect_leaks=1, без suppressions:
  sanitizers.log. Первый запуск под ptrace sandbox завершался fatal error LSan;
  тот же набор успешно выполнен вне sandbox, checks не отключались.
- Build validator10 tests PASS (config-negative.log). Новые проверки отклоняют
  старый INTERNAL choice в TLS bench, missing/multiple/fallback allocator modes,
  отсутствие CA/UTC/X.509 date flags, insecure option, урезанные TLS records,
  отсутствие PSRAM/изменение internal reserve и EXTERNAL в ordinary profiles.
- Восемь build/validation PASS: Flash16MB/chip/PSRAM/factory bounds/profile gates,
  mbedTLS allocator/certificate-date flags и хеши в idf-*.json, вывод в *.log.
  Build logs здесь сокращены до финала компиляции; полные логи локально вне Git.
- Все8 ELF: OTA entry points linked/non-UND, TLS worker только вTLS bench,
  memory worker только вmemory bench. Workspace constant34000bytes, не runtime
  allocation measurement. size-*.json. В двух TLS ELF linked
  mbedtls_x509_time_cmp/gmtime, соответствуют новым CI assertions.
- allocator-*.txt: disassembly реального esp_mbedtls_mem_calloc из обоих TLS
  ELF, heap_caps_calloc с caps0x404 = MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT.
  Это native code inspection, не запуск allocation на Xtensa.
- Оба SDK parser: candidate layout PASS (layout-*.log). Активный factory2MiB,
  partition CSV/ordinary overlays и per-SDK locks byte-for-byte от main.
  Эффективный sdkconfig обычного rev126.1.1 также сравнен с b085abb: differences0.
- GCC -fanalyzer syntax-only portable TLS probe/native host stub PASS;
  native worker проверен SDK компиляцией, не host analyzer. YAML/relative links/
  git diff --check PASS. No hardware/device/network probe operations.

| SDK | Profile | App bytes | mbedTLS allocator | X.509 dates | Build |
|---|---|---:|---|---|---|
|6.1.1|headless|1055488|internal|off (unchanged)|PASS|
|6.1.1|rev12|1573568|internal|off (unchanged)|PASS|
|6.1.1|rev12-memory-bench|1575216|internal|off (unchanged)|PASS|
|6.1.1|rev12-tls-bench|1578176|PSRAM-only|on|PASS|
|5.3.2|headless|1069920|internal|off (unchanged)|PASS|
|5.3.2|rev12|1529280|internal|off (unchanged)|PASS|
|5.3.2|rev12-memory-bench|1531040|internal|off (unchanged)|PASS|
|5.3.2|rev12-tls-bench|1534016|PSRAM-only|on|PASS|

## Воспроизведение и версии

./tools/build_idf.sh PROFILE SDK local для6.1.1 либо docker для5.3.2;
PROFILE=headless/rev12/rev12-memory-bench/rev12-tls-bench, каждый со своим sdkconfig.
Для старого TLS sdkconfig — сохранить и перегенерировать по TLS_MEMORY_REVIEW.
Build script не выполняет flash/monitor. В контейнер не передавались устройства.
Host: cmake -S . -B build-host -G Ninja -DLVGL_DIR=../lvgl, build -j4, ctest.
Sanitized: аналогично build-sanitized с -fsanitize=address,undefined
-fno-omit-frame-pointer и ASAN_OPTIONS=detect_leaks=1 (вне ptrace).

SDK6.1.1 установленный exact v6.1.1, git051e4c4eb20346e043cdd47bc388fde75ff8af63.
SDK5.3.2 official espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a,
git9d7f2d69f50d1288526d4f1027108e314e8c879f. CI6.1.1 image digest прежний.
В обоих SDK SHA256 components/mbedtls/port/esp_mem.c:
5b4d710ddb779b83c36aadb861bbfa1e59dac236e72f4b545fad2fb2f5154bb0.
Kconfig/esp_config.h/x509_crt.c просмотрены для обоих SDK: external mode без
fallback; HAVE_TIME_DATE включает проверки valid_from/valid_to всей цепочки.
LVGL9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74, touch1.2.1,
GT9111.1.3, SDK locks не изменены; OpenSSL/GCC/CMake/Ninja — versions.txt.

## Что не доказано

Нет запуска SNTP/HTTPS/сертификатов/allocations/reset/serial на плате.
На ней остаётся **ordinary rev12 b085abb**, owner-confirmed HMI/touch/Wi-Fi.
Новый allocator/сертификат/FPS/latency/soak/heap/stack/production budget требуют
отдельной новой аппаратной проверки после review/CI и разрешения на exact image.
PSRAM placement не отменяет internal HTTP/TCP/crypto нужды и не гарантирует PASS.
Результат прежней пробы b085abb (TLS OOM) не переписывается новым software build.
Нет OTA body/install/Flash/boot selection/motion/security settings/keys/release.
