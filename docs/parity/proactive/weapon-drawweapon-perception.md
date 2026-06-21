# PERC_DRAWWEAPON never fired on weapon-draw completion

**Confidence:** High (the perception constant exists but is sent from zero call sites in OpenGothic; the original fires it).

## Original function + address

`oCNpc::EV_DrawWeapon2` (Gothic2.exe @ 0x0074d580) is the handler that runs when a
weapon-draw transition animation (`t_<mode>_2_<mode>` / the `def_opt_frame` of the draw ani)
reaches its completion frame. After it confirms the draw-transition ani is no longer active and
finalizes the fight-mode (`SetActionMode(0)`, commits the pending weapon mode), it fires, behind a
virtual is-actor predicate (vtbl slot `+0x100`):

  CreatePassivePerception(this, 0x18, this, NULL)

`0x18` == 24 == `PERC_DRAWWEAPON`. The sender and self are both the drawing NPC, victim NULL — i.e.
a passive perception broadcast to nearby NPCs announcing "this actor has drawn a weapon."

This is the exact mirror of the sheathe path: `oCNpc::EV_RemoveWeapon2` (@ 0x0074e630) fires
`CreatePassivePerception(this, 0xb, this, NULL)` — `0xb` == 11 == `PERC_ASSESSREMOVEWEAPON` — guarded
by the is-player virtual `+0x104` (`oCNpc::IsAPlayer` @ 0x007425a0, `this==player`). OpenGothic already
reproduces the remove side (player-guarded) in `Npc::closeWeapon`; only the draw side is missing.

## OpenGothic file:line

- Constant defined but unused: `game/game/constants.h:433` — `PERC_DRAWWEAPON = 24,`
- Draw-completion event hook (correct insertion site): `game/world/objects/npc.cpp:1935` `Npc::implSetFightMode(...)`.
- Existing symmetric remove-perc the fix should mirror: `game/world/objects/npc.cpp:3718`
  `owner.sendPassivePerc(*this,*this,PERC_ASSESSREMOVEWEAPON);`

## Divergence

`PERC_DRAWWEAPON` (24) is declared in OpenGothic's perception enum but is **sent from no call site**
(grep across `game/` finds only the definition). In the original, completing a weapon draw fires this
passive perception so nearby NPCs can react (the script `Perception(PERC_DRAWWEAPON,...)` handlers never
run in OpenGothic). The remove counterpart (`PERC_ASSESSREMOVEWEAPON`) *is* implemented, so the draw/sheathe
perception pair is asymmetric.

`Npc::implSetFightMode` is the faithful completion site: it is invoked from the animation event pump
(`npc.cpp:2365`) when the `weaponCh` event of the draw/sheathe transition ani fires — i.e. exactly when the
original `EV_DrawWeapon2` finalizes the mode. `setFightMode(ev.weaponCh)` returns false for INVALID, so the
existing early-return already filters non-transitions. A real draw is any `weaponCh != NONE` (NONE is the
sheathe, already handled by `closeWeapon`).

## Proposed patch

Mirror OpenGothic's own existing remove-perc convention (player-guarded — matches the definite player case
the original covers via the is-actor virtual; see NOTE for the residual non-player uncertainty).

OLD (`game/world/objects/npc.cpp`, `Npc::implSetFightMode`, after the per-mode sfx block):
```cpp
  dropTorch();
  visual.stopDlgAnim(*this);
  updateWeaponSkeleton();
  }
```

NEW:
```cpp
  dropTorch();
  visual.stopDlgAnim(*this);
  updateWeaponSkeleton();
  // NOTE: in original-game oCNpc::EV_DrawWeapon2 @0x0074d580 fires
  // CreatePassivePerception(self, 24=PERC_DRAWWEAPON, self, NULL) when a draw transition
  // completes (mirrors EV_RemoveWeapon2 @0x0074e630 / PERC_ASSESSREMOVEWEAPON, already done
  // in Npc::closeWeapon). OpenGothic defines PERC_DRAWWEAPON but never sent it.
  if(ev.weaponCh!=zenkit::MdsFightMode::NONE && isPlayer())
    owner.sendPassivePerc(*this,*this,PERC_DRAWWEAPON);
  }
```

Grep-verified symbols: `owner` (`npc.h:540`, `World&`), `isPlayer()` (`npc.cpp:548`),
`World::sendPassivePerc(Npc&,Npc&,int32_t)` (`world.cpp:706`), `PERC_DRAWWEAPON` (`constants.h:433`),
`zenkit::MdsFightMode::NONE` (already switched on at `npc.cpp:1940`).

## Residual uncertainty / NOTE

The original draw guard uses vtbl slot `+0x100` whereas the remove guard uses `+0x104` (= `IsAPlayer`).
The two slots could differ (`IsAPlayer` vs `IsHuman`, guild < 0x11), meaning the original *may* fire
`PERC_DRAWWEAPON` for any humanoid, not only the player. Raw vtable bytes were not available to
disambiguate. The patch above conservatively uses `isPlayer()` to match OpenGothic's already-shipped
remove-perc behavior; if a save-game test shows NPCs are expected to react to *each-other's* draws, widen
the guard to `isHuman()` (grep-verify the predicate first). Either way, the player case — which drives the
script-visible `PERC_DRAWWEAPON` reactions — is currently broken and fixed by this change.
