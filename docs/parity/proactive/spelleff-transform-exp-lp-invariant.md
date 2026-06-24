# Transform spell: only `level` is preserved, but the engine also keeps exp / exp_next / learn-points

**Confidence:** High

## Original function + address

`oCSpell::CastSpecificSpell` (entry `0x00486960`) handles the transform spell category
(spell IDs in the `0x2f..0x3a` range). When the chosen NPC-instance is valid it creates the
transform target NPC via the script factory, then calls
`oCNpc::CopyTransformSpellInvariantValuesTo` (entry `0x0073d3d0`, called as
`original->CopyTransformSpellInvariantValuesTo(transformed)`).

That helper copies four oCNpc dwords from the original NPC onto the freshly created transformed
instance (plus model fatness and the re-checked model overlays). The four dwords are identified
unambiguously by `OpenScreen_Status` (entry `0x0073d980`), which reads the very same offsets and
passes them to `oCMenu_Status::SetExperience(exp, exp_next, level)` and
`oCMenu_Status::SetLearnPoints(lp)`:

- offset `0x234` -> experience (exp)
- offset `0x42c` -> experience-to-next-level (exp_next)
- offset `0x430` -> level
- offset `0x434` -> learn points (lp)

So in the original engine, the whole RPG-progression quartet (exp, exp_next, level, learn-points)
is "transform-invariant": it stays equal to the underlying character's values for the entire
duration of the transformation.

## OpenGothic file:line

`game/world/objects/npc.cpp:3285-3293` (forward transform inside `Npc::tickSpecial`/cast path).

```cpp
if(spellInfo!=0 && transformSpl==nullptr) {
  transformSpl.reset(new TransformBack(*this));   // snapshot of pre-transform hnpc
  invent.updateView(*this);
  visual.clearOverlays();

  owner.script().initializeInstanceNpc(hnpc, size_t(spellInfo)); // re-inits exp/exp_next/lp/level
  spellInfo  = 0;
  hnpc->level = transformSpl->hnpc->level;          // only level restored
  }
```

## Divergence

`initializeInstanceNpc(hnpc, spellInfo)` re-runs the transform-creature's Daedalus instance
constructor, which overwrites `hnpc->exp`, `hnpc->exp_next`, `hnpc->lp` and `hnpc->level` with the
creature instance's values (typically 0 for monster instances). OpenGothic then restores only
`hnpc->level`, leaving `exp`, `exp_next` and `lp` at the creature's (usually zero) values for the
whole transformed period. The original engine copies all four forward via
`CopyTransformSpellInvariantValuesTo`, keeping them synced with the underlying character.

Consequences while transformed:
- The status screen shows wrong experience / learn-points.
- Any XP gained while transformed is accumulated against the wrong `exp`/`exp_next` base, so
  level-up checks (`Npc_GetLevelByXP`-style comparisons against `exp_next`) behave incorrectly.
- `TransformBack::undo()` (npc.cpp:131-145) deliberately carries the *current* exp/exp_next/lp/level
  forward onto the restored original hnpc. Because the forward path left those at the creature's
  values, the corrupted base is propagated back onto the character after transform-back, not just
  during it.

`TransformBack::undo` already restores `body/head/visual/...`, and the pre-init snapshot
`transformSpl->hnpc` (taken at line 3286, before `initializeInstanceNpc`) still holds the original
exp/exp_next/lp/level — so the correct values are available; they are simply not re-applied on the
forward path.

## Proposed patch

Grep-verified symbols: `hnpc->exp`, `hnpc->exp_next`, `hnpc->lp`, `hnpc->level` all exist on
`zenkit::INpc` and are already read/written in this file (npc.cpp:134-144, 1332-1340).
`transformSpl->hnpc` is the pre-init snapshot.

OLD:
```cpp
    owner.script().initializeInstanceNpc(hnpc, size_t(spellInfo));
    spellInfo  = 0;
    hnpc->level = transformSpl->hnpc->level;
    }
```

NEW:
```cpp
    owner.script().initializeInstanceNpc(hnpc, size_t(spellInfo));
    spellInfo  = 0;
    // NOTE: in original-game oCSpell::CastSpecificSpell @0x00486960 the transform path calls
    // oCNpc::CopyTransformSpellInvariantValuesTo @0x0073d3d0, which keeps exp/exp_next/level/lp
    // (oCNpc offsets 0x234/0x42c/0x430/0x434, confirmed via OpenScreen_Status @0x0073d980)
    // invariant across the transform -- not just level.
    hnpc->exp      = transformSpl->hnpc->exp;
    hnpc->exp_next = transformSpl->hnpc->exp_next;
    hnpc->lp       = transformSpl->hnpc->lp;
    hnpc->level    = transformSpl->hnpc->level;
    }
```
