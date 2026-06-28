# Brawl re-hit on an already-unconscious NPC restarts the knockout instead of being a no-op

**Confidence:** High

## Original function + address

The damage pipeline reaches the knockout via:
`oCNpc::OnDamage` @`0x006660e0` → `oCNpc::OnDamage_Condition` @`0x0066cf30` (sets the
"unconsciousness" descriptor bit, `mask & 8`) → `oCNpc::OnDamage_Events` @`0x0067abe0`,
which, for the unconsciousness bit, calls `oCNpc::DropUnconscious` @`0x00735eb0`.

`oCNpc::DropUnconscious` begins with a guard: it queries
`oCNpc_States::IsInState(this+0x588, -4)` (AI state id `-4` = the unconscious state, the
same thing `oCNpc::IsUnconscious` @`0x00736750` tests) and, if the victim is **already**
in it, returns immediately. Concretely it does nothing on re-entry: it does not restart
the unconscious AI state (`StartAIState(-4,...)`), does not replay the
`T_STAND_2_WOUNDEDB` drop animation, does not reset HP, and does not re-run the
weapon/torch drop or `SetOther`. Only the *first* drop runs that body of work and sets
`attribute[0]` (HITPOINTS, `this+0x1b8`) to `1`.

Net original behaviour: a non-lethal blow (fists / `C_DropUnconscious==true`) on an
NPC that is already knocked out is effectively a **no-op** — the body lies undisturbed.
A *lethal* re-hit (weapon, `C_DropUnconscious==false`, victim drops below 1 HP) instead
sets the death bit (`mask & 4`, `OnDamage_Events` → `OnDeath` vtbl+0xa4) and kills, since
`DropUnconscious`'s early-return only protects against re-dropping, not against dying.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2213-2222`
(`Npc::takeDamage`, the `if(isDown())` re-hit branch)

## Divergence

OpenGothic's re-hit branch unconditionally calls
`onNoHealth(isDead() || !dontKill, HS_NoSound)`. A prior fix corrected the *polarity*
(lethal-vs-not), but the non-lethal case is still wrong: when the victim is already
unconscious (`isDown()` true, `isDead()` false) and the blow is non-lethal
(`dontKill==true`, i.e. fists), the expression evaluates to `onNoHealth(false, ...)`.
`Npc::onNoHealth(false,...)` (npc.cpp:598) re-runs the full knockout: `startState(
ZS_Unconscious)`, `setAnim(Unconscious A/B)`, `clearAiQueue()`, `setOther(lastHit)`,
re-drops weapon/torch, resets `ATR_HITPOINTS=1`, and re-disables the physics capsule.

So in OpenGothic, every fist graze on a downed body **re-triggers the knockout and
restarts the `ZS_Unconscious` state each frame it is hit** — replaying the drop
animation and resetting the get-up/state timer, so the NPC can be kept down indefinitely
by punching, and `other`/`lastHit` credit is rewritten. In the original, `DropUnconscious`
early-returns on `IsInState(-4)` and the body lies undisturbed. The branch's own NOTE
already asserts "a non-lethal blow (fists) leaves an unconscious victim down
(DropUnconscious early-returns while IsInState(-4))" — but the code does not implement
that early-return; it calls the disruptive `onNoHealth(false)` instead.

The lethal cases stay correct and are untouched: a weapon re-hit (`dontKill==false`) or a
hit on a corpse (`isDead()` true) still evaluates the death param to `true` and proceeds
to `onNoHealth(true,...)`.

## Proposed patch

```cpp
// OLD (npc.cpp:2213-2222)
  if(isDown()) {
    // NOTE: in original-game oCNpc::OnDamage_Condition @0x0066cf30 a re-hit on a downed NPC goes to
    // ZS_Dead only when the victim is already dead OR the blow is lethal (C_DropUnconscious==false);
    // a non-lethal blow (fists) leaves an unconscious victim down (oCNpc::DropUnconscious @0x00735eb0
    // early-returns while IsInState(-4)). 'dontKill' is OpenGothic's non-lethal / allow-unconscious
    // flag (same role as the changeAttribute arg below), so the raw 'death=dontKill' was inverted: it
    // killed knocked-out NPCs with fists and revived corpses hit by a stray arrow/spell.
    onNoHealth(isDead() || !dontKill,HS_NoSound);
    return;
    }
```

```cpp
// NEW
  if(isDown()) {
    // NOTE: in original-game oCNpc::OnDamage_Condition @0x0066cf30 a re-hit on a downed NPC goes to
    // ZS_Dead only when the victim is already dead OR the blow is lethal (C_DropUnconscious==false);
    // a non-lethal blow (fists) leaves an unconscious victim down (oCNpc::DropUnconscious @0x00735eb0
    // early-returns while IsInState(-4)). 'dontKill' is OpenGothic's non-lethal / allow-unconscious
    // flag (same role as the changeAttribute arg below), so the raw 'death=dontKill' was inverted: it
    // killed knocked-out NPCs with fists and revived corpses hit by a stray arrow/spell.
    // NOTE: in original-game oCNpc::DropUnconscious @0x00735eb0 the IsInState(-4) early-return makes a
    // non-lethal re-hit on an already-unconscious victim a true no-op: it does NOT restart
    // ZS_Unconscious, replay the drop animation, reset HP, or reassign 'other'. A lethal re-hit
    // (weapon, or victim already dead) still falls through to the kill. Reproduce that early-return so
    // punching a knocked-out body no longer perpetually re-triggers the knockout / resets its get-up
    // timer.
    if(isUnconscious() && dontKill) // -> non-lethal blow, isDead()==false implied for unconscious
      return;
    onNoHealth(isDead() || !dontKill,HS_NoSound);
    return;
    }
```

Symbols verified to exist in OpenGothic: `Npc::isDown` (npc.cpp:4501),
`Npc::isUnconscious` (npc.cpp:4496), `Npc::isDead`, `Npc::onNoHealth` (npc.cpp:598),
and `dontKill` (local, npc.cpp:2183). `isUnconscious()` excludes the dead case, so the
guard cannot intercept a corpse, and `dontKill==true` guarantees the blow is non-lethal —
exactly the `DropUnconscious` early-return condition.
