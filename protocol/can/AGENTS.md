# CAN — правила протокола

Наследовать корневой AGENTS.md. Прочитать README.md, docs/ARCHITECTURE.md,
docs/OTA_ARCHITECTURE.md и docs/OPEN_QUESTIONS.md.

- CAN не safety-rated канал и не замена аппаратному E-stop/блокировкам.
- Текущий 4-байтный heartbeat v1 и 500 мс — инертный host/bench envelope;
  это не согласованная версия production protocol и не разрешение движения.
- Реальные GPIO, arbitration ID, bitrate, termination, transceiver и USB/CAN
  mux не выбирать без отдельного документированного решения владельца.
- Будущий протокол описывает capabilities/version negotiation, heartbeat,
  boot/session identity, machine state, coordinate frame/units, RPM/feed,
  fault codes, intent/ACK/reject, sequence, bounded timeouts и FW transport.
- Приём валидирует длины, диапазоны, версии, reserved поля и порядок.
  Повторный пакет не исполняет действие дважды; пропуски/stale/duplicates/
  wraparound/disconnect/reboot проверяются host/fault-injection тестами.
- ACK доставки не равен готовности/разрешению движения. Reconnect не ARM/START.
- Update-сеанс отделён от motion; chunks имеют sequence/offset/length/integrity,
  ограниченные ACK/retry/reassembly и полный SHA/подпись на стороне загрузчика.
  При reboot/потере сеанса требуется новое согласование безопасного состояния.
- Никаких CAN ожиданий в UI callbacks и никакого STEP из CAN ISR.
  До появления трансиверов проверять всё с memory/mock transport.
- PR-01 описывает требования, но не выделяет IDs и не меняет wire format.
