# Issue #911 — Temple trap is not killing me

Upstream: https://github.com/Try/OpenGothic/issues/911

## Issue
A temple trap (a `zCTouchDamage` vob) that should damage/kill the player in vanilla
Gothic II deals too little or no damage in OpenGothic.

## Subsystem & OG files
- `game/world/triggers/touchdamage.cpp` / `.h` (class `TouchDamage`)
- damage application: `TouchDamage::tick` → `TouchDamage::takeDamage` →
  `Npc::changeAttribute(ATR_HITPOINTS, ...)`

## Original behavior (Ghidra — Gothic2.exe)
Engine class `zCTouchDamage` (the real logic lives here, not in the thin game
subclass `oCTouchDamage`):
- `zCTouchDamage::OnTimer` @0x615c70 — every interval, for each touching vob that
  passes `TestCollType` @0x615e10, it calls the damage path (inlined
  `FireDamageMessage` @0x616070).
- `zCTouchDamage::FireDamageMessage` @0x616070 — fires **one** damage event through
  the touched vob's event manager (vtable slot +0x24 = the OnMessage/damage entry),
  passing the damage **type/mode field** (`this+0x120`), the **amount** (`this+0x124`),
  and the hit position. It does **not** loop over damage types and does **not**
  subtract protection itself.
- The actual HP reduction therefore happens inside the standard NPC damage pipeline
  (`oCNpc::OnDamage` via an `oCMsgDamage`), which interprets the combined type/mode
  field and applies protection **once** according to the game's damage formula
  (including special modes, e.g. barrier/instant, and the `protection[]` semantics).
- `repeat_delay_sec` (`this+0x128`, scaled at OnTimer end) re-arms the timer; with no
  touching vob it `SetSleeping(1)`.

Net: original = single damage **message** per touch, resolved by the full
`oCNpc::OnDamage` pipeline.

## OpenGothic current behavior (file:line)
`TouchDamage::tick` (touchdamage.cpp:29) bypasses the NPC damage pipeline and applies
HP loss directly:
- builds a per-type `mask[]` (touchdamage.cpp:36-44),
- loops every enabled damage type and calls `takeDamage` **once per type**
  (touchdamage.cpp:47-51),
- `takeDamage` (touchdamage.cpp:60): `if(prot<0) return;` (treat as immune) else
  `changeAttribute(ATR_HITPOINTS, -max(val-prot,0))`.

So OpenGothic: (a) applies damage once **per enabled type** rather than one combined
event, and (b) computes `val-prot` itself instead of routing through `Npc`'s damage
logic.

## Divergence hypothesis (candidate — unconfirmed)
The reported "not killing me" is an **under-damage**. Most likely the trap's effective
damage type leaves `max(damage - protection[type], 0)` small/zero under OpenGothic's
simplified formula, whereas the original's `oCNpc::OnDamage` resolves the same
type/mode to lethal damage (different protection/mode handling, or a mode that ignores
protection). A second, separate latent bug points the other way: for a multi-type
TouchDamage vob OpenGothic multiplies damage by the number of enabled types (the
per-type loop), where the original fires a single combined event.

## Why not fixed here
Confirming the root cause needs (1) the specific trap vob's instance data — which
damage type(s) and amount it actually carries — and (2) parity of the full
`oCNpc::OnDamage` protection/mode formula, then (3) a runtime playtest at that trap.
None of those is available from this headless setup, and changing the damage math
speculatively could regress every other `zCTouchDamage` vob (lava, swamp, barrier).
Per the repo's no-workarounds rule, this is left as a scoped follow-up: route
`TouchDamage` through the same `Npc` damage entry the message pipeline uses, with a
single combined application, and verify against the trap in-game.

## Status
Investigated — fix deferred (needs trap instance data + `oCNpc::OnDamage` parity +
runtime test). Analysis above is the implementation guide.
