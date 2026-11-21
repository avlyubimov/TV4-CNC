# PR-07o — shared bounded mixed-codec channel

Base main389de77, PR #1–46 merged. Separate branch/PR; no devices/self-merge.

1. Add opt-in TX/RX wrappers around unchanged raw fragments and strict TU/MS
   codecs. One retained fragment-ID space per direction across all phases.
2. One owned TX frame outbox, exact local commit, bounded backpressure even
   after lower TX becomes IDLE on its last frame. Peek is not queue progress.
3. Explicit locally quiescent NEGOTIATION→SETUP→METADATA→IMAGE→OUTCOME traffic
   filter. Metadata/image IDs distinct/paired; full upper state/crypto/lease/
   deadlines remain mandatory. Local phase is not protocol/queue-flush proof.
4. Shared-clock host path through one mixed link for actual setup/metadata/
   independent signed image/lease/model/close/outcome, with all12 kind losses.
   Preserve legacy modes, no reinit/reset time/counters for phase/recovery.
5. Unit/fault tests, ARM exact import/export/frame/global budget + negative
   guards, host/sanitizers/standalone/CI matrix, text evidence/docs and new PR.

No new wire bytes/version/IDs/bitrate, driver/runtime/crypto owner/heap/Flash map.
Physical drivers may still have pending frames after local commit; external
queue lifetime/drain/routing/authentication/persistent identity remain blockers.
No fake physical-ready/install/upper-protocol-complete boolean or authority.
