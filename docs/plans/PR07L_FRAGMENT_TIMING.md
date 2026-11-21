# PR-07l — one-clock fragment/upper-layer composition

Base main277801e; PR #1–43 merged. Separate review, no devices or self-merge.

Add a HOST-ONLY deterministic two-direction fixture: one frame queue slot per
direction, one shared caller clock, explicit delivery/queue delays, selected
fragment losses/duplicates, bounded polling and explicit timeout recovery.
Compose unchanged negotiation/DATA/consumer/outcome APIs and actual independent
signed RAM admission with that same clock. Never hide elapsed transport time or
reset upper-layer deadlines/counters to recover a fragment loss.

Acceptance: successful whole-message retries and exact failure boundaries;
queue pressure/READY retention/loss/reboot/context/wrap/clock tests; signed
handoff chain and refusal after expired DATA; root+standalone+unsuppressed
sanitizers, unchanged ARM archives/gates/dependencies/SDK matrix. Preserve
hardware facts and blockers, save text evidence, update CI and create a PR.

Fixture timings are engineering assumptions, not measured bus/RAM/CPU budgets,
production defaults/driver/identity/authentication or metadata transport.
