# Passive perception range falls back to senses_range instead of the static percRange table

**Confidence:** Medium

## Original function + address

`oCNpc::CreatePassivePerception` (Gothic2.exe @ `0x0075b270`) is the routine that
delivers a *passive* perception (the ones pushed by `oCNpc::PerceiveAll` /
combat / damage events: ASSESSDAMAGE, ASSESSMURDER, ASSESSDEFEAT,
ASSESSOTHERSDAMAGE, ASSESSFIGHTSOUND, ASSESSENTERROOM, ...). It builds the list
of candidate receivers with `oCNpc::CreateVobList(this, &list, (float)percRange[perc])`,
i.e. the collection radius is read **purely from the static global `percRange[]`
table indexed by the perception id**. There is no per-NPC component: the
sender's and receiver's `senses_range` (`oCNpc` field at `+0x284`) is never
consulted on this path.

The `percRange[]` table is a process-global array (`oCNpc::SetPerceptionRange`
@ `0x0075e440` is its only writer; `oCNpc::IsInPerceptionRange` @ `0x0075e460`
its reader). It is populated by the script-side range setter (`Perc_SetRange`).
A perception whose range was never set therefore keeps the table default, and
`CreateVobList` with that default (a non-positive radius) builds an inverted
bounding box and collects nobody — so the passive perception is simply not
delivered.

By contrast, the *active* perception scan (`oCNpc::PerceptionCheck`
@ `0x0075dd30`, raising ASSESSPLAYER/ENEMY/FIGHTER/BODY/ITEM) collects with
`CreateVobList(this, senses_range)` and gates each hit through `oCNpc::CanSense`.
So `senses_range` is the active-scan radius, and `percRange[perc]` is the
passive-delivery radius; the two paths use different range sources in the
original.

## OpenGothic file:line

- `game/world/worldobjects.cpp:950` (`WorldObjects::passivePerceptionProcess`)
- supporting: `game/game/gamescript.cpp:80-87` (`GameScript::PerDist::at`)

## Divergence

`passivePerceptionProcess` computes the delivery radius as:

```cpp
const float range = float(owner.script().percRanges().at(PercType(msg.what),
                                                          npc.handle().senses_range));
```

and `PerDist::at(perc, r)` returns the stored `range[perc]` only when it is
`> 0`, otherwise returns the fallback `r` — here the **receiver's
`senses_range`**. Consequently, for any passive perception whose range was never
set via `Perc_SetRange`, OpenGothic delivers it out to the receiver's
`senses_range` (typically ~1500-2000), whereas the original delivers it to
nobody (unset `percRange[perc]` => empty candidate list).

In vanilla G2 this is usually masked because `Init_Perceptions` sets a range for
every perception the game uses; it becomes observable with mods (or any script
path) that fire a passive perc without a matching `Perc_SetRange`, and more
subtly it makes OpenGothic's passive range vary per-NPC (with `senses_range`)
rather than being a single global per-perc constant as in the original.

## Proposed patch

DEFERRED.

Reason: The fix is mechanically simple — for the *passive* path, drop the
`senses_range` fallback and require an explicitly-set per-perc range, e.g. give
`PerDist` an accessor that returns the raw `range[perc]` (or a sentinel meaning
"unset => deliver to no one") and use it in `passivePerceptionProcess` instead
of `at(perc, senses_range)`. However, two things must be verified first to avoid
a regression, and could not be confirmed within clean-room limits:

1. The static default value of the original `percRange[]` table (BSS-zero vs. a
   baked-in default). The single `SetPerceptionRange` xref is the script setter,
   not a default-table initializer, which points to "zero/unset until script
   sets it" — but this needs confirmation before changing OG's fallback, because
   removing the `senses_range` fallback would suppress any passive perc that the
   loaded scripts never range-set, exactly matching the original only if that
   assumption holds.
2. Whether the OpenGothic-supported script sets (vanilla + common mods) always
   call `Perc_SetRange` for every passively-sent perc; if not, removing the
   fallback changes observable behavior for those scripts and "empty beats false
   positives" argues for leaving the resilient fallback in place.

// NOTE: in original-game oCNpc::CreatePassivePerception @0x0075b270 the passive
// delivery radius is percRange[perc] only (no senses_range fallback); active
// scan oCNpc::PerceptionCheck @0x0075dd30 uses senses_range. Deferred pending
// confirmation of the static percRange default and script range coverage.
