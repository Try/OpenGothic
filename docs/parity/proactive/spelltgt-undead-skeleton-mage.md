# Spell auto-aim: skeleton-mage wrongly counts as UNDEAD spell target

**Confidence:** High

## Original function + address (prose only)

The original spell target-type filter is `oCSpell::IsTargetTypeValid(zCVob*, int targetType)`
at `0x00485fc0` (static, `__cdecl`). It is reached from `oCSpell::IsValidTarget`
(`0x004861d0`), which is called inside the focus-collection loop of
`oCNpc::CollectFocusVob` (`0x00733a10`) when the NPC is in the magic focus mode
(focus-mode slot `this+0x250 == 7`) with a spell selected — i.e. this is the function
that decides which NPC the player auto-aims at while a spell is readied.

For `targetType == TARGET_TYPE_ALL` (value 1) it returns true. Otherwise it walks the
type bitmask. For the `TARGET_TYPE_UNDEAD` bit it accepts a living NPC only when its
true-guild equals one of a fixed set of guild ids. Verified two ways against
`Gothic2.exe`: the decompile of `0x00485fc0`, and an instruction-level scalar-constant
search (`wde find`) scoped to that function. The undead guild ids it compares against are
exactly:

- `0x14` (20) — GIL_GOBBO_SKELETON
- `0x15` (21) — GIL_SUMMONED_GOBBO_SKELETON
- `0x1f` (31) — GIL_SKELETON
- `0x20` (32) — GIL_SUMMONED_SKELETON
- `0x22` (34) — GIL_ZOMBIE
- `0x25` (37) — GIL_SHADOWBEAST_SKELETON

The constant `0x21` (33 = GIL_SKELETON_MAGE) is **not** present anywhere in
`IsTargetTypeValid` (confirmed: `wde find 0x21` returns 0 hits in `0x00485fc0`, while
`0x14/0x15/0x1f/0x20/0x22/0x25` each return a hit). So in the original a Skeleton-Mage is
**not** a valid undead spell target.

## OpenGothic file:line

`game/world/objects/npc.cpp:3214-3217` — `Npc::isTargetableBySpell(TargetType)`, the
clean-room equivalent of `oCSpell::IsTargetTypeValid`. The `G2_UNDEAD` set is:

```
const auto G2_UNDEAD = (gil == GIL_GOBBO_SKELETON ||
  gil == GIL_SUMMONED_GOBBO_SKELETON || gil == GIL_SKELETON      ||
  gil == GIL_SUMMONED_SKELETON       || gil == GIL_SKELETON_MAGE ||
  gil == GIL_SHADOWBEAST_SKELETON    || gil == GIL_ZOMBIE);
```

Guild ids (`game/game/constants.h`): GIL_GOBBO_SKELETON=20, GIL_SUMMONED_GOBBO_SKELETON=21,
GIL_SKELETON=31, GIL_SUMMONED_SKELETON=32, **GIL_SKELETON_MAGE=33**, GIL_ZOMBIE=34,
GIL_SHADOWBEAST_SKELETON=37.

## Divergence

OpenGothic's `G2_UNDEAD` = {20,21,31,32,**33**,34,37} — one element more than the
original's {20,21,31,32,34,37}. The extra element is `GIL_SKELETON_MAGE` (33).

Behavioral effect: with a spell whose `target_collect_type` includes
`TARGET_TYPE_UNDEAD` (and not `TARGET_TYPE_NPCS`/`ALL`), OpenGothic will auto-aim / treat
a Skeleton-Mage as a valid spell target, whereas the original `Gothic2.exe` excludes it.
This changes spell focus acquisition against Skeleton-Mages.

## Proposed patch

OLD (`game/world/objects/npc.cpp:3214-3217`):
```
  const auto G2_UNDEAD = (gil == GIL_GOBBO_SKELETON ||
    gil == GIL_SUMMONED_GOBBO_SKELETON || gil == GIL_SKELETON      ||
    gil == GIL_SUMMONED_SKELETON       || gil == GIL_SKELETON_MAGE ||
    gil == GIL_SHADOWBEAST_SKELETON    || gil == GIL_ZOMBIE);
```

NEW:
```
  // NOTE: in original-game oCSpell::IsTargetTypeValid @0x00485fc0 the TARGET_TYPE_UNDEAD
  // branch only accepts true-guilds {20,21,31,32,34,37}; GIL_SKELETON_MAGE(33) is NOT
  // in that set (verified absent from the function's constants), so it must be excluded.
  const auto G2_UNDEAD = (gil == GIL_GOBBO_SKELETON ||
    gil == GIL_SUMMONED_GOBBO_SKELETON || gil == GIL_SKELETON   ||
    gil == GIL_SUMMONED_SKELETON       || gil == GIL_ZOMBIE     ||
    gil == GIL_SHADOWBEAST_SKELETON);
```

Grep-verified symbols: `GIL_GOBBO_SKELETON`, `GIL_SUMMONED_GOBBO_SKELETON`, `GIL_SKELETON`,
`GIL_SUMMONED_SKELETON`, `GIL_ZOMBIE`, `GIL_SHADOWBEAST_SKELETON`, `GIL_SKELETON_MAGE` all
exist in `game/game/constants.h` with the ids above. `Npc::isTargetableBySpell` exists at
`game/world/objects/npc.cpp:3205`.
