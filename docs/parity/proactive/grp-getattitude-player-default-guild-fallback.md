# Npc_GetAttitude falls back to the guild matrix toward the player instead of NEUTRAL

**Confidence:** High

## Original function + address

`Npc_GetAttitude(self, other)` external handler at `Gothic2.exe 0x006e8d00`
(registered via `DefineExternals_Ulfi`). It pops `other` (last arg) and `self`
(first arg) and, when both are non-null, returns `self->GetAttitude(other)`
(`oCNpc::GetAttitude`, `0x0072fab0`).

In prose, `oCNpc::GetAttitude(self, other)` does:

- `other == null` -> return ATT_NEUTRAL (2).
- `other` is **not** the player -> return the guild matrix value
  `oCGuilds::GetAttitude(self.guild, other.guild)`. The subject's personal
  perm/temp attitude is never consulted here.
- `other` **is** the player -> return the subject's **temp** attitude when it
  differs from perm, otherwise **perm**. It never consults the guild matrix in
  this branch.

The decisive detail is the **defaults**: the `oCNpc` constructor (`0x0072d950`)
initializes both the perm field (`+0x7e4`) and the temp field (`+0x7e8`) to `2`
(ATT_NEUTRAL), and `oCNpc::SetAttitude`/`SetTmpAttitude` clamp writes to `[0,4]`
so neither field is ever negative/"unset". Consequently, for an NPC that has
**never** had its attitude toward the player explicitly set, `Npc_GetAttitude`
returns ATT_NEUTRAL — not the NPC's guild stance. (The sibling player-gated
getters `oCNpc::IsHostile` @0x0072f870 and `oCNpc::GetPermAttitude` @0x0072fb30
share the exact same "guild only when other is not the player" structure.)

## OpenGothic file:line

`game/game/gamescript.cpp:2876-2885` — `GameScript::npc_getattitude`, which
delegates to `GameScript::personAttitude` (`gamescript.cpp:1427-1446`). The
player branch of `personAttitude` ends with `return guildAttitude(p0,p1);`
(`gamescript.cpp:1445`).

## Divergence

OpenGothic models the perm/temp fields as `ATT_NULL` (-1) when unset
(`npc.h:572-573`). In `personAttitude`'s player branch, when both
`tempAttitude()` and `attitude()` are `ATT_NULL`, control reaches
`return guildAttitude(p0,p1)` (line 1445). So `Npc_GetAttitude(npc, hero)` for an
NPC whose attitude toward the player was never set returns that NPC's **guild
matrix** stance (which can be HOSTILE/ANGRY/FRIENDLY) instead of the original's
ATT_NEUTRAL. This changes script-level branches that query
`Npc_GetAttitude(self, hero)` for un-configured NPCs/monsters.

Note that `personAttitude` is also the engine's internal hostility oracle
(`Npc::isEnemy` `npc.cpp:4414`, downed-other check `npc.cpp:571`, friendly-fire
`gamescript.cpp:1452`), where the guild fallback is **load-bearing** — it is how
OpenGothic makes guild-hostile monsters enemies of the player (the original
drives that from guild attitude in the AI/perception layer, not from
`GetAttitude`). The fix therefore must stay scoped to the script external and
must **not** remove the guild fallback from `personAttitude`.

## Proposed patch

Reimplement `npc_getattitude` to mirror `oCNpc::GetAttitude` directly rather than
routing through the combat-shared `personAttitude`. Grep-verified symbols:
`GameScript::guildAttitude` (`gamescript.h:166`), `Npc::isPlayer()`,
`Npc::attitude()` (`npc.h:235`, returns `permAttitude`),
`Npc::tempAttitude()` (`npc.h:240`, returns `tmpAttitude`),
`ATT_NULL`/`ATT_NEUTRAL` (`constants.h:247,249`).

```cpp
// OLD
int GameScript::npc_getattitude(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) {
  auto a = findNpc(aRef);
  auto b = findNpc(bRef);

  if(a!=nullptr && b!=nullptr){
    auto att=personAttitude(*a,*b);
    return att; //TODO: temp attitudes
    }
  return ATT_NEUTRAL;
  }

// NEW
int GameScript::npc_getattitude(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) {
  auto a = findNpc(aRef);
  auto b = findNpc(bRef);

  if(a!=nullptr && b!=nullptr){
    // NOTE: in original-game oCNpc::GetAttitude @0x0072fab0 (the Npc_GetAttitude handler) the guild
    // matrix is consulted ONLY when 'other' is not the player; toward the player it returns the
    // subject's temp attitude (when it differs from perm) else perm, both defaulting to ATT_NEUTRAL
    // (ctor @0x0072d950 inits the perm/temp fields to 2). It never falls back to the guild matrix
    // toward the player, so an NPC with no explicit attitude reads NEUTRAL, not its guild stance.
    // personAttitude keeps its guild fallback because it is the engine's combat hostility oracle
    // (Npc::isEnemy etc.), so the script external is reproduced here directly instead.
    if(!b->isPlayer())
      return guildAttitude(*a,*b);
    Attitude perm = a->attitude();
    if(perm==ATT_NULL)
      perm = ATT_NEUTRAL;
    Attitude temp = a->tempAttitude();
    if(temp==ATT_NULL)
      temp = ATT_NEUTRAL;
    if(temp!=perm)
      return temp;
    return perm;
    }
  return ATT_NEUTRAL;
  }
```
