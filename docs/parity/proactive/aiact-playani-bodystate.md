# AI_PlayAni does not force BS_STAND body-state

**Confidence:** High (on the divergence); Medium-High (on the proposed one-line fix)

## Original function + address
- `oCNpc::EV_PlayAni` @ `0x00757ab0` — the shared handler for both the
  `AI_PlayAni` and `AI_PlayAniBS` script externals (both build an
  `oCMsgConversation` with subtype `EV_PLAYANI` = `0xe`). After resolving and
  starting the named model animation, when the NPC is *not* bound to a MOBSI it
  applies the message's body-state to the NPC: `SetBodyState(npc, msgBodyState & 0x7f)`
  (the MOBSI branch instead defers to `oCMobInter::SetMobBodyState`).
- `AI_PlayAni` external @ `0x006ea780` (`DefineExternals_Ulfi`): pops only the
  animation name and stores **body-state 0** into the message's body-state field
  (offset `+0x8c`).
- `AI_PlayAniBS` external @ `0x006ea910`: pops name + body-state and stores the
  caller-supplied body-state into the same `+0x8c` field.

Net effect in the original engine: `AI_PlayAni(self, ani)` is exactly
`AI_PlayAniBS(self, ani, BS_STAND)` — it always drives the NPC's body-state to
`BS_STAND` (index `0`) while the animation plays. The two externals share one
event handler; the only difference is the body-state value carried in the
message (`0` vs the explicit argument).

## OpenGothic file:line
- `game/world/objects/npc.cpp:2683` — `case AI_PlayAnim:` calls
  `playAnimByName(act.s0, BS_NONE)`.
- `game/world/objects/npc.cpp:2694` — `case AI_PlayAnimBs:` calls
  `playAnimByName(act.s0, BodyState(act.i0))`.

## Divergence
OpenGothic's `AI_PlayAnimBs` correctly forwards the requested body-state, but
`AI_PlayAnim` passes `BS_NONE` (`= 0`, "leave body-state unasserted") instead of
`BS_STAND`. The played layer therefore contributes `BS_NONE` to
`Pose::bodyState()` (which is a `max()` over layer body-states, see
`game/graphics/mesh/pose.cpp:117`), so a plain `AI_PlayAni` animation does not
raise the NPC to `BS_STAND` and, in particular, does not assert the
`BS_FLAG_INTERRUPTABLE` / `BS_FLAG_FREEHANDS` flags that `BS_STAND` carries
(`game/game/constants.h:162`). The original engine always forces `BS_STAND` for
`AI_PlayAni`. Gates that read `bodyStateMasked()`/`hasStateFlag()` (interrupt
eligibility, free-hands checks, `== BS_STAND` tests) can therefore observe a
different body-state than the original while an `AI_PlayAni` animation is active.

This makes OpenGothic's `AI_PlayAni` differ from `AI_PlayAniBS(ani, BS_STAND)`,
whereas in the original they are identical.

## Proposed patch
`game/world/objects/npc.cpp`, `case AI_PlayAnim:` (grep-verified symbols:
`playAnimByName` declared at `npc.cpp:997`, `BS_STAND` at `constants.h:162`):

OLD:
```cpp
    case AI_PlayAnim:{
      owner.script().eventPlayAni(*this, act.s0);
      if(auto sq = playAnimByName(act.s0,BS_NONE)) {
```
NEW:
```cpp
    case AI_PlayAnim:{
      // NOTE: in original-game oCNpc::EV_PlayAni @0x00757ab0 the AI_PlayAni external
      // (@0x006ea780) carries body-state 0 in its EV_PLAYANI message, and the handler
      // applies SetBodyState(msg.bodyState & 0x7f), i.e. AI_PlayAni == AI_PlayAniBS(ani,BS_STAND).
      // OpenGothic passed BS_NONE, so AI_PlayAni failed to assert BS_STAND (and its
      // FREEHANDS/INTERRUPTABLE flags) on the playing layer.
      owner.script().eventPlayAni(*this, act.s0);
      if(auto sq = playAnimByName(act.s0,BS_STAND)) {
```

### Verification caveat
In OpenGothic the body-state argument is also consumed by `Pose::startAnim`
transition selection (`pose.cpp:195-203`, the `bs==BS_STAND` →
`T_<prev>_2_STAND` branch). `AI_PlayAniBS(ani, BS_STAND)` already exercises that
path, so making `AI_PlayAni` pass `BS_STAND` keeps the two externals consistent
exactly as the original does; but it can newly select a `T_..._2_STAND`
transition where the previous `BS_NONE` value did not. A reviewer should confirm
no in-game routine regresses because of an added stand transition. If that risk
is judged unacceptable, treat as **DEFERRED**: root cause is confirmed
(missing `BS_STAND` assertion), but a fully decoupled fix would require setting
the NPC body-state independently of the animation-layer `bs` argument, which the
current `playAnimByName` API couples together.
