# First-boot runtime diagnostics

Scope: collect boot milestones and bounded periodic health reports before display/touch commissioning.
Acceptance: ESP-IDF 6.1.1 headless and rev12 compile and pass binary validation; host tests pass.
No motion, CAN initialization or device flashing is introduced.

Serial logs now identify successful RGB panel creation, GT911 creation, memory before UI
initialization and LVGL readiness. Driver initialization messages confirm API success;
they do not prove visible output or correct physical touch coordinates.

Every five seconds the LVGL owner task reports loop count and elapsed window,
maximum UI work duration (microseconds), internal free/minimum/largest-block memory,
free PSRAM and its stack high-water mark (ESP-IDF bytes). The rev12 profile also
reports elapsed time since the latest valid touch sample and its latched fault flag.
The loop duration excludes task sleep and diagnostic logging. It includes dashboard
update, LVGL processing and, in rev12, the touch freshness check. These are not
scanout FPS, touch-to-photon latency or a guarantee of real-time performance.

No logging was added to LVGL input/flush callbacks. The 16 MHz RGB pixel clock,
single PSRAM framebuffer, partial draw buffer and task polling periods are unchanged.
Periodic serial output may still add scheduling time once per five-second interval;
measure on the board before claiming any responsiveness target.

From the ASCII-path worktree, after activating ESP-IDF:

```bash
idf.py -C firmware/esp32-hmi -B "$PWD/build-idf-6.1.1-rev12" -p /dev/ttyACM0 monitor
```

Press RESET to capture boot milestones; exit with Ctrl+]. This monitor command does
not flash firmware. The diagnostic changes must be flashed separately before their
new messages can appear; hardware testing of these changes is pending.
Rollback: revert this diagnostics commit and rebuild the previously tested profile.
