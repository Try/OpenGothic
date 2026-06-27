# AI_StopLookAt / clearAiQueue hard-snaps the head to neutral instead of fading it home

**Confidence:** High (divergence); fix **DEFERRED** (architectural — no surgical 1:1 patch)

## Original function + address (prose only)

The script external `AI_StopLookAt` (and the implicit stop performed when an AI
state ends or talking finishes) is serviced by `oCNpc::EV_StopLookAt`
(Gothic2.exe @ 0x00759e80). That handler does three things: it cancels the
active head-tracking conversation message held in the NPC field at offset 0x990,
walks the event manager and acks any still-queued LookAt conversation messages
(subType == 3), and then calls `oCAniCtrl_Human::StopLookAtTarget`
(Gothic2.exe @ 0x006b6640).

`StopLookAtTarget` is the important part: it releases the look target, sets two
interpolation fields (offsets 0x17c / 0x180) to 0.5, and then **fades the look
animation out gradually** — it `FadeOutAni`s the running `T_QLOOK` look-tracking
animation and cross-fades into `S_TLOOK`, the neutral idle-look animation. The
head therefore returns to its forward rest pose *smoothly over the fade-out*,
driven by the model animation system. At no point does the original write the
head bone angle directly to zero; the head never teleports.

This is also the behaviour the original uses when the EM is cleared via
`oCNpc::ClearEM` (Gothic2.exe @ 0x00746400, the body of the `Npc_ClearAIQueue`
external `FUN_006ee2d0`): `ClearEM` flushes the message queue and stands/stops
turn-anims, but it never zeroes the head angle either.

## OpenGothic file:line

- `game/world/objects/npc.cpp:2659-2663` — `case AI_StopLookAt`:
  ```
  currentLookAtNpc=nullptr;
  currentLookAt=nullptr;
  visual.setHeadRotation(0,0);
  ```
- `game/world/objects/npc.cpp:4871-4874` — `Npc::clearAiQueue()` does the same:
  `currentLookAt = nullptr; currentLookAtNpc = nullptr; visual.setHeadRotation(0,0);`
- `game/graphics/mesh/pose.cpp:832-834` — `Pose::setHeadRotation` writes the
  angle directly: `headRotX = dx; headRotY = dz;` (no interpolation), and
  `pose.cpp:455-458` applies `headRotX/Y` straight onto the BIP01_HEAD bone matrix.

## Divergence

OpenGothic drives head rotation **only while a look-at target is alive**:
`Npc::implLookAt` (`npc.cpp:1417-1455`) interpolates `visual.headRotation()`
toward the target at 200 deg/s, and it is called from the per-tick path
(`npc.cpp:2531-2533`, `implLookAtNpc`/`implLookAtWp`) which early-returns the
moment `currentLookAt`/`currentLookAtNpc` is null. There is **no per-frame decay
of `headRotX/headRotY` back to zero** anywhere — the only writers are
`implLookAt` (target present) and the explicit `setHeadRotation(0,0)` snap.

Consequently, when `AI_StopLookAt` (or `clearAiQueue`) fires, OpenGothic nulls
the target *and* slams the head bone to (0,0) in the same frame. The head
**teleports** from its turned pose to dead-ahead. The original fades it back
over the `T_QLOOK`→`S_TLOOK` cross-fade, so the head turns home smoothly.

Because `B_StopLookAt` / state-exit look-stops are common (dialog reactions,
ambient "glance at the hero" routines, perception hand-offs), the instantaneous
snap is broadly visible whenever an NPC had its head turned at the moment the
look ends.

If, instead, the snap line were merely deleted, the head would *freeze* at its
last turned angle (since nothing else drives it back) — also wrong. Neither
"snap to 0" nor "freeze" matches the original's smooth return.

## Proposed patch — DEFERRED

No surgical, 1:1 patch exists. Matching the original requires a *head-homing*
interpolation state: after the look target is cleared, `headRotX/headRotY` must
be driven back toward 0 over time (at roughly the same `implLookAt` rate),
rather than written to 0 in one frame. That means adding a "returning" branch to
the per-tick head path (around `npc.cpp:2531-2533`) that, when no look target is
present but `visual.headRotation()` is non-zero, steps the angle toward (0,0)
using the existing `implLookAt`-style clamp — and then auditing every other
caller of `setHeadRotation(0,0)` (e.g. `clearAiQueue`, and any turn/anim reset
paths) so the new decay does not fight code that intentionally holds or hard-
resets the head.

That touches the shared movement/animation tick and multiple reset sites, so it
is **not** a high-confidence surgical change and is deferred to avoid
regressions, per "empty beats false positives."

```
// NOTE: in original-game oCNpc::EV_StopLookAt @0x00759e80 -> oCAniCtrl_Human::StopLookAtTarget
// @0x006b6640 the head returns to neutral by fading out T_QLOOK and cross-fading S_TLOOK
// (FadeOutAni), i.e. a smooth animated return -- it never writes the head bone angle to 0.
// OpenGothic's AI_StopLookAt / clearAiQueue call visual.setHeadRotation(0,0), and Pose::setHeadRotation
// (pose.cpp) writes headRotX/Y directly with no decay path, so the head teleports forward in one frame.
```

## Grep-verified OpenGothic symbols

- `Npc::clearAiQueue` / `case AI_StopLookAt` — `game/world/objects/npc.cpp`
- `currentLookAt`, `currentLookAtNpc` — members, `npc.cpp`
- `Npc::implLookAt`, `implLookAtNpc`, `implLookAtWp` — `npc.cpp:1401-1455,2531-2533`
- `MdlVisual::setHeadRotation` / `headRotation` — `game/graphics/mdlvisual.cpp:439-445`
- `Pose::setHeadRotation`, `headRotX`, `headRotY` — `game/graphics/mesh/pose.cpp:832-838`, `pose.h:85,148`
