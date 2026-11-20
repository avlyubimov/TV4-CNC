# CAN: стендовый heartbeat и подготовка update transport

CAN ID, скорость и аппаратный драйвер не назначены. Передача всегда возвращает false.
`model_receive` принимает только тестовый heartbeat: ровно 4 байта
[версия=1, sequence uint8, reserved=0, reserved=0]. После первого пакета требуется
следующий sequence по модулю 256; повтор/пропуск защёлкивает FAULT.
Таймаут 500 мс от подключения либо последнего принятого пакета; время uint32 ms,
вызывать tick регулярно, интервал меньше половины диапазона uint32.
Reconnect и ACK никогда не переводят в READY. Никаких команд движения/авторизации
в MVP нет. Реальный протокол требует отдельного согласования и тестирования.


## Production protocol и обновления — ещё не реализация

Трансиверы пока отсутствуют. Negotiation/capabilities, machine state,
coordinates/frames/units, RPM/feed, fault codes, command ACK/reject и FW transport
будут отдельной версионированной спецификацией. Её версия не назначается
по байту 1 текущего mock heartbeat. CAN ID/bitrate/GPIO не распределены.

CAN bootloader требует session/sequence/offset/length/integrity, bounded
fragmentation/ACK/retry, подтверждённого safe state, полного SHA и подписи
в самом F407 bootloader. Пока только план/host моделирование, без Flash driver
и без физической передачи. CAN не является safety chain.

PR-07f: [logical fresh negotiation](../../docs/STM32_UPDATE_NEGOTIATION.md),
REQUEST/OFFER/ACCEPT/CONFIRM, volatile identity pool и one-time transport setup.
Нет production wire/entropy/persistent identity/authentication; реальные
версии/IDs/bitrate/GPIO по-прежнему не назначены. DATA APIs не меняются.

## PR-07a: логический update-сеанс на ПК

[update_session.h](update_session.h) и [контракт](../../docs/STM32_UPDATE_TRANSPORT.md):
portable sender/receiver, stop-and-wait, explicit limits, CRC-32C, двухфазный
consumer/ACK, bounded retries, stale-session/reorder/deadline failure tests.
Нет wire format, CAN driver/IDs/bitrate, Flash, подписи, COMMIT или jump.
Конечный статус — COMPLETE_UNVERIFIED, не доверенная/установленная прошивка.
Модуль независим от motion, вызывается только memory host tests.

```bash
cmake -S protocol/can -B build-update-host -G Ninja
cmake --build build-update-host
ctest --test-dir build-update-host --output-on-failure
```

Новых embedded зависимостей нет; стандартный OpenSSL3 нужен только host SHA
oracle. Отдельная ARM compile-only библиотека использует уже закреплённый
toolchain из STM32 foundation и не создаёт board image.

[AGENTS.md](AGENTS.md), [OTA architecture](../../docs/OTA_ARCHITECTURE.md),
[открытые решения](../../docs/OPEN_QUESTIONS.md), [roadmap](../../docs/ROADMAP.md).
