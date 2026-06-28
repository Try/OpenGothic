# look-at: AI_LookAtNpc target direction — one-shot snapshot vs per-tick re-acquisition

**Confidence:** Medium (genuine, decompiler-confirmed behavioral divergence; but DEFERRED — not amenable to a surgical, high-confidence, clearly-correct fix)

## Original function + address

In `Gothic2.exe` the head look-at target is held on the `oCAniCtrl_Human` instance, not
recomputed every frame:

- `oCNpc::EV_LookAt` (`0x00759a40`) handles the `AI_LookAt` / `AI_LookAtNpc` conversation
  message. When the model exposes a head controller it calls
  `oCAniCtrl_Human::SetLookAtTarget(zCVob*)` (`0x006b6490`) — or the `zVEC3` overload
  `0x006b6360` for a fixed point — **exactly once**, when the message is consumed, then
  returns done. The message is one-shot; it is not re-evaluated each tick.
- `SetLookAtTarget(zCVob*)` (`0x006b6490`) computes the aim point from the target vob's
  bounding box: horizontal **center** of the bbox `((min.x+max.x)/2, (min.z+max.z)/2)` at
  the bbox **top** (`max.y`). It runs `oCNpc::GetAngles`, applies the |yaw| < 0x5A (90°)
  gate, converts the yaw/pitch into normalized head-combine values (yaw·(1/180)+0.5,
  1−(pitch·(1/120)+0.5)) clamped to `[0,1]`, and stores them into the AniCtrl at
  `+0x17c` / `+0x180`. It also stores the (ref-counted) target vob at `+0x130`.
- `oCAniCtrl_Human::LookAtTarget()` (no-arg, `0x006b62f0` → `FUN_006b6170`) is what runs
  every frame from `oCAIHuman::DoAI` (`0x0069bab0` @ `0x0069c282`). It only interpolates
  the model's current head-combine values toward the **stored** `+0x17c`/`+0x180` targets;
  it does **not** re-read `+0x130` or recompute angles. So for a scripted `AI_LookAtNpc`,
  the head settles toward a direction frozen at the instant the command was issued.
  (The player's own head is the exception: `oCAIHuman::CheckFocusVob` @ `0x0069b7a0`
  re-calls `SetLookAtTarget` every frame against the current focus vob, so the *player*
  head does track a moving focus — but routine/scripted NPCs do not.)

## OG file:line

`game/world/objects/npc.cpp`
- `Npc::implLookAtNpc` (1419-1426): every call recomputes `dvec = otherHead - selfHead`
  from the target's **live** `visual.mapHeadBone()`.
- `Npc::tick` (2555-2557): `implLookAtNpc(dt)` / `implLookAtWp(dt)` are invoked every AI
  tick while the NPC is not down, for as long as `currentLookAtNpc` is set (it persists
  from `AI_LookAtNpc` at 2595-2598 until `AI_StopLookAt` at 2683-2686).
- `Npc::implLookAt` (1428-1466): direct head-bone Euler rotation, `maxRot=80`, pitch
  clamped `±20`.

## Divergence

OpenGothic **re-acquires the look-at direction every tick** from the target NPC's current
head-bone position, so an NPC under a scripted `AI_LookAtNpc` continuously tracks a moving
target. The original computes the look-at direction **once** at command time and then only
eases the head toward that frozen direction; a routine NPC's head does not follow a target
that walks away after the command. Secondary aim-point difference: original aims at the
**top-center of the target's bounding box**, OG aims at the **head bone** (horizontal angle
is effectively identical; vertical differs by roughly head height).

## Proposed patch

**DEFERRED.** Reasons:
1. OG's look-at is a deliberate simplified reimplementation (per-frame direct head-bone
   Euler rotation) of a fundamentally different original mechanism (a ref-counted-vob head
   *combine-animation* controller whose normalized blend values are snapshotted once). The
   numeric constants (180/120 deg combine scale, INTERPOLATE step, 90° gate) are not 1:1
   comparable to OG's `maxRot=80` / `±20` pitch, so "matching" is ambiguous.
2. The one-shot-vs-continuous difference is low-observability (dialog participants are
   stationary; the moving-target case is rare) and OG's continuous tracking is arguably the
   more natural behavior; freezing the direction at command time could look worse and risks
   regressing dialog/turn head-tracking. No clearly-correct, surgical, build-verifiable edit
   exists.

```
// NOTE: in original-game oCNpc::EV_LookAt @0x00759a40 + oCAniCtrl_Human::SetLookAtTarget
// @0x006b6490, the head look-at direction is snapshotted once (stored at AniCtrl+0x17c/
// +0x180) and only eased toward each frame by LookAtTarget() @0x006b62f0; OG recomputes
// the target direction every tick in implLookAtNpc (npc.cpp:1419) from the live head bone.
```
