# PR-07e — завершение проверки и отчёт отправителю

20 ноября2026. База main `e0bd84e`, PR #1–32 слиты, пять main workflows PASS.
[План](plans/PR07E_UPDATE_OUTCOME.md), [outcome API](../protocol/can/update_outcome.h),
[completion API](../firmware/stm32-bootloader/include/update_completion.h),
[evidence](evidence/build/stm32-update-outcome/README.md).

После DATA completion существовали независимые crypto/model/lease проверки,
но не было конечного отчёта и его доставки. Теперь software path связывает
DATA → signature/SHA → model → lease close → outcome/ACK. Проверяется только
в памяти/ARM compile. Нет runtime caller, CAN wire driver, Flash writer/jump.

## Значение результата

Это **исторический отчёт** о прошлой программной проверке, а не действующий
candidate, firmware install token или физическая готовность. После clean close
source может законно измениться/исчезнуть. Перед будущей установкой нужны новая
negotiation/identity, полная проверка сохранённых bytes и fresh physical quiescent/
operator/recovery/erase/vector gates. ACK не ARM/START и не разрешает движение.

| Code | Значение |
|---|---|
| CHECKED_CLOSED | P-256/SHA/metadata/local model прошли и lease чисто закрыт; только история |
| REJECTED_CLOSED | Signature/key/schema/compatibility/rollback/hash/length/profile/vectors отвергнуты, ресурс закрыт |
| CANCELLED_CLOSED | Отменено, ресурс закрыт |
| FAILED_CLOSED | Source/context/clock/deadline/release/binding/internal failure, ресурс закрыт |
| QUARANTINED | Ресурс не подтверждён закрытым; owner продолжает удерживать объекты/handle |

Reasons нормализованы и проверяются вместе с code, без raw SDK error numbers.
Неизвестные и неканонические пары отвергаются. REJECTED/FAILED не обозначают
завершённую установку; вообще ни одного install state здесь нет.

## Bootloader-local completion

Start принимает только EMPTY completion и BOUND [handoff](STM32_UPDATE_HANDOFF.md).
Return true означает принятую операцию, а не успешную проверку. Caller задаёт
expected DATA descriptor + SHA точных запрошенных metadata bytes и trusted local
[memory profile](STM32_IMAGE_PREFLIGHT.md); оба копируются до callbacks.
Expected hash сам по себе не trusted signature. Положительный отчёт требует
совпадения с independently verified metadata SHA.

Worker вызывает реальные существующие full admission и vector/model preflight
из того же immutable lease. Не принимает success boolean, старый receipt или
remote «готов». Затем закрывает lease. Guard после close callback снова проверяет
owner/boot/policy, RX context, monotonic clock/lifetime и cancellation до публикации.
Смена epoch/cancel/deadline в release callback не создаёт положительный отчёт.

Чистое завершение даёт READY. Profile/inspection scratch очищаются, ссылки на
handoff/RX/observer context сбрасываются; snapshot/clear/READY poll не вызывают
callbacks. Пока completion активен, эти borrowed objects остаются валидными
до выхода из start/poll, включая проверки после handoff CLOSED. Workspace
заимствуется только на синхронный start и очищается существующими проверками.

Одна release attempt на start/poll. Failure даёт CLOSING, snapshot пока false;
regular poll повторяет только cleanup, без crypto/read/resume. Даже последующий
release success после ошибки не создаёт CHECKED_CLOSED: первая причина сохранена.
Исчерпание прежнего handoff retry limit даёт QUARANTINED и отрицательный snapshot,
но **не освобождает ownership**. Clear/restart/ACK не забывают handle и не заменяют
recovery. Force-close/reset API нет. Reentrant abort только ставит cancel;
release не выполняется внутри активного read/observer callback.

## Отдельный logical outcome channel

Это C11 structs/API, **не raw CAN frames или production wire version**.
Не выбираются IDs/bitrate/fragmentation/CRC encoding/GPIO. Структуры нельзя
передавать packed memory: padding/ABI/endian не являются согласованным codec.

Binding включает boot/transfer, image length/SHA и SHA raw metadata. Receiver
берёт expected descriptor из **действительно завершённого DATA TX**, expected
metadata hash — из локального запроса, не из входящего отчёта. DATA TX borrowed
до terminal outcome RX state; detectable reinit/abort/context change вызывает
sticky failure. Boot/session uniqueness и ABA/replay после reset остаются
внешней negotiation policy, не реализованной аутентификацией.

Отправитель кэширует один отчёт, повторяет его после ack_timeout до явного
retry/total limit. ACK отражает все semantic fields; mismatch не завершает TX.
Первый валидный receive даёт DELIVERED и один output для caller; exact duplicates
дают только ACK, без повторной доставки. Другой terminal report для того же
binding вызывает sticky CONFLICT. Invalid same-binding reports и duplicates
ограничены retry count; foreign binding не продвигает состояние/deadline.

0<ack_timeout<linger<=total<=INT32_MAX, retry>0, без defaults. Clock uint32
modulo, interval<2^31. RX total отсчитывается от последнего DATA ACK, а не
позднего begin; duplicates не продлевают linger/total. Poll обязателен даже без
пакетов. После linger terminal FINISHED; поздний пакет не доставляется заново.
Если outcome пришёл до local DATA TX completion, он не принимается; producer
может повторить в своём конечном окне, пока DATA last-ACK retry завершается.
Ни один прежний DATA/handoff deadline не продлевается.

**Peer outcome/echo ACK не аутентифицированы.** Зная binding, peer может подделать
report; это явно проверяется в tests. Будущий HMI может показывать только
«загрузчик сообщил результат», с identity/ошибкой доставки. Нельзя выводить из
этого local verified state, физический останов или разрешение install/ARM.
Независимые локальные проверки загрузчика и аппаратная E-stop цепь обязательны.
Ожидание оператора не удерживает lease: итоговые records только история.

## Проверки и аппаратные границы

Outcome suite:6 groups,1001 sessions/lost report/final ACK, binding/metadata/
canonical pairs/conflict/retry/linger/deadline/wrap/abort/reboot/snapshot tests.
Boot suite расширена до17 groups:6 signed DATA→crypto→model→close→outcome/ACK
sessions, negative signature/hash/profile/vectors/binding/cancel, late close
context/deadline, reentry, input snapshot и pending release/quarantine.
Root27/27, sanitizers27/27, standalone7/7, ARM/static/source/frame/image/output
gates — [evidence](evidence/build/stm32-update-outcome/README.md). Все8 ESP SDK
profiles и pinned dependencies прежние. CPU/throughput/full stack на MCU не измерены.

Owner SWD/core/ID/Flash1024KiB/3,252V/backup1048576bytes/vector structure facts
сохранены. RAM/CCM/package/HSE/full IRQ/pinout/erase/protection/GPIO/CAN/restore,
production profile/metadata/version/keys и real physical quiescent proof открыты.
Нет actual F407 profile, Flash address reservation, CAN hardware, device/SWD/
UART/reset/Flash/option bytes/backup reads/network bench/motion/release/merge.
На ESP32 остаётся ordinary8f0d899. Откат — revert PR, без миграции устройства.

## PR-07h — отдельный release-aware путь

[Release handoff](STM32_RELEASE_HANDOFF.md) использует эти же guards/cleanup/
model/completion алгоритмы для strict15-field signed release draft. Explicit
local API выбирает режим; legacy strict11 path не переключается по JSON.
Raw metadata SHA связан с negotiation; production runtime/installer нет.
