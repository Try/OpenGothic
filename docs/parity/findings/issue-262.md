# Issue #262 — Gothic2 controls (useGothic1Controls=0) issues

- Category: input / combat controls
- Disposition: **DEFER** (only "target lock" remains; needs a new feature path + runtime tuning. The other two sub-items are already done per the issue.)

## Sub-items
1. Bow doesn't work — **done** (G2 bow control present; see
   `game/game/playercontrol.cpp:725-752`, `783-803`).
2. Left/Right swiping (incl. extended menu) — **done** (g2Ctrl strafe/swipe in
   `playercontrol.cpp:137-159`, `741-748`).
3. **Target lock — not implemented** (maintainer: "missing only target lock,
   mark help wanted"). This is the open part.

## OG files
- `game/utils/keycodec.cpp` — `keyLockTarget` is **parsed** (`setupSettings`,
  line 445) and lives in `allKeys` (line 81), but `implTr()` (lines 252-307)
  never maps it to any action, and `KeyCodec::tr()` (lines 107-125) never returns
  a lock-target action. So the bound key is dead.
- `game/utils/keycodec.h:145` — `KeyPair keyLockTarget;` declared.
- `game/game/playercontrol.cpp` — `setTarget(Npc*)` (lines 31-46) and
  `pl->setTarget(...)` already exist and are used for combat auto-facing
  (lines 727, 756); but nothing latches a *manually locked* target, and there is
  no toggle bound to `keyLockTarget`.

## Divergence
The original G2 supports an explicit target-lock toggle (hold/press to lock the
current focus NPC as the combat target until released/cleared). OG has the
plumbing (`Npc::setTarget`, focus tracking) but no action wired to
`keyLockTarget`, so the feature is absent.

## Why DEFER (not FIX)
A minimal wiring would be: add a `LockTarget` action returned by
`KeyCodec::tr()` when `keyLockTarget.mapping(code)` hits, then in
`PlayerControl::onKeyPressed/Released` latch `currentFocus.npc` via `setTarget`
and hold it (clearing on release or on focus loss). However the *exact* G2
semantics (toggle vs hold, auto-release range, interaction with the existing
combat auto-target at playercontrol.cpp:727/756, and HUD lock indicator) need
runtime verification against the original to avoid regressing current combat
facing. That behavioral tuning + a small UI indicator is beyond a confident
surgical patch, so DEFER with the wiring sketch above.

## Pointers
- Add `LockTarget` to `KeyCodec::Action` and a branch in `implTr()`/`tr()`.
- In `PlayerControl`, store a `lockedTarget` and prefer it over `currentFocus`
  when feeding `pl->setTarget(...)` in the Bow/Mage/mele aim blocks.
