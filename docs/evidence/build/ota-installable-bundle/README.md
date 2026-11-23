# Installable bench authoring — software PASS

Exact clean source `9ccbf1394c4c69e20565dc34b25bc3e416031a03`, PR #83 поверх
PR #82. Изменены local authoring/tests/docs; исполняемый firmware source не
менялся.274/274 host и274/274 ASan/UBSan/LSan, detect_leaks=1, PASS. Release
suite28methods покрывает два purpose, обе SDK descriptor версии, ephemeral
ECDSA-P256 подпись, capability/context/linkage/authority mismatch и прежние
source/ELF/image/permissions/no-overwrite/подмены. Source CI30/30 и неизменный
main15/15 PASS (`ci.json`), без production keys/настроек в CI.

Два **действительных compiled HMI** ELF/BIN, pinned SDK6.1.1/5.3.2, public-only
test context и disposable non-owner key: local prepare → signature/inventory/
appended SHA/descriptor/ELF SHA/version/source/key binding → independent повторный
verify PASS. Native C manifest policy принимает обе реальные подписи/size/hash/
installable:true; C stream verifier обоих BIN проверяет descriptor/checksum/
dual-SHA, в ordinary и ASan/UBSan/LSan режимах. Это host validation actual SDK
images, не работа этих images на плате и не native Flash installer test.

| SDK | Actual app bytes | Worker request | Own operation frame | Diagnostic writable |
|---|---:|---:|---:|---:|
|6.1.1|1600192|12288|480|0|
|5.3.2|1552720|12288|480|0|

Точные BIN/ELF/source/SDK/file inventory hashes — `results.json`. Это compiler
frames/выборка build outputs, не runtime peaks или production budget. Default
memory/auto-bench flags отключены. Native source/API, A/B layout, PSRAM XiP,
LVGL allocator, CA/date/time/resource/size bounds и dependency pins сохранены.

Версии0.1.0-fixture.1→.2 и reviewed flag относятся только к software fixture.
Owner private/trust key не использован, owner context не изменён. Test context
переиспользует прежние инженерные resource/time limits; это не утверждение
установочной политики владельца. Test private key после двух подписаний удалён;
public verification без него успешна. Сами BIN/ELF/signatures/key files остаются
локально, не разрешены для устройства/публикации и не входят ни в один sealed
hardware packet. Ни одного fake STM32 artifact нет.

Output всегда device_access/install_enabled/release_published/ci_identity_verified/
production_resource_qualified=false. Core bundle — девять файлов app/manifest/
signature/checksums/evidence/notes/license, не новый GitHub Release. При настоящей
публикации отдельно формируются полные notices и exact release inventory,
проверяются CI/downloads, утверждаются новый commit/tag/assets. Owner installable
payload/версии/context ещё не согласованы; published bench.2 unchanged CHECK-only.
Network INSTALL/resource/rollback при скачанном образе и отказах Wi-Fi/питания
остаются отдельными hardware критериями. [План](../../../plans/P1_INSTALLABLE_BUNDLE.md).
