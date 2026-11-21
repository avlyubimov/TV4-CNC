# PR-07m — bounded raw metadata/signature transport

Base main7640f43, PR #1–44 merged. Separate PR, no devices or self-merge.

Add an opt-in C11 metadata container codec and single-owner receiver around the
unchanged logical DATA/consumer/ACK API. Explicit local image binding and a
distinct, globally fresh metadata transfer ID; no new CAN IDs/driver/wire kinds.
Bound raw document/signature/key ID to existing independent verifier limits.
ACK only after bounded copy, exact duplicate without repeat writes/delivery,
one owned UNVERIFIED snapshot, preserved deadlines/generation/high-water.

Acceptance: independent container oracle; malformed/bounds/role/identity/
duplicate/clock/deadline/cancel tests; metadata bytes really cross the existing
shared-clock wire/fragment link before actual independent signed image admission;
wrong signature/raw digest/expired image context still reject. Host/standalone/
unsuppressed sanitizers, separate ARM/import/frame/global budget guards, unchanged
old libraries/SDK matrix/pins/board/output gates, text evidence and updated docs.

Container/session descriptors are local fixture inputs; authenticated setup,
production identity/routing/storage/worker/Flash/installer remain future gates.
No hashing/signature trust is added to the transport or duplicated crypto engine.
