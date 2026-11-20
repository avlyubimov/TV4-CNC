# PR-07f — свежий логический update-сеанс

21 ноября2026. База main `f0bd866`, PR #1–35 слиты, открытых PR перед работой нет.

1. Только memory-model REQUEST/OFFER/ACCEPT/CONFIRM перед DATA; обе стороны,
   explicit boot pair/compatibility tag/local policy generation/limits и clocks.
   Никаких production CAN version/IDs/codec/bitrate/GPIO или physical safe-state.
2. Caller-owned identity pool: один boot pair, монотонные local counters без
   wrap, peer request high-water сохраняется через clear; init не перезаписывает
   live pool. Boot epochs/их уникальность после reset предоставляет future
   reviewed backend, не случайные/тестовые default значения.
3. Binding image length/SHA и raw metadata SHA; selected block<=обеих сторон.
   Exact duplicate повторяет cached reply без нового ID/publication/deadline;
   conflict/clock/context/timeout/retry/exhaustion fail closed. Ready выдаётся
   один раз и только в конечном окне. Это transport setup, не authentication,
   проверка подписи/образа, install token или physical readiness.
4. Host loss/reorder/replay/reboot/wrap/context/argument/snapshot tests и signed
   negotiation→DATA→independent P-256/SHA→model→clean close→outcome chain.
   DATA RX deadlines не менять; copied historical values не оживляют сеанс.
5. Host/LVGL/sanitizers, standalone/ARM/static/import/frame/budget/image/output
   gates, прежняя SDK matrix8; docs/evidence/status и отдельный PR без merge.

Критерии: fresh IDs при последовательных передачах, старые REQUEST/ACCEPT/CONFIRM
не создают повторную DATA start publication; loss recovery конечен, число replies
ограничено; реальные crypto/model checks в integration сохранены. Добавить явный
тест, что полностью согласованные forged messages могут пройти logical handshake:
это не аутентификация CAN и не разрешение движения/установки.

Нет runtime caller, entropy/persistent identity provider, storage/Flash writer,
реального MCU profile/map/sector reservation, keys/releases или device access.
F407 HSE/full marking/RAM/CCM/GPIO/CAN/erase/recovery unknowns сохраняются.
