# PR-07k — bounded fragment/reassembly draft

Base: main `4db99df`, PR #1–42 merged. No device access or merge.

Implement an opt-in, single-owner C11 library around the PR-07j byte codec:
8-byte maximum payload, explicit LE header, one bounded message per direction,
monotonic message counter, exact duplicates, backpressure, cancellation and
progress/total deadlines. Route/generation remain explicit local inputs;
production identity, routing and authentication are unresolved.

Acceptance: independent envelope oracle; all codec kinds/DATA lengths; malformed,
loss/order/conflict/clock/context/exhaustion tests; existing signed RAM chains
through fragments; host and unsuppressed ASan/UBSan; standalone host and ARM
compile/import/global/frame budget guards; unchanged device/image output gates
and ESP-IDF matrix. Save text evidence, update status/contract/CI, separate PR.

No runtime driver, IDs/bitrate, board image/map, new crypto owner, metadata
transport, Flash writer, jump/install/motion authority or hardware claim.
