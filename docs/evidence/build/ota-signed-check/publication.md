# 23 ноября: разрешённый prerelease опубликован и скачан обратно

[Release v0.1.0-bench.2](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.2),
ID455827310, tag → `c013ddf1346bf1aaf3c60eacf14e07fe4846e2c8`.
Владелец отдельно утвердил exact tag/commit/11 assets после CI PASS.
Release отмечен prerelease, не stable/latest. GitHub также показывает два
автоматических source archives; они не дополнительные загруженные firmware assets.
[Машинный протокол](publication.json) сохраняет все размеры/SHA-256/asset IDs
и CI jobs: source push+PR30/30, merged main15/15, tag15/15 PASS.

Все 11 assets скачаны по HTTPS с обычной проверкой сертификата и совпали
с утверждённым локальным inventory. Его SHA-256:
`866b9ff48462cd87ec9c5d85f0dd0e1415f9ab3fe2a4ffa672c48abd7bfcfea3`.
BIN SDK6.1.1:1598560bytes,
`017236a2abec4f71b8737a396a9601fe603d61931babc05405a9df874d1663de`.
Подпись скачанного manifest, политика и полный скачанный BIN повторно проверены
существующим Python/OpenSSL verifier и [C caller](native_stream_check.c):
manifest0/VERIFIED/error0/1598560bytes, hash contexts закрыты после image_close.
Clock C caller синтетический; это не замер сети, RAM или аппаратного времени.

Оба лицензированных локальных payloads c013ddf (SDK5.3.2 и6.1.1) также прошли
полную native проверку; изменение одного байта отвергнуто с image error8.
Публиковался только SDK6.1.1. Собственный код MIT по решению владельца;
LICENSE/attribution и157 vendor notice files приложены. Ключи, private context,
Flash dumps, bootloader/table/NVS/otadata и STM32 не загружались.

Candidate unprovisioned: служит только реальным image body read-only CHECK,
его установка и production применение не приняты. Это не завершение P1/P2:
ресурсная SESSION4, install/confirmation/rollback, power-loss/soak и
воспроизводимость/аутентифицированное CI provenance ещё требуют проверки.
Первый configured стенд описан [отдельно](../../hardware/2026-11-23-esp32-signed-check-c013ddf/README.md).
