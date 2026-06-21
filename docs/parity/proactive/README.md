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
| Equip | ring/amulet/belt wrongly gated on attribute requirement | `inventory.cpp` `setSlot` |

## Deferred (analyzed, not applied — need runtime or larger work)
| Area | Why |
|---|---|
| Sight FOV cone (`percept-fov-angle`) | ~±80° vs ~±91°; broad stealth impact, subtle angle math |
| Fight range 2D vs 3D (`fight-range-2d-vs-3d`) | original uses horizontal range + height gate; shared-helper change |
| Multi-type immune damage (`damage-immune-multitype`) | original's invincibility decision is bit-order-dependent |
| Periodic perc fighter/item (`aistate-perc-fighter-item-missing`) | PERC_ASSESSFIGHTER/ITEM not raised; needs new helpers |
| Music-zone priority (`sound-zone-priority`) | OG ignores zone priority; priority-direction needs runtime confirm |

## Swept clean (no divergence found — verified faithful)
Dialog/info pipeline, melee hit/parry/combo/stagger, steal/swim/sleep/regen.

## Not yet swept (cut off by a session usage limit — resume next)
Triggers (`zCTrigger`/`zCMover`/`zCCodeMaster` fireDelay/counts), XP/leveling/learn,
projectile/bullet flight.

All applied fixes are build- and boot-verified, **not** gameplay-verified (no playtest from this headless setup).
