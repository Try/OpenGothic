# Dialog start: OpenGothic's 2000-unit distance gate on AI_ProcessInfos has no original equivalent

**Confidence:** Medium (the *divergence* is high-confidence/verified; whether it is a *bug worth fixing* is low — see DEFERRED reasoning).

## Original function + address (prose only)
The Daedalus external `AI_ProcessInfos` is bound in `oGameExternal.cpp` (warm-decompiler symbol `FUN_006dbc80` @ `0x006dbc80`). Its logic, described in prose: it dynamic-casts the global self-player vob to `oCNpc` (the partner / hero), takes `self` as the conversation owner, and — *only* gating on `oCInformationManager::HasFinished()` being true (i.e. no other conversation is already running) and on `self` being non-null — it allocates an `oCMsgConversation` of subtype `0x10` (ProcessInfos) and posts it to `self`'s event manager with the hero as the target. There is **no distance/range check of any kind** in this path.

The engine-side handler `oCNpc::EV_ProcessInfos` @ `0x0075a680` confirms the only start-gate the original uses: it resolves the conversation partner, and returns "not yet" (keeps the message queued) **only** while either participant's `zCEventManager` still reports busy (vtable+0x34 on each participant's EM). Once both are idle it calls `oCInformationManager::SetNpc` and begins. Again: **no distance gate**. `oCInformationManager::SetNpc` @ `0x006609f0` then just sets `SetMovLock(1)`, closes inventory, and starts the dialog camera — distance is never consulted to decide whether the dialog opens (a too-far / co-located case is only surfaced later by the camera as the runtime warning "StartDialogCam(): listener and speaker have same position!", not as a refusal to start).

## OpenGothic file:line
`game/world/objects/npc.cpp:2807-2825` — `case AI_ProcessInfo:` inside the AI-action dispatcher.

```
case AI_ProcessInfo: {
  const int PERC_DIST_DIALOG = 2000;
  ...
  if(act.target->qDistTo(*this)>PERC_DIST_DIALOG*PERC_DIST_DIALOG) {
    break;                       // <-- DROPS the action permanently (no pushFront)
    }
  ...
```

Grep-verified symbols: `PERC_DIST_DIALOG` (local const, npc.cpp:2808), `Npc::qDistTo(const Npc&)` (npc.h:133 / npc.cpp:735), `AI_ProcessInfo` (constants.h:`AiAction`), `AiQueue::aiProcessInfo` (aiqueue.cpp:299), push site `gamescript.cpp:2973`.

## Divergence
OpenGothic hard-codes a `2000`-unit (≈20 m) participant-distance gate at the moment `AI_ProcessInfos` is consumed, and when the partner is farther than that it executes `break;` — which **drops** the action (it is not re-pushed, unlike the `owner.isInDialog()` branch just above which does `queue.pushFront`). The original engine performs **no such distance test**: `AI_ProcessInfos` always posts the conversation message, and `EV_ProcessInfos` only waits for both participants' event queues to drain before starting. Consequently a forced/important conversation that a script initiates while the partner is still > 20 m away (e.g. an NPC scripted to hail the hero from across a clearing, or `AI_ProcessInfos` queued before the actor has finished walking up) starts normally in `Gothic2.exe` but is silently swallowed in OpenGothic.

The two other start-gate clauses in the same OG block — `if(this!=act.target && act.target->isPlayer() && act.target->currentInteract!=nullptr) break;` and the `owner.isInDialog()` re-push — also have no counterpart in the original's `EV_ProcessInfos` (which gates only on the two event managers being idle), but they are not the focus here.

## Proposed patch
**DEFERRED.**

Reason: although the absence of any distance gate in the original is verified with high confidence, removing OG's `PERC_DIST_DIALOG` clause is *not* a safe, surgical, build-verifiable parity fix:

1. OG's AI model is structured completely differently from the original's `zCEventManager`/`oCMsgConversation` message pump. The original's actual start-gate ("wait until both participants' event queues are idle") maps in OG to the per-participant `isAiBusy()` / queue state, not to a single distance number. A faithful reimplementation would mean replacing the distance heuristic with a busy/idle wait-and-requeue, which is a behavioral redesign, not a one-liner, and risks reintroducing the soft-lock the `2000` guard was apparently added to prevent (the block already carries no NOTE explaining its provenance).
2. The divergence is only observable in the rare "important dialog forced from > 20 m" case; ordinary perception-driven `B_AssessTalk` always fires well inside 20 m, so a blind removal is high-regression-risk for low, hard-to-validate benefit.
3. Picking a different magic constant or switching `break;`→`pushFront` would be feel-tuning, which the project rules route to DEFERRED.

Recommended follow-up before any change: confirm in-game whether any G2/NotR script calls `AI_ProcessInfos` with the partner > 2000 units away (search Daedalus for scripted hail sequences); only then decide between (a) raising/removing the constant or (b) converting the drop into a bounded requeue.
