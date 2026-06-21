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
| NPC mobsi gate | conditionFunc/useWithItem evaluated for player only, not all NPCs | `interactive.cpp` `checkUseConditions` |
| Routine fallback | gap-fallback picked most-recently-ended, not largest-start | `npc.cpp` `currentRoutine` |
| Trade sell price | std::ceil instead of round-to-nearest (overpays player) | `item.cpp` `sellCost` |
| TouchDamage | repeat_delay==0 damaged every frame instead of once per entry | `touchdamage.cpp` `tick` |
| Mover NEXT/PREV | SINGLE_KEYS didn't wrap (stuck at ends) | `movetrigger.cpp` `onGotoMsg` |
| Mover TRIGGER_CTRL | closed on first untrigger, ignoring trigger ref-count | `movetrigger.cpp` |
| Equip | ring/amulet/belt wrongly gated on attribute requirement | `inventory.cpp` `setSlot` |
| Item effects | equipped change_atr/change_value attribute bonuses never applied | `inventory.cpp` `applyArmor` |
| Item use | consumable (food/potion) skipped the cond_atr requirement gate | `inventory.cpp` `use` |
| TouchDamage | DAM_BARRIER didn't instant-kill a swimming victim (deep-water drown) | `touchdamage.cpp` `tick` |
| Anim SFX | last-frame SFX re-fired every tick on non-looping anims | `animation.cpp` `processSfx` |
| Magic level | `Spell_Cast_<tag>` got a 0-based level vs original 1-based | `npc.cpp` `commitSpell` |
| Ranged falloff | G2 hit-chance cut off at 45m vs original 100m decay | `damagecalculator.cpp` `rangeDamage` |
| Trade gold | billed/paid for requested count, not units actually transferred | `inventory.cpp` `transfer` |
| GetHeightToNpc | returned 0 (not INT_MAX) for an invalid NPC, unlike GetDistToNpc & the original | `gamescript.cpp` `npc_getheighttonpc` |
| Immortal heal | immortal guard only checked val<0, so heals/regen raised immortal NPC HP | `npc.cpp` `changeAttribute` |
| Climb-up ledge | JUMPUPLOW band lacked the step_height floor (knee-high ledge played climb anim) | `npc.cpp` `tryJump` |
| Pick-lock progress | combination index was per-player & reset on detach, not per-mob & persistent | `interactive.h` / `playercontrol.cpp` |
| Daytime sound | zCVobSoundDaytime window crossing midnight never played the primary sound | `worldsound.cpp` `tick` |
| Dialog order | equal-`nr` choices reordered by a non-stable scriptFn tie-break vs declaration order | `gamescript.cpp` `sort` |
| Spell FX node | TARGET-mode FX attached to emTrjOriginNode instead of emTrjTargetNode | `effect.cpp` `syncAttachesSingle` |
| Wld_IsTime | zero-width window [t,t] returned false all day vs true at exactly minute t | `gamescript.cpp` `wld_istime` |
| Talent "%" | stats menu used hitchance for all G2 talents; only 4 combat skills should | `gamemenu.cpp` |
| PERC_DRAWWEAPON | declared but never sent; guards never reacted to player drawing a weapon | `npc.cpp` `implSetFightMode` |
| Mob reservation | NPC walking to a bench didn't reserve it, so a 2nd NPC raced for the same mob | `interactive.cpp` / `worldobjects.cpp` |
| RemoveInvItem(s) | always returned 0; should return 1 when the item was present (script gates) | `gamescript.cpp` `npc_removeinvitem(s)` |
| GetAttitude(hero,npc) | returned the NPC's temp/perm; original returns guild value when subject is player | `gamescript.cpp` `personAttitude` |
| Magic camera | spell-casting used the ranged (bow) camera instead of the magic camera | `mainwindow.cpp` `solveCameraMode` |
| Footstep in water | swim/dive emitted ground (`_WATER`) footstep sounds; original suppresses them | `animation.cpp` `processSfx` |
| Guild attitude | table default was HOSTILE; original fills with FRIENDLY (uncovered guilds 64/65) | `gamescript.cpp` guild table init |
| SVM overlay | global barrier dropped other NPCs' concurrent overlay voices; original is per-NPC | `gamescript.cpp` `aiOutputSvm` |
| Timed overlay | Mdl_ApplyOverlayMdsTimed skipped overlays with ticks<=0; original applies one frame | `gamescript.cpp` `mdl_applyoverlaymdstimed` |
| Item drop yaw | dropped item kept the hand-bone rotation; original resets to world-aligned | `npc.cpp` `dropItem` |
| Mover easing | keyframe easing used polynomials vs original sinusoidal (mid-travel ~9% off) | `movetrigger.cpp` `calcProgress` |
| Combo lockout | a mistimed combo press permanently broke the chain; original ignores it | `pose.cpp` `continueCombo` |
| Log_AddEntry | appended duplicate journal lines; original suppresses byte-identical entries | `questlog.cpp` `addEntry` |
| Sub-choice removal | erased all choices sharing a handler fn; original removes one by text | `gamescript.cpp` `exec` |
| Godmode scope | only shielded HP; original blocks every negative attribute for godmode player | `npc.cpp` `changeAttribute` |
| Fire defaults | empty vobtree/slot → no flames on default-templated fires; original defaults them | `fireplace.cpp` |

## Deferred (analyzed, not applied — need runtime validation or are non-surgical/unsafe)
| Finding | Why deferred |
|---|---|
| `regen-rate-reciprocal` | original = +1 per N sec, OG = N per sec (reciprocal); likely near-dead in vanilla; orders-of-magnitude risk to flip blind |
| `damage-immune-multitype` | exact fix needs bit-order state; shortcut would make immune NPCs killable on rare mixed masks |
| `aistate-perc-fighter-item-missing` | PERC_ASSESSFIGHTER/ITEM not raised — adds new AI reactions; needs runtime |
| `ext-getdisttowp-metric`, `waynet-nearest-metric`, `waynet-edge-cost-metric` | Euclidean→Manhattan/octagonal distance; exact metric uncertain + broad script/routing impact |
| `camera-azimuth-clamp` | exact set of wide-azimuth camera modes unconfirmed; player-facing camera risk |
| `death-unconscious-perc-wipe` | conflicts with an OG "William in Jarkendar" workaround; needs runtime |
| `hit-stumble-weapon-drawn` | hinges on monster weaponState (troll/waran regression risk); needs runtime |
| `itemplace-waypoint-direction` | visual-only item yaw; touches all item rendering; needs a visual check |
| `detect-npcex-magic-filter` | non-surgical: needs the frozen/spell-state predicate mirrored |
| `bow-multi-munition` | UNSAFE as written: ITM_MULTI flag conflation would make all arrows infinite |
| `focus-elevation-cone` | adds a vertical elevdo/elevup focus gate; the elevation reference frame (player at-vector vs atan2-from-position) is approximated, and a wrong frame could reject legitimate item/lever focus — needs in-game validation |
| `anim-wounded-overlay-lowhp` | HP-driven `_WOUNDED` locomotion overlay (CheckModelOverlays @0x007301d0); visual-only, triggers only at HP≤2, needs exact MDS overlay-name construction + OG overlay API + on-screen check |
| `theft-assesstheft-isplayer-gate` | dropping the `isPlayer()` guard so NPC item-pickups broadcast PERC_ASSESSTHEFT adds new NPC-vs-NPC witness reactions; AI-behavior risk, needs runtime |
| `aistate-startstate-unconditional-queue-clear` | gating AI-queue clear to the hard-interrupt path; OG models hard/soft state switches differently and existing workarounds assume an always-cleared queue — needs in-game verification (agent-deferred) |
| `lock-picklock-progress` (save/load part) | persisting the in-progress combination index across save/load needs a serialization-version bump (the per-mob relocation itself is applied) |
| `monster-getnexttarget-sticky` | Npc_GetNextTarget should keep the current enemy if still valid (sticky) rather than flip to nearest every call; the original's validity check also re-acquires on flee-state (-4/-5) and charm/sleep/freeze, which have no grep-verified 1:1 OG equivalent — an isDown()-only sticky path would wrongly lock onto a controlled/fleeing foe. Combat AI, needs runtime |
| `perc-passive-range-senses-fallback` | passive perception uses percRange.at(perc, senses_range) fallback vs original's static percRange[] table; removing the fallback depends on the unconfirmable static default + whether scripts always Perc_SetRange — agent-deferred |
| `hitreact-ondamage-processinfos-dialog-guard` | original skips hit-reaction/damage while an EV_PROCESSINFOS dialog transition is queued; OG models dialog via AiQueue/outputPipe with no grep-verifiable per-Npc oCMsgConversation equivalent to gate on — agent-deferred |
| `death-corpse-physics-disabled` | original keeps the dead-NPC collision body enabled (corpses block projectiles/movement); OG disables physic on death, but OG's capsule may not track the prone pose so re-enabling risks an upright invisible wall — needs in-game check |
| `ext-getinvitembyslot-category-flat-index` | original Npc_GetInvItemBySlot ignores the category arg and uses a flat 0-based slot index; OG filters by category with a 1-based index. Upstream quirk with no stock-G2 callers; flat-index rewrite needs caller analysis — agent-deferred |
| `aimove-gotofp-arrival-heading` | AI_GotoFP should re-orient the NPC to the freepoint's facing on arrival; Medium confidence, movement-behavior change, the original couples turn into EV_GotoFP while OG splits goto/align (AI_AlignToFP can mask it), and the proposed implTurnTo(float,float,…) overload is unverified — needs runtime |
| `rune-deplete-reswitch-bookwide` | after a scroll's last charge, the original re-selects another known spell book-wide (stays in mage mode); OG only searches the 8 hotbar slots. Storage models aren't 1:1 and there's no grep-verified book order to mirror — agent-deferred |
| `ta-equal-window-routine-active-all-day` | the routine-side counterpart to the Wld_IsTime zero-width fix: a start==stop TA entry should be active ~all day, but matching it inside currentRoutine's per-entry loop risks a start==0 Rtn_Start shadowing all real routine windows (NPCs frozen at spawn) depending on iteration/first-vs-last-match order — needs routine-order analysis + runtime |
| `face-morph-frame-rate` | morph-ani frame rate should always be 1/speed (not duration/frame_count for non-loop anis); a clear formula fix but it changes ALL morph animations (faces/effects/creatures) visually with no runtime check, and its speed==0 edge (clamp-to-1) diverges from the original's frozen frame — needs on-screen validation + speed==0 handling |

