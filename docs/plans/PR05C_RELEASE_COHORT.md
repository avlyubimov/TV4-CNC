# PR-05c — offline review состава релиза

База main2c06cdc, PR #1–39 слиты, открытых PR нет. Проверить итоговый main CI.

1. Host-only Motion CLI вызывает существующий independent release verifier
   на bounded regular files; явные key/context/selection, no defaults/signing.
   Не собирать host I/O/clock frontend для ARM/ESP или upload как firmware.
2. HMI helper возвращает signed metadata из того же snapshot, что проверяет
   образ; legacy CLI/report сохранить. Cohort tool использует один проход.
3. Проверить raw signature/full images обеих ролей, ESP app descriptor/SDK,
   expected release/source/channel и prospective Motion peer range. HMI-only
   допустим, fake Motion record нет. Counters не переводить в SemVer/protocol.
4. Trusted contexts/driver path — только local explicit inputs; никаких путей/
   keys/команд из manifest. Snapshot bounded regular inputs до verifier calls.
   Отчёт metadata-only, все install/publish/hardware/provenance gates закрыты.
5. Unit/CLI/cohort signed fault tests, root/sanitizers/standalone/ARM/source/
   analysis/gates, прежняя SDK matrix8, evidence/docs и отдельный PR без merge.

Это не production bundle/CAN schema, bootability/live lease/vector check,
protocol numeric↔SemVer mapping, actual installed-state/key provider или release.
Нет изменения firmware C/UI/pins/Flash/partitions/resources, devices/SWD/UART/
reset/Flash/backup reads, signing keys/publishing/installer или self-merge.
