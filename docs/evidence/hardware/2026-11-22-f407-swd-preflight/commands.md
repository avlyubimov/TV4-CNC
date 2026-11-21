# Выполненные операции и программная верификация

Только после двух соответствующих разрешений владельца. Не повторять эти
команды автоматически: оба разрешённых сеанса завершены. Они не разрешали
Flash/OTP write/erase/unlock, option changes, чтение backup или новый install.

Версии: `/usr/bin/openocd`0.12.0, STLINK V2J29S7/API2, GNU gdb-multiarch17.1
(GDB в аппаратном сеансе **не запускался**). Local artifacts c09a20e прошли все
семь SHA256SUMS entries; main4fe84f1 CI5/5 success до доступа к плате.

```bash
lsusb -d 0483:3748
```

Один ST-Link/V2. В sandbox libusb initialization отказал, поэтому разрешённая
discovery и обе OpenOCD операции выполнены вне sandbox. Это не отказ
автоматической проверки одобрений, новых разрешений USB/udev не изменяли.

Оба вызова имели форму (private paths заменены):

```bash
umask 077
timeout --foreground --signal=TERM --kill-after=2s 50s \
  openocd -f '<local-evidence>/preflight.tcl' > '<local-evidence>/attempt.log' 2>&1
```

Tcl source использовал неизменённый
`firmware/stm32-motion/board/stm32f407vg/openocd.cfg`, отключал gdb/tcl/telnet
servers и оборачивал init/reset/read sequence в `catch`. После успешного init
отдельный catch выполнял cleanup reset/run текущего ПО; `shutdown error` при
отказе основного блока/cleanup. Никаких extra device retries внутри wrapper.

Последовательность исправленного reading block:

```tcl
init
reset halt
set cpuid [lindex [read_memory 0xE000ED00 32 1] 0]
# Refuse further F407 reads unless CPUID part[15:4] == 0xc24.
set device_id [lindex [read_memory 0xE0042000 32 1] 0]
# Refuse further Flash/options reads unless DEV_ID low12 == 0x413.
set flash_kib [lindex [read_memory 0x1FFF7A22 16 1] 0]
# Refuse mismatched capacity; expected1024KiB.
echo [flash banks]
echo [flash info 0 sectors]
set options [lindex [read_memory 0x40023C14 32 1] 0]
# Log RDP=(options >> 8)&0xff and nWRP=(options >> 16)&0xfff.
echo [stm32f2x options_read 0]
```

Первый вариант вместо явного echo использовал `flash banks`/`flash info 0`,
после чего неподдерживаемый `flash protect_check 0` завершил блок exit1.
Он не достиг options read, не является protection PASS и не скрыт из evidence.
Retry exit0, все три completion markers и обе raw voltage samples сохранены.

Installed command help проверен отдельно без hardware initialization:

```bash
openocd -c noinit -f firmware/stm32-motion/board/stm32f407vg/openocd.cfg \
  -c 'help flash info' -c 'help stm32f2x options_read' \
  -c 'help read_memory' -c shutdown
```

Новый программный parser разобрал12sector rows из actual retry log,
сопоставил number/origin/bytes со всеми profile.flash_sectors, проверил
`OPTCR=0x0fffaaed RDP=0xaa NWRP=0xfff` и cleanup/completion markers.
Это проверка сохранённых данных, не дополнительный SWD-сеанс.
Source/ELF/BIN не менялись и заново не прошивались. Новых host/SDK build
результатов этот docs/evidence PR не заявляет; CI выполнит существующую матрицу.
