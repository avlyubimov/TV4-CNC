# Проверки PR-02, 18 ноября 2026

- host.log: корневой CMake + Ninja + LVGL, **6/6 PASS**, включая motion.
- sanitized.log: те же **6/6 PASS** с ASan/UBSan/LeakSanitizer.
- arm-analysis.log: ARM GCC 14.2.1 (Ubuntu package 15:14.2.rel1-1), Cortex-M4F
  static archive; **612 text bytes, data/bss 0**; GCC -fanalyzer — PASS.
- board-gate.log: TV4_MOTION_BOARD_IMAGE=ON ожидаемо отклонён (exit 1).
- Motion test содержит 7 сценариев, включая 99 999 fault-injection cycles.

Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2; Linux x86_64.
CI использует официальный ARM GNU 14.2.Rel1 archive с закреплённым SHA-256.
Локально использован установленный пакет Ubuntu той же версии GCC; отдельный
официальный архив локально не загружался. Его digest взят с developer.arm.com.
Первый sanitizer запуск внутри ptrace sandbox завершился сообщением LSan о
несовместимости с ptrace. Полный итоговый запуск выполнен вне sandbox и прошёл;
санитайзеры не отключались, переменные suppressions/detect_leaks=0 не применялись.

ESP32 код и конфигурация не менялись, локальная ESP-IDF matrix повторно не
собиралась; существующие workflows проверяют её после push. Ни startup/linker,
ни .bin для STM32 не созданы. GPIO, SWD, CAN и приводы не использовались.
Объектные 612 байт не означают расход Flash/RAM готового board firmware.
