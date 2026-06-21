# Proactive parity findings

Parity divergences found by **diffing the original `Gothic2.exe` against OpenGothic
method-by-method** (Ghidra decompile-and-compare), independent of the GitHub tracker.
Each doc has the original fn+address, the OG file:line, the concrete divergence, and a
patch with a `// NOTE: in original-game …` citation.

## Applied (this branch)
| Area | Bug | OG location |
|---|---|---|
| Fall damage | missing +50cm fall-distance tolerance | `damagecalculator.cpp` `damageFall` |
| Spell damage | NPC_MINIMAL_DAMAGE (5) floor wrongly applied to spells | `damagecalculator.cpp` |
| Immortality | `-999` sentinel must kill immortal NPCs | `npc.cpp` `changeAttribute` |
| Fight distance | target root-bone projected via attacker's transform | `npc.cpp` `fightDistanceTo` |
| Item economy | value not scaled by condition (`hp/hp_max`) | `item.cpp` `cost` |
| Attitude | temp attitude ignored vs perm | `gamescript.cpp` `personAttitude` |
| Mobsi use | use-distance 165 vs original 150 | `interactive.cpp` `attach` |
| Magic | leveled spell scripts always invoked at level 0 | `gamescript.cpp` `invokeSpell` |
| Perception | default scan interval ~1ms vs original 5000ms | `npc.h` `perceptionTime` |
| Music | day/night theme threshold 04:00-21:00 vs 06:30-18:30 | `worldsound.cpp` |
| Music zones | overlapping zones picked by load order, not priority | `worldsound.cpp` `tickSoundZone` |
| Equip | ring/amulet/belt wrongly gated on attribute requirement | `inventory.cpp` `setSlot` |

## Deferred (analyzed, not applied — need runtime or larger work)
| Area | Why |
|---|---|
| Sight FOV cone (`percept-fov-angle`) | ~±80° vs ~±91°; broad stealth impact, subtle angle math |
| Fight range 2D vs 3D (`fight-range-2d-vs-3d`) | original uses horizontal range + height gate; shared-helper change |
| Multi-type immune damage (`damage-immune-multitype`) | original's invincibility decision is bit-order-dependent |
| Periodic perc fighter/item (`aistate-perc-fighter-item-missing`) | PERC_ASSESSFIGHTER/ITEM not raised; needs new helpers |

## Swept clean (no divergence found — verified faithful)
- Dialog/info pipeline, melee hit/parry/combo/stagger, steal/swim/sleep/regen.
- **Triggers** (`zCTrigger`): activation-count, retrigger-cooldown and fire-delay match
  the original — non-mover `maxActivationCount = uint32_t(max_activation_count)` reproduces
  the original's "0 = never, N = N times, -1 = infinite" (negative count never decrements
  past 0). Only minor edge-case: OG updates the retrigger timestamp on enable/disable too.
- **Projectiles**: Bullet-engine physics (realistic gravity, 30 m/s); the original
  `oCAIArrow` is a different physics model — architectural, no clean constant divergence.

## Not yet swept (resume next)
XP/leveling/learn (mostly script-side), food/potion effect application, torch/fire.

All applied fixes are build- and boot-verified, **not** gameplay-verified (no playtest from this headless setup).
