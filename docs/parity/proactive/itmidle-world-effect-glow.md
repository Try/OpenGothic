# World-Item idle effect (C_Item.effect / "magischer Schimmer") never spawned

**Confidence:** High (for the divergence); resolution **DEFERRED** (primarily visual, non-surgical).

## Original function + address
- `oCItem::ThisVobAddedToWorld` (Gothic2.exe @ 0x00712df0). When an item vob is added to a
  live (non-editor) world it disables physics, puts the body to sleep, and — gated on the
  world's editor/spawn-only flag being clear — calls `oCItem::InsertEffect`.
- `oCItem::InsertEffect` (Gothic2.exe @ 0x00712c40). Gated on a global "item effects enabled"
  flag, on the item not already owning an effect handle, and on the item's `effect` string
  (the Daedalus `C_Item.effect` field, the in-world magic shimmer) being non-empty. It creates
  and plays a looping `oCVisualFX` built from that effect name on the item vob, and binds the
  PFX shape to the item's visual mesh. The handle is later torn down by `oCItem::RemoveEffect`
  (@ 0x00712c00) when the item leaves the world / is picked up.

Net behavior: any world item whose instance sets a non-empty `effect` (the on-ground glow for
magic/special items) shows a continuously-playing visual FX bound to its mesh. OpenGothic does
not.

## OpenGothic file:line
- `game/world/objects/item.cpp:55` and `:125` — `hitem->effect` is read/written for save games
  only.
- `game/world/objects/item.cpp` Item constructors (T_World / T_WorldDyn paths) and
  `game/world/worldobjects.cpp:641-695` (`addItem` / `addItemDyn`) — create the mesh view and,
  for dynamic drops, physics, but never spawn an effect from `hitem->effect`.
- Grep confirms `effect` appears in the item subsystem only inside `fin.read(...)` /
  `fout.write(...)`; no `runEffect` / `Effect` / `PfxEmitter` is ever constructed from it.

## Divergence
World items configured with `C_Item.effect` (e.g. magic-item shimmer) glow/emit their FX in
original Gothic the moment they enter the world; in OpenGothic they are inert. The `effect`
field round-trips through saves but is otherwise dead data.

## Proposed patch
DEFERRED.

Reason: the observable effect is purely visual (a looping `oCVisualFX` / particle-and-light
overlay on the dropped/placed item). Per the clean-room rules ("Pure visual → DEFERRED"), and
because a faithful port is not surgical — it would require attaching an `Effect`
(`game/graphics/effect.h`) or `PfxEmitter` to every `Item`, driving its lifetime against
`ThisVobAddedToWorld` / `RemoveEffect` semantics (spawn on add, destroy on take/clearView),
honoring the global enable flag and the empty-string gate, and binding the PFX shape to the
item visual — this is left for a dedicated change rather than forced here.

NOTE drafting guidance for whoever implements it:
`// NOTE: in original-game oCItem::ThisVobAddedToWorld @0x00712df0 -> oCItem::InsertEffect`
`// @0x00712c40 spawns a looping VisualFX from C_Item.effect on the world item.`
