# PR-07u — acknowledged RX pause and bound channel owner

Base main `bc35a37`, PR #1–52 merged, main CI5/5 PASS, no open PR at start.

1. Bind actual frame ingress and actual channel RX in an opt-in two-actor owner.
   Driver obtains an epoch permit before capture; original permit/time travel
   with externally retained input. No implicit retagging after pause/resume.
2. Owner requests pause; driver acknowledges the exact request only after its
   prior calls finish and actual producer-pending is published. Owner waits for
   acquired acknowledgement AND actual ring/staged/channel drain before advance.
   No caller stopped/drained/success boolean, force flush or live reinit.
3. While paused, producer cannot publish. Resume preserves all clocks/counters/
   phase/IDs, issues a fresh non-wrapping epoch; stale offer/ack cannot affect a
   later cycle. Partial RX may resume same phase after ingress drain, never skip
   a phase. Only existing lower fragment timeouts recover after ACK + actual
   ingress cleanup, preserving original clocks/counters/phase/upper windows.
   Ingress loss/age/clock, context/exhaustion and other faults cannot recover.
4. Preserve all older modes/budgets/pins/SDK/crypto/lease/model/image/close gates.
   Add actual pthread pause/offer/held/staged/epoch/fault tests and signed HOST
   chains through the new owner, including concrete phase barriers.
5. Exact ARM export/import/frame/global/native atomic guard + negatives, host/
   ASan/UBSan/LSan/TSan/standalone/analyzer/source/board/output/scope/CI checks,
   text evidence/versions/licenses/source hashes/docs and separate PR.

The acknowledgement proves only this software actor stopped publishing, not
physical peripheral/DMA/IRQ/remote drain. Truthful driver capture/loss, target
clock/ISR/RTOS/cache/DMA/route lifecycle/identity/resources/runtime/installer/
hardware remain open. No device/SWD/UART/reset/Flash/CAN/motion/merge/release.
