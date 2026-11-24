# Сетевой INSTALL bench.3 и незавершённая аппаратная приёмка

**Сетевая транзакция INSTALL подтверждена на четвёртом сеансе.** Общий
плановый стенд не PASS: операторские операции и перезапуск попали в контрольное
окно, затем сценарий вернул прежний factory до независимого Flash/VALID readback.
Ниже отделены измеренные результаты от оставшихся проверок.

Образы из чистого main `08932ee4ef57e585d0101d90cc54800d6e87ae3b`;
[сборки, подпись, публикация и обратная загрузка](../../build/esp32-ota-install-bench3/README.md).
Платы разрешены владельцем заранее; новый prerelease и bench contexts отдельно
одобрены. eFuse, STM32, CAN и приводы не затрагивались. Каждый сеанс ограничен
1800s с резервом420s для единственной условной попытки полного восстановления.

## Сеанс 1: контроль/CHECK PASS, INSTALL не завершён в общем окне

[Числовые результаты](attempt1.json). Сеанс закрыт за1098,544s: fresh16MiB,
device MD5/SHA, две копии на разных NVMe → baseline `0.1.0-bench.1`/verify и
preservation вне factory → UART90 control PASS → операторский CHECK PASS →
операторские «Установить»/«Подтвердить» → лимит общего окна420s → полный возврат
исходных factory2MiB/ota_0 3MiB/otadata8KiB, verify/preservation/UART30 PASS.

Контроль:37 boot checks и explicit owner INSTALL policy PASS,16 periodic samples;
internal free≥120599bytes/largest≥63488, OTA stack≥10880/12288, touch age≤14ms.
Metadata CHECK: CANDIDATE_READY6/error0,9 resource checks, internal119699/
largest63488, stack7072. Новый manifest подписан и совместим; image CHECK/INSTALL
completion и загрузка скачанного слота **не подтверждены этим сеансом**.

Ожидание первого нажатия использовало большую часть общего420s окна. После
двойного подтверждения INSTALL начался, но его terminal result до общего лимита
не получен. Это отказ стендового срока; по нему нельзя утверждать ошибку
криптографии, TLS или SDK installer и нельзя присваивать INSTALL PASS.
4 обычных USB write calls/3 UART captures; полный16MiB recovery не понадобился.
Исходные bootloader/table/PHY/ota_1/resources/store сохранены, runtime NVS допускается
менять штатному Wi-Fi. На ESP32 возвращён fecc05d, F407 ee7d179 не открывался.

## Следующий отдельный пакет

Те же compiled bytes и опубликованный immutable bench.3, без изменения firmware:
CHECK получает180s; после успешного CHECK начинается отдельное окно INSTALL360s.
Максимум540s для двух фаз, прежние общие1800s и recovery reserve сохраняются.
16 procedure methods (включая поздний CHECK/оба deadlines),13 loader methods PASS;
предыдущий сеанс должен быть closed с verified fecc05d return. Новые fresh16MiB и
две независимые копии обязательны. Source/CA/hostname/date/signature/floors,
monitor ownership, собственные network timeouts и двухшаговое UI confirmation
сохраняются. Закрытый пакет первого сеанса повторно не используется.

## Сеанс 2: контроль PASS, CHECK не поступил в180s

[Результаты](attempt2.json): закрыт за858,721s, control PASS, ни одного session
report или terminal CHECK/INSTALL в180s. Владелец сообщил, что не успевает
подойти к экрану. Все исходные factory/ota_0/otadata возвращены/verify/UART30,
full recovery0; на ESP32 fecc05d. Это не отказ firmware installer.

## Сеанс 3: отказ получения времени до HTTPS

После сообщения владельца «я у платы» подготовлен новый ограниченный пакет.
[Результаты](attempt3.json): control PASS; service stage9/error3 после SNTP window
10003ms, Wi-Fi disconnects0/touch age5ms, local internal minimum118127bytes,
stack10032. HTTPS/session reports отсутствуют, INSTALL не допущен. Source
`operation()` использует error3 для SNTP result/cleanup failure; точный enum не
логируется. Тайм-аут — вывод по elapsed, а не выдуманный native result.
Исходные разделы возвращены/verify/UART30, full recovery0. TLS/clock guards
сработали до использования недостоверного времени.

Для этого сеанса предыдущий full16MiB read был привязан SHA-256 и verified
complete return. Заново считан только текущий NVS24KiB, затем assembled16MiB
**целиком** сопоставлены с Flash через штатный SDK verify/MD5. Новый SHA-256 и
две NVMe-копии проверены перед записью. Это revalidated snapshot, не утверждение
нового полного USB read. Любое изменение вне NVS отказывает до записи.
20 procedure methods/13 loader methods PASS, включая повреждение source,
truncated NVS и изменения boot/table/PHY/factory/otadata/обоих slots/resources.

Один дополнительный read-only host SNTP probe получил verified ответ за20ms
в одобренном календарном диапазоне; часы ПК/платы им не менялись. Это не доказательство
ESP32 reachability и не trusted anchor для firmware. Следующий отдельный ограниченный
повтор использует те же compiled bytes, SNTP10s, CA/hostname/date, floors и UI gates.
Production budget, physical FPS/touch-to-display, runtime5.3.2, Wi-Fi/power-loss
остаются открытыми.

## Сеанс 4: успешный INSTALL и короткое наблюдение новой загрузки

[Результаты](attempt4.json). Сеанс закрыт за305,379s. Те же exact baseline и
published candidate, refreshed NVS24KiB/full16MiB SDK verify/две NVMe-копии.
Оператор выполнил CHECK/двойной INSTALL и перезапуск в исходном90s capture.
Сценарий ожидал в этом capture только один idle baseline boot; второй policy
marker привёл к `Wrong receiving local policy` и штатному возврату. Это ошибка
соответствия сценария фактическому порядку действий, а не отказ INSTALL.

Сохранённые байты повторно разделены по двум ROM boot markers. Исходный строгий
transaction parser без изменения guards подтвердил полный поток:

- metadata CANDIDATE_READY6/error0/9resource checks → READY5/error0;
- signed network INSTALL state10/error0, HTTP/policy/image/resource errors0,
  **798 resource checks** → RESTART8/error0;
- семь serialized окон CHECK/INSTALL, start/stop complete0/1, все boundary и
  local minimum floors, time order, отсутствие fatal/HTTP errors и touch freshness;
- SESSION local min internal97763/PSRAM5903620bytes, operation stack≥7072/12288,
  touch age≤18ms. Last admission largest63488/PSRAM largest5898240.

Это тот же неизменённый session/SDK writer, который проверяет owner signature,
точные size/SHA/descriptor/hardware/version до активации. В UART затем загружен
ota_0 `0.1.0-bench.3`; ELF prefix `d90650d48` совпадает с опубликованным actual
image, configured self-test `checks_pass=1 sdk_finish=1 worker12288` зафиксирован.
Владелец подтвердил download/write/install/reboot и нормальный новый экран.

Нельзя объявлять полный boot/Flash acceptance: запись captured только до uptime
6665ms/одного periodic sample. Два original checks `periodic_resources` и
`increasing_uptime` не PASS, Wi-Fi reconnect не успел попасть в UART. Независимый
readback точных bytes скачанного слота и otadata VALID не выполнен до возврата.
`sdk_finish=1` — наблюдаемый SDK результат, не выдуманное отдельное чтение VALID.

Все исходные factory/ota_0/otadata возвращены и проверены, full recovery0, на
ESP32 fecc05d. После завершения владелец изменил дальнейшую политику: **при успехе
оставлять проверенный новый релиз на плате**, не выполнять обычный возврат.
Следующие пакеты должны учитывать это; безопасное native rollback сохраняется.

## Найденный дефект отображения и следующий результат

Владелец сообщил о дёргании всего интерфейса во время изменения счётчика записи
и предоставил приватное16,7s видео. Кадры просмотрены локально; видео не публикуется.
В actual config `LCD_RGB_ISR_IRAM_SAFE` выключен. Pinned SDK Kconfig описывает
защиту LCD/GDMA ISR во время Flash operations; RGB single-framebuffer/bounce10
shares PSRAM bandwidth, а driver уже восстанавливает detected underflow на VSYNC.
Кроме того, `firmware_view_ota_update()` заново задаёт все captions при каждом
изменении snapshot; LVGL9 `lv_label_set_text()` invalidates и reallocates даже
неизменённый текст. Это конкретные факторы для следующего fix, не доказанный
единственный root cause и не измерение physical FPS/latency.

Следующий отдельный PR: IRAM-safe LCD/GDMA для explicit OTA, устранение лишних
перерисовок progress/captions, host/LVGL regression и оба SDK; затем resource/visual
стенд с точным readback/VALID и сохранением нового проверенного релиза на плате.
P1 ещё не завершён: network rollback/fault trials, Wi-Fi/power-loss и production
policy/budget открыты. Published bench.3 не пересобирать и не заменять.
