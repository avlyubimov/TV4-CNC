# Reset/rollback qualification — software preparation

[Plan](../../../plans/P1_RESET_ROLLBACK.md), [numeric results](results.json),
[exact main CI 15](source-ci.json). Main8d1603e is the qualification base; actual
installed network image is immutable bench.4/source `6c7b1d5`, independently verified
again against the owner public key, frozen tag and all 11 asset digests. Runtime
firmware/SDK/dependency/pins/config/layout/security are unchanged; this is not a
new firmware build, signing operation, context or release.

The host SDK double now uses the exact native state values 0/1/2/3/4/UINT32_MAX.
Actual installer/backend tests cover refusing INSTALL and confirmation from
NEW/INVALID/ABORTED/UNDEFINED, and preserving an already-VALID image.

- [274 full host tests](host-tests.log) PASS, actual LVGL XRGB8888/RGB565 included.
- [274 ASan/UBSan/LSan tests](sanitizer-tests.log) PASS, leak detection enabled.
- [13 procedure/fault/timer tests](procedure-tests.log) PASS: metadata-only bounds,
  protected apps/Flash, exact original checkpoint recovery, one conditional full
  recovery, refused short reserves and stalled-read deadline without extending
  the whole-session alarm.
- [8 actual UART collector tests](collector-tests.log) PASS using cached real
  boot bytes over a fake port: exact partition/version/ELF before self-test,
  partial lines, one controlled interruption/two boot traces, missing triggers/
  fallback, fatal logs, byte bounds and the legacy factory without inventing a
  newer self-test marker.
- [15 loader/tamper/expiry tests](loader-tests.log) PASS before any serial SDK
  import, including published digest/signature bindings and the dual-VALID state.

The host fakes do not prove physical rollback. Hardware results and observed
metadata states are separate; future network INSTALL failure, Wi-Fi/power loss,
production budgets and runtime 5.3.2 are not inferred from these checks.
