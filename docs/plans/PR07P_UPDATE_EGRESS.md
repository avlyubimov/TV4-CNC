# PR-07p — bounded egress custody/driver completion

Base main555007d, PR #1–47 merged. New branch/PR; no devices/self-merge.

1. Add portable fixed-capacity frame custody queue with generation/serial tickets,
   one correlated driver claim, explicit residency deadline and no wrap/reinit.
2. Bind actual channel TX and actual queue in one opt-in egress owner. Pump only
   copies to owned queue before channel commit; phase advance waits for all
   matching local driver completions, never a caller success/drain boolean.
3. Cancel drops only unclaimed queued frames; timeout/uncertain ownership retains
   claimed frame until exact terminal return. Late return reclaims but never
   revives failed state. Clear retains clocks/tickets/fragment counters/phase;
   context/clock/exhaustion sticky. No hidden driver calls or forced forget.
4. Unit/fault and real shared-clock signed RAM chains through delayed completion
   tickets; preserve old modes/upper crypto/lease/model/deadlines. No new bytes,
   IDs/bitrate/driver/runtime/crypto owner/heap/Flash map or physical authority.
5. ARM import/export/frame/global guard + negatives, host/sanitizers/standalone,
   pinned dependency/source/board/output/scope checks, CI/text evidence/docs/PR.

A matching local driver report is a trusted adapter input, not remote delivery,
authentication, physical safe state or install approval. Hardware adapter and
persistent identity/routing/flush/reset/worker/resource/installer gates remain open.
