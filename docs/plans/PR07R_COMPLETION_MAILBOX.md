# PR-07r — SPSC completion mailbox and bound consumer

Base main2c6fd1c; PR #1–49 merged. Separate branch/PR, no device actions/merge.

1. Add fixed-capacity C11 SPSC mailbox with owned ticket/result copies, one
   producer and one consumer, release/acquire publication and slot reuse. Full
   returns BUSY without overwrite; no allocation/spinning/locks in firmware API.
2. Bind the actual serialized driver-events owner and mailbox. Producer reads
   immutable binding only. Consumer polls current time, handles <=1 report,
   retains any pending copy until lower inbox accepts it; no force-flush/forget.
   Begin/claim/phase/clear honor actual mailbox/pending/inner ledger state.
3. Test FIFO/capacities/full/wrap/foreign/lifetime, real two-thread transfers,
   concurrent actual frame completion and complete signed RAM chains through
   mailbox service. Original upper crypto/lease/model/close/deadlines unchanged.
4. Add exact ARM imports/exports/frame/global/negative guard, compile-time native
   atomics requirement, object-code audit, source/static/host/sanitizer/standalone/
   scope/board/output checks, CI and exact text evidence/docs/statuses.

C11 threads/compile results do not approve target ISR/RTOS/cache/DMA execution,
real completion truth/reset/reconciliation or hardware. Persistent identity,
routing/resources/runtime/staging/install/physical gates remain open. Initialization
must finish and be externally published before both actors start; stop/join both
before destruction. No concurrent init/reinit/clear/reset of the mailbox.
