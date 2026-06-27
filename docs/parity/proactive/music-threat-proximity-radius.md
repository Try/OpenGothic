# Combat/threat-music trigger ignores the original's ~1000-unit enemy-proximity gate

**Confidence:** High

## Original function + address

`oCAIHuman::GetEnemyThreat` (Gothic2.exe @ 0x00696950) is the function that decides the
hero status (oHERO_STATUS_STD / _THR / _FGT) which the music subsystem reads via
`oCGame::GetHeroStatus` (@ 0x006c2d10) and acts on in `oCZoneMusic::ProcessZoneList`
(@ 0x00640560, debug string "Change hero status to oHERO_STATUS_*").

`GetEnemyThreat` collects NPCs in a bounding box around the hero
(`zCBspBase::CollectVobsInBBox3D`) and, for each candidate, computes the enemy-to-hero
distance using a fast octagonal-norm approximation
(`0.9375*maxAxis + 0.375*(sum of the two smaller axes)`) and only treats that enemy as a
threat when the distance is **less than 1000.0 units** (constant `447a0000f = 1000.0`).
Additional gates the enemy must pass: it has its weapon drawn (`GetWeaponMode != 0`), it
targets the hero (`enemy.target == hero`), and it is in state `ZS_ATTACK` or `ZS_MM_ATTACK`.
Only then is the status raised to THR (hero unarmed) or FGT (hero armed). Enemies farther
than ~1000 units never raise the combat-music status, no matter how far they have aggroed.

## OpenGothic file:line

`game/world/worldobjects.cpp:444` (`WorldObjects::isTargetedBy`), reached from
`game/world/worldsound.cpp:265` (`bool isFgt = owner.isTargeted(player) || player.isDead();`).

`isTargeted` / `isTargetedBy` are used **only** by the music combat trigger
(`worldsound.cpp:265` is the sole caller of `World::isTargeted`), so changing the gate is
safe for other systems.

## Divergence

OpenGothic's `isTargetedBy` already replicates the weapon-drawn, target-is-hero, and
attack-state gates, but it has **no distance gate**. Any attacking enemy anywhere in the
world that targets the hero raises the status to THR/FGT. In the original only enemies
within ~1000 units count. Consequences in OG: a distant aggroed archer/mage, or an enemy
that has the hero as target while still closing in from far away, makes combat music start
early and keeps it from dropping back to the zone STD theme until that far enemy disengages.

## Proposed patch

`game/world/worldobjects.cpp`, `WorldObjects::isTargetedBy`:

OLD:
```cpp
bool WorldObjects::isTargetedBy(Npc& npc, Npc& dst) {
  if(npc.target()!=&dst)
    return false;
  if(npc.processPolicy()!=NpcProcessPolicy::AiNormal || npc.weaponState()==WeaponState::NoWeapon)
    return false;
  if(!npc.isAttack())
    return false;
  return true;
  }
```

NEW:
```cpp
bool WorldObjects::isTargetedBy(Npc& npc, Npc& dst) {
  if(npc.target()!=&dst)
    return false;
  if(npc.processPolicy()!=NpcProcessPolicy::AiNormal || npc.weaponState()==WeaponState::NoWeapon)
    return false;
  if(!npc.isAttack())
    return false;
  // NOTE: in original-game oCAIHuman::GetEnemyThreat (Gothic2.exe @0x00696950) only enemies
  // within ~1000 units of the hero raise the threat/fight music status (octagonal-norm
  // distance < 1000.0); far-off attackers do not switch the music to THR/FGT.
  const float threatRange = 1000.f;
  if((npc.position()-dst.position()).quadLength() > threatRange*threatRange)
    return false;
  return true;
  }
```

Grep-verified OG symbols: `Npc::position() const -> Tempest::Vec3` (npc.h:96),
`Npc::target()`, `Npc::weaponState()`, `Npc::isAttack()` (npc.h:287), `Npc::processPolicy()`,
and `Tempest::Vec3::quadLength()` (already used in worldsound.cpp:332,363). The original uses
an octagonal-norm approximation; Euclidean `quadLength()` against the same 1000-unit threshold
is the clean equivalent (worst-case <6% difference).
