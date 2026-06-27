# Brawl: hostile-fist knockout decided by attitude instead of attacker weapon-mode

**Confidence:** Medium (divergence is solid; the fix is NOT high-confidence/surgical — see DEFERRED).

## Original function + address (prose only)

`oCNpc::OnDamage_Condition` @ `0x0066cf30` is where the original engine decides, on a
registered hit, whether the victim should drop unconscious (result-flag bit `0x8`) or die
(result-flag bit `0x4`). Read in prose, its unconscious branch fires when **all** of these
hold: the victim's HP is exactly 1 or the victim already reads as (masked) dead; an attacker
NPC is present; `oCAniCtrl_Human::IsInWater(...)==0`; and the non-lethal predicate
`C_DropUnconscious` is true (the engine resolves it from the Daedalus symbol if present, else
from a per-NPC default virtual). The death branch then fires only when the unconscious bit was
*not* set and `oCNpc::IsDead()` is true. Notably, this function contains **no
`oCNpc::GetAttitude` / attitude lookup at all** — the lethal-vs-knockout split is driven by the
attacker's fight mode / `C_DropUnconscious`, never by whether the two NPCs are hostile.
Re-hits on an already-downed victim are absorbed separately: `oCNpc::DropUnconscious`
@ `0x00735eb0` early-returns while the victim `IsInState(-4)`.

## OpenGothic file:line

- `game/world/objects/npc.cpp:558` — `Npc::checkHealth(bool onChange, bool allowUnconscious)`
  (the standing-victim knockout/death decision)
- `game/world/objects/npc.cpp:2135` — `dontKill` (the non-lethal flag fed in as
  `allowUnconscious`)
- `game/world/objects/npc.cpp:2172` — `Npc::takeDamage` downed-victim re-hit path

## Divergence

OpenGothic carries the engine's non-lethal flag as `dontKill`
(`(b==nullptr && splId==0) || (bMask & COLL_DONTKILL)) && !isSwim() && !isDive()`), i.e. "this
blow is melee/fist or explicitly flagged don't-kill, and not in water." Two code paths then
consume it **inconsistently**:

1. Downed-victim re-hit (`npc.cpp:2172`) uses `dontKill` directly:
   `onNoHealth(isDead() || !dontKill, …)`. A fist blow (`dontKill==true`) on a knocked-out NPC
   keeps them unconscious — fists never kill here. This matches the original and the in-file
   NOTE.

2. Standing-victim knockout (`changeAttribute` → `checkHealth`, `npc.cpp:568-569`) *discards*
   the non-lethal flag and re-decides on **attitude**:
   `currentOther==nullptr || !allowUnconscious || !isHuman() || personAttitude(*this,*currentOther)==ATT_HOSTILE`
   → `onNoHealth(true)` (death). So a *standing* human beaten to 0 HP by fists **dies** when the
   victim/attacker attitude resolves to `ATT_HOSTILE`, even though the very same victim, once
   down, can no longer be killed by fists.

The original's `OnDamage_Condition` never consults attitude; its knockout is gated on the
attacker's fight-mode/`C_DropUnconscious`. The observable consequence: knocking a genuinely
hostile *human* enemy to 0 HP with bare fists (a real Gothic mechanic — KO-and-rob without a
murder) yields a corpse in OpenGothic instead of an unconscious body.

## Why attitude is used (and why this is hard)

The attitude gate is a deliberate proxy, not an oversight: OpenGothic models monster melee as
`WeaponState::Fist` too (`npc.cpp:3866-3868`), so `dontKill` is `true` for a wolf/monster claw
just as for a human punch. If `checkHealth` dropped unconscious whenever `allowUnconscious` held,
**monsters could never kill the player**. Routing the decision through
`personAttitude(...)==ATT_HOSTILE` makes monster/enemy attacks lethal while letting friendly/
neutral sparring partners be knocked out — correct for the common brawl quests (arena/duel NPCs
stay non-hostile during the fight). The cost is only the hostile-human-fist edge case above.

## Proposed patch

**DEFERRED.** A more faithful discriminator than attitude would be the *attacker's* identity and
fight mode rather than the relationship, e.g. drop unconscious when
`other.isHuman() && other.weaponState()==WeaponState::Fist` (grep-verified: `Npc::isHuman`
@`npc.cpp:1323`, `Npc::weaponState` @`npc.cpp:3961`, `WeaponState::Fist` used throughout),
which would preserve monster lethality (`other.isHuman()` filters out claws) and sword lethality
(non-fist) while making human fist blows non-lethal regardless of momentary hostility. This is
deferred rather than applied because:

- It is not surgical: `checkHealth` is reached from many callers (script `Npc_ChangeAttribute`,
  fall/drown damage, regen) where `currentOther`/`other` is stale or absent, so the new predicate
  cannot simply replace the attitude clause without re-auditing every caller's
  `currentOther`/weapon-mode state.
- Regression surface is real: the existing attitude gate is a working approximation that the
  test corpus implicitly depends on (monster lethality, hostile-NPC kills). Flipping it risks
  silently changing far more hits than the targeted edge case.
- The original's true lever is `C_DropUnconscious` (script-overridable) plus the descriptor's
  fist-mode bits, which OpenGothic does not model; matching it exactly would mean wiring the
  attacker fight-mode bit through `oSDamageDescriptor`-equivalent state, a larger change than a
  parity micro-fix.

NOTE for any future fix: in original-game `oCNpc::OnDamage_Condition` @0x0066cf30 the
knockout-vs-death split is gated on the attacker's fist fight-mode / `C_DropUnconscious`, never
on `oCNpc::GetAttitude`; OpenGothic's `Npc::checkHealth` substitutes a `personAttitude(...)
==ATT_HOSTILE` test, which over-kills hostile bare-fist victims that the original would knock
unconscious.
