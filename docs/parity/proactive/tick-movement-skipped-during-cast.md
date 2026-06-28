# Tick parity: per-frame movement/physics skipped during spell cast/invest

**Confidence:** Medium (strong structural evidence inside OpenGothic; original side inferred from tick order, not from a byte-exact decompile of the merged tick body).

## Original fn + address
The per-frame oCNpc tick body (the unnamed caller that invokes `oCNpc::UpdateNextVoice`
@0x0073e3c0 at 0x0073e4ca and `oCNpc::Regenerate` @0x00741fd0 at 0x0073e4d1; Ghidra folds
this body into the `UpdateNextVoice` listing range 0x0073e3c0..0x0073e8d0, so it has no
separate symbol) runs in the order: UpdateNextVoice -> Regenerate -> virtual AI/event
dispatch -> movement/physics. The AI dispatch for a channeling caster is
`oCAIHuman::MagicMode` @0x00472fd0. Crucially the movement/physics advance at the tail of the
tick runs **every frame, regardless of AI state** — a casting NPC is still subject to gravity,
slope-slide, swim/dive buoyancy and mover-platform carry, exactly like a waiting or idle NPC.

## OG file:line
`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2567-2568` (the
`if(tickCast(dt)) return;` early-return), against the movement calls that every *other* tick
branch performs:
- wait branch: `mvAlgo.tick(dt,MoveAlgo::WaitMove)` at npc.cpp:2574
- attack branch: `implAttack` ticks `mvAlgo` (FaiMove)
- goto branch: `implGoTo` ticks `mvAlgo`
- normal branch: `mvAlgo.tick(dt)` at npc.cpp:2592

## Divergence
`if(tickCast(dt)) return;` returns from `Npc::tick` *before* `mvAlgo.tick(dt)` whenever a cast
is in progress (most persistently during the invest/channel loop, where `tickCast` returns
true every frame while waiting for `castNextTime`). The cast branch is the **only** exit path
in `Npc::tick` that returns without ticking `mvAlgo` — wait, attack, goto and the normal path
all advance movement. The original runs its movement/physics advance unconditionally after the
AI dispatch, so a caster there keeps falling/sliding/swimming and keeps being carried by a
moving platform; the OpenGothic caster is frozen in space for the whole cast/invest. (Pose and
spell-cast animation still advance, since they are driven separately by
`MdlVisual::updateAnimation` on the render path, so the cast itself is unaffected — only
world-space physics is.) This is a residual instance of the same early-return-before-the-
unconditional-work pattern as the just-applied regen-before-cast move.

## Proposed patch
Mirror the wait branch: advance physics in the same passive ("wait") mode before bailing out
of the cast tick.

OLD (npc.cpp:2567-2568):
```cpp
  if(tickCast(dt))
    return;
```

NEW:
```cpp
  if(tickCast(dt)) {
    // NOTE: in original-game the per-frame oCNpc tick (caller @0x0073e4d1, the body that calls
    // UpdateNextVoice @0x0073e3c0 and Regenerate @0x00741fd0) runs its movement/physics advance
    // unconditionally after the virtual AI dispatch (the caster's handler being
    // oCAIHuman::MagicMode @0x00472fd0) -- so a channeling/investing caster is still subject to
    // gravity, slope-slide, swim/dive and mover-platform carry every frame. OpenGothic's cast
    // early-return was the only tick branch that returned without ticking mvAlgo (wait/attack/
    // goto/normal all do), freezing a caster in world space (e.g. not carried by a moving
    // platform, not falling) for the whole cast. Tick passive movement like the wait branch.
    mvAlgo.tick(dt,MoveAlgo::WaitMove);
    return;
    }
```

Symbols verified to exist: `Npc::tickCast` (npc.cpp:4266), `MoveAlgo::tick` /
`MoveAlgo::WaitMove` (movealgo.h:34,58), `mvAlgo` member, already used identically at
npc.cpp:2574.

### Caveat / why not higher confidence
The exact original tick body is folded by Ghidra into the `UpdateNextVoice` range and has no
clean decompile, so the "movement runs after AI dispatch every frame" claim is inferred from
the documented tick order and from general ZenGin physics behavior, not byte-confirmed for the
cast case specifically. If a reviewer prefers byte-exact confirmation before touching the cast
path, treat this as DEFERRED pending a disassembly of 0x0073e4d1..0x0073e8d0; the structural
asymmetry (cast is the sole tick exit not ticking mvAlgo) is the load-bearing evidence.
