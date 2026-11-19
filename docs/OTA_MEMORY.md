# PR-04f — PSRAM-only OTA workspace

После [baseline b93cf3b](evidence/hardware/2026-11-18-b93cf3b/README.md)
largest internal block 12288 bytes, свободно около 25–27KB. Session workspace
host sizeof 34024 bytes туда не помещается. TLS peak не измерен; runtime HTTPS
не включать. Этот PR даёт bounded allocation/lifetime механизм, не устраняет
все проблемы internal RAM и не выдаёт измерение за достаточный TLS reserve.

## Контракт

`memory.h/c`: lease zero-initialized, один worker владеет acquire/run/release.
Он не thread-safe; нельзя освобождать/сбрасывать из LVGL/cancel callback.
Повторный acquire занятого lease возвращает BUSY. Caller/ops context живёт
до release; все borrowed pointers после release недействительны.

1. Policy без reviewed=true возвращает NOT_REVIEWED до allocation/snapshot.
   Нет production defaults: internal free/largest и spare PSRAM floors должны
   быть ненулевыми, согласованы после измерений. Тестовые числа не являются
   разрешённым бюджетом для платы.
2. Snapshot before: internal floors, PSRAM свободная память с резервом и
   largest block не меньше sizeof(session). Size calculations без сложения,
   поэтому большие size_t floor не обходят overflow checks.
3. Единственный SDK allocation — heap_caps_malloc(sizeof(session),
   MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT), без internal fallback/retry.
   Проверяются esp_ptr_external_ram на начале/конце. Нет глобальных malloc hooks
   и изменений LVGL/cJSON/mbedTLS allocators или SDK memory configs.
4. Snapshot after: headroom мог измениться из-за Wi-Fi/других задач. При отказе
   allocation освобождается, lease остаётся пуст. При успехе session обнулена.
   Admission — snapshot, не резервирование TLS heap: future worker должен
   учитывать гонку после snapshot, пики и ошибки allocator/TLS в любой момент.
5. Release отказывается от active stages, закрывает hash contexts и очищает
   session перед free на idle/terminal. Empty release идемпотентен. Ошибки
   acquisition/region/snapshot не оставляют выделенный session buffer.
6. Host/headless/SDK без PSRAM adapter возвращает UNAVAILABLE. Generic test
   allocator не является реальным PSRAM backend.

API ничего не выделяет на startup: оно не вызывается main/UI и не создаёт
задачи/таймеры/сеть/Flash writes. Никаких production keys, install/boot selection.
Snapshot не доказывает достаточный budget для TLS, worker stack, crypto/cJSON
или графики; эти allocation остаются SDK/default. Не включать worker до review.

## Размер и CI

const uint32_t tv4_ota_workspace_bytes = sizeof(tv4_ota_session) сохраняется в
ELF. check_ota_memory.py читает значение из file-backed section ELF32 Xtensa,
не исполняя firmware; ограничение 16..36KiB + C static_assert ≤36KiB.
В BSS не резервируется dummy buffer. Exact target sizeof и app SHA/size:
[evidence](evidence/build/ota-memory/README.md).

Host failure tests: insufficient internal/PSRAM/contiguous block, unreviewed
policy, overflow floor, allocator failure, wrong region/end, before/after
snapshot failure, concurrent allocation headroom change, busy lifecycle,
crypto context cleanup, repeated retry/release. Остальные тесты сохраняются.
SDK 6.1.1/5.3.2 × headless/rev12 compile/link adapter и linked size constant.

## Что остаётся

- На плате этот код не выполнялся, allocation latency/heap/TLS peak не измерены.
- Проверить target PSRAM allocation по отдельному разрешению без HTTPS сначала;
  затем измерить TLS peak и выбрать floor/worker stack с запасом по факту.
- Реальный TLS time/trust/version/layout provisioning, bounded runtime worker
  snapshots/UI, CA/redirect/network loss, soak и USB A/B migration/rollback.
- UART capture procedure: прежний open/close сопровождался reset. Не повторять
  под видом read-only/без reset до отдельного решения и согласования.

RGB/DMA/framebuffer/draw buffers/PCLK/PSRAM timings, configs/locks/layout,
app_main/Wi-Fi/board/STM32/CAN/vendor sources не меняются. Не объявлять снижение
internal memory pressure или улучшение FPS до аппаратных замеров.
Откат — revert PR: runtime/persistent state не менялись, плата остаётся b93cf3b.

## Обновление аппаратного статуса

Allocation-only backend на [1c38d1e проверен: три цикла PASS](evidence/hardware/2026-11-18-1c38d1e-psram/README.md).
Предыдущие UNKNOWN про allocation относятся к состоянию PR-04f до этого теста.
TLS peak, production budget, cache-off/DMA/whole-PSRAM и soak остаются открытыми.

## PR-04m — последующие resource boundaries

[Resource guard](OTA_RESOURCE_GUARD.md) дополняет workspace acquire отдельной
local heap/stack policy во время guarded session, не заменяя lease/lifetime.
Нет резервирования TLS memory, default floors или гарантии внутри blocking SDK.
