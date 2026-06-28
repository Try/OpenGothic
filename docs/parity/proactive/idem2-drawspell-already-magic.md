# Idempotency parity: drawing a spell while already in magic mode

**Confidence:** Medium-High

## Original function + address (prose)

`oCNpc::EV_DrawWeapon` @ `0x0074cc10` is the engine handler that processes a draw-weapon
message. Its magic-ready path (taken when the `oCMsgWeapon` flag bit `0x4` is set) reads the
NPC's current weapon mode through the field decoded by `oCNpc::GetWeaponMode` @ `0x00756cb0`
(member offset `+0x250`, clamped to `0..7`: 0 = none, 1 = fist, 3/4 = 1H/2H, 5/6 = bow/cbow,
7 = magic). The precondition guard reads, in prose: *if the weapon mode is non-zero, return 1
(success) immediately* — i.e. it does no work. Because magic is mode `7`, re-issuing a
magic/spell draw while the NPC is **already** in magic mode hits this guard and is a clean
no-op: the engine does not re-select the spell, does not rebuild the in-hand rune view, and
does not restart the draw transition.

## OpenGothic file:line

`game/world/objects/npc.cpp:4124` — `Npc::drawSpell(int32_t spell)` (reached from
`AI_DrawSpell` at `npc.cpp:2962` and from `drawMage()` at `npc.cpp:4121`, itself driven per
spell-belt key in `playercontrol.cpp:664`).

## Divergence

Unlike its sibling draw routines, `drawSpell` has **no already-in-state early return**:

- `drawWeaponFist`  (`npc.cpp:4044`): `if(weaponSt==WeaponState::Fist) return true;`
- `drawWeaponMelee` (`npc.cpp:4068`): `if(weaponSt==Fist||W1H||W2H) return true;`
- `drawWeaponBow`   (`npc.cpp:4094`): `if(weaponSt==Bow||CBow||...) return true;`
- `drawSpell`       (`npc.cpp:4124`): *(missing)*

When the NPC is already in `WeaponState::Mage`, `drawSpell` skips the `closeWeapon` branch
(because `weaponSt==Mage` is whitelisted) and proceeds to re-run the full draw body:
`setInteraction(nullptr,true)`, `visual.startAnim(*this,WeaponState::Mage)` (which itself
no-ops via `if(st==fgtMode) return true;` at `mdlvisual.cpp:777`), then
`invent.switchActiveSpell(spell,*this)` → `switchActiveWeapon` (a stat remove/re-add cycle via
`applyWeaponStats(-1)` then `(+1)`) and `updateRuneView`, followed by `updateWeaponSkeleton()`.
For a re-issue of the **same** spell that is already drawn, the original performs nothing,
whereas OpenGothic rebuilds the rune view / re-binds the weapon skeleton, which can restart the
in-hand rune effect. This is the same "original early-returns when already in state X; OG
restarts/disrupts" pattern already fixed for re-hit-on-unconscious and AI_StandUp-while-standing.

The fix is scoped to the **same-spell** case only. Switching to a *different* spell while in
magic mode (a legitimate OpenGothic behavior that lets `switchActiveSpell` change the active
rune) is intentionally left untouched, so the patch is a strict subset of the original's no-op.

## Proposed patch (OLD / NEW)

File: `game/world/objects/npc.cpp`, function `Npc::drawSpell` (line 4124).

OLD:
```cpp
bool Npc::drawSpell(int32_t spell) {
  if(mvAlgo.isFalling() || mvAlgo.isSwim() || bodyStateMasked()==BS_CASTING)
    return false;
  auto weaponSt=weaponState();
```

NEW:
```cpp
bool Npc::drawSpell(int32_t spell) {
  if(mvAlgo.isFalling() || mvAlgo.isSwim() || bodyStateMasked()==BS_CASTING)
    return false;
  // NOTE: in original-game oCNpc::EV_DrawWeapon @0x0074cc10 the magic-ready path (oCMsgWeapon
  // flag 0x4) early-returns success (return 1) when oCNpc::GetWeaponMode @0x00756cb0 (field
  // +0x250) is non-zero -- re-issuing a spell draw while already in magic mode (mode 7) is a
  // no-op. OpenGothic re-ran switchActiveSpell (weapon-stat churn + updateRuneView) and
  // updateWeaponSkeleton, restarting the in-hand rune view. No-op when the same spell is
  // already drawn; switching to a different spell stays unchanged.
  if(weaponState()==WeaponState::Mage) {
    auto act = invent.activeWeapon();
    if(act!=nullptr && act->isSpellOrRune() && act->spellId()==spell)
      return true;
    }
  auto weaponSt=weaponState();
```

All referenced OpenGothic symbols are verified present: `Inventory::activeWeapon()`
(`inventory.h:101`), `Item::isSpellOrRune()` (`item.h:70`), `Item::spellId()` (`item.h:73`),
`WeaponState::Mage`, and `weaponState()` (`npc.cpp:4146`).
