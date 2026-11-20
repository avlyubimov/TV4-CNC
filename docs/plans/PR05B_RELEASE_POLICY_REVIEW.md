# PR-05b — согласованность проверок release metadata

20 ноября 2026. База main `455ccc6`, PR #1–33 слиты; пять main workflow PASS.

1. Проверить существующие HMI signed manifest/envelope, SemVer, channel,
   component и trusted local context. Не добавлять второй parser/SemVer policy,
   вымышленные production versions, motion component или trust provisioning.
2. Воспроизвести malformed Unicode escape и noncanonical Base64 расхождения;
   исправить local lexical gate/offline checker без изменения pinned cJSON,
   зависимостей, рабочих UI/network путей или выделения дополнительной памяти.
3. Добавить независимые Unicode/encoding tests и differential signed corpus:
   одинаковые raw bytes/context/key/signature проходят настоящий native API и
   Python offline verifier. Ключ только ephemeral в памяти host helper.
4. Проверить bounded SemVer subset, channel/component independence, replay/
   downgrade, protocol/peer/local unknowns, component/domain separation, output
   cleanup и tampered bytes. Добавить CI и воспроизводимые evidence.
5. Обновить release/next-stage/roadmap/hardware unknowns, отдельный PR без merge.

Критерии: оба verifier дают одинаковое accept/reject на corpus; invalid encoding
не оставляет candidate; host/LVGL и ASan/UBSan/LSan PASS, native static analysis
и прежняя SDK matrix8 PASS. Для STM32 сохраняются все прежние gates/CI.

Не выполнять device/SWD/UART/reset/flash/network probes, не менять GPIO/layout,
не назначать CAN IDs/codec/bitrate, production key/version, реальную F407 memory
map или секторные адреса. Owner повторил только суффикс VGT6: полная маркировка,
HSE, RAM/CCM/vector/erase/protection/GPIO/CAN всё ещё неизвестны.
