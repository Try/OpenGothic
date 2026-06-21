# Applied parity fixes vs. the GitHub tracker

How the 39 applied fixes on this branch relate to the open issue tracker. All fixes are
build- and boot-verified (full clean rebuild passes); **none are gameplay-verified** — an
in-game pass is the recommended next step before merge.

## Directly addresses an open tracker issue
| Issue | Fix | Note |
|---|---|---|
| [#647](https://github.com/Try/OpenGothic/issues/647) Throne NPC offset | preserve NPC Y at mobsi slots | direct |
| [#585](https://github.com/Try/OpenGothic/issues/585) NPC walk-teleport misalignment | exact-snap on goto arrival | direct |
| [#920](https://github.com/Try/OpenGothic/issues/920) `time.slw` stacking | single-instance slow-time | direct |

## Partially addresses / likely relevant
| Issue | Fix | Note |
|---|---|---|
| [#637](https://github.com/Try/OpenGothic/issues/637) Move-trigger problems | mover NEXT/PREV wrap; TRIGGER_CONTROL ref-count | two more mover bugs fixed; the elevator floor-clip sub-item remains (runtime) |
| [#623](https://github.com/Try/OpenGothic/issues/623) bookshelf mover | mover fixes above | the moving-bookshelf is a `zCMover`; the wrap/ref-count fixes may help — needs in-game check |

## Net-new parity fixes (not currently tracked as issues)
The other ~26 proactive fixes were found by diffing `Gothic2.exe` and are not in the tracker.
Highlights with clear gameplay impact:
- **Container `contains` whitespace** — items after a `", "` separator were silently dropped in
  *shipped* data (27 occurrences); chests were losing contents.
- **ACROBAT talent** gave no fall-damage protection (original doubles safe fall height).
- **Fall damage** under-counted (missing +50cm), **sight cone** ±80° vs ±91°, **fight range**
  folded Y into 3D distance (mis-judged on slopes/stairs), **sell price** over-paid (ceil vs round),
  **mob-use distance** 1000 vs 500, **music** day/night window + zone-priority, **perception**
  default 5000ms, **`Log_SetTopicStatus`** spawned phantom quests, **`Wld_DetectItem`** missed
  weapon/subtype masks, **TouchDamage** `repeat_delay==0` hit every frame.

See `docs/parity/proactive/README.md` for the full list (applied + deferred) and per-fix docs.

## Deferred (need runtime tuning or are non-surgical) — ~14
Distance-metric ports (waynet, GetDistToWP), camera azimuth clamp, regen rate/interval
reciprocal, stumble-when-armed, multi-type-immune, item spawn yaw, perc fighter/item,
death-unconscious perc-wipe, bow-multi-munition (unsafe as written). Each documented with
the reason it needs an in-game check before applying.
