# Issue #523 — Revisit `aiQueue` and `aiQueueOverlay` (AI_PointAt as overlay)

**Category:** scripting / aiQueue routing
**Disposition:** DEFER (label: invalid; works as non-overlay; overlay routing is
plausible parity improvement but needs in-game verification + save-format care)

## Request
`AI_PointAt` / `AI_PointAtNpc` / `AI_StopPointAt` should run as **overlay**
actions so the point-finger gesture plays concurrently with `AI_OutputSvm`
dialogue (e.g. "he went thataway" while gesturing). Today they sit in the main
queue and execute sequentially.

## OG files
- `game/world/objects/npc.cpp` — `Npc::aiPush` (3383): only `AI_OutputSvmOverlay`
  is routed to `aiQueueOverlay`; everything else (incl. PointAt) goes to the
  main `aiQueue`. Overlay queue is ticked at `nextAiAction(aiQueueOverlay,dt)`
  (2374). PointAt dispatch: `AI_PointAt`/`AI_PointAtNpc`/`AI_StopPointAt`
  (2876–2893).
- `game/world/aiqueue.cpp` — `aiPointAt` (394), `aiPointAtNpc` (401),
  `aiStopPointAt` (408).
- `game/game/gamescript.cpp` — externals push via `aiPush` (PointAt at 2926/2933
  region for LookAt; StopPointAt at 3115).

## Original behavior (prose, clean-room)
In the original the AI command queue distinguishes a normal queue from an
overlay queue; gesture/look commands that are meant to play *during* speech are
appended to the overlay queue so the body-state animation (T_POINT /
`AI_PointAt`) blends over the talk output rather than serializing after it.

## Divergence
OG only treats `AI_OutputSvmOverlay` as overlay. `AI_PointAt`,`AI_PointAtNpc`,
`AI_StopPointAt` (and arguably `AI_LookAt`/`AI_LookAtNpc`) are in the main queue,
so a point issued mid-dialogue blocks behind the output instead of overlaying.

## Why DEFER (not a surgical FIX)
- The `aiQueueOverlay` save/load path (npc.cpp:342/365, aiqueue.cpp:20) already
  persists; rerouting PointAt changes serialized queue contents — needs
  save-compat thought.
- `AI_StopPointAt` currently lands in the same queue as the matching start;
  splitting start→overlay but leaving stop in main (or vice-versa) would desync.
- Whether vanilla G2 scripts actually depend on overlay-PointAt is unverified;
  the issue is labelled **invalid** by the maintainer.

## Guidance for a future FIX
Route the three PointAt acts (and Stop) through the overlay queue in
`Npc::aiPush` (npc.cpp:3383), mirroring the `AI_OutputSvmOverlay` branch:
```
// NOTE: in original-game AI_PointAt is an overlay gesture played during speech
if(a.act==AI_OutputSvmOverlay || a.act==AI_PointAt ||
   a.act==AI_PointAtNpc || a.act==AI_StopPointAt)
  aiQueueOverlay.pushBack(std::move(a)); else
  aiQueue.pushBack(std::move(a));
```
Then verify: (1) gesture blends with SVM output, (2) StopPointAt cleanly clears
T_POINT, (3) save round-trip, (4) no regression in non-dialogue PointAt scripts.
Do NOT apply blind — requires in-game check first.
