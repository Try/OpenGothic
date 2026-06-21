# Proactive parity findings

Parity divergences found by **diffing the original `Gothic2.exe` against OpenGothic
method-by-method** (via the Ghidra decompile-and-compare loop), independent of the
GitHub issue tracker. Each doc has the original fn+address, the OG file:line, the
concrete divergence, and a proposed/applied patch with a `// NOTE: in original-game …`
citation.

## Applied (in this branch)
| Area | Bug | OG location |
|---|---|---|
| Fall damage | missing +50cm fall-distance tolerance → under-counted falls | `damagecalculator.cpp` `damageFall` |
| Spell damage | NPC_MINIMAL_DAMAGE (5) floor wrongly applied to spells | `damagecalculator.cpp` |
| Immortality | `-999` sentinel must kill immortal NPCs (scripted deaths) | `npc.cpp` `changeAttribute` |
| Fight distance | target root-bone projected through attacker's transform | `npc.cpp` `fightDistanceTo` |
| Item economy | value not scaled by condition (`hp/hp_max`) | `item.cpp` `cost` |
| Attitude | temp attitude ignored vs perm (enemy/fight/friendly-fire) | `gamescript.cpp` `personAttitude` |
| Mobsi use | use-distance 165 vs original 150 | `interactive.cpp` `attach` |
| Magic | leveled spell scripts always invoked at level 0 | `gamescript.cpp` `invokeSpell` |

## Deferred (analyzed, not applied)
| Area | Why deferred |
|---|---|
| Sight FOV cone (`percept-fov-angle`) | OG ~±80° vs original ~±91°; broad stealth impact + subtle angle math — needs runtime validation |
| Fight range 2D vs 3D (`fight-range-2d-vs-3d`) | original uses horizontal range + separate height gate; OG folds Y into Euclidean — broad shared-helper change |
| Multi-type immune damage (`damage-immune-multitype`) | original's invincibility decision is bit-order-dependent; not cleanly surgical |

All applied fixes are build- and boot-verified, **not** gameplay-verified (no playtest from this headless setup).
