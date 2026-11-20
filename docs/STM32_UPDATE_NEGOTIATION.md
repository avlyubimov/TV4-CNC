# PR-07f — свежий логический update-сеанс до DATA

21 ноября2026. База main `f0bd866`, PR #1–35 слиты, пять main workflows PASS.
[План](plans/PR07F_UPDATE_NEGOTIATION.md),
[evidence](evidence/build/stm32-update-negotiation/README.md).
Модуль `protocol/can/update_negotiation` вызывается только host RAM tests.
Это не CAN codec/driver/production protocol и не аутентификация.

## Четыре сообщения

| Сообщение | Смысл в модели |
|---|---|
| REQUEST | Fresh requester boot+counter, expected responder boot, image bytes/opaque SHA, raw metadata SHA, local compatibility tag и block ceiling; DATA id ещё нулевой |
| OFFER | Responder выдаёт свой boot+новый counter для DATA, выбирает block<=обеих ceilings; binding остальных полей сохраняется |
| ACCEPT | Requester точно подтверждает semantic fields OFFER после локальной выдачи REQUEST |
| CONFIRM | Responder подтверждает ACCEPT после выдачи OFFER; requester принимает после выдачи ACCEPT |

`compatibility_tag[32]` — ненулевой opaque reviewed local tag, не fingerprint
доверенного ключа, production SemVer или доказательство совместимости с платой.
Generation ненулевая и локальная: каждый API получает текущую policy generation,
изменение keys/profile/context должно менять её у будущего owner. Значения tag,
generation и epochs в тестах — фикстуры, production defaults не добавлены.

REQUEST/OFFER/ACCEPT/CONFIRM — typed C objects. Проверяются kind, boot pair,
ненулевые IDs/size, block1..256, size cap и точное semantic binding. Struct
padding не сравнивается; нельзя сериализовать memory layout как CAN frame.
Длина128bytes не задаёт fragmentation. Wire version/IDs/bitrate/пины отсутствуют.

После ACCEPT responder может однократно получить параметры для подготовки RX;
requester — после CONFIRM. `tv4_neg_take` выдаёт только descriptor/metadata SHA/
block, повторный вызов не публикует их снова. Сам модуль не вызывает DATA init/
begin, не читает source, не открывает lease и не меняет движение.

Оба существующих DATA API принимают выбранный **responder boot epoch** в legacy
`local_boot` parameter. Это не физический boot ESP32. Такое использование
проверено в integration; DATA APIs и их deadlines не изменены.

## Identity pool и lifecycle

Caller-owned pool хранит fixed local/peer boot pair, local last_issued,
peer request high-water и одного active owner. Повторный init live pool или
coordinator возвращает отказ без перезаписи. Все objects изначально zero-init;
их нельзя копировать/перемещать или менять live fields, ISR/threads не поддержаны.

Local counter резервируется до публикации, не откатывается при abort/error и
не переходит через UINT32_MAX. Peer high-water изменяется только при принятии
нового REQUEST и сохраняется через close/abort/clear. Старый REQUEST после
clear и даже создания нового coordinator на том же pool не резервирует ID.
Другой active owner получает BUSY; чужой owner не стирается и не захватывается.

Уникальность epochs после reset/power loss этот код **не создаёт**. Нужен
отдельный reviewed entropy/persistent identity provider/collision policy.
При peer-only reboot local epoch можно сохранить только с переносом issued
counter из trusted local state в новый pool после завершения старого owner;
новый peer epoch должен отличаться. Сброс counter/повтор epochs или копирование
pool нарушают contract. Persistent recovery/anti-replay не подтверждены.

## Ограниченные ожидания

Явные limits: size>0, block1..256,0<retry<total<=INT32_MAX,send_limit>0.
send_limit включает initial transmission и repeats отдельно для каждой фазы.
Total window начинается на begin/первом принятом REQUEST и никогда не продлевается.
Clocks локальные uint32 monotonic modulo2^32; интервал наблюдений<2^31.

Exact duplicates повторяют cached reply без нового ID/take/progress. Canonical
conflict для известного binding вызывает sticky failure; wrong boot/request/tag/
malformed fields не заменяют текущий binding. REQUEST/OFFER/ACCEPT повторяются
по retry timer; CONFIRM — первоначально и по duplicate ACCEPT. После take
responder сохраняет cache для восстановления потерянного CONFIRM до close/
abort/expiry. Owner должен закрывать его после наблюдения DATA либо отмены;
слишком ранний close сознательно прекращает peer retry recovery.

READY/take ограничены исходным deadline. Poll нужен даже без валидных пакетов.
Timeout/context/clock/counter/retry/conflict ошибки очищают cache и освобождают
только собственный pool owner, сохраняя replay counters. Clear возможен лишь в
terminal state и не меняет clock/configuration/generation. Clock/context fault
требует нового reviewed coordinator/anchor с сохранённым pool; clear не является
скрытым reconfigure или resume.

Уже начатый DATA имеет собственный прежний lifetime. Future owner обязан
согласованно отменять DATA/handoff при смене boot/policy и не оживлять его из
копии descriptor. Copied result не отзывается автоматически и не install token.
Ожидание оператора перед будущей установкой требует свежего сеанса и повторной
полной проверки saved bytes/physical/operator/recovery условий.

## Что проверено и что не является доверием

7 unit groups/1024 sequential sessions: loss каждого сообщения/всех фаз,
duplicates/order/replay, snapshots, pool ownership, two-sided exhaustion,
clock wrap/backward/expiry, generation/peer changes и однократный take.
Есть явный тест: matching **forged OFFER/CONFIRM проходят handshake** без peer.
Идентификаторы/tag/echo не аутентифицируют CAN. Forged higher REQUEST может
занять volatile high-water; production authentication/availability design открыт.

6 новых signed integration chains: negotiation→DATA с потерями→independent
Mbed TLS P-256/SHA→explicit vector model→clean lease close→historical outcome/ACK.
Три отрицательных: changed image при корректном DATA CRC; invalid signature;
валидная повторная подпись другой metadata при старом negotiated raw SHA.
Все отклоняются существующими crypto/binding checks и освобождают lease.
Это не production release metadata: draft counters/schema/test IDs прежние.

Host31/sanitizers31/standalone8 и ARM/static/source/image/output gates PASS.
Новый ARM object1930text,data/bss0,max individual frame40bytes; весь transport5442
в прежнем8KiB ceiling. Host coordinator368/pool32/message128bytes; это caller
storage, не полный RAM/stack/CPU budget платы. Mbed TLS/dependency/SDK pins прежние.

Нет runtime caller, entropy provider, actual F407 profile/Flash reservation,
writer/commit/jump, CAN wire/driver или physical quiescent assertion. Owner SWD/
core/Flash1024KiB/backup facts сохранены; HSE/full marking/RAM/CCM/sector/GPIO/
CAN/restore неизвестны. На ESP32 ordinary8f0d899, новых device operations нет.
Откат — revert PR: DATA/outcome APIs/форматы и NVS/config/layout не менялись.
