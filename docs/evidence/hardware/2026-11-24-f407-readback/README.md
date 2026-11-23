# 24 ноября: F407 readback и health PASS

Один автономно разрешённый сеанс ST-Link V2/OpenOCD0.12.0,21,791s, exit0.
Init/halt, direct read1MiB Flash, ID/protection/build/diagnostics/guards, resume
1200ms и второй snapshot, final resume. Reset/program/erase/option bytes/OTP/
RAM test requests/CAN/motion не выполнялись. Debug Sleep/IWDG halt freeze —
volatile политика debugger; target RAM work area0 исключает checksum/write
algorithms поверх работающей RAM. Это не полностью пассивное наблюдение.

ID0x101f6413, Cortex-M4r0p1/CPUID0x410fc241, Flash1024KiB, RDP0/nWRP0xFFF.
Voltage sample3,259416V — показание probe, не калибровка или нагрузочный тест.
Все2540bytes приложения/векторы совпали с clean ee7d179; остаток sector0 FF,
секторы1–11 совпали с прежним checksum-verified snapshot. Новый current snapshot
сохранён с SHA-256 в двух приватных копиях на независимых NVMe. Это один новый
полный SWD read и проверка ожидаемых bytes, не два независимых hardware reads
и не доказательство factory происхождения.

State RUNNING4/error0, data/BSS/один CCM word/guards1, motion_outputs0.
Ticks24057142→24058339, heartbeat и watchdog feeds240571→240583; boot_count2
не изменился, две warm-history CRC проверены offline. Счётчики номинальные,
частота/реальный watchdog timeout/full RAM stress не измерены. HSE/pinout/
physical GPIO/CAN/inhibit остаются открытыми. На F407 оставлен прежний ee7d179.

[Числовой результат](result.json), [исходный P0 стенд](../2026-11-22-f407-ee7d179-bench/README.md).
Ни raw Flash, ни его SHA/пути/USB serial в Git не публикуются.
