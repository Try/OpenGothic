# AI message-queue: overlay/high-priority messages are front-inserted (non-blocking) in the original, but OpenGothic appends every command to the back of a single FIFO

**Confidence:** High (on the original mechanic and the divergence). **Fix: DEFERRED** (a correct fix is structurally invasive and regression-prone; see below).

## Original function + address (prose only)

The original message-queue insertion happens in `zCEventManager::InsertInList` (Gothic2.exe @0x00787300). A message is appended to the **end** of the manager's message array **only when** `IsHighPriority()` returns 0 **and** `IsOverlay()` returns 0; in every other case (`IsHighPriority() != 0` **or** `IsOverlay() != 0`) the message is `memmove`-shifted in and inserted at **index 0 — the front** of the queue.

`zCEventManager::ProcessMessageList` (@0x00787000) walks the array from front to back and, after dispatching each message, reads the same overlay predicate (vtable slot +0x10): if it returns 0 the loop **returns immediately** (the message is *blocking* — it stops everything behind it); if it returns non-zero processing **continues** to the next message (the message is *non-blocking / overlay*).

I confirmed the two vtable slots used by `InsertInList` map to these predicates via the `oCMsgConversation` vtable (base 0x0083de0c): `oCMsgConversation::IsOverlay` (@0x0076ab00) sits at slot +0x10, and the inherited `oCNpcMessage::IsHighPriority` (@0x007631b0) sits at slot +0x18.

`oCMsgConversation::IsOverlay` (@0x0076ab00) classifies the conversation sub-types. It returns **0 (blocking)** for: `EV_OUTPUT`(4), `EV_OUTPUTSVM`(5), `EV_CUTSCENE`(6), `EV_WAITTILLEND`(7), `EV_ASK`(8), `EV_WAITFORQUESTION`(9), `EV_STOPLOOKAT`(0xa), `EV_STOPPOINTAT`(0xb), `EV_PLAYANI_NOOVERLAY`(0xe), `EV_STOPPROCESSINFOS`(0x11), `EV_SNDPLAY`(0x13), 0x15-0x17. It returns **1 (overlay / non-blocking / front-inserted)** for: `EV_PLAYANISOUND`(0), `EV_PLAYANI`(1), `EV_PLAYSOUND`(2), `EV_LOOKAT`(3), `EV_POINTAT`(0xc), `EV_QUICKLOOK`(0xd), `EV_PLAYANI_FACE`(0xf), `EV_PROCESSINFOS`(0x10), `EV_OUTPUTSVM_OVERLAY`(0x12).

Net original behavior: body-language / overlay commands (`AI_LookAt`, `AI_LookAtNpc`, `AI_PointAt`, `AI_PointAtNpc`, `AI_PlayAni`, `AI_ProcessInfos`, `AI_OutputSVM_Overlay`, …) jump to the **front** of the queue and do **not** wait for queued blocking commands (e.g. a 3-second `AI_Output`) ahead of them; the stop-variants (`AI_StopLookAt`, `AI_StopPointAt`) and the speech/output commands stay blocking and FIFO at the back.

## OpenGothic file:line

- `game/world/aiqueue.cpp:37` `AiQueue::pushBack` — always `push_back` (only special-case is coalescing consecutive `AI_LookAtNpc`).
- `game/world/objects/npc.cpp:3558` `Npc::aiPush` — routes **only** `AI_OutputSvmOverlay` to the separate `aiQueueOverlay`; **every other** action (including `AI_LookAt`, `AI_LookAtNpc`, `AI_PointAt`, `AI_PointAtNpc`, `AI_PlayAnim`, `AI_ProcessInfo`) goes to the main FIFO `aiQueue` via `pushBack`.
- `game/world/objects/npc.cpp:2553` `Npc::nextAiAction` — pops the front of the queue one action per tick; blocking actions `pushFront` themselves until finished, so any action queued behind them cannot run until they complete. There is no front-insertion path for newly-pushed overlay-class actions.

## Divergence

OpenGothic models exactly one overlay sub-type (`AI_OutputSvmOverlay`, EV_OUTPUTSVM_OVERLAY=0x12) by diverting it to a side queue that is ticked every frame. The original treats **eight** sub-types as overlay and front-inserts all of them ahead of pending blocking commands.

Observable consequence: when a script issues a body-language command *after* a blocking one, e.g.

```
AI_Output  (self, hero, "DIA_...");   // blocking, plays for seconds
AI_LookAtNpc(self, hero);             // EV_LOOKAT -> overlay/front in original
```

the original turns the head/points/plays the overlay animation **immediately** (front-inserted, runs concurrently with the still-playing output), while OpenGothic only applies it **after** the blocking `AI_Output` finishes, because it sits behind the output in the single FIFO. The same applies to `AI_PointAt`, `AI_PlayAni`, and the prompt timing of `AI_ProcessInfos`. This shifts dialog gesture/look timing relative to speech.

## Proposed patch — DEFERRED

DEFERRED. A faithful fix requires (a) classifying each `AiAction::Action` as overlay vs blocking per the original `IsOverlay` table, (b) front-inserting overlay actions, and (c) making overlay actions non-blocking so the queue keeps draining past them. OpenGothic's architecture differs structurally: overlay is emulated with a separate once-per-tick `aiQueueOverlay`, and look/point are additionally driven every tick by `implLookAtNpc`/`implLookAtWp` off `currentLookAtNpc`/`currentLookAt`. Re-routing more actions into the overlay queue cannot be done piecemeal without breaking ordering against their blocking stop-variants — e.g. `AI_LookAtNpc`(overlay) vs `AI_StopLookAt`(blocking, EV_STOPLOOKAT=0xa) and `AI_PointAt`(overlay) vs `AI_StopPointAt`(blocking, EV_STOPPOINTAT=0xb) would land in different queues and could execute out of order. A safe change needs a unified queue with a per-action overlay flag plus non-blocking continuation, which is too large/regression-prone for a surgical parity patch and should be designed deliberately.

// NOTE: in original-game zCEventManager::InsertInList @0x00787300 a message is appended at the
// back only when IsHighPriority()==0 && IsOverlay()==0; overlay/high-priority messages
// (oCMsgConversation::IsOverlay @0x0076ab00: EV_LOOKAT/EV_POINTAT/EV_PLAYANI/EV_PROCESSINFOS/
// EV_OUTPUTSVM_OVERLAY/...) are inserted at the front and processed non-blocking by
// zCEventManager::ProcessMessageList @0x00787000.
