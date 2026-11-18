# Архитектура TV4-CNC

Инженерный retrofit ТВ-4-07-20 КА с сохранением ручного управления.
Аппаратный статус — [HARDWARE_STATUS.md](HARDWARE_STATUS.md), порядок работы —
[ROADMAP.md](ROADMAP.md). Этот документ отделяет существующий HMI от целевой
системы; описание роли компонента не является заявлением о готовом драйвере.

## Ответственность контроллеров

| Компонент | Обязанности | Состояние на PR-01 |
|---|---|---|
| ESP32-S3 HMI | LVGL 9/FreeRTOS, UI, Wi-Fi, программы/инструменты, диагностика/журнал; будущие microSD, OTA Manager, CAN transport | HMI/host simulation/Wi-Fi/startup реализованы; список сети и изображение наблюдались; остальное по IMPLEMENTATION |
| STM32F407 Motion | Планировщик X/Z, timer/DMA STEP/DIR, encoders/spindle sync, homing/limits, faults/watchdog, CAN state machine | PR-02: portable supervisor/host tests/ARM library; board firmware и motion отсутствуют |
| F407 bootloader | Независимый recovery и проверяемая запись образов по CAN | Отдельный будущий компонент; не зависит от motion app |
| F103 Blue Pill | Bench CAN/протокол/таймеры/IRQ/bootloader experiments | Отдельный стенд, не контроллер движения |
| Аппаратная безопасность | E-stop, снятие опасного момента/питания, блокировки manual/CNC | Независима от обоих MCU, CAN и OTA; требуется отдельная аппаратная проверка |

ESP32 никогда не генерирует STEP и не определяет тайминг движения. CAN callback
не управляет импульсами. Публикуемые координаты/состояния должны указывать
единицы, систему отсчёта и достоверность; симуляционные показания не live telemetry.

```text
GT911 / LCD 800x480
        |
ESP32: LVGL owner <-- snapshots <-- network/storage/diagnostics/CAN workers
        | intent queue + pending/confirmed/error; no blocking UI I/O
        |                          GitHub Releases -> HTTPS -> OTA Manager
        |                                  | own inactive OTA slot
        |                                  + CAN update -> F407 bootloader
        |                                  + optional microSD staging/cache
CAN (non-safety; physical transport not commissioned)
        |
STM32F407: protocol validation -> machine state -> planner -> timers/DMA STEP
            encoders / spindle / limits / watchdog / fault manager
        |
Drives X/Z/spindle (not enabled by the present project)

Independent hardwired E-stop / interlocks / contactor-STO safety path
```

## Состояния и разрешения

В текущем HMI есть SIMULATION, DISCONNECTED, READY, HOLD, FAULT. Виртуальные
reconnect/ACK не включают реальную машину; transport_send инертен. Тестовый
heartbeat 4 bytes/v1/500 мс не является production CAN specification.
Целевые IDLE/HOMING/JOG/RUN/UPDATING и переходы F407 нужно определить/проверить
в PR-02/07 отдельно. Не утверждать, что эти состояния уже реализованы.

Motion разрешается только после самостоятельной проверки F407 и аппаратных
interlocks, negotiation и явной команды оператора. Boot/reset/reconnect/timeout/
watchdog/fault возвращают запрещённое движение; ACK лишь подтверждает ошибку,
а не запускает ось/шпиндель. Потеря CAN должна иметь заранее утверждённую
реакцию F407, испытанную без приводов до commissioning станка.

## Конкурентность и хранилища

- app_main — существующий LVGL owner. Сеть, NVS, будущие microSD/CAN/OTA не
  выполняют I/O из UI callbacks. Очереди/reassembly ограничены; тайм-ауты явные.
- HMI tools/journal сейчас RAM; Wi-Fi profiles — отдельный NVS namespace.
  Будущие схемы настроек версионируются, мигрируются с восстановлением и
  совместимы с rollback app. Не публиковать NVS/Flash dumps с паролями.
- microSD FAT32: программы G-code, экспорт журналов/настроек, OTA cache;
  отсутствие карты не блокирует boot или диагностику. Card detect, pin map,
  безопасное извлечение и file limits определяются в PR-06, не угадываются.
- UI ru-RU, централизованные IDs, резерв kk-KZ/en-US; app не поднимает
  удалённое управление станком. Пароли/ключи не входят в логи/snapshots.

## Обновления и отказоустойчивость

OTA обязательно для ESP32 и F407 в штатной эксплуатации без USB. Пока оно
не реализовано. [OTA_ARCHITECTURE.md](OTA_ARCHITECTURE.md) задаёт подпись
манифеста/образы, независимые состояния обновления, A/B, health confirmation,
CAN recovery и отдельную первую USB/SWD установку. Смена разметки не app OTA.
Автопроверка разрешена, установка только с подтверждением оператора и
проверкой совместимости/безопасного состояния. OTA не управляет аппаратной защитой.

[RELEASE_PROCESS.md](RELEASE_PROCESS.md) определяет release artifacts и ручной
owner gate после CI. Production-подпись, STM32 firmware и OTA capabilities
не выдаются за существующие компоненты.

## Производительность и дальнейшая проверка

Частоты RGB scanout, LVGL redraw и coordinate updates различаются. Цели:
feedback <50 мс, 30+ FPS где достижимо, координаты 20–30 Гц; это не результаты.
В [RUNTIME_DIAGNOSTICS.md](RUNTIME_DIAGNOSTICS.md) перечислены текущие метрики
и пробелы (CPU, физическая latency, export, reset/watchdog/reconnect counters).
Работа при сетевых/CAN ошибках, длительный run и memory pressure требуют
испытаний на плате; ни успешный build, ни фото этого не заменяют.
