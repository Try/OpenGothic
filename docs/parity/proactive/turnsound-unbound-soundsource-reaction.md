# Sound-source reaction (AI_TurnToSound / AI_WhirlAroundToSource / Snd_GetDistToSource) is unbound and untracked

**Confidence:** Medium-High (the unbound state and missing source-position tracking are grep-certain; the script-side trigger is reconstructed from the engine plumbing rather than read out of GOTHIC.DAT). **Fix status: DEFERRED (non-surgical).**

## Original functions + addresses (prose only)

In `Gothic2.exe` an NPC stores a per-instance **"last heard sound source position"** at instance offset `+0x9f4 / +0x9f8 / +0x9fc`. The static helper `oCNpc::CreateSoundPerception` (0x0075bb70) is the writer: it takes a perception type, an emitter vob and a world position, looks up the audible radius for that perception type in the global `percRange[]` array, builds a bounding box of that radius around the sound position, collects every vob inside it, and for each receiving `oCNpc` that has the perception enabled it (a) writes the sound position into that NPC's `+0x9f4..0x9fc` fields and (b) starts the perception's AI state. Footsteps reach this through `oCAIHuman::CreateFootStepSound` (0x0069b180) -> `oCNpc::AssessQuietSound_S` (0x0075c4d0), which calls `CreateSoundPerception(0xE /*PERC_ASSESSQUIETSOUND*/, emitter, pos, 0, 0)`.

Three script externals then *consume* that stored `+0x9f4` position:
- `AI_TurnToSound` (external thunk 0x006f04e0) — reads `self+0x9f4..0x9fc` and posts an `oCMsgMovement` of subtype 5 (turn) toward that position; i.e. the NPC turns to face the noise.
- `AI_WhirlAroundToSource` (external thunk 0x006f01d0) — identical but posts subtype 10 (whirl) toward `self+0x9f4`.
- `Snd_GetDistToSource` (external thunk 0x006f91c0) — returns `(int)oCNpc::GetDistToSound(self)` (0x0075d500), the distance from the NPC to its stored `+0x9f4` sound source.

(`AI_WhirlAround` 0x006f0030 takes `self,other` and whirls toward the *other vob*, not the sound source — that one is correctly bound in OG.)

## OpenGothic file:line

- `game/game/gamescript.cpp:248-302` — full `ai_*` external bind list. `ai_whirlaround` is bound (line 261, -> `aiWhirlToNpc`); **`ai_turntosound`, `ai_whirlaroundtosource`, `snd_getdisttosource` are absent** (grep over `game/` returns no hits for any of them).
- `game/world/worldobjects.cpp:930-976` — `sendImmediatePerc` / `passivePerceptionProcess`: the quiet-sound message carries `msg.pos = self.position()` but the position is **never stored on the receiving NPC**; only `perceptionProcess(...)` is invoked.
- `game/world/objects/npc.cpp:2424` — footstep trigger: `sendImmediatePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND)` (player, non-sneak only).
- `game/world/objects/npc.cpp:3020` — confirms `B_AssessQuietSound` actually runs as an AI state in OG.
- `game/world/objects/npc.h` — no `soundPos`/`soundSource` field exists (grep for `soundsource|lastnoise|noisepos` over `game/` is empty).

## Divergence

In vanilla G2 an NPC that hears a quiet sound enters its `PERC_ASSESSQUIETSOUND` state (`B_AssessQuietSound`), whose body turns the guard to face the noise via `AI_TurnToSound(self)` (and damage/whirl reactions use `AI_WhirlAroundToSource`; threat-assessment scripts read `Snd_GetDistToSource`). The directional turn works because the engine deposited the noise position in `self+0x9f4` when it created the perception.

OpenGothic reproduces the *perception delivery* (the state starts) but:
1. never records the noise position on the hearing NPC (`worldobjects.cpp:930` discards `msg.pos` after dispatch), and
2. leaves `ai_turntosound`, `ai_whirlaroundtosource` and `snd_getdisttosource` unbound.

Net effect: guards in OG enter the assess-quiet-sound / damage-whirl states but never physically turn toward the noise/damage source, and any script branch gated on `Snd_GetDistToSource` evaluates against the external's default return (0 = "source is right next to me") instead of the true distance.

## Proposed patch — DEFERRED

No surgical, build-verifiable fix exists. A faithful bind requires four coordinated pieces of new infrastructure, not a one-line change:
1. a new per-`Npc` field to mirror `oCNpc+0x9f4` (a `Tempest::Vec3 soundSrc`);
2. writing it from `WorldObjects::passivePerceptionProcess` for the sound perceptions (`PERC_ASSESSQUIETSOUND` / `PERC_ASSESSFIGHTSOUND`) using `msg.pos`;
3. a new `AiQueue::AiAction` + `npc.cpp` dispatch arm that turns toward a stored position (OG already has the turn primitive `Npc::implTurnTo(float dx,float dz,...)` at `npc.h:497`, so the motion half is available); and
4. binding `ai_turntosound` / `ai_whirlaroundtosource` (-> that action) and `snd_getdisttosource` (-> distance to the stored position) in `gamescript.cpp`.

Per the clean-room rule ("surgical high-confidence build-verifiable fix ONLY, else DEFERRED"), and the project guidance that an unbound VOID `ai_*` action is benign absent a surgical bind, this is recorded as DEFERRED. The non-void `Snd_GetDistToSource` is the most defensible single piece to bind later, but it too needs the stored-position field (1)+(2) to return anything meaningful, so it cannot land in isolation either.

<!-- NOTE: in original-game oCNpc::CreateSoundPerception @0x0075bb70 writes the noise
     position into each hearing NPC at +0x9f4; AI_TurnToSound @0x006f04e0,
     AI_WhirlAroundToSource @0x006f01d0 and Snd_GetDistToSource @0x006f91c0
     (via oCNpc::GetDistToSound @0x0075d500) read it back. -->
