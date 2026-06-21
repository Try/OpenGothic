# Hit-reaction skipped while player dialog (EV_PROCESSINFOS) is queued

**Confidence:** Medium-high on the original behavior; **DEFERRED** on the fix (architectural mismatch).

## Original function + address (prose only)

`oCNpc::OnDamage` @ `0x006660e0` is the orchestrator that, after computing the damage
descriptor, runs the hit-reaction sub-stages in order: `OnDamage_Hit` (hit-chance / HP
application), `OnDamage_Condition`, then — gated on descriptor flag bit 0
(COLL_APPLYVICTIMSTATE) — `OnDamage_Anim` (the stagger/stumble/warn/fly animation
selector at `0x00675bd0`), `OnDamage_Effects_Start`, `OnDamage_Script`, the
bodystate-0x400 interrupt-override clear, then `OnDamage_Events` and `OnDamage_Sound`.

Before any of that runs, `OnDamage` performs an early-out guard:

- It first checks `IsConditionValid(this)`.
- Then, **only if `this->IsAPlayer()` is true OR the attacker `IsAPlayer()` is true**
  (the virtual at vtable offset `+0x104`, confirmed `oCNpc::IsAPlayer` @ `0x007425a0`,
  which returns `this == player`),
- it iterates the victim's own `zCEventManager` message queue and, for every
  `oCMsgConversation` in it, reads the message subtype field (`+0x24`). If any queued
  conversation message has subtype in the half-open range `(0x0f, 0x12)` — i.e. subtype
  `0x10` = `EV_PROCESSINFOS` or `0x11` = `EV_STOPPROCESSINFOS` (subtype names confirmed
  via `oCMsgConversation::MD_GetSubTypeString` @ `0x0076ab60`) — it sets descriptor flag
  bit 1 and **returns immediately**.

Effect: when the player is a participant in the hit (as victim or as attacker) and a
dialog is being processed/torn-down (an `EV_PROCESSINFOS` / `EV_STOPPROCESSINFOS` message
is still pending in the victim's EM queue), the **entire** hit reaction is suppressed —
no hit-chance roll, no HP change (`OnDamage_Hit`), no stagger/stumble/warn animation
(`OnDamage_Anim`), no PERC_ASSESSDAMAGE script call (`OnDamage_Script`), and no aargh
sound. This is the original engine's guard against the player being damaged or staggered
during the info-processing transition of a dialog.

## OpenGothic file:line

`game/world/objects/npc.cpp:2084` — `Npc::takeDamage(Npc&, const Bullet*, CollideMask, int32_t, bool)`.

This function (and its callers `takeDamage(...)` overloads at lines 2041/2062/2069) is the
OG counterpart of `oCNpc::OnDamage`. It runs `perceptionProcess(...,PERC_ASSESSDAMAGE)`,
`fghAlgo.onTakeHit()`, `DamageCalculator::damageValue(...)`, the stumble/interrupt block
(2121-2135), the FLY throw (2138-2140), and HP application + ASSESSOTHERSDAMAGE perc
(2142-2160). There is **no** check anywhere on this path for an in-progress / pending
`EV_PROCESSINFOS`-style dialog message involving the player.

## Divergence

The original drops all combat feedback and damage for a hit that involves the player
while a dialog info block is being processed (`EV_PROCESSINFOS` / `EV_STOPPROCESSINFOS`
queued). OpenGothic always applies the full hit reaction (damage, stagger, sound),
because `Npc::takeDamage` has no equivalent dialog-processing guard. Observable
difference: an attack landing on (or by) the player during the brief
info-processing/teardown window of a dialog would, in the original, be a no-op, but in
OpenGothic deals damage and can stagger the player out of the dialog flow.

## Proposed patch

**DEFERRED.**

Reasons:

1. **Architectural mismatch.** The original's guard is expressed in terms of the
   `oCMsgConversation` objects sitting in a Vob's `zCEventManager` queue with concrete
   subtypes `EV_PROCESSINFOS`/`EV_STOPPROCESSINFOS`. OpenGothic does not model dialog as
   `oCMsgConversation` messages on a per-Npc EM queue; conversation/info flow goes through
   `AiQueue` / `outputPipe` (`game/world/objects/npc.cpp:182`, `aiOutputOrderId()` 418-425)
   and the higher-level dialog/`GameScript` info machinery. There is no
   grep-verifiable OG field equivalent to "a queued EV_PROCESSINFOS message", so any port
   would have to invent a proxy condition (e.g. "player is mid info-block"), which risks a
   false positive that fires in different cases than the original.

2. **Low / hard-to-validate observability.** The window is a single info-processing
   transition and only matters when a hit involving the player resolves inside it; without
   a reproducible in-game case it is not safe to assert OG currently misbehaves in a way a
   player would notice, versus this being effectively dead-code in normal play.

Per the "empty beats false positives" rule, no surgical edit is proposed. If a faithful
reimplementation is later desired, the correct shape is: in `Npc::takeDamage`, before
applying damage/reaction, early-return when (`this==player || &other==player`) and the
victim is currently inside an info-processing dialog transition — but only once OG exposes
a precise, grep-verifiable equivalent of the EV_PROCESSINFOS/EV_STOPPROCESSINFOS pending
state.

// NOTE: in original-game oCNpc::OnDamage @0x006660e0 — if (self.IsAPlayer() ||
// attacker.IsAPlayer()) and any queued oCMsgConversation in the victim's EM has subtype
// EV_PROCESSINFOS(0x10) or EV_STOPPROCESSINFOS(0x11), OnDamage returns before
// OnDamage_Hit/OnDamage_Anim, suppressing all damage and hit-reaction.
