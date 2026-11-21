# PR-07n — bounded metadata setup и общий namespace

База main `d18cf93`, PR #1–45 слиты. [План](plans/PR07N_METADATA_SETUP.md),
[проверки](evidence/build/metadata-setup/README.md). Opt-in C11 library
`tv4_metadata_setup`, без runtime caller/драйвера/устройств. Прежние TU8 kinds,
DATA CRC/consumer/ACK, independent verifier/lease/model/cleanup сохраняются.

## Привязка и владение

После image REQUEST/OFFER/ACCEPT/CONFIRM оба original coordinator должны быть
CLOSED, а реальные image TX/RX — начаты, пусты, без ошибок. CLOSED теперь хранит
request/offer до explicit clear; abort/failure их стирает, clear тоже стирает.
Параметры CLOSED не дают live lease, auth или install permission.

Responder единожды резервирует metadata ID **image.transfer+1** в том же
`tv4_neg_pool`, до другого allocation. Новый `tv4_neg_reserve_metadata` принимает
original CLOSED responder/pool/generation, не отдельный descriptor. Same boot,
exact latest counter/peer request, owner NULL; no wrap/rollback. Это software
counter API без проверки времени — setup проверяет actual image endpoint первым.
Повторная попытка, другой allocation или active owner отклоняются. Timeout/abort
не возвращают номер. Следующая image negotiation пропускает использованный ID.

Оба setup owner удерживают свои pools от begin через handshake и metadata DATA
до close против **реального complete metadata TX / TAKEN metadata RX**. Установка
флага success или одна копия descriptor не являются close API. Передаётся owned
UNVERIFIED snapshot; независимые signature/raw digest/full image checks нужны
после него. Оригинальные CLOSED coordinator/pools/image TX/RX живут неподвижно,
эксклюзивно до close/abort; их нельзя clear/reinit/copy во время setup. Поля C
структур read-only для caller, это не защита памяти от DMA/злонамеренного C кода.

Каждый active вызов проверяет original boot/policy/counters/owner/descriptor/tag/
limits/start/progress, отсутствие image DATA и вызывает настоящий image poll.
Нет нового image init/begin/увеличения его исходных окон. Setup и metadata delay
расходуют original progress/total budget. Ошибка setup освобождает только свой
pool, не отменяет отдельные image/metadata endpoints или чужое владение; owner
сам выполняет их cleanup. Нельзя продолжать старые DATA после нового сеанса.

## Обмен и время

| Фаза | Содержание | Переход |
|---|---|---|
| REQUEST HMI → Motion | Original image binding/request/tag/block + opaque container size/SHA; metadata ID zero | WAIT_REQUEST → WAIT_ACCEPT |
| OFFER Motion → HMI | Те же поля + заранее зарезервированный paired metadata ID | WAIT_OFFER → WAIT_CONFIRM |
| ACCEPT HMI → Motion | Exact echo OFFER | WAIT_ACCEPT → READY |
| CONFIRM Motion → HMI | Exact echo OFFER | WAIT_CONFIRM → READY |

Limits явные: retry>0, retry<total<=INT32_MAX, send_limit1..255 на фазу,
max_bundle98..1215. Нет production timing defaults. One owner/monotonic uint32
ms, observation gaps<2^31, poll без traffic. Total начинается при begin обоих
owner, не при получении REQUEST. Повтор/отказ не сбрасывает send count/time/total.
Новая фаза сбрасывает только свой send count. Duplicate ранней фазы не откатывает
READY. Responder продолжает bounded cached CONFIRM после single take, позволяя
восстановить потерянный первый CONFIRM. Не продолжать вызывать next бесконечно
после завершения exchange: send budget остаётся ограниченным, poll обязателен.

Take копирует транспортные параметры один раз. После concrete close/abort/
recoverable failure clear возвращает IDLE с сохранёнными counters/high-water и
observed clock; для begin нужен новый image/request ID. CLOCK/CONTEXT sticky.
Смена boot pair/generation требует отдельного reviewed external lifetime.

## Complete-message codec и fragments

Отдельный canonical **MS draft1**,172bytes; он не меняет TU codebook/version.
Header8: `MS`,1,kind1..4,body164 LE16,reserved0,0. Body: request ID8,image
binding76,metadata descriptor44,compatibility tag32,block LE16,reserved0,0.
Descriptors: boot/transfer/size LE32 + SHA32. REQUEST metadata ID zero, другие
paired image+1. Exact length/no padding/tails. Encode failure leaves output
untouched/written0; decode failure zeroes message. Hashes/tag opaque, codec не
проверяет container SHA/raw SHA/signature или доверие peer.

Explicit immutable **raw fragment mode** переносит owned bytes8..286 прежним
4byte envelope/maximum8byte frame. Старые typed begin/take требуют прежний TU
codec; смешивание с raw API отклоняется. Raw take возвращает отдельный
RAW_TAKEN_UNCHECKED, written0/output untouched при отказе, single delivery/wipe
при успехе. Caller применяет выбранный bounded codec **до dispatch**. Clear не
меняет режим/counters/clocks; прежние fragment lifetime/quota/deadline правила
сохраняются. Local mode/route/generation не сериализованы: это не authenticated
multiplexing, CAN IDs, driver или replay isolation. Cross-mode MS→typed take и
TU→MS decode проверены отказами.

## Программная приёмка и ограничения

Новые unit/oracle/raw tests дополняют старые режимы. Общий caller clock в host
fixture охватывает original image negotiation, MS raw fragments с потерей всех
четырёх фаз, metadata DATA и настоящую independent signed image admission.
Producer document/signature arrays wiped; crypto читает delivered owned snapshot.
Положительные6 release15 +2 legacy11 chains, wrong signature/re-signed wrong raw
digest/expired image RX refusals. CPU crypto duration не тарифицирована; synthetic
ms/frame counts не physical CAN throughput/latency measurement. Test keys RAM only.

Новый separate ARM archive: ceiling4096text, data/bss0, static individual frame128;
exact exports/import allowlist и6 negative guard methods. Старые budgets не
увеличены. Host struct/frame размеры не full stack/MCU RAM placement approval.
Dependencies/SDK8 profiles/crypto17664 workspace/16384arena/Flash layout прежние.
Production handoff не линкуется с setup/fragment test helper; host-only scope gates.

Новая пара ID закрывает cross-role reuse только в рамках exclusive fixed volatile
pool. Production unique boot epochs/persistence/reset collision policy/tag
provenance/routing/queue lifetime/authentication всё ещё external. Matching forged
exchange может пройти; raw container SHA здесь тоже expectation, не integrity proof.
Production bundle/local trust context/storage/worker/runtime/installer/resource
budgets, actual F407 RAM/vector/clock/erase geometry/GPIO/CAN/physical inhibit и
operator/recovery gates открыты. Flash1024KiB не задаёт RAM или bootloader адреса.
Никаких приводов, image binary, SWD/UART/Flash/reset/CAN действий или self-merge.
