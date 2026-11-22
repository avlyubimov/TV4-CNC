# STEP/DIR timer design — software evidence

22 ноября 2026. Base main `392706355193b7544132e2296f7ef8572e1f3c10`.
[План/границы/команды](../../../NEMA23_BENCH_PLAN.md), [числа и source hashes](checks.json).

- Полный host/LVGL160/160 и ASan/UBSan/LeakSanitizer160/160 PASS. LSan сначала
  был заблокирован ptrace sandbox; повторён весь набор вне sandbox, без
  отключения проверок. Mbed TLS archive из existing cache проверен прежним
  SHA-256/source gate; первоначальный сетевой fetch был недоступен в sandbox.
- Actual-source tests: counts1..256, полная counter/PWM2/RCR/OPM модель для
  границ1/2/256, rounding, 16-bit period/prescaler boundary, odd clock/rate,
  DIR setup/hold, UINT32 limits, invalid/unknown timings и очищение stale plan.
  20000 deterministic cases:9709 accepted/10291 refused. Во всех host models
  число external pulses0; это software state, не measured pin voltage.
- Pinned ARM GCC14.2.1 archive464text/data0/bss0; единственный export compile,
  imports только compiler unsigned64 division и memset. Compiler frame72bytes,
  limits1024text/frame96; это не full-stack peak/CPU/jitter qualification.
- Default ARM не имеет design target; CMake отвергает сочетание design с real
  или synthetic image. P0 image contract/negative tests PASS, STEP/DIR symbols
  в P0 ELF отсутствуют; board/startup/protocol/ESP32 runtime sources неизменны.
- GCC15.2 actual-source `-fanalyzer -c`, strict warnings и workflow YAML parse
  PASS. Новый target автоматически входит в существующие host/sanitizer jobs;
  motion CI отдельно проверяет ARM archive/imports/frames/budgets/scope и
  сохраняет только text/JSON, без нового firmware/release artifact.

Software source до commit был dirty; hashes именно проверенных исходников
сохранены. P0 ELF этой проверки — regression artifact, не новый trial package.
STM32 не открывалась/не прошивалась, physical outputs не включались. Motor/
driver ещё не получены/не идентифицированы; питания/тока/pins/ENA settings нет.
Независимая аварийная цепь, physical pulse timing, stop/residual steps и motor
behavior остаются блокерами аппаратной приёмки. P1 OTA имеет приоритет.
