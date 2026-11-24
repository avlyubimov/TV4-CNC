# P1: локальное и подписанное разрешения INSTALL

Provisioned context раньше автоматически открывал INSTALL после metadata CHECK.
Actual service/SDK regression воспроизводит постановку INSTALL в очередь без
отдельного допуска. Owner context и опубликованный unprovisioned bench.2 были
согласованы только для CHECK. Политика исполняется existing service/UI/session,
а не ещё одним слоем криптографии или транспортной библиотекой.

Локальный optional boolean `install_reviewed` по умолчанию false для старых
contexts и отсутствующего provisioning. `reviewed:true` и time/resource review
не означают разрешение записи. Generated header/public summary/связанный ELF
проверяют явную политику; неверные типы, stale/mixed/mismatched markers закрыты.
Owner context автоматически на true не менять. Manifest не включает локальный
допуск. CMake отслеживает JSON и generator для отзыва; key-file rotation требует
явного reconfigure и проверки новых exact artifacts.

Signed HMI component получает optional boolean `installable`; missing/false —
CHECK-only, true разрешает рассмотрение установки. Это часть подписанных точных
байтов. Signature проверяется до JSON; bounded lexical preflight теперь допускает
true/false, а строгая field policy сохраняет integer/shape/type/duplicate/encoding
ограничения. Missing legacy metadata остаётся пригодным для CHECK. Новый field
старые strict clients отвергнут; сначала нужен USB baseline с новой политикой.

Service/UI допускают INSTALL только при local review AND signed capability.
При отзыве допуска confirmation очищается, disabled callback не отправляет
INSTALL; «Только проверка» объясняет состояние. В fresh install session capability
проверяется до image allocation и sink.begin/Flash, затем native SDK adapter ещё
раз проверяет его до esp_ota_begin. Candidate identity/signature/hash/size/version/
hardware/protocol, ресурсы/time/CA/hostname/date, inactive slot/native rollback и
операторское двойное подтверждение сохраняются. CHECK/CANCEL/committed RESTART
прежние; автоматической установки нет. Новых задач, сетевых API/буферов/Flash
адресов и irreversible settings нет. Internal candidate вырос на один bool с
alignment; workspace/stack budgets проверяются действительной сборкой и стендом.

Программная приёмка:274 host/274 ASan/UBSan/LSan PASS с leaks enabled; actual
service operator CHECK→READY с обеими local policies и signed readonly candidate;
positive reviewed admission только в очередь, не утверждение hardware INSTALL.
Native adapter без capability gate воспроизводит прежний отказ теста. Настоящая
signed session с missing/false отвергается до sink.begin, но полный read-only
image pass остаётся успешным; native begin отвергает readonly candidate.
Independent Python/C policy сопоставляет missing/false/true, неверные типы,
duplicates и совместное peer field. Ephemeral signing keys только в host tests.
CHECK-bundle tool явно подписывает false и отвергает relabelled installable даже
с действительной подписью. Real LVGL render/RGB565 проверяет caption/layout,
disabled callback и отзыв confirmation. Actual-source GCC analyzer PASS для
manifest/JSON/session/installer и шести service local-policy вариантов.

Далее actual owner CHECK-only control/auto ELF обоих pinned SDK, CI и новый
finite signed read-only CHECK с fresh16MiB/two-NVMe/verified fecc05d return.
Не переносить hardware PASS a74bfcd на новый source. Native USB confirmation/
rollback проверен родителем; network INSTALL/Flash peaks/Wi-Fi/power failure/
production budgets ещё открыты. Published bench.2 не изменять и не устанавливать.
Release authoring пока остаётся CHECK-only. Новый configured installable payload,
SemVer/policy/publication требуют отдельного review. Новый release/tag и self-merge
не входят в автономное разрешение на board checks до24ноября08:00 Asia/Almaty.

## Завершённая приёмка

[Реальный 3b09bba CHECK](../evidence/hardware/2026-11-24-esp32-install-policy-3b09bba/README.md)
и [шесть actual ELF/software checks](../evidence/build/esp32-ota-install-policy/README.md)
PASS. Read-only policy boot markers, control38checks, full CHECK406checks/error0,
verified fecc05d return подтверждены. Это не network INSTALL или physical UI.
Authoring нового installable payload — следующий отдельный PR83; owner plan и
новый релиз по-прежнему требуют отдельного review, ни один key/context не повышен.
