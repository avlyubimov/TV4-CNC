# PR-07d — read-only vector/memory preflight

20 ноября2026. База main4b7631e, PR #1–31 слиты, пять main workflows PASS.
Продолжить существующие admission/handoff, не создавать новый bootloader.

## Работа

1. Portable bounded validator explicit local memory profile: image range/capacity,
   vector count/alignment/reserved slots, non-overlapping RAM spans, initial
   stack range/alignment и Thumb handler bounds внутри signed image.
2. Worker-only handoff entry после signature/SHA: read того же immutable lease,
   snapshot profile, guards before/after read/final cleanup, привязка отчёта к
   image/session/source/policy. Ошибка отзывает candidate и закрывает lease.
3. Positive/negative/exhaustive boundary tests и signed RAM end-to-end cases;
   root LVGL/sanitizers, ARM/source/import/frame/static/image/output CI gates.
4. Docs/evidence, exact versions прежние, отдельный PR без merge/прошивки.

## Приёмка и ограничения

- PROFILE_MODEL_MATCH — только соответствие явно переданной модели, не
  утверждение реальных RAM/Flash/vector layout, bootability или install permission.
- Ни одного real F407 memory profile/default, app/bootloader/OTA address/sector,
  Flash writer/commit/jump/VTOR/MSP/reset/clock/GPIO/CAN или permissive safety stub.
- Owner SWD/core/Flash1024KiB/3,252V/backup facts сохранены. Package/RAM/CCM/HSE/
  pinout/erase/protection/GPIO/CAN/restore/physical quiescent gates не закрываются.
- Synthetic addresses принадлежат tests, не конфигурации платы. Profile identity
  связан с signed memory_profile и local policy generation; его уникальность и
  соответствие hardware обеспечиваются будущим reviewed board contract.
- Стек8-byte и обязательные handlers вне vector bytes — строгая проектная
  политика, не заявления о настройке STKALIGN или достаточном runtime stack.
