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
| TouchDamage split | multi-type zone applied full damage per type; original splits scalar across types | `touchdamage.cpp` `tick` |
| Inventory sort tie | ties broke by instance index; original ties alphabetically by display name | `inventory.cpp` `less` |
| Ambient delay | random-sound reschedule was one-sided; original is symmetric delay±delayVar | `worldsound.cpp` `tick` |
| Spell out-of-mana | G2 held invest-cast didn't auto-release at zero mana (only G1 did); original does both | `playercontrol.cpp` |
| POINT melee bonus | POINT damage added STRENGTH; original adds DEXTERITY to the POINT type | `damagecalculator.cpp` `swordDamage` |
| Amulet/belt equip | equipping a 2nd amulet/belt silently swapped; original refuses when occupied | `inventory.cpp` `use` |
| Untrigger react | untrigger/untouch ignored the react flags; original drops when both are off | `abstracttrigger.cpp` |
| Key-locked chest | key-locked container opened without the key; original gates on CanOpen | `interactive.h` / `playercontrol.cpp` |
| InsertNpcAndRespawn | external was unbound (NPC never spawned + VM stack corruption); now spawns + records delay | `gamescript.cpp` |
| Trade multiplier | missing TRADE_VALUE_MULTIPLIER defaulted to 1.0 (full value); original 0.3 + clamp ≤0 | `gamescript.cpp` |
| Storm-prehit AI | enemy_stormprehit nested under isPrehit() was dead; original is an independent band | `fightalgo.cpp` `fillQueue` |
| Spell-FX invest | high invest level snapped the on-weapon FX to base; original holds the last invest key | `visualfx.cpp` `key` |
| Transform exp/LP | transform restored only level; original keeps exp/exp_next/lp/level invariant | `npc.cpp` transform path |
| Focus HP bar | bar gated on isDead() (ZS_DEAD) vs the original's current-hitpoints>0 gate | `mainwindow.cpp` |
| Stack-merge owner | stack-merge overwrote owner/owner_guild; original keeps the existing stack's owner | `inventory.cpp` `addItem` |
| NPC item-use gate | consumable cond_atr gate blocked NPCs too; original aborts only for the player | `inventory.cpp` `use` |
| EquipBest tie | equal damage/protection ties broke by value; original ties by display name | `inventory.cpp` `bestItem` |
| isMonster golems | Fire/Ice Golem + Dragon counted as monsters (auto-crit, minHp=0); original excludes them | `npc.cpp` `isMonster` |
| IsDrawingWeapon | was an inverted copy of IsDrawingSpell (returned spells, not weapons) | `gamescript.cpp` `npc_isdrawingweapon` |
| ASSESSBODY KO | body perception missed unconscious NPCs (dead-only); original is dead-or-unconscious | `npc.cpp` `updateNearestBody` |
| SVM key trim | SVM voice-line key wasn't space-trimmed; original TrimLeft/Right before lookup | `svmdefinitions.cpp` `find` |
| Stack-merge owner | stack-merge overwrote owner/owner_guild; original keeps the existing stack's owner | `inventory.cpp` `addItem` |
| Spell undead target | Skeleton-Mage(33) wrongly in the UNDEAD auto-aim set; original excludes it | `npc.cpp` `isTargetableBySpell` |
| Rune belt gate | belting a rune ran the circle/attr gate; original registers runes without CanUse | `inventory.cpp` `setSlot` |
| Trigger enable/disable | enable/disable went through the fire-delay gate; original applies them immediately | `abstracttrigger.cpp` `processEvent` |

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
| `blood-hitfx-zero-damage-gate` | gating the flesh hit-FX on value>0 (no blood on immune/absorbed hits) also mutes the collision *sound*, which OG conflates into the same addWeaponHitEffect call but the original emits separately — needs a sound/particle split refactor to avoid an audio regression |
| `steer-avoidance-turn-rate-360` | avoidance side-step turn rate (hardcoded 360°/s) should be guild turn_speed capped 100°/s; but onMoveFailed is an explicit "emulate bouncing" approximation, not a Rbt reimpl, so matching the raw scalar may not reproduce the original's behavior — movement-feel, needs runtime tuning |
| `turn-faiturn-2x-combat-rate` | combat turn-to-enemy (implTurnToFai) applies a ×2 multiplier the original AI path lacks (only the player target-lock uses ×4); a deliberate OG feel change — needs runtime validation |
| `spellproj-landscape-shadows-npc-hit` | spell bullet detonates on landscape even when an NPC is closer along the same tick step (physics resolves wall/NPC in exclusive branches); fix sweeps NPC within the truncated segment — physics collision-ordering change, needs runtime |
| `mobanim-animnpc-wrong-postag-multiseat` | multi-seat mobsi transition anim uses the last occupied seat's position tag, not the animating NPC's; routing through posSchemeName (first slot) is a partial fix — full per-NPC threading + on-screen check deferred |
| `bsint-player-weapon-stumble-guard` | armed player is hard-interrupted/stumbled on hit; original suppresses the interrupt for an armed player (queues a non-interrupting T_GOTHIT). High-value combat-parity but changes every armed-player hit reaction and the surgical patch only restores the no-interrupt half (OG can't model the queued reaction) — needs runtime validation |
| `ladder-mid-climb-detach-fall` | letting go mid-climb should drop the player (S_FALLDN); OG detaches cleanly in place. Needs a verified physics+Anim::Fall handoff + rung off-by-one — agent-deferred |
| `flee-waypoint-selection-distance-adaptive` | flee TRIGGER is pure Daedalus (no engine divergence); the engine flee waypoint pick differs wholesale (away-trace at ~2× enemy dist vs fixed 5m radius) with no 1:1 fields — agent-deferred |
| `pickpocket-steal-container-missing` | engine-side steal-container + "victim aware of theft" detection (oCNpc::OpenSteal/IsVictimAwareOfTheft) is unimplemented; OG pickpocket is dialog-only. Whole-feature gap across player-control/NPC/inventory/UI — agent-deferred |
| `hitsnd-parry-tag-order` | original canonicalizes (sorts) the two participants' tags in parry/collision FX names; OG uses fixed attacker→defender order. Different naming family (material vs weapon-type) + asset availability unverifiable from the exe — agent-deferred |
| `sleep-regen-rate-inverted` | independent re-confirmation of [[regen-rate-reciprocal]] via the ZS_Sleep path — same root cause (REGENERATE* is seconds-per-point, OG treats it points-per-second). Still deferred for the same reasons (orders-of-magnitude flip, negative-drain removal, near-dead in vanilla, needs the retail values + runtime check) |
| `onstate-multistate-walk-count` | item on_state[] fires once per command for a single state; original fires each on_state[i] once per state actually reached as the anim arrives. Faithful fix is cross-subsystem (pose state machine has no Npc/VM access) — agent-deferred |
| `waynet-wayto-early-termination` | route search settles `begin` waypoints when merely reached, not when settled (no priority-queue), so non-uniform edge weights can lock a non-shortest path; correct fix is an algorithm-level Dijkstra/A* port entangled with the deferred edge-cost metric — agent-deferred |
| `ext-unbound-externals-inventory` | reference: ZenKit balances the stack for unbound externals, so they are benign no-ops; the genuine unbound ones are engine-handled void AI actions (binding risks double-fire) or feature gaps (Hlp_CutscenePlayed needs a played-registry). DEFERRED; corrects the InsertNpcAndRespawn stack-corruption claim |
| `dlgturn-processinfo-distance-gate` | OG drops AI_ProcessInfos when participants are >2000u apart; the original gates on both event queues being idle (no distance). Removing the constant is a behavioral redesign with soft-lock risk — needs runtime |
| `aitick-checkangrytime-missing` | per-frame CheckAngryTime threat/angry decay (oCNpc fields 0x7e4/0x7e8/0x7ec) is absent in OG — a missing subsystem with no OG fields to patch against; AI-tick otherwise swept faithful — agent-deferred |
| `monster2-ismonster-orc-upper-bound` (secondary) | the original IsMonster has no orc upper bound (orcs are monsters), OG clamps at SEPERATOR_ORC; flipping it changes orc death-handling + auto-crit — broader, deferred pending an orc-behavior pass |
| `armor-disguise-guild` | armor should apply disguise_guild to the live guild on equip (restore true guild on unequip). DEFERRED: OG's `trGuild` is lazily seeded (GIL_NONE until Npc_SetTrueGuild), so the agent's patch swaps the live guild then trueGuild() returns the disguised value → unequip restores to the disguise = permanent disguise. Needs a trGuild-pinning redesign + runtime check |
| `reach-melee-1ha2ha-guild-bonus` | weaponRange uses fight_range_1ha/2ha guild values where the original GetFightRange (@0x0067cd80) uses a single base field + itemRange; divergence is real but the correct base value is uncertain (the agent's "return just item.range" drops the base entirely, contradicting GetFightRange) and it's combat-feel, related to the deferred fai-grange — needs runtime |
| `changelvl-revisit-daily-routine-reset-statedriven` | level re-entry resets ALL NPCs to their daily routine; the original (SetDailyRoutinePos with slot!=NEW) leaves AI-state-driven NPCs at their saved state. OG has no IsAIStateDriven analogue and resetPositionToTA is shared with Wld_SetTime — needs a verified predicate + flag-plumbing |
| `give-dropvob-owner-stamp` | OG stamps dropped items with the dropper's owner; the original never sets owner on drop (only a player-dropped flag). Wholesale ownership-model mismatch (OG dispatches Npc_OwnedByNpc as an item-field compare vs the original's NPC-side method) — agent-deferred |
| `news-assessmurder-not-centralized-in-death` | PERC_ASSESSMURDER is emitted from 2 combat call-sites gated on collision flags; the original broadcasts it from the single DoDie routine for any cause of death. Touches 3 sites, needs single-fire + killer-attribution runtime check — agent-deferred |
| `ctrl-sneak-talent-gate` (REJECTED — false positive) | claimed crouch shouldn't be SNEAK-talent-gated, but the original gates it via CanToggleWalkModeTo→HasTalent(8,1) where talent 8 IS TALENT_SNEAK — OG's canSneak() matches exactly. Verified faithful, not applied |

