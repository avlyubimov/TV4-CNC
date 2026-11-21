# PR-07u — подтверждённая пауза приёма и actual channel RX

База main `bc35a37`, PR #1–52 слиты, main CI5/5 PASS, открытых PR перед работой
нет. [План](plans/PR07U_INGRESS_OWNER.md), [проверки](evidence/build/ingress-owner/README.md).
Optional C11 `tv4_ingress_owner` владеет actual [frame ingress](STM32_FRAME_INGRESS.md)
и actual [channel RX](STM32_UPDATE_CHANNEL.md). Новых runtime callers, CAN IDs/
bitrate/GPIO/Flash, crypto/SDK/heap/worker/ISR/RTOS/clock port здесь нет.

## Два участника и разрешение на публикацию

Один driver actor: permit/offer/retry/pause_request/ack/lost. Один protocol actor:
poll/stage/receive/take/pause/quiescent/advance/recover/resume. Zero-init once,
completed init externally published before both, stable address/lifetime,
stop/join обоих до destruction. Нет live copy/move/reinit/flush/inner mutable
calls/private field aliases. Driver не читает mutable protocol state; protocol
actor не читает producer-private fields. All spans valid/disjoint/exclusive.

Init принимает explicit прежние fragment/ingress bounds, route/generation и last
fragment/serial плюс last local epoch. UINT32_MAX last epoch запрещён; первая
epoch=last+1. Это volatile local counter, не persistent/authenticated identity.

Driver получает exact owner/epoch permit **до capture**. Original permit и
captured остаются с внешним BUSY input, captured передаётся отдельно от fresh
now. Нельзя retag старый кадр новым permit. Offer проверяет exact owner/epoch,
OPEN gate и actual ingress; full/retry/owned copy/age правила прежние. Permit
не обещает приём: owner может запросить pause до offer, тогда BUSY без копии.
Caller сохраняет input либо явно сообщает lost; actual backend loss reporting
и история capture остаются обязанностями будущего target adapter.

## Request → driver ACK → actual drain → advance → resume

Protocol actor release-публикует REQUESTED. Driver читает exact pause token и
вызывает ack только своей единственной сериализованной ролью после завершения
предыдущих calls. Ack выполняет максимум один actual pending retry; full даёт
BUSY без ACK. Consumer освобождает slot через stage/receive, driver повторяет
тот же request. Только после публикации actual pending driver release-публикует
PAUSED. Accepted copy, которая публикуется конкурентно с request, остаётся
старой owned работой и должна быть обработана перед advance.
Если driver ACK выигрывает гонку до возврата pause, он уже возвращает PAUSED;
это не отменяет отдельную обязательную проверку actual drain/quiescent.

Consumer acquire-видит PAUSED и проверяет actual ingress ring/staged/held и actual
channel RX IDLE. До этого quiescent/advance BUSY, даже если более ранний empty
snapshot был пустым. READY message требует actual take; partial message блокирует
advance. Нет внешнего stopped/drained/success boolean, fake ACK или force flush.
While PAUSED offer/retry не публикуют новые entries; lost всё ещё может защёлкнуть
ошибку. Quiescent — software publication/custody observation, не physical drain.

Advance вызывает actual strict sequential channel phase/role-ID API максимум
один раз за pause. Higher negotiation/setup/DATA/independent crypto/lease/model/
image/close gates по-прежнему обязательны. Локальная traffic phase их не проверяет
и не подтверждает peer authentication/install/physical state.

Resume требует ACK и actual ingress drain; partial RX разрешено продолжить в
**той же** phase без clear/reinit. Phase/IDs/fragment and ingress counters,
atomic ring positions и observed clocks сохраняются. Resume release-публикует
новую epoch, затем OPEN. Старый offer/ACK STALE и не влияет на новый цикл.
Epoch не wrap/reset; исчерпание закрывает lifetime. Нет автоматической смены
route/generation или live перемещения объекта. Hardware route cancellation и
перезапуск периферии этим handshake не реализованы.

## Приём и ограниченное восстановление

Stage возвращает только original ingress token без payload. Clock читается
**после stage**. Receive проверяет exact token/original capture age и feeds actual
channel RX в **current now**, не captured. Raw payload наружу не выдаётся;
take остаётся strict TU/MS/TAKEN_UNVERIFIED, refusal output zero. Poll обязателен
без трафика. Ingress fault или actual RX error отменяет actual RX и requests
pause; нет верхнего success callback или install authority.

После failed RX fresh checked owned copies можно явно retire через receive →
FRAG_STATE без feed RX. Это bounded cleanup собственных копий, не hardware buffer
return и не force-forget TX claim. При sticky ingress fault stage/receive не
разблокируют lifetime; stop/join/reconciliation остаются внешним обязательством.

Recover допускает **только прежний lower fragment progress/total timeout** и
только после exact driver ACK и actual ingress drain, без ingress fault. Он
вызывает прежний channel clear, сохраняющий phase/IDs/counters/observed time;
gate остаётся PAUSED до explicit resume. Context/clock/exhaustion/ingress age/
loss/прочие ошибки здесь не восстанавливаются. Ни одна операция не меняет original
upper image RX/negotiation/crypto/lease/model/close deadline. Восстановленный
lower channel не оживляет expired upper receiver/admission.

## Проверки и границы

Unit: actual pending/staged/partial/READY drain, source wipe/strict take, same-phase
resume, one advance, exact owner/epoch, old callbacks, clock/context/loss/age/
exhaustion, loss after ACK, zero refusals; actual lower timeout recovery после
ACK+fresh retirement с retained clocks/counters/phase/atomic positions.

Два реальных pthread actors:256 actual channel messages/8448 frames,1280
acknowledged phase cutovers,1276 stale ACKs и1024 stale offers, capacities1..4.
Пять controlled races: empty before publication, permit before request,
pending/staged while ACK, loss after ACK и timeout/cleanup/recovery. Stress clock
fixed near wrap; это не realtime/bus/CPU measurement. Unit фильтры фаз не
подменяют upper protocol; signed mode проверяет настоящие upper states.

HOST `--ingress-owner-staging` передаёт все12 kinds и raw metadata/signature/key
ID/image через actual dispatch/completion/ingress owner. Восемь signed positive
chains (6 release15+2 legacy11),3 independent signature/raw digest/expired image
refusals и2 original-image lifetime refusals. Каждая positive chain выполняет10
actual RX phase barriers; fragment losses проходят bounded recovery. Producer
arrays wiped, independent full verifier/live lease/model/clean close прежние.

Explicit synthetic inputs прежние: ingress2/residency8ms/service1ms, custody2/
residency16ms, completion2/service1ms, fragments progress8/total256/frame160,
driver1/3ms. Производственных defaults/новых outer windows нет. Host signed
link serialized; separate pthread/TSan проверяют concurrent handshake. Actual
peripheral/IRQ/DMA/cache/RTOS/clock/remote drain/latency/soak не проверены.

Software checks и exact budgets в evidence: новый ARM archive1512text/data0/bss0/
frame88,16 exports/10 DMB,4096/frame128 ceilings; старые archives/budgets/SDK/
workspace/arena прежние. Host owner888/permit16/pause16 не MCU RAM/full-stack
approval. Native RMW нижнего ingress может повторять exclusive stores; этот
handshake не атомарная hardware cancellation или independent physical inhibit.

Owner F407 SWD/ST-Link V2/Cortex-M4r0p1/raw ID0x101f6413/Flash1024KiB/3,252V/
backup1048576bytes/structural vectors PASS сохранены; новых наблюдений нет.
Full marking/package/RAM/CCM/HSE/vectors/erase/protection/GPIO/CAN/backup recovery,
truthful backend reset/loss/completion/route cutover, persistent authenticated
identity/resources/staging/runtime/installer/operator/physical gates открыты.
Flash1MiB не резервирует RAM/loader addresses. Devices/SWD/UART/reset/Flash/CAN/
motion/самостоятельного merge/релиза нет.
