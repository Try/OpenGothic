# AI_Output / AI_OutputSVM(_Overlay): missing ZS_TALK cross-conversation skip gate

**Confidence:** Medium (divergence is decompile-certain; real-world trigger frequency is moderate; clean fix blocked by a missing OG primitive → DEFERRED)

## Original function + address

- `oCNpc::EV_Output` @ `0x007576f0`
- `oCNpc::EV_OutputSVM` @ `0x007571f0`
- `oCNpc::EV_OutputSVM_Overlay` @ `0x00756a60`

All three output-message handlers share an identical guard that runs *before* the
output unit is created (before `StartTalkingWith` / `oCNpc_States::StartOutputUnit`).
Reading the speaker as `this` and the addressed NPC (the message's conversation
target, field `+0x6c`) as `target`, the guard is:

- If `target` exists **and** `target` is already *talking with* a third NPC
  (`GetTalkingWith(target) != 0`) that is **not** the speaker
  (`GetTalkingWith(target) != this`), then:
  - look up the `ZS_TALK` state index via `zCParser::GetIndex`;
  - if the speaker is itself currently talking with someone
    (`GetTalkingWith(this) != 0`) **and** either the speaker **or** the speaker's
    own talk-partner is in state `ZS_TALK`
    (`oCNpc_States::IsInState(..., ZS_TALK)`), then the function **silently aborts
    the output** — it returns `1` (treated as "done") without creating the output
    unit, without playing voice, and without showing the subtitle.
- In every other case it falls through to the normal path that calls
  `StartTalkingWith(this, target)` + `StartOutputUnit(...)`.

Net effect: the engine refuses to let conversation A's spoken line "barge into"
NPC B while B is busy inside a *different* active conversation (and a `ZS_TALK`
state is in progress on the speaker's side). The line is dropped, not deferred.

(Separately confirmed in the same dumps: `EV_OutputSVM_Overlay` @0x00756a60 also
calls `StartTalkingWith(this, target)` on first-time setup — i.e. even an overlay
SVM establishes a talking-with relationship in the original. OG's overlay path is
fire-and-forget and never registers a partner. This is the same missing-primitive
root cause and is noted here for completeness.)

## OpenGothic file:line

- `game/world/objects/npc.cpp:424-444` — `Npc::performOutput()` (the OG gate for all
  three output actions). It checks output ordering (`aiOutputOrderId`), the per-NPC
  self-talk barrier (`aiOutputBarrier` only when `act.target==this`), and an
  `aiPolicy>=AiFar` CPU early-out. It does **not** consult any "is the target
  already in another conversation" condition.
- `game/world/objects/npc.cpp:2901-2923` — `AI_Output` / `AI_OutputSvm` /
  `AI_OutputSvmOverlay` dispatch.

## Divergence

In the original, when NPC A is scripted to `AI_Output`/`AI_OutputSVM`(`_Overlay`)
at NPC B while B is already locked into a separate dialog/talk with NPC C and a
`ZS_TALK` is running on A's side, A's line is **dropped**. OpenGothic has no
talking-with registry, so `performOutput` always proceeds: out of dialog it routes
through `GameScript::GlobalOutput` (`gamescript.cpp:53-63`) which unconditionally
plays the WAV and returns true. Result: overlapping/cross-talking voice lines in
crowded ambient scenes (taverns/camps) where the original would suppress them.

## Proposed patch

**DEFERRED.**

Reason: a faithful reimplementation needs the original's *talking-with* relation —
`oCNpc::GetTalkingWith` / `StartTalkingWith` / `StopTalkingWith` — which is the
dialog-partner registry the output units and `EV_StopProcessInfos`
(@0x0075a660, which calls `StopTalkingWith`) maintain. OpenGothic has no equivalent:
the closest field, `Npc::currentOther` (`npc.cpp:626`, set via `setOther`), is the
*perception* "other" and is written by combat/perception paths
(`npc.cpp:594,2106,2135,4485`), not a symmetric "these two NPCs are conversing"
flag. Grep confirms no `GetTalkingWith`/`talkingWith` symbol exists in
`game/`. `Npc::isInState(ZS_Talk)` (`gamescript.cpp:1348-1350`) is available, but
the `GetTalkingWith(target)!=this` and `GetTalkingWith(this)!=0` predicates cannot
be evaluated without first introducing a talking-with tracker. Approximating the
gate with `currentOther` would mis-fire (combat targets, last-perceived NPCs) and
risk *swallowing legitimate dialog lines* — violating "empty beats false
positives." A correct fix is a separate, larger change: add a dedicated
talking-with partner field set in the `AI_ProcessInfo`/output paths
(`npc.cpp:2949-2957`) and cleared in `AI_StopProcessInfo` (`npc.cpp:2959-2966`),
then port the `ZS_TALK` guard into `performOutput`.

```
// NOTE: in original-game oCNpc::EV_Output @0x007576f0 (and EV_OutputSVM @0x007571f0,
// EV_OutputSVM_Overlay @0x00756a60) the output is silently dropped when the addressed
// NPC is already GetTalkingWith() a third NPC and the speaker (or its talk-partner)
// is in ZS_TALK. OpenGothic has no talking-with registry, so performOutput proceeds
// and cross-conversation lines overlap. Deferred: needs a talking-with tracker first.
```
