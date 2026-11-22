# Software fix — consumer clock после touch snapshot

Код a685614, база main a5f8b7e после PR #69. Из чистого source worktree
собраны16 реальных вариантов pinned ESP-IDF6.1.1/5.3.2, включая normal/trace/
worker8192/worker-TLS. Использовались отдельные incremental build trees.
Последующий commit — docs/evidence only; descriptor этих BIN/ELF — a685614,
не SemVer релиза. Новые бинарники не записывались на плату и не опубликованы.

Основание: [аппаратный контроль](../../hardware/2026-11-22-esp32-worker-tls-control/README.md)
с одной точкой UINT32_MAX и автоматическим возвратом fecc05d. Этот исходный
touch_fresh FAIL сохранён; TLS image тогда не записывался и HEAD не исполнялся.
После закрытия того сеанса выполнялись только software checks.

## Воспроизведение и исправление

Actual app_main с настоящим LVGL9.2.2 и SDK boundary:

| Сценарий | До исправления | После |
|---|---|---|
| Producer публикует свежий touch во время queue-peek, freshness clock уже прочитан | assert healthy FAIL | PASS |
| Та же публикация во время resource snapshot, report clock уже прочитан | assert touch_age<100 FAIL | PASS |
| Действительно устаревший на1000ms sample | fault/health refusal PASS | fault/health refusal PASS |

[Before](regression-before.json), [after](regression-after.json),
[health assertion](before-touch-clock-race.log), [age assertion](before-touch-report-race.log).
Так воспроизводится `uint32_t(t-(t+1))==UINT32_MAX`, без маскировки clamp к0.
Исправлены два порядка чтения в существующем main; boot input/read_touch уже
читали clock после snapshot и сохранены. <100ms freshness и500ms fault/health,
poll20ms, held-touch release и latched fault остаются прежними.

## Проверки

- [Host](host-tests.log):159/159 PASS, включая три новых regression в actual
  app_main/real LVGL и все существующие startup/rollback/service/crypto tests.
- [ASan/UBSan/LSan](sanitizer-tests.log):159/159 PASS, detect_leaks=1,
  halt_on_error=1. Leak checking не отключалось.
- [GCC15.2 fanalyzer](analyzer-result.json): actual ESP app_main через тот же
  SDK boundary/real LVGL headers PASS; [команда](app-main-analyzer-command.json).
- [Реальная SDK matrix](matrix.json):16/16 PASS. SDK6.1.1 installed commit
  051e4c4eb20346e043cdd47bc388fde75ff8af63; SDK5.3.2 в прежнем immutable Docker
  5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a.
  CI6.1.1 сохраняет pinned image2b4f966c37eaedc19542b0637a5f850b2eff9478d8e858192aa0073d92c2dc91.
- [Сравнение](comparison.json): sdkconfig/partition hashes одинаковы baseline
  во всех16 вариантах; dependency locks прежние. App_main own compiler frame
  не изменился ни в одном варианте. Некоторые Flash images выросли на16bytes;
  это не runtime stack/heap/CPU/performance qualification.

Новые cases зарегистрированы в существующей CMake suite и автоматически
выполняются обоими host jobs CI. Pins/LVGL/touch/GT911, GPIO/CH422G/RGB, clocks,
DMA reserve64KiB, TLS16384/4096, task sizes, memory windows/IDs и OTA gates
не менялись. В diagnostic capture добавлен один clock read после snapshot;
нет новых task/lock/allocation, нового I/O в LVGL callback или relaxed parser.

## Предел результата

Software injections доказывают исходный дефект и regression исправления.
На физической плате исправление пока не испытывалось. На ESP32 оставлен
fecc05d с подтверждённым владельцем HMI/touch/Wi-Fi/SETUP; разрешение стенда
закрыто. Реальные TLS/crypto/installer peaks, signed slot install/confirmation/
rollback/cold-power/soak остаются открыты. Следующий hardware запуск требует
нового exact-image пакета и отдельного разрешения; использованный a5f8b7e
пакет не повторять. Никаких eFuse/STM32/CAN/motion/key/release/merge операций.
