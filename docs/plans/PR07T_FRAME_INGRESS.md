# PR-07t — bounded owned frame ingress

Base main `25e635c`; PR #1–51 merged, main CI5/5 PASS, no open PRs.

1. Add a separate opt-in C11 SPSC frame mailbox: immutable route/generation,
   owned copies, capacity1..4, one retained producer frame on full and explicit
   retry without changing its capture time. Further BUSY leaves new input owned
   by the caller; an actual unretainable/hardware loss must latch overrun.
2. Consumer stages an unchecked zero-payload token, then reads fresh common
   monotonic time and checks original capture age before obtaining an owned
   frame. Wrong/stale tokens cannot consume the staged frame. Clock/expiry/
   exhaustion/overrun faults are sticky; no live reset/flush/clear API.
3. No mutable protocol reads by producer. Stable lifetime, externally published
   init, exactly two actors and stop/join before destruction. Empty is only an
   observation, not producer-stop/phase-cutover/backend-drain evidence. No real
   driver/ISR/RTOS/cache/DMA/clock/authentication/physical or install authority.
4. Add unit, real threaded FIFO/channel and fault tests; deliver actual signed
   update chains through ingress plus existing dispatch/completion paths. Preserve
   all earlier modes, independent crypto/lease/model/close and original deadlines.
5. Add exact ARM import/export/frame/global/native-atomic guard and negative
   tests, host/sanitizers/TSan/standalone/analyzer/source/board/output/CI checks,
   text evidence with exact dependencies/licenses, documentation and separate PR.

No device/SWD/UART/reset/Flash/CAN/motion/merge/release actions. Actual producer
quiescence/phase-cutover, driver loss reporting, target port/clock, persistent
authenticated identity/routing/resources/staging/runtime/installer/hardware open.
