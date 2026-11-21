# F407: первая разрешённая SWD-проверка без записи Flash

База main4fe84f1, PR #56 слит. Образ c09a20e остаётся неизменным.

1. Сверить local artifact SHA/main CI/software tools; получить отдельное
   разрешение владельца на reset/halt/read/reset-run изолированной F407.
2. Выполнить bounded сеанс: CPUID/DBG_ID/Flash size/геометрия/protection/options,
   без Flash/OTP writes/erase/unlock/option changes/CAN/приводов/backup reads.
   Сохранять exit/log; ошибка не является PASS и не разрешает новую попытку.
3. Исправить обнаруженную несовместимость runbook с OpenOCD0.12, повторить
   только по новому разрешению; сохранить обе попытки и реальные пределы.
4. Обновить hardware/open questions/next stage/roadmap и активный статус,
   проверить ссылки/scope/неизменность firmware. Отдельный PR, без self-merge.

Измеримый результат — фактический SWD/protection/sector report текущей платы,
а не новый firmware layer. Flash installation/startup/HSI/RAM/watchdog/fault
test ещё требует верифицированного backup/second copy/marking и отдельного
разрешения на exact ELF. Ревизия ядра не silicon revision; Flash driver geometry
не является erase/recovery experiment. Никаких новых bootloader addresses.
