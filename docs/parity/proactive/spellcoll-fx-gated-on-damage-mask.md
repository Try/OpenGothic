# Spell collision burst (emFXCollDyn) suppressed on COLL_DONOTHING / non-targetable NPCs

**Confidence:** High on the divergence (decompiler-confirmed); Low on a one-line surgical fix → **patch DEFERRED**.

## Original fn + address

`oCVisualFX::ProcessCollision` (Gothic2.exe @ `0x004958d0`) is the dynamic-collision dispatch
for a flying spell FX hitting an NPC / vob. Read top-to-bottom, its ordering is decisive:

1. Near the start (label `~0x00495ad9`) it unconditionally spawns the **dynamic collision burst**
   via `CreateAndPlay((zSTRING*)(this+0x278), targetVob, originVob, …)`. `this+0x278` is the
   `emFXCollDyn` sub-FX name. This happens **before** any targetability / damage decision — the
   only earlier `return` is the self-hit guard (`this+0x4a8 == target`).
2. Only afterward does it resolve the script gate: it looks up `C_CanNpcCollideWithSpell`, calls
   it (`zCParser::CallFunc … 0xab40c0`), and if the returned mask is `0` (COLL_DONOTHING) it does
   `goto LAB_004962a4`, which **skips both** `ApplyDamages`/the `oCMsgDamage` post **and** the
   second, mask-gated FX `CreateAndCastFX(this, this+0x28c=emFXCollDynPerc, …)` at `LAB_00496153`.

So vanilla's rule is: **the `emFXCollDyn` impact burst always spawns on any NPC/vob collision;
the `emFXCollDynPerc` sub-FX and the damage application are gated on the collide mask.**
(In G1 the gate is the spell's `target_collect_type` targetability instead of a script, but the
same CreateAndPlay-before-gate ordering holds.)

## OG file:line

`game/world/objects/npc.cpp:2179-2182` — `Npc::takeDamage(Npc&, const Bullet*, const VisualFx*, int32_t)`:

```
CollideMask bMask = owner.script().canNpcCollideWithSpell(*this,&other,splId);
if(bMask!=COLL_DONOTHING)
  Effect::onCollide(owner,vfx,position(),this,&other,splId);
takeDamage(other,b,bMask,splId,true);
```

`Effect::onCollide` (`game/graphics/effect.cpp:309-344`) bundles three things into one all-or-nothing
call: the main `emFXCollDyn`/`emFXCollStat` burst spawn (326-336), the `emFXCollDynPerc` spawn
(338-343), and the `PERC_ASSESSMAGIC` perception (321-324).

## Divergence

Because the whole `Effect::onCollide` call is wrapped in `if(bMask!=COLL_DONOTHING)`, a spell that
hits an NPC for which `C_CanNpcCollideWithSpell` returns `0` (G2 immunity / summon-vs-master /
script "no collide"), or a G1 spell hitting an NPC outside its `target_collect_type` (e.g. an
undead-only spell striking a human), produces **no `emFXCollDyn` collision burst at all** — the
spell visually fizzles with nothing. Vanilla `ProcessCollision` always spawns that burst (step 1
above) and only suppresses the *perc* sub-FX and the damage (step 2). This is the same world-vs-NPC
asymmetry family as the already-fixed cases: the world-impact path
(`game/world/bullet.cpp:148`) calls `Effect::onCollide(…, npc=nullptr, …)` **unconditionally**, so
a spell hitting geometry shows its burst, but a spell hitting a non-targetable NPC does not. It is
**distinct from** the excluded `fxkey-collide world path` fix (that was the `SpellFxKey::Collide`
key on the dying bullet; this is the `emFXCollDyn` impact-burst *spawn* gate).

## Proposed patch — DEFERRED

A correct fix is **not a safe one-liner**, so it is deferred:

- Naively dropping the `if(bMask!=COLL_DONOTHING)` guard (always calling `Effect::onCollide`) would
  *also* ungate the `emFXCollDynPerc` spawn (effect.cpp:338-343) and the `PERC_ASSESSMAGIC`
  broadcast (effect.cpp:321-324), both of which vanilla *does* gate behind the same COLL_DONOTHING
  early-return (`goto LAB_004962a4` skips `CreateAndCastFX(this+0x28c)`). That trades one missing
  effect for two over-fired ones — a net parity loss.
- The faithful fix is to **split** `Effect::onCollide` so the primary `emFXCollDyn`/`emFXCollStat`
  burst (326-336) spawns unconditionally, while `emFXCollDynPerc` (and, pending confirmation of where
  vanilla raises the collision `PERC_ASSESSMAGIC`, the assess-magic perception) stay gated on the
  mask. That means plumbing the `CollideMask` into `Effect::onCollide` and gating internally, then
  calling it unconditionally from `npc.cpp:2180`. The world caller (`bullet.cpp:148`, `npc==nullptr`)
  already skips the perc/assess-magic branches, so it is unaffected.

Suggested NOTE to carry with the eventual fix:

```
// NOTE: in original-game oCVisualFX::ProcessCollision @0x004958d0 the emFXCollDyn impact burst
// (CreateAndPlay this+0x278) is spawned unconditionally, BEFORE C_CanNpcCollideWithSpell; only the
// emFXCollDynPerc sub-FX (CreateAndCastFX this+0x28c @0x00496153) and ApplyDamages are skipped when
// the mask is COLL_DONOTHING (goto @0x004962a4). OpenGothic wrapped the whole Effect::onCollide in
// `if(bMask!=COLL_DONOTHING)`, so a spell hitting an immune / non-targetable NPC showed no impact
// burst at all (a spell hitting world geometry, bullet.cpp, still does).
```
