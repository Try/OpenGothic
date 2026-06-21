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
| Sight FOV | NPC horizontal vision cone ±80° vs original ±91° | `npc.cpp` `canRayHitPoint`/`canSeeItem` |
| Fight range | 3D distance vs original horizontal + same-height gate | `fightalgo.cpp` `qDistTo` |
| Equip | ring/amulet/belt wrongly gated on attribute requirement | `inventory.cpp` `setSlot` |

## Deferred (analyzed, not applied — genuinely need runtime or are unsafe to apply blind)
| Area | Why |
|---|---|
| Multi-type immune damage (`damage-immune-multitype`) | exact fix needs the original's bit-order `stillAllNonPositive` state; the shortcut would make immune NPCs killable on masks like `FLY\|POINT` (rare mixed-type case) |
| Regen rate vs interval (`regen-rate-reciprocal`) | original = +1 per N seconds, OG = N per second (reciprocal); likely near-dead in vanilla; orders-of-magnitude risk to flip blind — needs the attribute values + runtime |
| Periodic perc fighter/item (`aistate-perc-fighter-item-missing`) | original raises PERC_ASSESSFIGHTER/ASSESSITEM each scan; OG doesn't — needs new nearest-fighter/item helpers + raises new AI reactions (feature-add, needs runtime validation) |

