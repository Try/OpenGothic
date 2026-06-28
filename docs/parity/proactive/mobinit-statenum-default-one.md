# oCMobInter default `stateNum` (max-state bound) is 1, not 0

**Confidence:** Medium (init default fully verified in the binary; **fix DEFERRED** because a naive
header change regresses pure `oCMOB`).

## Original fn + address (prose)
`oCMobInter::oCMobInter` (Gothic2.exe @0x0071d010) initialises the member at `+0x1f8` to the literal
constant **1**. That member is the mob's state-count / maximum-state bound: `oCMobInter::GetStateNum`
(@0x00718c90) is a one-liner `return *(int*)(this+0x1f8);`, and the state machine uses it as the
**inclusive upper bound** of a valid state. In `oCMobInter::StartStateChange` (@0x0071fea0) the
forward/backward direction is only chosen when `-1 <= from <= (+0x1f8)` and `-1 <= to <= (+0x1f8)`,
and in `oCMobInter::Interact` (@0x0071f210) the autonomous forward step is gated on
`0 < state && state < (+0x1f8)`. The separate "current/visual" state lives at `+0x1f4`
(`GetState`, default 0) and the "target" state at `+0x1fc` (default 0); these are distinct fields
from the count at `+0x1f8`. So the engine's freshly-constructed mob defaults to a one-state machine,
i.e. `stateNum == 1`.

## OG file:line
`/Users/admin/Downloads/opengothic/game/world/objects/interactive.cpp:44`
(`stateNum = inter.state;`) and the member declaration
`/Users/admin/Downloads/opengothic/game/world/objects/interactive.h:164` (`int stateNum=0;`).

## Divergence
OpenGothic declares `int stateNum=0;` (header) and only overwrites it from the zen for non-`oCMOB`
types (ctor line 41-50) or from `stepsCount` for ladders (line 99). The original's in-memory default
for that field is **1**, not 0. For every real `oCMobInter`/`oCMobContainer`/`oCMobDoor`/… vob the
field is replaced by the zen "stateNum" property (`inter.state`) on spawn and by the savegame on
load, so the defaults coincide in practice. The divergence is only observable for a vob whose
`stateNum` property is absent, and for a **pure `oCMOB`** base vob (`vob.type == oCMOB`), which OG
leaves at 0 while the original engine has no such field at all.

## Proposed patch — DEFERRED
Changing the OG default to 1 is **not** a safe surgical fix:

- A bare `int stateNum=1;` would give pure `oCMOB` decoration vobs (which OG also routes through
  `Interactive`, but which have no state animations) a phantom second state, so `implTick`/
  `isTrueDoor`/`stateMask` would treat them as a 0→1 state machine — a regression the original
  avoids because pure `oCMOB` simply has no `stateNum` member.
- Conditioning the default on vob type inside the ctor reproduces the engine value but adds branching
  with no demonstrable behavioural payoff, since every interactive vob already receives its real
  `stateNum` from `inter.state`/savegame before any tick runs.

```
// NOTE: in original-game oCMobInter::oCMobInter (Gothic2.exe @0x0071d010) the state-count member
// (+0x1f8, read by GetStateNum @0x00718c90, used as the inclusive max-state bound in
// StartStateChange @0x0071fea0 / Interact @0x0071f210) is constructed to 1, not 0. OG leaves
// stateNum=0 for pure oCMOB; do NOT blanket-change the default to 1, as that gives stateless
// oCMOB decoration vobs a phantom 0->1 transition. Any fix must keep pure-oCMOB at 0.
```

No edit applied: the only behaviour-faithful change is gated on vob type and yields no observable
parity gain for shipping worlds, so per "empty beats false positives" this is recorded, not patched.
