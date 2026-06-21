# Wld_DetectNpcEx: missing magic-frozen / spell-effect exclusion filter

> DEFER: non-surgical — requires mirroring the original's frozen/petrified/spell-effect predicate from the magic subsystem (FindNpcEx filterMagic). Needs the exact spell-state predicate before applying.

**Confidence:** Medium

## Original function + address
`oCNpc::FindNpcEx` (Gothic2.exe `0x00740b80`), reached from the `Wld_DetectNpcEx`
external thunk (`0x006e15c0`). The thunk pops the four declared parameters
(`npcInstance, aiState, guild, detectPlayer`) and calls
`FindNpcEx(self, instance, guild, aiState, nearest=1, excludePlayer=(detectPlayer==0), filterMagic=1)`.

The seventh argument (`filterMagic`) is hard-wired to 1 for `Wld_DetectNpcEx`. With it
set, after the instance/guild/aiState/player checks pass, the original additionally
*rejects* a candidate NPC that is currently under any of a fixed set of magic effects:
it walks the NPC's active-spell list looking for several specific spell IDs and, when the
NPC has active spell instances, also rejects NPCs in the petrified / frozen aistate
(state codes -4 and -5). Only NPCs surviving that filter are eligible to be returned as
the nearest detected NPC. (The plain `Wld_DetectNpc` / `oCNpc::FindNpc` path does NOT
apply this filter — it is specific to the Ex variant.)

Effect: an NPC that is frozen, petrified, or otherwise under one of those magic effects
is invisible to `Wld_DetectNpcEx` in the original engine.

## OpenGothic location
`game/game/gamescript.cpp:1813-1825` (`wld_detectnpcex`): the lambda filters only on
instance, aiState, guild, self-exclusion, `!isDead()` and the player flag. There is no
equivalent magic-effect / frozen-state exclusion, so a frozen or petrified NPC is still
returned.

## Divergence
Scripts that call `Wld_DetectNpcEx` (commonly AI guild/scan logic and some spell/quest
checks) will, in OpenGothic, detect NPCs that the original engine deliberately hides
while they are under the relevant magic effects. This changes which NPC `other` is bound
to after the call, and the boolean result, whenever a frozen/petrified candidate is the
nearest match.

## Proposed patch
Conservative: gate on the same active-magic / frozen-aistate condition the original
checks. Exact spell-ID set and frozen-state predicate must be mirrored from the magic
subsystem; sketch only.
```cpp
// game/game/gamescript.cpp  (wld_detectnpcex lambda, ~line 1814)
// OLD
    if((inst ==-1 || int32_t(n.instanceSymbol())==inst) &&
       (state==-1 || n.isInState(uint32_t(state))) &&
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead() &&
       (player!=0 || !n.isPlayer())) {

// NEW
    // NOTE: in original-game oCNpc::FindNpcEx (filterMagic arg = 1 for Wld_DetectNpcEx)
    // additionally excludes NPCs under specific magic effects (frozen/petrified/etc.).
    if((inst ==-1 || int32_t(n.instanceSymbol())==inst) &&
       (state==-1 || n.isInState(uint32_t(state))) &&
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead() &&
       (player!=0 || !n.isPlayer()) &&
       !n.isFrozenByMagic() /* mirror original spell-ID + frozen-aistate check */) {
```
