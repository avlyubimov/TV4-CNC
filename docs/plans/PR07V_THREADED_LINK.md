# PR-07v — signed update through a dedicated HOST driver actor

Base main `5e76626`, PR #1–53 merged, main CI5/5 PASS, no open PR at start.

1. Add an opt-in HOST-only pthread driver for the existing signed fragment link.
   Protocol/crypto remain on the calling thread. The driver uses only published
   dispatch and ingress producer APIs, immutable bindings and its own bounded
   backend copies. No protocol-private aliases, live reinit or hidden flush.
2. Keep one explicit logical clock, original upper windows and all previous
   modes. Read current time after take/stage. Retain original capture permit/time
   through BUSY; process exact pause requests on the driver thread before actual
   owner drain/phase change/recovery. Software ACK is not physical drain proof.
3. Exercise all twelve kinds, actual signed metadata/image/independent verifier/
   lease/model/clean close, loss/duplicates/recovery, signature/raw-digest/expired
   image refusals and retained driver ownership through the original timeout.
   Stop/join before fixture destruction; stopping must not synthesize returns.
4. Add the signed threaded mode to normal, ASan/UBSan/LSan and TSan CI. Retain
   all existing race cases, exact eleven ARM archive/import/export/frame/global/
   native-atomic guards and negative checks, pinned dependencies and SDK matrix.
5. Save reproducible text evidence, source hashes/versions/licenses, update
   current statuses and create a separate PR. No self-merge, device or release.

Acceptance does not approve a target ISR/RTOS/cache/DMA/clock/backend port,
production identity/routing/resources/staging/installer, hardware memory/Flash
map or physical motion/CAN. No SWD/UART/Flash/reset/device/backup operations.
