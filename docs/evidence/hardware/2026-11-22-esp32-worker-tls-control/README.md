# a5f8b7e — контроль остановлен, TLS не запускался, fecc05d возвращён

22 ноября2026 по отдельному exact-plan разрешению выполнен один сеанс
608.667s из разрешённых1800s. Источник main a5f8b7e/ESP-IDF6.1.1,
rev12-ota с memory diagnostics и worker8192. **Контроль не прошёл touch_fresh;
TLS image не записывался, три SNTP/HEAD пробы не выполнялись.** Это не TLS PASS.

## Подтверждено на плате

- Чип/Flash16MiB/параметры защиты совпали с разрешёнными preconditions.
  Fresh16777216bytes считаны, device MD5/SHA-256 и две новые приватные копии
  на разных NVMe с fsync PASS. SHA-256 снова
  `ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`;
  совпадение с прежним checkpoint не означает повторное использование файла.
- Только контрольное factory app a5f8b7e записано/verify; bootloader/table/
  otadata и Flash вне factory сохранены до запуска. PSRAM8MiB/XiP/RGB/GT911/
  UI-ready/self-test/window/startup mask127 PASS, no fatal log, error counters0.
- UART90.101s:16 periodic samples, из них15 после IP event. На uptime26335ms
  `touch_age_ms=4294967295`; остальные15 значений0–19ms, touch_errors0. Проверка
  touch_fresh намеренно оставлена FAIL. По этому счётчику нельзя утверждать,
  что реальные касания отсутствовали 49 дней или что измерена physical latency.
- После остановки возвращён **весь factory2MiB fecc05d из этого свежего снимка**,
  verify и сохранность вне factory до boot PASS; UART30.046s/4 samples PASS.
  Владелец подтвердил главный экран, касания, сохранённый Wi-Fi и OTA SETUP
  с недоступной установкой. Временный control UI отдельно владельцем не проверялся.
- Две normal writes, два UART captures, ноль diagnostic-TLS writes и ноль full
  recovery attempts. На ESP32 fecc05d/factory/A-B, F407 ee7d179 не открывался.
  eFuse/STM32/CAN/motion не изменялись; сеанс и разрешение закрыты.

[Полная сводка](summary.json), [numeric observations](observations.json),
[измерения](measurements.json), [артефакты](artifacts.json),
[build inspection](build-inspection.json), [проверки](checks.json).
Raw UART, dumps/NVS, serial/MAC и authorization receipts приватны.

## Числа и пределы

Control worker watermark6784 из8192bytes; BOOT local minimum15719,
BOOT-end largest9216, restored history7848. После IP event internal
free16759–16795/largest7680, PSRAMfree6090060. Один counter snapshot не является
полной одновременной картиной heap/stack; IP event не непрерывная radio load.
Return fecc05d: internal free22731–22779/largest7680/history6956,3 из4 samples
после IP. Эти числа не qualified floors, worst-case или TLS/crypto/install peak.
NVS не декодировалась, вся Flash между boot не сравнивалась, полный16MiB restore
не потребовался. Cold-power/soak/FPS/latency/slot confirm/rollback не испытывались.

## Программная причина и дальнейшая проверка

В исходном main два consumer clocks читались раньше snapshot touch queue:
перед freshness check и перед серией resource reads. Producer имеет priority3,
main priority1; он может опубликовать timestamp на1ms новее старого clock.
Unsigned subtraction даёт UINT32_MAX. Host injection в actual app_main/real LVGL
воспроизводит экспортный underflow и отдельный false-health path; исправление
читает clock после копирования snapshot. Реальные preemption timestamps этой
единственной аппаратной точки не трассировались; software regression — отдельное
доказательство дефекта, не аппаратная приёмка исправления.

Порог <100ms/500ms fault, строгий hardware parser и исходный failed sample
сохранены. Не подменять FAIL исключением точки или clamp к0. После software
review/CI — новый exact-image стенд только по новому разрешению. Ни prepared
TLS BIN, ни закрытый сеанс не разрешают повторную запись/чтение/reset.
