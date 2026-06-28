# Spell target validity: dead NPCs wrongly pass the typed (NPCS/HUMANS/ORCS/UNDEAD) gate

**Confidence:** Medium. The divergence is *certain* (unambiguous in the
`Gothic2.exe` decompile); the observable effect is G1-scoped because the only
G2 caller pre-filters corpses (see "Observability").

## Original function + address (prose only)

The spell target-type filter is `oCSpell::IsTargetTypeValid(zCVob*, int targetType)`
at `0x00485fc0` (static, `__cdecl`). Structure of the decompile:

- `targetType == 1` (`TARGET_TYPE_ALL`, exact equality) -> return 1 immediately,
  with **no** death test (a corpse is a valid ALL-target).
- otherwise it walks the type bitmask, and **every NPC-typed branch gates on the
  target being alive**, i.e. `oCNpc::IsDead(target) == 0`:
  - bit `0x02` `ITEMS`: target must inherit `oCItem`.
  - bit `0x04` `NPCS`: target inherits `oCNpc` **and** `IsDead()==0`.
  - bit `0x08` `ORCS`: target is `oCNpc`, `IsDead()==0`, **and** `GetTrueGuild() > 0x3a`.
  - bit `0x10` `HUMANS`: target is `oCNpc`, `IsDead()==0`, **and** `GetTrueGuild() < 0x10`.
  - bit `0x20` `UNDEAD`: target is `oCNpc`, `IsDead()==0`, **and** true-guild in the
    fixed undead set {0x14,0x15,0x1f,0x20,0x22,0x25} (the same set the existing
    `spelltgt-undead-skeleton-mage` doc verified).

So in the original a **dead** NPC fails every type-specific branch and
`IsTargetTypeValid` returns 0 for it (only the bare `TARGET_TYPE_ALL` accepts a
corpse). Reached in G2 from `oCSpell::IsValidTarget` (`0x00486417`) and
`oCSpell::CanThisCollideWith` (`0x00496b50`) (confirmed via `wde xrefs 0x00485fc0`).

## OpenGothic file:line

`game/world/objects/npc.cpp:3422-3450` — `Npc::isTargetableBySpell(TargetType)`,
the clean-room port of `oCSpell::IsTargetTypeValid`. None of the NPC-typed
branches carry the `!isDead()` conjunct:

```cpp
if(bool(t&(TARGET_TYPE_ALL|TARGET_TYPE_NPCS)))   // NPCS lacks !isDead()
  return true;
...
if(bool(t&TARGET_TYPE_HUMANS) && isHuman())       return true;   // lacks !isDead()
if(bool(t&TARGET_TYPE_ORCS) && gil>SEPERATOR_ORC) return true;   // lacks !isDead()
if(bool(t&TARGET_TYPE_UNDEAD) && g2 && G2_UNDEAD) return true;   // lacks !isDead()
if(bool(t&TARGET_TYPE_UNDEAD) && !g2 && G1_UNDEAD)return true;   // lacks !isDead()
```

## Divergence

The original drops a dead NPC from every typed spell-target branch; OpenGothic's
port omits the `IsDead()==0` conjunct, so a corpse is considered a valid
`TARGET_TYPE_NPCS / HUMANS / ORCS / UNDEAD` target. (The `TARGET_TYPE_ALL`
short-circuit is faithful — it should accept corpses, and it does.) This is a
distinct conjunct from the already-documented UNDEAD guild-set fix
(`spelltgt-undead-skeleton-mage.md`), which only narrowed *which living guilds*
count as undead and never restored the alive-test.

## Observability

- **G1 (observable):** `GameScript::canNpcCollideWithSpell` (`game/game/gamescript.cpp:1240`)
  resolves G1 spell/projectile collision directly through `isTargetableBySpell`
  with **no** death pre-filter. A G1 spell whose `target_collect_type` carries a
  typed bit (e.g. NPCS) and that overlaps a corpse currently returns
  `COLL_DOEVERYTHING` on the dead body (applies damage/effects) instead of
  `COLL_DONOTHING` (pass through) as the original alive-gate dictates.
- **G2 (masked):** the only G2 path, the player magic auto-aim, runs through
  `WorldObjects::testObj` whose `checkFlag` already rejects dead targets — the
  magic `searchPolicy` sets `NoDeath|NoUnconscious` (`game/world/world.cpp:884`)
  *before* `checkTargetType`/`isTargetableBySpell` is reached. Adding `!isDead()`
  is therefore a harmless no-op in G2 and a correctness fix in G1.

## Proposed patch

`game/world/objects/npc.cpp`, `Npc::isTargetableBySpell`.

OLD:
```cpp
bool Npc::isTargetableBySpell(TargetType t) const {
  if(bool(t&(TARGET_TYPE_ALL|TARGET_TYPE_NPCS)))
    return true;
  ...
  if(bool(t&TARGET_TYPE_HUMANS) && isHuman())
    return true;
  if(bool(t&TARGET_TYPE_ORCS) && gil>SEPERATOR_ORC)
    return true;
  if(bool(t&TARGET_TYPE_UNDEAD) && g2 && G2_UNDEAD)
    return true;
  if(bool(t&TARGET_TYPE_UNDEAD) && !g2 && G1_UNDEAD)
    return true;

  return false;
  }
```
NEW:
```cpp
bool Npc::isTargetableBySpell(TargetType t) const {
  // NOTE: in original-game oCSpell::IsTargetTypeValid @0x00485fc0 the bare
  // TARGET_TYPE_ALL (param==1) accepts any vob incl. corpses, but EVERY typed NPC
  // branch (NPCS bit4, ORCS bit8, HUMANS bit0x10, UNDEAD bit0x20) additionally
  // gates on oCNpc::IsDead()==0. OpenGothic dropped that alive-test, so a corpse
  // counted as a valid typed spell target. Masked in G2 (the magic auto-aim
  // searchPolicy already sets NoDeath before this is reached) but live in G1's
  // canNpcCollideWithSpell, which used to apply spell effects to dead bodies.
  if(bool(t&TARGET_TYPE_ALL))
    return true;
  if(bool(t&TARGET_TYPE_NPCS) && !isDead())
    return true;
  ...
  if(bool(t&TARGET_TYPE_HUMANS) && isHuman() && !isDead())
    return true;
  if(bool(t&TARGET_TYPE_ORCS) && gil>SEPERATOR_ORC && !isDead())
    return true;
  if(bool(t&TARGET_TYPE_UNDEAD) && g2 && G2_UNDEAD && !isDead())
    return true;
  if(bool(t&TARGET_TYPE_UNDEAD) && !g2 && G1_UNDEAD && !isDead())
    return true;

  return false;
  }
```
(`isDead()` is the existing const predicate at `npc.cpp:4561`. The `TARGET_TYPE_ALL`
clause is split out from `NPCS` so the corpse-accepting ALL path stays faithful while
the typed branches gain the alive-test.)
