# Выполненный сеанс ee7d179 — не повторять автоматически

Разрешение владельца относится к одному сеансу ≤180s; он завершён. Изолированная
F407, только питание/SWD. Exact source/hash — identity.json, raw results — openocd.txt.

До init локальный launcher проверил clean source commit/ELF/BIN/config bytes,
exact-head CI26/26, три snapshot files по1048576bytes с recorded SHA/byte equality.
Выходные session files создавались no-clobber, umask077/dir0700. Новый full-Flash
backup с устройства не читался. Hardware launcher запускал OpenOCD **один раз**:

```text
timeout --foreground --signal=TERM --kill-after=2s 175s   /usr/bin/openocd -f <checked-session>/install-bench.tcl
```

Скрипт заморожен с теми же OpenOCD config/symbols. [Redacted source](executed-sequence.tcl.txt)
нельзя запускать без замены placeholders/повторного review/нового разрешения.
До program: CPUID part0xc24, exact ID0x101f6413, Flash1024KiB,
OPTCR RDP0xAA/nWRP0xFFF; никакого unlock/option write.

Основной путь: init/reset-halt → guard → program exact ELF verify →104-byte
identity → reset-run/1500ms/halt/INITIAL → resume/1500ms/halt/PROGRESS →
RAM watchdog request/resume/5000ms/halt/WATCHDOG → RAM fault request/resume/
5000ms/halt/FAULT → resume/1500ms/halt/FINAL → exact identity/resume/shutdown.
Все пять reads проверяют42diagnostic words, CCM/SystemCoreClock, по8guard words
на каждом стеке и echo21history words. UART/GDB/trace servers не открывались.

Предусмотренный conditional fallback выполняет не более одного full1MiB snapshot
program/verify/reset после отказа и только при доступном SWD/прошедших guards.
Он **не выполнялся**: main block прошёл, process exit0, wall16.196s.

Локальный parser после сеанса (без MCU) проверил пять snapshots, boot1→2→3,
IWDG reset bits/history/reason, guard/self-test/progress/identity markers, CRC32
пяти history records и UDF0 opcode по fault PC в исходном проверенном ELF.
Новых программных/hardware claims сверх этих наблюдений нет. Полные private
пути/backup SHA в публичный отчёт не включены.
