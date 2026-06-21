# Mdl_ApplyOverlayMdsTimed drops overlays when ticks <= 0

**Confidence:** Medium-High

## Original function + address

The external handler `Mdl_ApplyOverlayMdsTimed` (`FUN_006f9fd0`, the only non-`DefineExternals`
referrer of the `"Mdl_ApplyOverlayMdsTimed"` literal at `0x008b492c`) pops its two parameters
(`timeTicks` as an int, `overlayName` as a string) and, after resolving the NPC, calls
`oCNpc::ApplyTimedOverlayMds(npc, overlayName, (float)timeTicks)` **unconditionally** — the only
guard is the NPC-not-null check. There is no test on the tick value before applying.

`oCNpc::ApplyTimedOverlayMds` (`0x00756890`) applies the model-proto overlay, calls
`oCAniCtrl_Human::InitAnimations`, and pushes an `oCNpcTimedOverlay` node whose remaining-time field
(offset `0x14`) is set to the float `timeTicks` exactly as passed.

`oCNpc::oCNpcTimedOverlay::Process` (`0x0075f4a0`) each frame subtracts the frame delta
(`DAT_0099b3d8`) from that field and removes the overlay only when the result is **strictly less than
zero**. Consequently, an overlay applied with `timeTicks == 0` is still applied — it survives until
the next `Process` (where `0 - dt < 0` evaluates true) and is removed one frame later; a negative
value behaves the same way (applied, then removed on the next frame). In every case the original
applies the overlay at least briefly.

## OpenGothic file:line

`game/game/gamescript.cpp:1939-1945` (`GameScript::mdl_applyoverlaymdstimed`)

```cpp
void GameScript::mdl_applyoverlaymdstimed(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname, int ticks) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && ticks>0) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->addOverlay(skelet,uint64_t(ticks));
    }
  }
```

## Divergence

The `&& ticks>0` guard means that for `ticks == 0` (and any `ticks < 0`) OpenGothic skips the overlay
entirely — it is never applied. The original always applies it (visible for at least one frame, and
the `InitAnimations`/`ApplyModelProtoOverlay` side effects always run). A Daedalus script that calls
`Mdl_ApplyOverlayMdsTimed(self, "...", 0)` therefore sees the overlay take effect in the original but
not in OpenGothic. This is a missing-apply on the `ticks <= 0` edge.

Note `addOverlay(time==0)` in OpenGothic also means a *permanent* overlay (see
`Npc::addOverlay` at `game/world/objects/npc.cpp:804`, which only adds `tickCount()` when
`time!=0`), so naively dropping the guard would convert a `ticks==0` timed overlay into a permanent
one — that is NOT what the original does (original removes it next frame). The fix must preserve the
"effectively immediate, one-frame" semantics, e.g. by clamping a non-positive tick count up to the
minimum positive duration so the overlay is added timed and expires on the next update rather than
being skipped.

## Proposed patch

OLD (`game/game/gamescript.cpp:1939-1945`):
```cpp
void GameScript::mdl_applyoverlaymdstimed(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname, int ticks) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr && ticks>0) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->addOverlay(skelet,uint64_t(ticks));
    }
  }
```

NEW:
```cpp
void GameScript::mdl_applyoverlaymdstimed(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname, int ticks) {
  // NOTE: in original-game Mdl_ApplyOverlayMdsTimed (Gothic2.exe FUN_006f9fd0) the overlay is
  // applied unconditionally; oCNpc::oCNpcTimedOverlay::Process (0x0075f4a0) removes it only once
  // its remaining time goes strictly below zero, so ticks<=0 still applies the overlay for one
  // frame. Clamp non-positive durations to 1 tick so a timed (non-permanent) overlay is added and
  // expires on the next update, instead of being skipped (which dropped the apply entirely) or
  // turned into a permanent overlay (time==0).
  auto npc = findNpc(npcRef);
  if(npc!=nullptr) {
    auto skelet = Resources::loadSkeleton(overlayname);
    npc->addOverlay(skelet,uint64_t(ticks>0 ? ticks : 1));
    }
  }
```

Grep-verified symbols: `GameScript::mdl_applyoverlaymdstimed` (`game/game/gamescript.cpp:1939`),
`Npc::addOverlay(const Skeleton*, uint64_t)` (`game/world/objects/npc.cpp:804`, adds `tickCount()`
only when `time!=0`), `AnimationSolver::update` expiry compares `ov.time!=0 && ov.time<tickCount`
(`game/graphics/mesh/animationsolver.cpp:101`) — a clamped value of `1` becomes
`tickCount()+1`, expiring on the following update, matching the original's one-frame lifetime.
