# PR-07j — portable update byte codec draft

База main `deace1c1a2f10d5c55b8a56a351f4c7a47f7b762`, PR #1–41 слиты,
открытых PR нет. Проверить main CI до публикации новой ветки.

1. Отдельная opt-in C11 codec library: existing REQUEST/OFFER/ACCEPT/CONFIRM,
   DATA/ACK, historical outcome/ACK в canonical bounded LE bytes. Draft format
   не production CAN version, arbitration IDs или Classic CAN fragmentation.
2. Fixed header/types/length/reserved checks, explicit enum codebook, no packed
   struct/padding/tail exposure; decode failures clear output, encode failure
   emits zero length. Shared negotiation shape validator не меняет policy/state.
3. CRC сохраняется как поле; existing DATA RX проверяет его один раз. Codec
   structural success не authentication/signature/session/safe-state/install.
   Все deadlines/lease/crypto/model/cleanup/transport algorithms сохраняются.
4. C boundary/fault/canonical/mutation tests + independent Python golden oracle,
   bounded deterministic malformed corpus; actual signed RAM backend chains
   через codec, включая loss/duplicate/invalid signature/metadata bindings.
5. Host/default sanitizers, standalone, strict actual-source static analysis,
   ARM object/import/frame/no-global budgets и прежние board/source/output gates.
   Не подключать codec в app_main/ISR/CAN/native host review frontend.
6. Evidence/docs/roadmap/scoped rules и отдельный PR; дождаться exact-head CI.
   SDK matrix8/dependencies/Flash layout/PROJECT_VER прежние. Нет devices/SWD/
   UART/reset/Flash/backup rereads/motion/keys/tags/releases/self-merge.

Приёмка: стабильные bytes по независимому oracle, bounds/failure zeroing, no
padding leakage, actual P-256/full image/lease/model/close/outcome integration.
Codec и ACK не дают install/COMMIT/jump/ARM authority. Actual RAM/Flash/clock/
GPIO/CAN/identity/fragmentation/runtime/recovery gates остаются открытыми.
