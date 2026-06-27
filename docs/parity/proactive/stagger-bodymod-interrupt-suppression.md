# Hit-stagger gate ignores BS_MOD_* modifier flags (burning / drunk / nuts / controlled / transformed / hidden)

**Confidence:** Medium

## Original function + address

`oCNpc::IsBodyStateInterruptable` (Gothic2.exe `0x0075efa0`) is the single gate the
hit-reaction pipeline consults before turning a registered hit into a hard stumble.
It reads the NPC's *raw, composite* bodystate word (the `oCNpc` field at `+0x76C`,
the value carried by `GetBodyState`) and decides:

- if **any** of the modifier bits in the mask `0x3F80` are set, it returns *not
  interruptable* (0); that mask covers bits 7..13 — i.e. `BS_MOD_HIDDEN` (0x80),
  `BS_MOD_DRUNK` (0x100), `BS_MOD_NUTS` (0x200), `BS_MOD_BURNING` (0x400),
  `BS_MOD_CONTROLLED` (0x800), `BS_MOD_TRANSFORMED` (0x1000) and bit 13 (0x2000);
- otherwise it returns interruptable iff the `BS_FLAG_INTERRUPTABLE` bit (0x8000) is set.

In `oCNpc::OnDamage_Anim` (Gothic2.exe `0x00675bd0`) the branch that does the *hard*
stagger — `ClearEM` -> `Interrupt` -> `SetBodyState(BS_STUMBLE=0x15)` -> post the
directional `T_..STUMBLE[B]` transition animation — is taken only when
`IsBodyStateInterruptable()` is non-zero (alongside the not-armed-in-fight and
not-casting gates). So a victim that is on fire, drunk, "nuts", magically controlled,
transformed, or hidden is *never* yanked into the full stumble; it falls through to the
soft `T_GOTHIT` reaction instead. The suppression is driven entirely by the modifier
bits surviving in the composite bodystate word.

## OpenGothic file:line

- `game/world/objects/npc.cpp:2177-2189` — the stagger gate
  (`auto state = bodyStateMasked(); if(... (state&BS_FLAG_INTERRUPTABLE)!=BS_NONE ...)`).
- `game/world/objects/npc.cpp:3468-3471` — `Npc::bodyStateMasked()` returns
  `bs & (BS_MAX | BS_FLAG_MASK)`.
- `game/graphics/mesh/pose.cpp:117-122` — `Pose::bodyState()` aggregates layers as
  `max(b, i.bs & (BS_MAX | BS_FLAG_MASK))`.
- `game/game/constants.h:150-198` — `BS_MOD_MASK` (line 163) omits `BS_MOD_TRANSFORMED`,
  and `BS_MAX_FLAGS`/`bodyStateMasked` never test the modifier bits at all.

## Divergence

OpenGothic strips **all** `BS_MOD_*` modifier bits before the interruptable test — twice:
once in `Pose::bodyState()` (the per-layer `& (BS_MAX | BS_FLAG_MASK)`), and again in
`bodyStateMasked()`. The stagger gate at `npc.cpp:2178` therefore only ever sees the base
state and the `INTERRUPTABLE`/`FREEHANDS` flags. Consequently an NPC whose active
animation carries an interruptable base state *plus* a modifier overlay (e.g. a burning
or controlled creature standing/walking) is treated as interruptable and gets the full
`visual.interrupt()` + `StumbleA/StumbleB` stagger on every registered hit, whereas the
original suppresses the stumble for exactly those modifier states (the `0x3F80` mask).
A secondary, latent inconsistency: OpenGothic's `BS_MOD_MASK` excludes
`BS_MOD_TRANSFORMED` (0x1000), so even the code paths that *do* test modifier bits
(`Pose::hasStateFlag`, `Npc::hasStateFlag`, `BS_MAX_FLAGS`) can never match a transformed
state, while the original's mask includes it (and bit 0x2000).

## Proposed patch

**DEFERRED.**

Reasons:

1. **Not surgical.** The modifier bits are discarded at the lowest level
   (`Pose::bodyState()` masks each layer with `& (BS_MAX | BS_FLAG_MASK)` before the
   `std::max` aggregation), and again in `bodyStateMasked()`. Restoring the original's
   `0x3F80` suppression at the single gate in `npc.cpp:2178` is impossible without a new
   accessor that exposes the raw OR-ed modifier bits from the pose layers; the existing
   `hasStateFlag` API does an *exact* `(i.bs & (BS_FLAG_MASK|BS_MOD_MASK))==flg` compare,
   so it cannot answer "is any BS_MOD_* bit set" for a layer that also carries
   `BS_FLAG_INTERRUPTABLE`. A correct fix is multi-site (Pose accessor + constants:
   add `BS_MOD_TRANSFORMED` to `BS_MOD_MASK`) and touches every `bodyState()` consumer,
   which is beyond a high-confidence surgical change.

2. **Manifestation uncertainty.** Confirming the user-visible effect requires verifying
   that OpenGothic actually populates these modifier bits in pose layers for the same
   situations the original does (the loaded MDS bodystate eventtags). Grep shows the
   `BS_MOD_*` constants are defined but never explicitly set in OpenGothic C++ (they would
   arrive only via animation-script bodystate flags), so the divergence is a definite
   *logic* mismatch but its in-game footprint is unverified.

Recommended follow-up if pursued: add `Pose::bodyStateMods()` returning the OR of
`i.bs & BS_MOD_MASK` across layers, extend `BS_MOD_MASK` to include `BS_MOD_TRANSFORMED`,
and gate the stumble with `(mods & BS_MOD_MASK)==0` to mirror
`IsBodyStateInterruptable`'s `0x3F80` rejection.

<!-- NOTE: in original-game oCNpc::IsBodyStateInterruptable @0x0075efa0 the raw bodystate
     (oCNpc+0x76C) is rejected as non-interruptable when any bit of 0x3F80 (BS_MOD_HIDDEN/
     DRUNK/NUTS/BURNING/CONTROLLED/TRANSFORMED + bit13) is set; oCNpc::OnDamage_Anim
     @0x00675bd0 only hard-stumbles when IsBodyStateInterruptable()!=0. -->
