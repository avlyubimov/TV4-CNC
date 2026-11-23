# P1 — подготовка подписанного installable HMI payload

После signed CHECK и native USB confirmation/rollback следующий сетевой INSTALL
требует другого configured payload. Published unprovisioned bench.2 менять или
устанавливать нельзя. Этот шаг расширяет существующий prepare_ota_release.py,
а не добавляет криптографию, HTTPS или новый слой OTA.

План сохраняет точный reviewed clean source/SDK/SemVer/key pin и явный purpose:
старый signed-check-bench или новый signed-install-bench, только prerelease.
Первый всегда подписывает installable:false; второй требует true и действительный
configured candidate с install_reviewed:true, target app version и тем же pinned
public key. Public DER должен присутствовать и в actual ELF, и в BIN. Это
дополнительная проверка связи build inputs, не аттестация поведения произвольного
кода: source/CI/ELF binding/review и native self-test остаются обязательными.

Signed capability не разрешает запись самостоятельно. Receiving baseline требует
своего local review и конкретного операторского подтверждения. Offline output
всегда device_access/install_enabled/release_published/ci_identity_verified/
production_resource_qualified=false. Разные scope notices/evidence и повторная
signature/image/purpose проверка не позволяют unsigned metadata превратить
CHECK в INSTALL. Только app, без bootloader/table/otadata/NVS/private key.

Software tests используют ephemeral keys и НЕИСПОЛНЯЕМЫЕ synthetic images:
оба SDK descriptor formats, matching provisioned context, linked key, local
review/version/key refusal, mismatched purpose и rehashed authority claims.
Общие byte-bound, signature, app/ELF, source, no-overwrite и key0600/dir0700
проверки сохраняются. CI запускает эти проверки в обычной и sanitizer матрице.

Далее два действительных ELF/BIN обоих pinned SDK с отдельным test key и
0.1.0-fixture.1→.2 проверяют offline authoring/verification и native manifest
policy. Их нельзя прошивать или публиковать; owner key/context не используются.
Согласованные owner installable versions/context/resource policy и новый exact
commit/tag/assets approval ещё нужны для настоящего payload/publication.
Нельзя подставить reviewed:true в реальный owner plan автоматически.

Этот PR не меняет исполняемую firmware, не публикует новый release/tag, не
выполняет Flash/rollback/power-loss/device тесты и не делает self-merge.
Сетевой INSTALL/resource acceptance остаётся отдельным измеримым результатом.

## Программная приёмка завершена

[Actual compiled/signed fixtures и independent C/Python checks](../evidence/build/ota-installable-bundle/README.md)
на source9ccbf13 PASS: оба pinned SDK,274 host/274 sanitizers,28 release methods,
source CI30/main15. Test private key удалён. Никакой device/publication approval
из этого результата не следует. Реальный owner plan остаётся отдельным решением.
