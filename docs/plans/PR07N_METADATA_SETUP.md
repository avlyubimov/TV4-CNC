# PR-07n — metadata setup и общий volatile namespace

База main `d18cf93`, PR #1–45 слиты. Отдельный PR, без устройств/merge.

1. Сохранить CLOSED image negotiation до explicit clear и добавить single-use
   metadata ID reservation в тот же responder pool: image+1 без wrap/reuse.
2. Отдельный bounded REQUEST/OFFER/ACCEPT/CONFIRM coordinator и exact LE codec;
   привязать к original CLOSED negotiation и ещё пустому live image TX/RX.
3. Explicit raw fragment mode; прежний typed mode и TU codebook оставить strict.
4. One-clock host loss/retry/fault tests и delivered metadata → independent
   signed image/lease/model/close цепочки; сохранить прежние test modes.
5. Separate ARM archive/import/frame budget, negative guards, CI/text evidence,
   согласованные docs; publish PR, дождаться CI, не сливать.

Приёмка: один metadata ID из общего счётчика, no rollback counters; owner pool
удерживается до actual completed metadata TX/TAKEN metadata RX; no fresh image
windows после setup/metadata. One-time params не auth/install/live receipt.
Duplicates не сбрасывают send/total clocks. Потерянный CONFIRM восстанавливается
после responder take. Context/clock failures sticky. Codecs canonical/disjoint,
ошибка decode zeroes output. Старые budgets не повышать; no heap/I/O/globals.

Инженерное ограничение: paired image+1 allocation — экспериментальный local
contract внутри fixed boot pair, не production provider/authentication или
reboot replay protection. Hash fields opaque; independent verifier обязателен.
F407 package/RAM/geometry/HSE/GPIO/CAN и installer остаются blockers. Никаких
адресов Flash, CAN IDs/bitrate, приводов, ключей, firmware runtime callers.
