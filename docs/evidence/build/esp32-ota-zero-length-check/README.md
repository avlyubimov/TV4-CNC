# Configured control/one-shot CHECK: software evidence

Exact clean source `a29c9edde10b8d2359572d04d544d8973fdaa6b3`, baseline main
`c3b82f0ef8db764f9f242ad09caec015f58ce19a`. Four actual owner-configured
control/auto ELF/BIN on pinned ESP-IDF6.1.1 and5.3.2 passed configuration,
Flash16MiB/PSRAM, app descriptor/checksum/appended SHA-256 and exact ELF binding.
`results.json` records hashes and compiler frames; runtime qualification is separate.

237/237 host and237/237 ASan/UBSan/LSan tests passed, leak detection retained.
Actual service variants0–5,30 HTTPS adapter cases and offline/health/memory/SNTP/session/resource/cancel
faults,9 build-guard methods and actual-source GCC `-fanalyzer` passed.
The finite local bench driver/parser passed21 fault/deadline/restore cases before
USB access; SDK calls replaced in those cases, no device imported or opened.
Source CI30/30 and baseline main15/15 passed before the board session (`ci.json`).
Owner key/context and binary images remain local; no private key is passed to CI.

See [actual hardware observations](../../hardware/2026-11-24-esp32-signed-check-a29c9ed/README.md).
