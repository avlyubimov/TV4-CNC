# Partial network cancellation: software evidence

Qualification base main `5cacc6f`; existing signed bench.4 source `6c7b1d5`,
ESP-IDF 6.1.1. The installed runtime image, dependency pins, TLS, signature/compatibility,
resource thresholds and local contexts remain unchanged. No new firmware is
installed or released by the UI change below. [Plan](../../../plans/P1_PARTIAL_CANCEL.md).

- [274 host tests](host-tests.log) PASS, including actual LVGL rendering.
- [274 ASan/UBSan/LeakSanitizer tests](sanitizer-tests.log) PASS.
- The real signed installer test now injects cancellation and lost transport
  after all 28 partial 17-byte boundaries: 56 cases require exact sink prefix,
  one abort, closed transport/hash contexts, cleared candidate and no seal or
  boot selection. These are host injections, not physical Wi-Fi experiments.
- [11 procedure tests](procedure-tests.log), [7 parser/collector tests](collector-tests.log),
  and [6 loader/tamper/expiry tests](loader-tests.log) PASS before serial access.
  They cover protected active/other partitions, restoring the known image before
  its original metadata, one conditional full recovery, refused short reserves,
  partial-slot classification and absolute read/operator deadlines.
- [Main CI](source-ci.json): 15 successful jobs, including both pinned SDKs.
  [Numeric software results](results.json) bind the exact changed test source.

The current public release/tag and all 11 cached official asset digests were
rechecked. Signature, full image SHA/size/compatibility were independently
verified offline again with the existing local public key; no private key or
new context is used. Hardware cancellation, Flash readback and final retained
image are recorded separately after the finite session closes.

## Follow-up from the real cancellation: clear UI status

The tested bench.4 reports cancellation as service ERROR / code10, so its screen
uses the generic update-failure message even though the session correctly says
CANCELLED. The source change gives that existing code a name, checks its relation
to the backend enum at compile time, and displays **«Обновление отменено»**.
The state values, error wire encoding, retry/check/install gates and 5 Hz progress
coalescing are unchanged. The new localized ID is appended, preserving old IDs.

[274 host UI tests](host-tests.log) and [274 sanitized tests](sanitizer-tests.log)
PASS after this change. Actual LVGL XRGB8888/RGB565 tests check the cancellation
label, enabled Check / disabled Install and Reboot, and zero redraws from an
unchanged cancelled snapshot. The actual SDK-bound service test checks the
named code. This text fix is software-tested; the board retains immutable bench.4
and will show the old message until a separately approved future release.

The published full test logs are the final runs including the UI change. The
separate pre-device run also passed 274/274 in each mode and is recorded in
results.json; private original phase logs remain retained.
