# PR-07s — owned frame dispatch to the driver actor

Base maindb50840; PR #1–50 merged. Separate branch/PR, no device operations/merge.

1. Bind actual completion owner and one bounded owned delivery slot. Protocol
   actor publishes only an actual claimed frame with original admission/deadline;
   driver actor takes a zero-payload token, reads fresh time after take and
   checks before exposing an owned frame. No mutable protocol reads by driver.
2. Driver reports require exact current ticket. Full completion mailbox retains
   the first report for retry, never overwrites uncertain with terminal. Published
   or taken delivery/claim survives timeout/cancel until truthful terminal return.
3. Driver validates original age/current monotonic time and atomic revocation.
   Refused check returns zero output; token still requires explicit terminal return.
   Producer RETURNED publication AND actual consumer cleanup precede slot reuse.
   No hardware transmit/reset/inhibit authority or target ISR/RTOS claims.
4. Add bounded/FIFO/lifetime/fault/stale/retained-report and actual two-actor
   roundtrip tests; signed RAM chains through both dispatch and completion paths.
   Preserve old APIs/modes/upper crypto/lease/model/close/windows and budgets.
5. Exact ARM export/import/frame/global/native-atomic guard + negatives, static/
   host/ASan/UBSan/TSan/standalone/source/scope/board/output/CI checks, text evidence,
   pinned dependency versions/licenses/source hashes/docs/statuses and separate PR.

Actual hardware completion truth, target ISR/RTOS/cache/DMA/clock/cancellation,
persistent identity/routing/resources/staging/runtime/installer/physical gates open.
