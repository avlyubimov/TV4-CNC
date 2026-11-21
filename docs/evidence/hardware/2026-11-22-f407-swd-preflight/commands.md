# Выполненные операции и программная верификация

Выполнены пять отдельных разрешённых сеансов: два preflight, backup,
точный c09 install/bench и новый manual-reset recovery/conditional restore.
Все завершены; новый образ и новый SWD-сеанс требуют отдельного scope.
Разделы preflight/backup ниже описывают их историческое read-only состояние;
поздняя запись/restore выделены отдельно.

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
На этапе preflight Source/ELF/BIN не менялись. Последующие hardware attempts
и программное исправление описаны ниже и в новом build evidence.

## Третий отдельно разрешённый сеанс — текущий Flash snapshot

Bound175s + kill-after2s внутри разрешённых180s; private directory0700,
process umask077, существующие destinations отклоняются до init. После
reset/halt и CPUID/ID/Flash/RDP guards:

```tcl
# Each destination is a NEW private local file, never an existing backup.
flash read_bank 0 $destination_a 0 0x100000
flash read_bank 0 $destination_b 0 0x100000
```

Затем тот же cleanup catch/reset-run/shutdown; process exit0. Два отдельных
полных reads сравнивались уже локально: exact1048576bytes каждый, полное byte
comparison, streaming SHA-256, начальные MSP/Thumb reset и permissions0600.
Checksum file и private manifest остаются рядом с дампами вне Git. Это текущий
снимок, не подтверждение происхождения factory image; два файла на одном
filesystem не закрывают gate независимой копии на другом носителе.
[Redacted log](backup-session-redacted.txt), [receipt](backup-receipt.json).
Ни один байт Flash/OTP/options не записывался; запись host files не Flash write.


Владелец поручил самостоятельно выбрать папку для независимой копии. OS metadata
показала разные физические родители: source nvme1n1 и выбранная copy nvme0n1.
Локальное копирование новой1MiB file (no-clobber, fsync) и повторный SHA прошли,
mode0600. Это файловая операция, не новый SWD-сеанс; полное дерево mount/device
metadata и private пути/backup SHA не публиковались. Source/copy manifests локальные.

## Четвёртый сеанс — точный c09 install/bench

Отдельно разрешены до180s: program ELF/verify/reset, identity/RAM/progress и
два RAM test requests, условный один full snapshot restore при отказе и доступном
SWD. Local guard сверил clean c09 ELF SHA и private snapshot SHA/размер до init.
External timeout175s+kill-after2s; debug servers disabled; log private0600.
[Actual log](install-c09-failed.txt): program/verify/clean identity PASS,
после run SWD lost; conditional restore остановился на reset-halt/read APB,
**до вызова program snapshot**. Весь bench exit1; self-tests не подтверждены.

## Пятый сеанс — manual RESET recovery и restore

После нового разрешения владелец удержал RESET. Config из текущей ветки с
DBG_SLEEP и IWDG freeze, SWD100kHz (включая reset events) настроены до `init`.
Source не выполнял initial Flash write. После ready marker владелец отпустил
RESET и подтвердил; отдельный новый local flag продолжил уже разрешённый сеанс.
Повторное использование stale release flag отвергается до init.
Read guards/identity совпали, FLASH_ACR/RCC_CR/CFGR/DBG_CR захвачены. После
reset/run прочитаны42 diagnostic words: state5/error8, self-tests0.
Условный fallback сверил protection, выполнил **один** program полного1MiB
snapshot по0x08000000, verify/reset-run. Marker VERIFIED_SNAPSHOT_RESTORED.
[Actual log](recovery-c09-and-restore.txt). Bench exit1 сохранён; restore PASS
опирается на отдельные Verified OK/reset/marker, не на общий exit code.

Оба Tcl сценария предварительно проверены без hardware на9 ветвях каждый:
успех, ошибки identity/self-test/progress/watchdog/fault и отказ восстановления.
Это проверка orchestration, не доказательство аппаратного результата. Полные
скрипты с private paths и raw logs локальные; их команды не повторять автоматически.
Новое software evidence — [исправление](../../build/stm32f407-bringup-fix/README.md).
