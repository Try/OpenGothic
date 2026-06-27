# Combat-music parity: missing "passive" threat-music trigger (forest sector / near-death hero)

**Confidence:** Medium-High (the divergence itself is certain and grep-verified absent in OG;
the only residual uncertainty is whether OpenGothic's `Npc::portalName()` surfaces the same
"WALD" sector tag the original BSP lookup returns).

## Original function + address (prose only)

The music threat level (`oHEROSTATUS`: 0=STD, 1=THR, 2=FGT) that drives the `_STD/_THR/_FGT`
theme-name suffix in `oCZoneMusic::GetNewTheme` (Gothic2.exe @0x00641ba0) is produced by
`oCGame::GetHeroStatus` (@0x006c2d10), which simply forwards to the player AI's
`oCAIHuman::GetEnemyThreat` (@0x00696950). That function:

1. Collects vobs in a box around the hero, and for every armed NPC (`GetWeaponMode != 0`)
   within ~1000 units (`447a0000` = 1000.0) that is targeting the hero and is in state
   `ZS_ATTACK`/`ZS_MM_ATTACK`, returns immediately: **1 (THR)** when the hero's weapon is
   holstered (`oCNpc::GetWeaponMode(hero) == 0`), else **2 (FGT)**.
2. If no such active attacker is found, it then applies a **passive/environmental threat rule**:
   `if (status == 0 && (oCNpc::GetAttribute(hero, 0) < 6 || <hero sector name contains "WALD">))
    status = 1;` — i.e. threat (THR) music is forced when the hero is near death or standing in
   a forest sector. `GetAttribute(hero,0)` is `attribute[0]` = ATR_HITPOINTS
   (`oCNpc::GetAttribute` @0x0072ff20, returns `*(this + idx*4 + 0x1b8)`); the sector lookup is
   `zCVob::GetSectorNameVobIsIn` (@0x00600ae0) tested with a case-sensitive substring search for
   `"WALD"` (the literal lives at 0x008b0e94; `zSTRING::Search` @0x0046c920 is case-sensitive).

OpenGothic faithfully reproduces step 1 (the active-attacker path, including the already-fixed
1000-unit gate and the hero-weapon THR-vs-FGT split) but has **no equivalent of step 2**.

## OpenGothic file:line

`game/world/worldsound.cpp:265-274` (`WorldSound::tickSoundZone`):

```cpp
bool  isFgt = owner.isTargeted(player) || player.isDead();
GameMusic::Tags mode = GameMusic::Std;
if(isFgt) {
  if(player.weaponState()==WeaponState::NoWeapon) {
    mode  = GameMusic::Thr;
    } else {
    mode = GameMusic::Fgt;
    }
  }
```

`owner.isTargeted(player)` (→ `WorldObjects::isTargetedBy`, worldsound.cpp via worldobjects.cpp:444)
only ever reflects an actively-attacking armed enemy. When no enemy is attacking, `mode` stays
`Std`, so OpenGothic never plays the THR variant for a near-death hero or in forest sectors.

## Divergence

In the original, walking through a `WALD`-named (forest) sector — or dropping below 6 HP — forces
the zone's `_THR` music variant even with no enemy present. OpenGothic always plays the `_STD`
variant in those situations, a persistent, audible music mismatch in forest regions.

## Proposed patch

Grep-verified OG symbols: `Npc::attribute(Attribute)` (npc.h:216), `ATR_HITPOINTS`
(constants.h:473, reachable via npc.h), `Npc::portalName()` (npc.h:128 / npc.cpp:716,
returns the mesh sector tag the NPC stands on), `GameMusic::Thr` (already used in this file).

OLD (`game/world/worldsound.cpp`, in `tickSoundZone`):
```cpp
  GameMusic::Tags mode = GameMusic::Std;
  if(isFgt) {
    if(player.weaponState()==WeaponState::NoWeapon) {
      mode  = GameMusic::Thr;
      } else {
      mode = GameMusic::Fgt;
      }
    }
```

NEW:
```cpp
  GameMusic::Tags mode = GameMusic::Std;
  if(isFgt) {
    if(player.weaponState()==WeaponState::NoWeapon) {
      mode  = GameMusic::Thr;
      } else {
      mode = GameMusic::Fgt;
      }
    }
  // NOTE: in original-game oCAIHuman::GetEnemyThreat (Gothic2.exe @0x00696950): when no enemy is
  // actively attacking, the hero status is still forced to THREAT (1) if the hero is near death
  // (GetAttribute(0)=ATR_HITPOINTS < 6) or standing in a sector whose name contains "WALD"
  // (case-sensitive, via zCVob::GetSectorNameVobIsIn @0x00600ae0). OpenGothic only handled the
  // active-attacker path, so forest/near-death threat music never played.
  else if(player.attribute(ATR_HITPOINTS) < 6 ||
          player.portalName().find("WALD")!=std::string_view::npos) {
    mode = GameMusic::Thr;
    }
```

### Residual risk / verification note
The HP clause is a faithful 1:1 mapping. The forest clause depends on `Npc::portalName()`
(physics-mesh sector tag) yielding the same `"WALD"` token the original's BSP
`GetSectorNameVobIsIn` returns; if OpenGothic's sector tags for forest meshes are not literally
spelled `WALD` (case-sensitive), only that half of the rule would be inert — it cannot cause a
false combat-music trigger, since the clause is purely additive in the no-attacker branch.
