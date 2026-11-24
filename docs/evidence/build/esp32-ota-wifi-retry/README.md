# OTA transport interruption and same-process retry: software evidence

Base main `bf5c325`, after owner-merged PR #88. Existing signed runtime is
`6c7b1d5` / ESP-IDF 6.1.1, receiving `.3` and published bench.4. No firmware,
dependency pins, release assets, context, TLS/time/resource thresholds or
UI confirmation gates are changed. [Plan](../../../plans/P1_WIFI_DISCONNECT_RETRY.md).

- [274 host tests](host-tests.log) and [274 ASan/UBSan/LeakSanitizer tests](sanitizer-tests.log) PASS.
- Each of the existing 56 signed-session partial cancellation/transport failures
  now also performs a fresh authenticated full installation in the same process.
  The failed workspace is reset, previous partial sink bytes are retained, and
  the retry must replace the complete image before exactly one seal/activation.
- The actual SDK installer compiled against the host SDK boundary now checks
  partial write failure → idempotent abort → fresh begin/write/seal/activate;
  the running partition remains selected until successful activation. This is
  a mock SDK test, not a physical Flash result.
- [9 procedure](procedure-tests.log), [9 collector](collector-tests.log), and
  [6 loader/tamper/expiry](loader-tests.log) tests PASS before serial access.
  These retain real-disconnect/new-IP checks, refuse premature reconnection,
  cancellation in place of transport failure, missing cleanup, stale touch,
  inadequate resources, repeated operations, short reserves and changed inputs.
- [Base main CI](source-ci.json): all 15 jobs successful, including both pinned
  ESP-IDF SDKs. [Results](results.json) bind the changed test sources.

The current public release/tag and all 11 cached official asset digests were
rechecked; the existing public key independently verifies signature, complete
image SHA/size and compatibility offline. No private key or new context is used.
The board test uses the owner's panel Disconnect operation; it cannot qualify
involuntary RF/AP loss. Hardware observations, including failed attempts, are
reported separately and do not follow from these software results.
