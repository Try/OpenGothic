# ASSESSMURDER witness perception is not centralized in the death routine (scattered across combat call-sites, gated on collision flags)

**Confidence:** Medium-High (root cause certain from the decompiler; the safe fix is a small 3-site relocation, so it is flagged for build + in-game verification rather than a blind apply.)

## Original function + address (prose only)

`oCNpc::DoDie` (Gothic2.exe `@0x00736760`) is the engine's single, universal death
routine — the analogue of OpenGothic's `Npc::onNoHealth(death=true)`. Near its tail,
*after* dropping in-hand items, switching the NPC to the dead AI state (`StartAIState -3`)
and re-enabling vob physics (`SetPhysicsEnabled(this,1)`), it broadcasts the murder
perception:

- `if (param_1 != 0) CreatePassivePerception(this, 6 /*PERC_ASSESSMURDER*/, param_1 /*killer*/, this /*victim*/);`

Two properties matter for parity:

1. **It is centralized in `DoDie`.** Every death funnels through `DoDie`, so the murder
   perception is raised once, for *any* cause of death, as long as a killer vob (`param_1`)
   is known. The sender of the perception is the victim (`this`), OTHER is the killer,
   VICTIM is the victim.
2. **It is gated only on `param_1 != 0`** (a known killer). It is **not** conditioned on
   how the damage was dealt — no collision-mask / damage-flag test guards it.

`oCNpc::CreatePassivePerception` (`@0x0075b270`) then performs the receiver-side filtering
(skip sender, receiver alive, has `PERC_ASSESSMURDER` registered, in range).

## OpenGothic file:line

OpenGothic has no central murder broadcast in its death handler. `Npc::onNoHealth`
(`game/world/objects/npc.cpp:584`, the `DoDie` analogue) never sends `PERC_ASSESSMURDER`.
Instead the broadcast is sprinkled across exactly two combat-specific call-sites
(repo-wide `grep PERC_ASSESSMURDER` returns only these two senders):

- `game/world/objects/npc.cpp:2172` inside `Npc::takeDamage(...)`:
  ```cpp
  if(bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) {
    owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
    if(isUnconscious()){
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSDEFEAT);
      }
    else if(isDead()) {
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSMURDER);   // <- only here for combat
      }
  ```
- `game/world/objects/npc.cpp:3933` inside `Npc::finishingMove()`:
  ```cpp
  currentTarget->checkHealth(true,false);
  owner.sendPassivePerc(*this,*this,*currentTarget,PERC_ASSESSMURDER); // <- killer is sender
  ```

## Divergence

Because OpenGothic raises the murder perception from inside the damage pipeline rather than
from the central death routine, it diverges from `DoDie` in two ways:

1. **Collision-flag gating (over-restriction).** The `npc.cpp:2172` send is nested under
   `if(bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING))`. The original `DoDie` murder
   broadcast has no such gate — it fires for any killer-caused death. A lethal hit whose
   collide-mask carries neither flag still kills the NPC (the `changeAttribute(ATR_HITPOINTS,…)`
   at `npc.cpp:2164` runs unconditionally for `hitResult.value>0`), yet OpenGothic then
   skips the murder perception, so nearby NPCs with `PERC_ASSESSMURDER` (guards, escorts)
   never run `B_AssessMurder` and never report the killing.

2. **Non-combat / scripted kills with a known aggressor.** Any death that reaches
   `onNoHealth(death=true)` without flowing through the gated `takeDamage` murder send or
   `finishingMove` (e.g. a lethal `changeAttribute`/`checkHealth` path) produces no murder
   perception, whereas the original always emits one from `DoDie` when a killer is present.

3. **Wrong sender in `finishingMove`.** `npc.cpp:3933` uses the *killer* (`*this`) as the
   perception sender (`msg.self`), so the witness scan is centered on the killer's position
   and uses the killer's identity for the "skip sender" test. `DoDie` uses the *victim*
   (`this`) as sender with the killer as OTHER. OTHER/VICTIM end up correct, but the
   detection origin differs (≈melee range — minor, listed for completeness).

Net effect: in OpenGothic the "an NPC was murdered, witnesses report it" reaction is tied to
specific combat code paths and a collision-flag predicate, instead of being a property of
*dying with a known killer* as in the original. This weakens crime-witnessing for
non-standard kills.

## Proposed patch (DEFERRED for blind apply — verify with a build + in-game first)

Centralize the broadcast in the death handler, mirroring `DoDie`, and remove the two
scattered sends so combat deaths do not double-fire. All referenced symbols are
grep-verified: `Npc::onNoHealth` (npc.h:518), `Npc::lastHit` (`Npc*`, npc.h:587, set to the
attacker in `takeDamage` at npc.cpp:2076/2094), `WorldObjects/World::sendPassivePerc`,
`PERC_ASSESSMURDER` (constants.h:415), `currentTarget`.

In `Npc::onNoHealth`, death branch (add after `startState(...)`):
```cpp
  // NOTE: in original-game oCNpc::DoDie @0x00736760 the death routine itself broadcasts
  // PERC_ASSESSMURDER (CreatePassivePerception id 6) once, for ANY cause of death, guarded
  // only by "a killer is known" (param_1!=0) — never by a collision/damage-flag test.
  // Sender is the victim, OTHER the killer, VICTIM the victim.
  if(death && lastHit!=nullptr)
    owner.sendPassivePerc(*this,*lastHit,*this,PERC_ASSESSMURDER);
```

Then remove the now-redundant scattered senders:
- `game/world/objects/npc.cpp:2172` — delete the `else if(isDead()) sendPassivePerc(...PERC_ASSESSMURDER)` arm (keep the OTHERSDAMAGE / DEFEAT arms, which mirror the separate `AssessOthersDamage`/`AssessDefeat` engine paths).
- `game/world/objects/npc.cpp:3933` — delete the `sendPassivePerc(*this,*this,*currentTarget,PERC_ASSESSMURDER)` line (the kill at 3931-3932 already routes `currentTarget` through `checkHealth -> onNoHealth`, whose `lastHit` is this killer).

**Why DEFERRED rather than blind apply:** (a) it touches three sites and changes when the
perception fires within a tick (all sends are deferred via `sndPerc` to the next tick, so
timing should be equivalent, but this needs confirmation); (b) for the rare combat death
where `lastHit` could be stale/cleared (e.g. the `lastHit=nullptr` reset at npc.cpp:2416),
the relocated send must still pick the correct killer — verify that a normal melee/spell kill
still raises the witness reaction and that it is raised exactly once. Confirm in-game: kill an
NPC near a guard via (i) melee, (ii) a finishing move, (iii) a low-collision-flag/scripted
kill, and check the guard runs `B_AssessMurder` in each case without duplicate reactions.
