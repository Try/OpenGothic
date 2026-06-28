# Engine-side C_NPC.aivar[] reads/writes — NO FINDING

**Confidence:** High (negative result)

## Conclusion
No high-confidence engine-side `C_NPC.aivar[]` read/write divergence was found. In the
original `Gothic2.exe`, the ZenGin engine treats the `aivar[]` array as opaque,
Daedalus-only storage: no engine C++ function reads or writes a specific `aivar` index
to gate or trigger a behavior. OpenGothic's two existing `aivar` touches therefore have
no additional engine-side `aivar` access to mirror.

## aivar offset (established, ground truth)
The C_NPC script-instance fields are embedded inline inside `oCNpc`:
- `oCNpc::GetAttribute` @ engine returns `*(this + idx*4 + 0x1b8)` → `attribute[]` at **0x1b8**.
- `oCNpc::GetGuild` reads `this+0x230`; `oCNpc::Archive` writes `start_aistate` from `this+0x264`.
- `spawnPoint` is a 20-byte inline `zSTRING` at 0x268; `spawnDelay`=0x27c.
- `oCNpc::SetSenses` writes `this+0x280` (`senses`); `oCNpc::CanSense`/`CanSee` read `this+0x284`
  (`senses_range`, compared against a float distance).
- Therefore **`aivar[0]` = 0x288**, `aivar[99]` ends at 0x417, and `wp` (zSTRING) begins at
  0x418 (= 0x288 + 100*4), which `oCNpc::Archive` serializes — confirming the array bound.

## Scan performed
For each method I checked both `this + [0x288..0x414]` (own-aivar) and any `<ptr> + [0x288..0x414]`
double-deref (other-npc aivar), over the decompiled output of:
- All 636 `oCNpc::` methods (incl. `OnDamage*`, `Assess*_S`, `FightAttackMagic`, `EV_CastSpell`,
  `ReadySpell`, `Interrupt`, `PerceiveAll`, `PerceptionCheck`, `DoDie`).
- All 201 `oCAIHuman::` / `oCAniCtrl_Human::` / `oCNpcFocus*` methods.
- All 138 `oCMag_Book::` / `oCSpell::` / `oCNpc_States::` methods.

Every in-range offset that appeared resolved to the *calling class's own* member (e.g.
`oCAIHuman` fields 0x2d0/0x2d4, `oCAniCtrl_Human` ani-state arrays at 0x300/0x328/… step 0x28,
`oCMag_Book` field 0x32c, `oCSpell` field 0x3b8, item/inventory pointers in `EquipItem`/
`UseItem`/`DoInsertMunition`), **not** a dereference of an `oCNpc*` into `[0x288..0x417]`.
The only `oCNpc` this-relative reads in that vicinity are `senses` (0x280) and `senses_range`
(0x284), which sit just below the aivar array and are not aivar.

## Notes on the listed candidates
- `AIV_PARTYMEMBER` (idx 15, 0x2c4): already handled by OpenGothic
  (`game/game/gamescript.cpp:1457`, `isFriendlyFire`). The engine's own friendly-fire/attitude
  path (`oCNpc::OnDamage*`, attitude resolution) does **not** read `this+0x2c4`; it works off
  attitude, so there is nothing extra to mirror.
- `AIV_SpellLevel` (idx 88, 0x3e8): already handled — OpenGothic clears it in `beginCastSpell`
  (`game/world/objects/npc.cpp:4294`). No engine C++ read of 0x3e8 on an `oCNpc*` exists.
- `AIV_SELECTSPELL` / spell selection: the engine cast path (`oCNpc::EV_CastSpell`,
  `FightAttackMagic`, `oCAIHuman::MagicInvestSpell`, `oCMag_Book::GetSelectedSpell`) selects the
  spell via the spellbook object state, never via an `oCNpc` aivar index. Spell selection
  `aivar` is set/read purely in Daedalus (`ZS_MagicAttack`).
- `AIV_INVINCIBLE`, `AIV_LASTTARGET`, `AIV_NPCSTARTEDTALK`, `AIV_DROPDEADANDKILL`,
  `AIV_MM_REAL_ID`: no engine read/write of the corresponding `0x288 + idx*4` offset on an
  `oCNpc*` was found. These are pure-Daedalus (set/checked in `B_*` script functions).

## Outcome
NO FINDING — DEFERRED (no engine-side aivar access exists to mirror). Empty beats a false positive.
