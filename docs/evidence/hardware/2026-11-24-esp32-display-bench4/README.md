# Bench.4: signed INSTALL, независимый readback и сохранённый новый релиз

[Software/release/CI](../../build/esp32-ota-display-bench4/README.md),
[план](../../../plans/P1_OTA_DISPLAY_PROGRESS.md),
[числовой итог](measurements.json), [первый отказ](attempt1.json).
Source `6c7b1d56769fe8a084c4360e6c2df4bb594f58fa`;
Waveshare ESP32-S3-Touch-LCD-7 Rev1.2, ESP-IDF6.1.1, tv4-16m-ab-v1.

## Подтверждённый результат

Второй finite сеанс закрыт PASS за951.809s/1800s. На плате **оставлен
0.1.0-bench.4 в OTA1, native VALID**; обычного возврата старого образа нет.
Factory fecc05d сохранён как recovery, receiving `.3` того же display-fix source
остался VALID в OTA0. Две обычные USB write calls: app receiving в OTA0 и initial
otadata selection; candidate записан штатным HTTPS installer, не USB.
Bootloader/table/factory/PHY/gaps/resources/store сохранились; runtime NVS
разрешённо меняется Wi-Fi service. eFuse/option bytes/STM32/CAN/motion не затронуты.

| Проверка | Результат |
|---|---|
| Receiving control90s | self-test, полный memory trace,16 periodic samples, touch/Wi-Fi PASS |
| Signed metadata CHECK | state6/error0,9 resource checks |
| Signed HTTPS INSTALL | state10/error0,798 resource checks; TLS/подпись/SHA/размер/совместимость сохранены |
| Окна ресурсов | CHECK2,3,4,5; INSTALL2,3,4; serialized start/stop/cleanup PASS |
| Нормальный reboot | UART оставлен открытым, полный self-test и90s/≥12 samples до первого ROM attach |
| Настоящий BIN readback |1600464bytes, SHA-256 `1e3de8c629ceb1800c5ba1f947858a5a54e5cce1587810eaa45f7febf5e27945` |
| Native metadata | seq1/VALID2 OTA0 и seq2/VALID2 OTA1, точные native CRC/records/tails |
| Полная Flash | настоящий итоговый read16MiB, device MD5/SHA/two-NVMe; обе apps/padding и все защищённые области сверены |
| Завершающие boots | дополнительный reset/UART90 и final reset/UART30 PASS; новая версия остаётся работать |
| Recovery | full recovery0, обычного возврата0 |

SESSION local minimum internal **98391bytes**, PSRAM **5904064bytes**;
минимальный measured operation stack **7072/12288bytes**, touch age≤**16ms**.
Largest63488/PSRAM-largest5898240 в session reports. Это конкретный изолированный
прогон с прежними floors, не production worst-case/полный transient largest trace.

Владелец подтвердил стабильный экран при записи в первом сеансе этого же
неизменённого receiving image; во втором — установку и штатный перезапуск bench.4.
После закрытия сеанса владелец отдельно подтвердил главный экран, касания и
сохранённый Wi-Fi на уже оставленном bench.4. Численные FPS/touch-to-display
не измерялись; owner observations и UART self-test различаются.

## Первый сеанс: INSTALL успешен, общая процедура остановлена

CHECK9/INSTALL798/error0 и новое boot OTA1 наблюдались, но ранний пользовательский
reboot попал в прежний5s tail перед ROM-проверкой. Native metadata не совпала
с ожидаемой; полная общая приёмка не PASS. Неожиданные8KiB не были сохранены,
поэтому конкретное неожиданное state/bytes неизвестно. Отсутствие self-test до
ROM и timing указывают на преждевременное вмешательство стенда; это инженерная
диагностика, не доказательство потерянного PENDING/ABORTED record. Все изменённые
OTA0/OTA1/otadata восстановлены с verify/UART30; full recovery0.

Повторный отдельный one-use packet того же подписанного release дожидается
полного native self-test, IP event и90s healthy UART **до ROM**, сохраняет raw
metadata приватно **перед** проверкой и не ослабляет CRC/VALID/floors/TLS/timeouts.
15 fault +16 loader +8 collector host tests PASS. Исходная прошивка/подпись/release
не менялись; владелец заранее разрешил isolated-board tests, новые hardware
permission requests не добавлялись. Точный prerelease/context approval получен
отдельно.

## Снимки и пределы

Перед записью используется предыдущий полный read16MiB + свежий NVS24KiB;
собранные16MiB должны полностью совпасть с текущим device MD5 и двумя fsynced
копиями на разных NVMe. Это повторная полная сверка, а не новый read16MiB по USB.
**Итоговый** снимок уже действительно полностью считан16MiB, отдельно сверены
apps/metadata/protected regions и device MD5, сохранены две новые копии. Приватные
keys/contexts/NVS/raw UART/backup hashes и dumps не опубликованы.

Bench contexts действуют только до25ноября2026 08:00 Asia/Almaty; ключ/floors/CA/
hostname/date/timeouts прежние. Это экспериментальный prerelease. Network negative
install/rollback, Wi-Fi loss, interruption/power-loss, production policy/budget,
bit reproducibility и runtime5.3.2 остаются отдельными этапами. CAN/приводы не
подключались и аппаратно не проверялись.
