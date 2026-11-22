# P1 ESP32 OTA: программная проверка

База main d1317a8 / PR #1–58. Исполняемые исходники и пять локальных сборок:
**clean `f3ecd916cab47483f994977795c180271c746777`**. Последующий коммит добавляет
только это evidence; его SHA не приписывается уже собранным бинарникам.
Владелец выбрал настройку без ключа. Устройства не открывались, Flash/reset/
SNTP/HTTPS на платах не выполнялись; текущие ESP32 ordinary8f0d899 и F407 ee7d179
не менялись. Test keys существуют только во временных host fixtures.

| Проверка | Наблюдённый результат | Доказательство |
|---|---|---|
| Полный host/LVGL/native набор | 79/79 PASS, 5,58s | [host-tests](host-tests.log) |
| ASan/UBSan/LSan | 79/79 PASS, 12,44s, вне ptrace, без отключения leak detection | [sanitizers](sanitizers.log) |
| Signed session + реальный SDK adapter с fake SDK | PASS: begin/write/seal/activate errors, cancel, подпись/digest, truncation/Wi-Fi failures; resource fault на всех124границах positive trace | [fault tests](ota-fault-tests.log) |
| Сохранённый read-only session | PASS, fault94границы, workspace34024bytes host | [fault tests](ota-fault-tests.log) |
| Boot window и SDK accept/rollback | PASS: непрерывная свежесть3s, stale pulse/пропуск/health failure/30s/wrap; invalid/pending/SDK failures, factory сохранён | [fault tests](ota-fault-tests.log) |
| Local public context и USB package | 13 tests PASS: no-key default/reset, private/wrong key, missing/unknown/duplicate context, версии/floors/CI leak, dirty/stale source/ELF, адреса/hash/inventory/security/XiP | [fault tests](ota-fault-tests.log) |
| LVGL states/approval/layout | 800×480, 32-bit/RGB565, Cyrillic, все OTA states/31-byte version/44px buttons; confirm/cancel/hash replacement/reboot, ordinary navigation/leak checks PASS | render/render_rgb565 в [host-tests](host-tests.log); PNG только локально outputs/p1-esp32-ota |
| ESP-IDF6.1.1 | 5/5 profiles build/config/header/Flash/PSRAM/partition/symbol/sizeof PASS; writer только rev12-ota | [build-summary](build-summary.json), [config](ota-configuration.json) |
| USB bundle exact source/app/ELF/16MiB layout/4write hashes + independent verify | PASS, unprovisioned, hardware/release authorized=false | [plan](migration-plan.json), [checksums](migration-SHA256SUMS.txt) |
| GCC analyzer actual session/service-window/SDK adapter | 3 translation units PASS | [checks](checks.log) |
| Changed-document links / diff / shell / Python / YAML | PASS | [checks](checks.log) |

SDK6.1.1 commit051e4..., точный полный SHA — build-summary; dependency/SDK/container
pins не менялись. На хосте GCC15.2, target Xtensa compiler — существующий pinned
IDF6.1.1 toolchain. Локальной5.3.2 установки нет: совместимость5.3.2 проверяется
существующим immutable SDK container в расширенной CI matrix5profiles×2SDK.
CI выполняет builds/negative tests на exact PR head; ссылку на результаты см. в
Checks PR. Эти локальные журналы не выдаются за выполненный5.3.2 hardware тест.

## Размеры и память

Target workspace по константе ELF: **34000 bytes** во всех5профилях, PSRAM-only
allocation сохранён; host34024 отличается ABI pointers/alignment. OTA app
1597984bytes, запас factory2MiB499168bytes (24%), OTA slots по3MiB. Bootloader
и app SHA-256 указаны отдельно; BIN/ELF/полные карты/ключи/дампы в Git не добавлялись.
Настоящие локальные артефакты: `build-idf-6.1.1-rev12-ota/` и
`build-migration-f3ecd91/` в рабочей копии.

Для RGB bounce-buffer/cache совместимости только OTA profile включает SDK XiP:
инструкции16MMUpages +rodata8pages = **1572864bytes (1,5MiB) PSRAM**, округление64KiB
по ELF reserved ranges. Single framebuffer дополнительно768000bytes; DMA bounce
buffers/32KiB draw buffer/64KiB internal reserve/PSRAM80MHz не урезались. Это
статический бюджет, а не free/largest heap после запуска; другие allocations,
TLS peak/fragmentation и configured installer требуют аппаратного измерения.
[Linked sections](linked-size.json) не являются runtime heap watermark.

[Compiler frames](compiler-frames.json):59функций из6actual target translation
units unprovisioned profile, максимум отдельного frame960bytes; operation432,
boot worker64. Это не сумма call chain, SDK/TLS frames или runtime stack headroom.
Worker stack3072bytes при unconfigured,8192 при будущей настроенной установке.
Предыдущий TLS HEAD minimum не превращён в production admission floors.

## Воспроизводимые команды и границы

```bash
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$PWD/../lvgl"
cmake --build build-host -j4
ctest --test-dir build-host --output-on-failure
# build-sanitize: -fsanitize=address,undefined -fno-omit-frame-pointer;
# executable linker flags -fsanitize=address,undefined, стандартный LSan.
ctest --test-dir build-sanitize --output-on-failure
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12-ota 6.1.1 local
python3 tools/prepare_ota_migration.py --build build-idf-6.1.1-rev12-ota --sdk 6.1.1 --output build-migration-review
python3 tools/prepare_ota_migration.py --verify build-migration-review
```

`build-host` использует LVGL9.2.2 exact pinned source; existing independent STM32
crypto Mbed TLS3.6.7 archive/source178 verification остаются частью набора.
Все5профилей собраны командой build_idf.sh по отдельности, без устройства.
Raw локальные логи сохранены в outputs/p1-esp32-ota; в публичном evidence
достаточные текстовые результаты без частных путей и бинарных образов.

Не выполнены: аппаратная USB migration/recovery, configured signed OTA download/
install/reboot/confirm/rollback, потеря питания на границах Flash/otadata,
перезагрузка/выделение PSRAM XiP на экземпляре платы, реальные SRAM/PSRAM/stack
peak/low-water, FPS/touch latency/tearing под записью. Без owner key/versions/
resource/time context установка закрыта. P2 Releases и production machine gates
не реализуются/не объявляются принятыми этим evidence. Никакого eFuse/option
bytes/OTP, CAN/motion или самостоятельных merge/release.

[Процедура и следующий отдельный стенд](../../../ESP32_OTA_INSTALLATION.md).
