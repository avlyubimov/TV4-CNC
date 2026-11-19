# PR-05a build evidence

План/критерии: [PR05A_BUILD_CANDIDATES.md](../../../plans/PR05A_BUILD_CANDIDATES.md).
Контракт: [BUILD_CANDIDATES.md](../../../BUILD_CANDIDATES.md).

Дата: 19 ноября 2026. База main18b2bf5 (PR #26 слит, четыре workflow main PASS).
Финальный проверенный code source: `70416d936b95f09e4f99389e642add82f199234d`.
Позднейшие изменения — документация/evidence и CI-only exact workspace trust;
firmware C/SDK inputs прежние. CI заново собирает точный PR checkout SHA.
Local bundles честно ссылаются на70416d9, не на будущий HEAD.

## Результаты

| Проверка | Результат |
|---|---|
| Host + LVGL32/RGB565 | 19/19 CTest suites PASS, 1.52s |
| ASan/UBSan/LSan | 19/19 PASS, 3.62s; detect_leaks=1, halt_on_error=1, без suppressions |
| Новая Python suite | 16 tests PASS; SDK×profile fixture matrix и fault cases |
| Native SDK matrix | 8/8 build/config/full app/ELF/source/profile PASS |
| Independent review verifier | 8/8 PASS с локальным app; metadata-only replay отдельно без image |
| Deterministic bundle | 8/8, повторная упаковка тех же inputs побайтно совпала |
| Внешний sha256sum | 8×4 checksum entries PASS |
| Target workspace / linked symbols | Все8 ELF: 34000bytes, entry points/profile gates PASS |
| Неактивный candidate layout | Оба SDK parser/roundtrip PASS, active CSV не менялся |
| Dependency locks/settings | Diff относительно main отсутствует; profiles/reserves/records прежние |
| Workflow/docs/scope | YAML, Actions full SHA/contents:read, local links, diff/allowlist/privacy PASS |

| SDK | Profile | App bytes | Свободно в factory2MiB |
|---|---|---:|---:|
| 6.1.1 | headless | 1055968 | 1041184 |
| 6.1.1 | rev12 | 1574128 | 523024 |
| 6.1.1 | rev12-memory-bench | 1575792 | 521360 |
| 6.1.1 | rev12-tls-bench | 1578960 | 518192 |
| 5.3.2 | headless | 1070416 | 1026736 |
| 5.3.2 | rev12 | 1529872 | 567280 |
| 5.3.2 | rev12-memory-bench | 1531600 | 565552 |
| 5.3.2 | rev12-tls-bench | 1534864 | 562288 |

Все descriptor versions —70416d9, включая Docker linked worktree. Full clean
build trees первоначально созданы на21b4a34; после усиления только Python/tests/
workflow проверки все8 повторно собраны инкрементально на clean70416d9.
Firmware C/SDK/config inputs прежние, изменился Git display version descriptor.
Это не сравнение binary reproducibility между SDK/другими датами/чистыми builders.

## Команды и материалы

```bash
cmake -S . -B build-host -G Ninja -DLVGL_DIR=<exact-lvgl-9.2.2-source>
cmake --build build-host -j 4
ctest --test-dir build-host --output-on-failure
python3 tests/build_candidate_tests.py -v

cmake -S . -B build-sanitized -G Ninja -DLVGL_DIR=<exact-lvgl-9.2.2-source> \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitized --output-on-failure

# Для каждого из4 профилей:
TV4_IDF_PATH=<installed-6.1.1-sdk> ./tools/build_idf.sh PROFILE 6.1.1 local
./tools/build_idf.sh PROFILE 5.3.2 docker
python3 tools/check_ota_memory.py build-idf-SDK-PROFILE/tv4_hmi.elf
# create/verify — точные команды в BUILD_CANDIDATES.md; expected source70416d9.
sha256sum --check checksums.sha256
```

Sanitizers запускались вне ptrace sandbox, чтобы LSan работал штатно. Никакого
отключения leak detection для PASS. SDK symbol checks — точный Python snippet
из firmware.yml. Layout check — tools/check_ota_layout.py с parser обоих SDK.

`matrix.json` содержит exact app SHA/source/size. В SDK-PROFILE/ сохранены все
пять файлов каждого реального metadata bundle; их можно независимо проверить
`build_candidate.py verify` с expected source SHA выше. При отсутствии локального
binary правильный replay имеет image_checked=false. `verify-*.json` фиксируют
состоявшиеся локальные проверки с image_checked=true, не включают сам образ.
`workspace-*.json`, `symbols-*.log`, `build-*.log` и test/layout logs сохраняют
компактные результаты; полные локальные build trees/logs не включены.
`versions.json`/`actions-pins.json` задают версии; `source-hashes.json` фиксирует
финальные reviewed tools/tests/workflows (включая CI-only ownership fix).

## CI container ownership regression

Первый PR run9134fe5 собирал app/ELF, но candidate step отказал: Git exit128
при чтении HEAD. В SDK root container checkout принадлежит другому UID;
checkout action задаёт safe.directory в своём временном HOME, который не
переносится в последующие SDK shell steps. Это также мешало раннему Git version
discovery. Строгий source check остановил упаковку, fallback image не принят.

Firmware workflow теперь до build добавляет **только GITHUB_WORKSPACE** в
safe.directory внутри эфемерного container HOME и проверяет HEAD против
github.sha. Нет wildcard trust, изменения checkout ref или постоянного user config.

`ci-ownership-repro.log` — отдельная реальная OS/Git regression: private copy
standalone checkout70416d9 внутри SDK5.3.2 container, process uid0, checkout/.git
uid12345. До fix Git отверг его с dubious ownership/exit128; после exact path
trust clean HEAD/display version/source/candidate create/verify PASS. App input —
ранее собранный6.1.1 headless70416d9; это тест offline inspector, не новая сборка
6.1.1 внутри5.3.2. Host source/app mounts read-only, network=none; CHOWN capability
использовалась только для приватной временной копии, контейнер удалён. Host UID/
права не менялись. Docker Desktop переводит ownership bind mounts, поэтому
использована private container copy с реальным другим UID, не mock Git output.

Репродукция: copytree checkout в /tmp, chown только worktree/.git directories
uid12345, Git rev-parse →128; git config --global --add safe.directory с **этим
одним path** → HEAD70416d9/describe70416d9/status clean; далее обычные create/
verify с expected source70416d9. В actual CI exact checkout assertion выполняется
перед каждой из8 сборок. Все эти операции относятся к файлам/container, не плате.

## Ограничения

Bundle/inventory не подписаны и не являются trusted OTA manifest/attestation.
Clean source/ELF linkage не доказывают происхождение произвольного внешнего ELF
или авторство изменённых JSON/checksums. Runtime install/Flash/layout migration/
production key/версия/лицензия/release approval остаются отдельными gates.

Все публичные файлы allowlisted metadata, без app/ELF/Flash/NVS/raw configs или
private paths/credentials. Build IDF может печатать советы flash; эти команды
не исполнялись. Устройств/USB/UART/reset/SNTP/HTTPS probes в этапе нет. На плате
остаётся ordinary8f0d899 с ранее подтверждёнными HMI/touch/Wi-Fi. FPS/CPU/latency/
resource reserves/soak и startup PSRAM заново на железе не измерялись.
