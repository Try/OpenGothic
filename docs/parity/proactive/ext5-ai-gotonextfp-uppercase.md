# AI_GotoNextFP does not upper-case the free-point name argument

**Confidence:** High

## Original function + address

`AI_GotoNextFP` external handler @ **0x006ec270** (`oGameExternal.cpp`, registered by
`oCGame::DefineExternals_Ulfi` @ 0x006d4780).

Behaviour, described in prose (no decompiled source reproduced):

- The handler pops the single string parameter (the free-point name) via
  `zCParser::GetParameter`, then immediately calls **`zSTRING::Upper`** on it,
  upper-casing the name in place.
- It then resolves the `self` NPC instance and, on success, builds an `oCMsgMovement`
  of sub-type `0xe` (the GotoFP/GotoNextFP movement event) carrying the now-upper-cased
  name, clears the message's "nearest" discriminator field (+0x74 = 0), and posts it to
  the NPC's event manager.

The sibling external `AI_GotoFP` @ **0x006ebfa0** performs the identical
`zSTRING::Upper` on its name argument before posting the same sub-type `0xe` message, so
upper-casing the script-supplied free-point name is the original engine's consistent
contract for both gotos.

## OpenGothic file:line

`game/game/gamescript.cpp:3293-3297` (`GameScript::ai_gotonextfp`).

## Divergence

OpenGothic passes the raw, case-preserving script string straight through:

`ai_gotonextfp` -> `AiQueue::aiGoToNextFp(to)` (stores `act.s0 = to` verbatim,
`game/world/aiqueue.cpp:129`) -> `AI_GoToNextFp` tick
(`game/world/objects/npc.cpp:2572-2577`) -> `World::findNextFreePoint(*this, act.s0)`
(`game/world/world.cpp:931`) -> `WayMatrix::findFreePoint(... name ...)` ->
`WayPoint::checkName(name)` (`game/world/waypoint.cpp:42`).

`WayPoint` stores every way/free-point name **upper-cased** (`upcaseof` in the ctors,
`game/world/waypoint.cpp:9-32`), and `checkName` matches **case-sensitively**
(`name==n`, and the inexact branch `name.find(n)`, both against the unmodified query —
`waypoint.cpp:42-50`). Consequently a script argument that is not already upper-case
(e.g. `AI_GotoNextFP(self, "fp_roam_path")`) never matches any stored free-point: the
exact compare fails and the substring search of an upper-case haystack for a lower-case
needle also fails. The NPC therefore finds no next free-point and silently fails to move,
whereas the original engine — having upper-cased the argument first — matches and moves.

The fix is harmless for already-upper-case names (upper-casing is idempotent), so it
cannot regress the common case; it only repairs the mixed/lower-case inputs the original
tolerated.

## Proposed patch

Grep-verified OG symbols used: `findNpc`, `AiQueue::aiGoToNextFp(std::string_view)`
(`aiqueue.cpp:129`, copies into `std::string act.s0`), `Npc::aiPush`, and the in-file
upper-casing idiom already used by `GameScript::snd_play` / `snd_play3d`
(`gamescript.cpp:3491-3493`, `3505-3506`) with `<cctype>` already included
(`gamescript.cpp:6`).

OLD (`game/game/gamescript.cpp:3293-3297`):

```cpp
void GameScript::ai_gotonextfp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view to) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->aiPush(AiQueue::aiGoToNextFp(to));
  }
```

NEW:

```cpp
void GameScript::ai_gotonextfp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view to) {
  // NOTE: in original-game AI_GotoNextFP (Gothic2.exe oGameExternal.cpp @0x006ec270) the
  // free-point name argument is upper-cased (zSTRING::Upper) before the goto movement
  // message is queued. OpenGothic stores way/free-point names upper-cased and matches them
  // case-sensitively (WayPoint::checkName), so a non-upper-case script name never matches
  // and the NPC silently fails to move. Upper-case here to mirror the original.
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return;
  std::string name {to};
  for(auto& c:name)
    c = char(std::toupper(c));
  npc->aiPush(AiQueue::aiGoToNextFp(name));
  }
```

(`aiGoToNextFp` takes `std::string_view` and copies into `act.s0`, so passing the local
`std::string name` is safe — the copy happens before `name` goes out of scope.)

Note: `GameScript::ai_gotofp` (`gamescript.cpp:3170`) shares the exact same root cause —
the original `AI_GotoFP` @ 0x006ebfa0 also `zSTRING::Upper`s its argument — and would
warrant the identical one-line treatment, but is left out of this surgical change since
the brief targets `AI_GotoNextFP`.

## Externals checked

- **AI_GotoNextFP** — DIVERGENT (this finding): missing upper-case of the FP name.
- **AI_StandUp / AI_StandUpQuick / AI_ContinueRoutine / AI_RemoveWeapon / AI_DrawWeapon /
  Npc_ClearAIQueue / AI_Dodge / AI_AlignToWP** — faithful: argument-less actions that
  only enqueue the corresponding movement/AI event; OpenGothic enqueues the matching
  `AiQueue::ai*` action.
- **AI_TurnAway** — faithful: original posts `oCMsgMovement` sub-type 7 carrying the
  target NPC; OpenGothic's `aiTurnAway(npc)` matches (null target tolerated identically).
- **Npc_ChangeAttribute** — faithful: original `oCNpc::ChangeAttribute` @ 0x0072ff60
  clamps every attribute's negative result to 0 and additionally clamps HP→HPMAX and
  MANA→MANAMAX; OpenGothic's `Npc::changeAttribute` (`npc.cpp:1244`) reproduces all three
  clamps, the godmode/immortal guards, and the `val==0` early-out. (Original also calls
  `CheckModelOverlays` after each change; OpenGothic drives attribute overlays elsewhere —
  out of scope here.)
- **AI_ReadySpell** — POSSIBLE secondary divergence (not patched): original @ 0x006f5690
  posts the `oCMsgMagic` (sub-type 9) unconditionally, with no `manaInvest` guard;
  OpenGothic gates on `mana>0` (`gamescript.cpp:3252`), dropping `AI_ReadySpell(..,0)`
  calls. Lower confidence that scripts rely on a 0/negative invest, so deferred.
- **AI_SetWalkMode** — KNOWN LIMITATION (not patched): original @ 0x006ec800 passes the
  raw mode value (including the 0x80 "weapon drawn" bit) into `oCMsgMovement` sub-type 9
  with no clamp; OpenGothic strips 0x80 and restricts to 0..3 with an explicit
  `//TODO: weapon flags` (`gamescript.cpp:3125`). Needs `WalkBit` weapon-walk support, not
  a drop-in fix — DEFERRED.
- **AI_Teleport** — behavioural nuance (not patched): original @ 0x006de400 calls
  `oCNpc::BeamTo` synchronously when the NPC is idle (no pending EM messages), otherwise
  queues `oCMsgMovement` sub-type 0x10; OpenGothic always queues `aiTeleport`. End state
  (teleport) matches; the immediate-vs-queued timing difference is not a clean surgical
  fix — DEFERRED.
- **Npc_PercEnable / Npc_SetTeleportPos / AI_PlayAniBS body-state** — not fully decompiled
  this pass; no confirmed divergence asserted.
