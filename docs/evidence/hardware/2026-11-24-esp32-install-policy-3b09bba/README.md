# CHECK-only INSTALL policy: аппаратный стенд PASS

Exact source `3b09bba97ff94b56c0be529c9e9cbc88ba55247c`, открытый PR #82,
Waveshare ESP32-S3-Touch-LCD-7 Rev1.2, ESP-IDF6.1.1. Сеанс23ноября22:52:49 —
23:11:04 UTC (24ноября Asia/Almaty), закрыт за1094,730s из1800s. Действовало
общее явное разрешение владельца на обратимые проверки изолированных плат до
08:00; отдельный per-hash ответ владельца не приписывается. CI source30/30 и
unchanged main15/15 PASS до USB; [software evidence](../../build/esp32-ota-install-policy/README.md).

Проверены chip/revision, Flash16MiB и отключённая защита. Свежие16MiB считаны
штатным RAM stub, сверены device MD5, SHA-256 и сохранены в двух приватных
копиях на разных физических NVMe. До любого diagnostic write требовались обе
проверенные копии. Бэкапы, их hashes и raw UART не опубликованы.

Последовательность: configured control app-only/verify/preservation/UART90
PASS (38 strict checks) → ровно один auto app-only/verify/preservation/UART420
PASS → весь исходный factory2MiB fecc05d/verify/preservation/UART30 PASS.
Три normal SDK write calls, три UART captures; full recovery0. После каждой
записи до запуска сверены bytes вне factory; после CHECK сверена неизменность
otadata/OTA slots/resources/store. Bootloader/table/otadata не записывались;
NVS законно обновлялся приложением и Wi-Fi credentials сохранены.

Единственный полный signed image CHECK: IMAGE_VERIFIED7/error0/http0/policy0/
image0/resource0,406 checks; итог READY5/error0, attempted1/pass1/read_only1.
Published bench.2 —1598560bytes/SHA
`017236a2abec4f71b8737a396a9601fe603d61931babc05405a9df874d1663de`.
Его manifest без installable остаётся CHECK-only; локальный install_reviewed
false и ровно один boot marker install_policy=check_only в обоих новых образах.
Flash sink/INSTALL/OTA selection не выполнялись. Прежние подпись/SHA/size/
compatibility, CA/hostname/date, UTC anchor, timeout и resource guards сохранены.

SESSION scope1 local min internal99371bytes, сетевой largest63488bytes,
PSRAM local min5909976/largest5898240, остаток worker6912/12288. На completion
free internal119347/PSRAM5947144; cleanup119675/5981992. Scope0 minima в JSON —
восстановленная lifetime history, не новое локальное окно и не production floor.
Пять окон BOOT/WORKSPACE/SNTP/SESSION/CLEANUP закрыты с start/stop0. Во время
CHECK touch polling age2–15ms/errors0; control10–12ms и return2–13ms. Это
freshness software snapshot, не физическая touch-to-display latency или FPS.

Итоговые measurements принимаются только после session_closed. Промежуточная
видимость растущего файла не использована для PASS/FAIL или повторных resets.
На ESP32 оставлен точный factory fecc05d, на F407 прежний ee7d179. Повторной
записи/сброса после закрытия, тестов питания, eFuse, CAN, STM32 или приводов нет.
Physical UI ночью владелец не наблюдал; UART подтверждает RGB/GT911/PSRAM/Wi-Fi
и self-test. Runtime5.3.2 не проверялся. Новый source CHECK-qualified, но signed
network INSTALL/Flash-write resource peak/Wi-Fi or power-loss/production budget
ещё не приняты. Native USB confirmation/rollback a74bfcd — отдельный ранее
завершённый результат. Следующий шаг — другой reviewed configured installable
payload и его отдельная публикация; bench.2 не менять и не устанавливать.
