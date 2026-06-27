# Daily NPC refresh on midnight day-rollover is missing

**Confidence:** High (behavior fully absent in OpenGothic; original logic decoded and offsets verified)

## Original function + address

`oCWorldTimer::Timer` (Gothic2.exe @0x00780d80) is the per-frame world-clock
tick. It accumulates elapsed sky-time into the fractional time field; when the
accumulated value exceeds one full day it subtracts a day's worth and increments
the integer **day counter** (`this+4`). On that day-rollover branch — and only on
it — it calls `oCGame::RefreshNpcs` (@0x006cb5f0) followed by
`oCRtnManager::CheckRoutines`. Off the rollover branch it calls only
`CheckRoutines`. So `RefreshNpcs` fires exactly once each time normal play crosses
midnight (the "day-change event").

`oCGame::RefreshNpcs` (@0x006cb5f0) walks the world NPC list and, skipping the
player, calls `oCNpc::RefreshNpc` (@0x00742110) on each, then `DeleteTorches`
(@0x006cb640) to remove dropped/burning torch vobs.

`oCNpc::RefreshNpc` (@0x00742110), for every living non-player NPC
(guard `0 < attr[HITPOINTS]`), does:
- if no torso/armor item is equipped, auto-equips the best wearable armor and
  invokes the script function `B_RefreshArmor` (SELF = the NPC);
- if a ranged weapon is owned but no munition is available, creates 50 rounds of
  the matching munition;
- restores attributes to full: `attr[HITPOINTS] = attr[HITPOINTSMAX]` and
  `attr[MANA] = attr[MANAMAX]`;
- re-applies model overlays (`CheckModelOverlays`).

Attribute offsets confirmed via `oCNpc::GetAttribute` (@0x0072ff20):
`return *(int*)(this + param*4 + 0x1b8)`, so `0x1b8/0x1bc/0x1c0/0x1c4` =
`HITPOINTS / HITPOINTSMAX / MANA / MANAMAX`. The two writes in `RefreshNpc`
(`0x1b8 = 0x1bc`, `0x1c0 = 0x1c4`) are therefore a full HP and mana heal.

Net effect in the original: every real-time midnight crossing heals all living
non-player NPCs to full HP/mana, re-equips armor (firing `B_RefreshArmor`),
refills ranged munition, refreshes overlays, and deletes stray torches.

Note: this is the *normal-time* midnight path. A scripted time skip
(`Wld_SetTime` → `oCGame::SetTime` @0x006c4de0) takes a different path
(`SetDailyRoutinePos`/`SpawnImmediately`) and does **not** call `RefreshNpcs`,
so the daily refresh is genuinely tied to the incremental day-counter increment.

## OpenGothic file:line

- `game/world/world.cpp:375` `World::tick` — no day-change detection.
- `game/game/gamesession.cpp:313` `GameSession::tick` / `:325`
  `wrldTime.addMilis(...)` — advances `gtime` (and thus `gtime::day()`,
  `game/game/gametime.h:17`) but never compares the new day against the previous
  day, so nothing is triggered on rollover.

There is no `RefreshNpc`/`RefreshNpcs`/`B_RefreshArmor`/`DeleteTorches` equivalent
anywhere in `game/` (grep-verified). OpenGothic NPCs are therefore never healed,
re-armored, or re-supplied at midnight.

## Divergence

In OpenGothic, living non-player NPCs that were damaged, disarmed, or ran out of
arrows the previous day stay that way indefinitely; the original silently
restores them to full HP/mana, re-equips armor and refills munition at each
midnight. Dropped torches also persist across midnight in OpenGothic. This is a
true behavioral gap on the world-timer day-wrap.

## Proposed patch

**DEFERRED.**

Reason: this is a missing feature, not a one-symbol divergence, and a partial
implementation would itself diverge. A faithful fix needs all of:
1. day-change detection in the tick loop (track the previous integer day,
   compare against `game.time().day()` after `addMilis`, and serialize the
   previous-day value so save/load does not spuriously re-trigger);
2. a `RefreshNpc` routine reproducing the full original semantics — armor
   auto-equip + `B_RefreshArmor` script call (via `GameScript::invokeState`-style
   helper), ranged-munition top-up to 50, `changeAttribute(ATR_HITPOINTS, …)` /
   mana restore to max (symbols `ATR_HITPOINTS/ATR_HITPOINTSMAX/ATR_MANA/
   ATR_MANAMAX` and `Npc::changeAttribute` are grep-verified present), and model-
   overlay refresh;
3. the `DeleteTorches` sweep.

Each sub-step has its own parity surface (which NPCs qualify, munition item
resolution, overlay re-application order) that must be decoded against the
original before reimplementation. Implementing only the HP/mana heal would create
a new, opposite divergence, so per "empty beats false positives" this is logged
for a dedicated change rather than patched piecemeal here.

// NOTE: in original-game oCWorldTimer::Timer @0x00780d80 the day-counter
// increment branch calls oCGame::RefreshNpcs @0x006cb5f0 → oCNpc::RefreshNpc
// @0x00742110 (full HP/mana heal, armor re-equip + B_RefreshArmor, munition
// refill) and DeleteTorches @0x006cb640; OpenGothic's World/GameSession tick
// performs none of this on midnight day-rollover.
