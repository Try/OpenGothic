# Issue #647 — Incorrect offset of NPC when interacting with THRONE

Upstream: https://github.com/Try/OpenGothic/issues/647

## Issue
When an NPC uses an interactive object (mobsi) such as a THRONE, the NPC is
snapped to a slightly wrong position/height/orientation. The "throne offset"
manifests as the NPC floating, sinking, or sitting shifted relative to the
original game.

## Subsystem & OG files
- `game/world/objects/interactive.cpp` / `.h` (class `Interactive` = mobsi)
  - `attach()` ~L774, `setPos()` ~L879, `setDir()` ~L890,
    `nodePosition()` ~L900, `nodeTranform()` ~L921
- `game/world/objects/npc.cpp`: `setPosition()` ~L393/404, `setDirection()` ~L441

## Original behavior (functions + addresses)
Class `oCMobInter` (Gothic2.exe). Interaction entry is `StartInteraction`
(0x721580) → calls `ScanIdealPositions` (0x71dc30) then `SetIdealPosition`
(0x71e240).

- `ScanIdealPositions` (0x71dc30): walks the visual's model nodes and, for every
  node whose name contains the substring `ZS_POS` (string `ZS_POS` @0x8b7ac8),
  creates a `TMobOptPos` slot. The slot stores the node's **model-node-to-world
  matrix** (`GetTrafoModelNodeToWorld`). If the node name also contains `DIST`,
  the slot's "isDist" flag (struct offset +0x40) is set to 1; if it contains
  `NPC` it is counted as a multi-NPC slot.

- `SearchFreePosition` (0x71dfc0, vtable +0x11c, invoked with float param
  150.0f) returns the nearest free slot for the user.

- `SetIdealPosition` (0x71e240) places the user NPC at the chosen slot. Two
  branches keyed on the slot's isDist flag (+0x40):
  - **isDist == 0 (normal slot, e.g. throne `ZS_POS0`):**
    1. `SetTrafoObjToWorld(npc, slotMatrix)` — copies the **full** slot
       node-to-world rotation onto the NPC (heading taken straight from the
       slot's orientation basis).
    2. `SetPositionWorld(npc, pos)` where `pos = (slot.x, **npc's own current
       world Y**, slot.z)`. The slot supplies X and Z; the NPC keeps the Y it
       already had (the value read from the npc vob, not the slot node's Y).
  - **isDist != 0 (DIST slot):** keeps the NPC standing back, computes a
    point along the slot→NPC direction at the stored distance, sets only the
    yaw via `SetHeadingYWorld` (`SetHeading`, 0x71e1f0, rotates about world Y
    only).
  Net for a throne: orientation = slot basis; X/Z = slot; **Y = NPC's prior
  world Y** (no vertical teleport to the node height, no ground ray here).

## OpenGothic current behavior (file:line)
- `attach()` (interactive.cpp:777-802): `mat = nodeTranform(&npc,to)`;
  `mat.project(mv)` extracts the slot translation into `mv`; `setPos(npc,mv)`;
  `setDir(npc,mat)`.
- `nodeTranform()` (interactive.cpp:921-958): for a **non-dist** slot it returns
  `visual.bone(nodeId)` verbatim — i.e. the full node-to-world matrix including
  the node's own Y.
- `setPos()` (interactive.cpp:879-888): `npc.setPosition(mv)` using **all three**
  components `mv.x, mv.y, mv.z` straight from the node matrix.
- `setDir()` (interactive.cpp:890-897): projects (0,0,0) and (0,0,1) through the
  full matrix and feeds the 3D difference to `Npc::setDirection(Vec3)`
  (npc.cpp:441), which via `angleDir` reduces it to a yaw angle (X/Z only).
- `nodePosition()` (interactive.cpp:900-919): the ground-ray snap is `#if 0`'d
  out (commented "no need in 'ground rays'").

## Divergence hypothesis
For a normal (non-DIST) throne slot, OpenGothic teleports the NPC to the **node's
own Y** (the translation read out of the `ZS_POS0` node-to-world matrix), whereas
the original **preserves the NPC's existing world Y** and only overrides X/Z from
the slot. Throne `ZS_POS0` nodes are authored at the seat/model height, not at
the NPC's foot height, so taking the node Y directly raises or lowers the NPC by
the seat offset — the reported "incorrect offset." (Orientation matches closely
because both derive heading from the slot basis.) The disabled ground-ray code
would not reproduce original behavior either; the original simply keeps the
NPC's Y.

## Proposed fix
In `Interactive::attach()` (or `setPos`), keep the NPC's current world Y and only
override X/Z from the slot translation for non-dist slots:

```
// NOTE: in original-game oCMobInter::SetIdealPosition (Gothic2.exe 0x71e240),
// the non-DIST slot branch sets SetPositionWorld(slot.x, npc.currentY, slot.z)
// -- X/Z come from the ZS_POS node, but the NPC keeps its own world Y.
auto prevY = npc.position().y;
mat.project(mv);
if(!to.isDistPos())
  mv.y = prevY;            // preserve NPC foot height; only XZ snap to slot
```

Heading already follows the slot basis via `setDir`, matching the original's
`SetTrafoObjToWorld`. Keep the DIST branch untouched (it already keeps the NPC
back and yaw-aligns). No ground-ray snap should be added — the original does not
perform one in this path.

## Status
**Applied** in `game/world/objects/interactive.cpp` (`Interactive::attach`, right after
`mat.project(mv)`): for non-DIST slots, `mv.y = npc.position().y` so the NPC keeps its own
world Y and only X/Z snap to the slot. Builds clean. Behavioral verification (sit on a
throne in-game and check height) still needs a playtest.
