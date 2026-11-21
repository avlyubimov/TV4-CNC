# PR-07q — owned driver completion event inbox

Base main efa2727, PR #1–48 merged. Separate branch/PR; no device actions or merge.

1. Bind one actual egress owner and a fixed-capacity completion event inbox.
   Posting copies a local ticket/result only; full means backpressure, no drop.
   One serialized owner; no ISR/thread/RTOS/driver safety claim.
2. Service polls the original egress at current owner time before processing at
   most one event. No timestamp supplied by the reporter, no deadline extension.
   Cancel/clear cannot discard pending events or held claims. Begin/claim/phase
   advance wait for the actual inbox, including stale/duplicate reports.
3. Unit/fault checks for capacity/wrap, owned copies, foreign/stale/duplicate
   tickets, backpressure/retry, late terminal/uncertainty/cancel/sticky faults.
   Extend signed RAM chains through delayed event service and independent upper
   admission/lease/model/close checks. Preserve all previous modes and budgets.
4. Add separate ARM export/import/frame/global guard and negatives, strict static
   analysis, host/sanitizers/standalone/source/scope/board/output checks; CI matrix
   unchanged. Save text evidence, exact versions/licenses/hashes and docs/statuses.

Local reports remain trusted adapter inputs, never remote delivery/authentication,
physical readiness or install permission. Actual synchronized driver adapter,
completion provenance/reset/reconciliation, persistent identity/routing/resources,
staging/runtime/installer and all physical hardware gates remain open.
