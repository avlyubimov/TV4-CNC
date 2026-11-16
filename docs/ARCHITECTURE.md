# Architecture

```text
Touch + 800x480 LCD
        |
ESP32-S3 / ESP-IDF / LVGL 9
  UI owner task <--- state snapshots <--- CAN RX task
  touch events ---> validated intent queue ---> CAN TX
                                       |
                                CAN (non-safety)
                                       |
STM32F407 motion controller
  machine state + watchdog + homing/limits
  deterministic trajectory planning
  hardware timers for STEP/DIR
  encoders + driver fault inputs
                                       |
                                drives X / Z / spindle
```

Independent hardware safety chain must interrupt hazardous motion without dependence on ESP32, CAN, STM32 firmware or UI.

## Modes
SIMULATION (default), DISCONNECTED, IDLE, HOMING, READY, JOG, RUN, HOLD, FAULT. Define legal transitions explicitly. Simulation may not activate any physical output.

## HMI screens
Home: large X/Z position, spindle RPM, feed, mode/status, active tool, JOG; Programs; Tools; Machine Settings; Diagnostics; Event Log. Target 800×480 with scroll or tabbed layouts; do not squeeze desktop mockup onto screen.

## Concurrency
LVGL only on one designated task. Transport publishes immutable snapshots. Never block touch handler awaiting a command acknowledgement. Acknowledge real actions with separate pending/confirmed/error states.

## Physical integration
The machine retains original manual mode and gearbox. Rear spindle encoder concept requires measurement and separate mechanical validation. Gear tooth cutting / C-axis / milling attachment are future milestones, not MVP.
