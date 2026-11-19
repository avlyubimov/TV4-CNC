# PR-04m — software evidence, 19 ноября 2026

База main475be1031c7f58c01a94d4af0d13a83d270da059, PR #25 слит,4workflows CI success.
Результаты рабочего дерева до commit feat/ota-resource-guard: локальный
project_version475be10-dirty в6.1.1, Docker5.3.2 fallback1 (worktree Git metadata
вне mount). Source hashes и matrix.json сохранены; CI строит exact PR SHA.
Это не аппаратно проверенные/публикуемые firmware images.
[Scope и limitations](../../../OTA_RESOURCE_GUARD.md).

## Проверки

- Host CTest18/18 PASS, ASan/UBSan/LeakSanitizer18/18 PASS, detect_leaks=1 вне
  ptrace sandbox без suppressions. host.log/sanitizers.log/boundary-tests.log.
- Новый policy test: exact-floor pass, каждый из5 ресурсов ниже floor, zero/
  unreviewed/missing policy/reader/output, contradictory largest>free, invalid
  largest floor, reader failure с zero output, SIZE_MAX, host sampler unavailable.
- Signed end-to-end mock session: resource failure на каждой из94достигнутых
  boundaries в этом локальном run. Тест сначала получает N от healthy run и
  инъецирует1..N на тех же подписанных bytes: длина DER signature может меняться
  между runs. Так покрыты active fetch/chunk, post-close, authentication,
  verified candidate, finishing и last-step resource failure. Отказ sticky,
  candidate/metadata/HTTP очищены, hash contexts закрыты, opens==closes.
- Отдельные cases: late stack failure для image/metadata-only, missing report/
  policy/reader, unreviewed policy и host/native-stub unavailable до fetch,
  cancel/deadline/signature отказ с healthy resources, no reset/no re-run after
  terminal, resource cause сохраняется после cleanup/cancel/deadline.
- Все прежние17tests остались: UI/render XRGB8888/RGB565/Wi-Fi/crypto/HTTP/time/
  SNTP/HEAD/PSRAM/session/motion. В тестах нет реального native TLS/платы.
- Восемь SDK build/validation PASS (idf-*.json/log), новые guarded session/check/
  native sampler entry points retained/non-UND. Все прежние profile/date/bench
  symbol gates PASS. Target workspace **34000bytes на всех8ELF**, host34024:
  отдельный resource report не увеличил session workspace. size-*.json.
- SDK config/ordinary overlays/locks/active factory2MiB/layout unchanged от main;
  ordinary rev12 effective sdkconfig6.1.1 сравнен с8f0d899, differences0.
- Оба SDK layout parsers PASS. GCC -fanalyzer syntax-only resource/session PASS;
  native sampler SDK compiled, но не исполнялся. YAML/CI symbol snippet/relative
  links/JSON/diff checks PASS.

| SDK | Profile | App bytes | Build/validation |
|---|---|---:|---|
|6.1.1|headless|1055968|PASS|
|6.1.1|rev12|1574128|PASS|
|6.1.1|rev12-memory-bench|1575792|PASS|
|6.1.1|rev12-tls-bench|1578960|PASS|
|5.3.2|headless|1070416|PASS|
|5.3.2|rev12|1529872|PASS|
|5.3.2|rev12-memory-bench|1531600|PASS|
|5.3.2|rev12-tls-bench|1534864|PASS|

Новые символы сохранены в firmware для проверки SDK linkage, не runtime call.
В JSON compiler artifact hashes/Flash16MB/PSRAM/allocator/date/profile fields;
build logs сокращены до финального вывода, полные локально вне Git.

## Воспроизведение/версии

./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2), четыре existing
profiles, отдельные sdkconfig. Host CMake -G Ninja -DLVGL_DIR=../lvgl,build-j4,
ctest; sanitized flags -fsanitize=address,undefined -fno-omit-frame-pointer,
link -fsanitize=address,undefined, ASAN_OPTIONS=detect_leaks=1 вне ptrace.

SDK6.1.1 exact v6.1.1/git051e4c4eb20346e043cdd47bc388fde75ff8af63.
5.3.2 immutable espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a,
git9d7f2d69f50d1288526d4f1027108e314e8c879f. CI6.1.1 image digest прежний.
LVGL9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74/touch1.2.1/GT9111.1.3,
per-SDK locks и cJSON1.7.19 hashes/namespace прежние. Host tools versions.txt.

## Границы

Нет device/flash/reset/serial/SNTP/HEAD calls или нового runtime caller/task/UI/
production floors в этом PR. На плате **ordinary8f0d899**, owner HMI/touch/Wi-Fi
PASS; её historical measurements не переписываются software checks.
Snapshot current heap/HWM — не reservation, не transient-peak trace и не hard
preemption/overflow protection внутри SDK. Policy fixtures синтетические,
не production бюджет. Guard только на session phase/chunk boundaries; нет
runtime OTA body/installer/Flash/keys/release/eFuse/CAN/motion activation.
