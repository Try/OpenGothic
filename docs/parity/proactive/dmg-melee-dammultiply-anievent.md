# dmg-melee-dammultiply-anievent — melee damage ignores the DEF_DAM_MULTIPLY fight-anim multiplier

**Confidence:** Medium (binary divergence is certain; gameplay magnitude depends on whether the
shipped MDS data uses the tag, and a faithful fix is multi-file rather than surgical — so DEFERRED).

## Original fn + address (prose)

Every melee strike in the original builds its damage descriptor in
`oCAniCtrl_Human::CreateHit` (Gothic2.exe `0x006b0830`). Before sending the hit message, it sets the
descriptor's *damage multiplier* slot (`oSDamageDescriptor` field `+0x50`) from
`oCNpc::GetDamageMultiplier(attacker)` (`0x0075f2f0`), which simply returns the attacker NPC field at
`+0x988` (default `1.0f`, written in the `oCNpc` ctor `0x0072d950`). That `+0x988` field is written
*only* by `oCNpc::SetDamageMultiplier` (`0x0075f300`), whose sole caller is
`oCAniCtrl_Human::GetFightLimbs` (`0x006af1e0`, call site `0x006afb2e`) while it parses the active
fight animation's model event tags: when it encounters the `DEF_DAM_MULTIPLY` tag it `_atof`s the
argument and stores it as the NPC's current damage multiplier.

The descriptor multiplier is then consumed in `oCNpc::OnDamage_Hit` (`0x00666610`): after the
per-type weapon+attribute damage array is built it does `descriptor[+0x4c] *= descriptor[+0x50]`
(total damage *= multiplier) before protection is subtracted. So a melee attack frame carrying e.g.
`DEF_DAM_MULTIPLY 2` deals double weapon+attribute damage on that swing.

For comparison: the ranged path (`oCAIArrow::ReportCollisionToAI` `0x006a1530`) and the generic
`oCNpc::OnDamage(zCVob*,zCVob*,float,int,zVEC3*)` (`0x0067b860`) both hard-set the descriptor
multiplier to `1.0f`, which is why OpenGothic is already faithful for arrows/spells — the multiplier
is a *melee-animation-only* mechanic.

## OG file:line

- `game/game/damagecalculator.cpp:191-214` — `swordDamage` G2 branch: per-type
  `vd = max(atr + src.damage[i] - other.protection[i], 0)` with no damage-multiplier factor.
- `game/world/objects/npc.cpp:2454-2455` — `Npc::tickTimedEvt`:
  `case zenkit::MdsEventType::DAMAGE_MULTIPLIER: break;` (the tag is recognized but discarded).
- `game/graphics/mesh/animation.cpp:469` — `Animation::Sequence::processEvent` never emits a timed
  event for `DAMAGE_MULTIPLIER`, and `Animation::EvTimed` (`game/graphics/mesh/animation.h:22-27`)
  has no field to carry the multiplier value, so the float argument is dropped at load time.

## Divergence

The original multiplies the whole melee weapon+attribute damage of a swing by a per-NPC "damage
multiplier" that individual attack-animation frames set via the `DEF_DAM_MULTIPLY` model event tag
(default 1.0). OpenGothic parses the tag through ZenKit (`MdsEventType::DAMAGE_MULTIPLIER` exists and
reaches `Npc::tickTimedEvt`) but the value is thrown away in `processEvent`, the NPC stores no
multiplier, and `swordDamage` applies none. Any vanilla/mod attack that relies on `DEF_DAM_MULTIPLY`
(power/finisher strikes, special monster swings) therefore deals plain weapon+attribute damage in
OpenGothic instead of the scaled amount.

## Proposed patch — DEFERRED

A faithful fix is not surgical: it spans four concerns and one unverified data fact.

1. `Animation::EvTimed`/`processEvent` must be extended to carry the `DEF_DAM_MULTIPLY` float
   (currently the value is dropped; `EvTimed` has no numeric field, and `processEvent` emits no event
   for this tag). The value's source field on `zenkit::MdsEventTag` must be confirmed
   (`frames`/dedicated field) before wiring.
2. `Npc` must hold a `damageMultiplier` (default 1.0, equivalent to `oCNpc+0x988`), set in the
   `DAMAGE_MULTIPLIER` case of `tickTimedEvt`.
3. `DamageCalculator::swordDamage` (G2 branch) must multiply the per-type
   `atr + src.damage[i]` (pre-protection, matching `OnDamage_Hit`'s `+0x4c *= +0x50` ordering) by
   that multiplier.
4. Reset semantics are unverified: `CreateHit` reads the *current* `+0x988` each swing, so the
   value persists until the next `DEF_DAM_MULTIPLY` tag rewrites it. Replicating this without the
   original's reset cadence risks a stuck multiplier, so the reset point (per-attack vs.
   per-tag-only) needs confirmation against the MDS fight scripts.

Additionally, whether the shipped Gothic2 `Humans.mds`/monster MDS actually emit non-1.0
`DEF_DAM_MULTIPLY` values is not verifiable from the executable alone; if they never do, the
observable impact is nil. Given the multi-file surface, the reset-semantics uncertainty, and the
unconfirmed data usage, this is recorded as DEFERRED rather than patched.

`// NOTE: in original-game oCAniCtrl_Human::CreateHit @0x006b0830 sets oSDamageDescriptor+0x50 to`
`// oCNpc::GetDamageMultiplier(attacker) @0x0075f2f0 (oCNpc+0x988, default 1.0, written only by`
`// SetDamageMultiplier @0x0075f300 from the DEF_DAM_MULTIPLY fight-anim tag in GetFightLimbs`
`// @0x006af1e0); OnDamage_Hit @0x00666610 then applies total_damage *= that multiplier before`
`// protection. OpenGothic discards the tag value and applies no melee damage multiplier.`
