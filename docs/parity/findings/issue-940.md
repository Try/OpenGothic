# Issue #940 — Picking up: things disappear a bit too early

## Issue
When the player takes a world item, the item's world visual is removed too early.
Expected (original Gothic II): the item stays visible while the "take" animation
(`T_STAND_2_IGET` / `S_IGET`) plays, and only disappears at the hand-contact frame of
that animation. In OpenGothic the visual vanishes the instant the take is initiated.

## Subsystem & OG files
- `game/world/objects/npc.cpp` — `Npc::takeItem` (take-item logic), `AI_TakeItem`
  dispatch, animation-event handling (`ITEM_INSERT`, etc.).
- `game/world/world.cpp` / `game/world/worldobjects.cpp` — `World::takeItem` →
  `WorldObjects::takeItem` (actual world-vob removal).
- `game/graphics/mesh/animationsolver.cpp` — `Anim::ItmGet` → `S_IGET`.
- `game/graphics/mdlvisual.cpp` — inventory/hand slot mesh handling.

## Original behavior (Gothic2.exe — functions + addresses)
Taking a world item is driven through the message/animation system, not done
synchronously:

- `oCNpc::EV_TakeVob` (0x007534e0, dispatched from `oCNpc::OnMessage` 0x0074c37f via an
  `oCMsgManipulate` TAKEVOB message). This routine selects the take animation —
  `T_STAND_2_IGET` (string @ 0x008b14cc) for `oCItem`, `T_STAND_2_OGET` for other vobs —
  starts it with `zCModel::StartAni`, and returns "not finished" (0). It does NOT remove
  the world vob. The message persists across frames while the animation plays.
- The world vob is only removed when the take animation reaches its hand-contact event
  (`EV_TAKEVOB`, string @ 0x008ba3ec), which invokes the virtual take through the model
  event-dispatch slot (caller @ 0x0083d804) into `oCNpc::DoTakeVob` (0x007449c0).
  `DoTakeVob` is what disables physics (`SetPhysicsEnabled(0)`, `SetCollDetDyn`), removes
  the vob from the world subtree, and puts it in the NPC's right-hand slot
  (`PutInSlot(NPC_NODE_RIGHTHAND, ...)`).

Sequencing (original): start TAKEVOB message → choose `T_STAND_2_IGET` and start it →
animation plays with item still visible in world → at the animation's take event frame,
`DoTakeVob` removes the world vob and attaches it to the hand. The disappearance is thus
tied to a specific frame, not to the moment the action begins.

## OpenGothic current behavior (file:line)
OpenGothic removes the world visual synchronously at the start of the take:

- `game/world/objects/npc.cpp:3406` — `Npc::takeItem` picks the `Anim::ItmGet` sequence
  (`setAnimAngGet`).
- `game/world/objects/npc.cpp:3410` — immediately calls `owner.takeItem(item)`
  (`World::takeItem`, `world.cpp:634`) → `WorldObjects::takeItem`
  (`worldobjects.cpp:596`).
- `game/world/objects/worldobjects.cpp:602-604` — `items.del(ret.get())` removes the
  rendered world item and `ret->setPhysicsDisable()` disables it, then
  `onItemRemoved(...)`. This happens on the same frame the animation is started.
- `game/world/objects/npc.cpp:3428` — only after removal does `implAniWait(totalTime)`
  block for the animation duration.

The `ITEM_INSERT` animation-event handler at `game/world/objects/npc.cpp:2222`
(`invent.putCurrentToSlot`) only manages the inventory/hand-slot mesh — it has nothing to
do with the world vob, which is already gone. There is no `DEF_TAKEVOB`-equivalent event
gating the world removal anywhere in `game/`.

## Divergence hypothesis
The original defers world-vob removal to the take animation's contact event frame
(`EV_TAKEVOB` → `DoTakeVob`), so the item stays visible for most of `T_STAND_2_IGET`.
OpenGothic removes the world item up-front in `Npc::takeItem` (npc.cpp:3410 →
worldobjects.cpp:602-604), at animation start. The whole take animation then plays over an
already-empty spot, which reads as "disappears too early."

## Proposed fix
Defer the world-vob removal so it fires at the take animation's contact frame instead of
at initiation:

- In `Npc::takeItem` (npc.cpp), start the `Anim::ItmGet` sequence and reserve the target
  item, but do NOT call `owner.takeItem(item)` yet. Keep the world `Item` visible (and
  physics-disabled so it can't be taken twice) for the duration.
- Drive the actual `World::takeItem` + inventory insertion from the take animation's event
  point. The cleanest hook is the existing animation-event loop in
  `Npc::processEvents` (npc.cpp:2214) — handle `ITEM_INSERT` (DEF_INSERT_ITEM /
  the take's contact tag) for the pending world-take by performing the world removal and
  `addItem` there, rather than in `takeItem` itself. If the chosen take sequence carries
  no usable item event, fall back to removing at a fixed fraction of the sequence (or at
  `sq->totalTime()` completion) so behavior degrades gracefully.

```cpp
// NOTE: in original-game oCNpc::EV_TakeVob (0x007534e0) only STARTS the take
// animation (T_STAND_2_IGET); the world vob is removed later by oCNpc::DoTakeVob
// (0x007449c0), invoked from the animation's TAKEVOB contact event — NOT at the
// moment the take is initiated. Removing the world item up-front (here) makes it
// disappear too early (issue #940).
```

## Status
**Deferred — not applied (intentionally).** The correct fix is event-driven: stop removing
the world vob in `Npc::takeItem` and instead remove it at the take animation's contact
event. But `Npc::takeItem` returns the taken `Item*` synchronously and its callers depend on
that contract — the AI queue (`npc.cpp:2856`, `if(takeItem(*act.item)==nullptr) ...`) and
player control (`playercontrol.cpp:369`). Deferring removal means the result is no longer
known at call time, so this requires reworking the take into a pending/animation-completion
state across the AI queue and `PlayerControl`. That is a gameplay-critical change that needs
in-game testing (which can't be done from this headless setup) to avoid regressing all item
pickup — so it is left as a scoped follow-up rather than a half-applied refactor (per the
repo's no-workarounds rule). Analysis above stands as the implementation guide.
