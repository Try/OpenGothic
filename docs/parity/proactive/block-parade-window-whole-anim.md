# Block/parade active window is gated on DEF_WINDOW; original blocks the whole parade animation

**Confidence:** High

## Original fn + address

`oCNpc::CanParade` (Gothic2.exe `0x006b15b0`), called from `oCAniCtrl_Human::HitCombo`
(`0x006b0260`) at the attacker's hit-frame. In the original, a parry succeeds when, and
only when:

1. the defender has an *active animation whose name contains `"PARADE"`*
   (`zSTRING::Search(..., s_PARADE, 1) > 0`), AND
2. the attacker lies within a ±90° front cone of the defender
   (`GetAngles(...)`, `abs(angle) > 0x5a` → return 0), AND
3. the weapon-mode / `IsHuman` / `JUMP`-name guard clauses pass.

Crucially, `CanParade` walks the defender's *currently-playing* animation list and matches
purely by name + angle. There is **no DEF_WINDOW (combo-window) sub-gate**: the parry is
live for the *entire duration* the parade animation is on the active list. The original engine
never consults a parade animation's `DEF_WINDOW` event tag to decide whether a hit is blocked.

This is consistent with the asset author's intent. In `Humans_1hST1.mds` the parade
`t_1hParade_0` (15 frames) carries `*eventTag (0 "DEF_WINDOW" "0 12")` with the comment
*"in allen Frames, die NICHT Parade-Frames sind, kann die Parade vorzeitig abgebrochen
werden"* — i.e. `DEF_WINDOW` marks the *cancel/combo* frames (0–12), and the committed
parade frames are the **remainder** (12–15). So `DEF_WINDOW` on a parade is the cancel
window, not the block-active window.

## OG file:line

`game/graphics/mesh/pose.cpp:630` — `Pose::isDefence`:

```cpp
bool Pose::isDefence(uint64_t tickCount) const {
  for(auto& i:lay) {
    if(i.bs==BS_PARADE && i.seq->isDefWindow(tickCount-i.sAnim))
      return true;
    }
  return false;
  }
```

`isDefence` is the timing-window gate used by `Npc::takeDamage` (`npc.cpp:2097`) to decide
`isBlock`. `isDefWindow` (`animation.cpp:286`) is true only while
`defWindow[0] <= t < defWindow[1]`, i.e. frames `0..12` of a 15-frame parade.

## Divergence

OpenGothic only registers a block during the `DEF_WINDOW` frames of the parade animation
(`0..12` of `15`), whereas the original blocks for the whole parade. The last ~3 frames
(~20% of the parade, and — per the MDS comment — the *actual* committed parade frames) leave
the defender unguarded, so a strike landing in the parade's recovery tail deals full damage
in OpenGothic but is parried in the original. The same gate also silently disables blocking
for any BS_PARADE animation that has no `DEF_WINDOW` at all (e.g. creature parades that alias
a strafe animation), which the original would still treat as a valid parry by name.

Note the gate is *additional* to the already-fixed ±90° facing cone (`isInFocusAngle(*,*,90)`
at `npc.cpp:2096`) and the FLY-attack bypass — those remain unchanged; this is purely the
timing-window mismatch.

## Proposed patch

Mirror `CanParade`: a strike is parried for the whole duration of an active parade
animation, identified (as the original does, via `s_PARADE`/`s_JUMP` substring tests) by a
name containing `"PARADE"` and not `"JUMP"`. Jump-back dodge animations (names contain
`"JUMP"`) keep their existing `isDefWindow` handling so the separate `isJumpBack` /
`isInJumpBackAngle` dodge path is untouched. Sequence names are upper-cased at load
(`animation.cpp:161-166`), so the substring tests are reliable.

OLD (`game/graphics/mesh/pose.cpp:630`):
```cpp
bool Pose::isDefence(uint64_t tickCount) const {
  for(auto& i:lay) {
    if(i.bs==BS_PARADE && i.seq->isDefWindow(tickCount-i.sAnim))
      return true;
    }
  return false;
  }
```

NEW:
```cpp
bool Pose::isDefence(uint64_t tickCount) const {
  for(auto& i:lay) {
    if(i.bs!=BS_PARADE)
      continue;
    // NOTE: in original-game oCNpc::CanParade @0x006b15b0 a strike is parried whenever the
    // defender has an active "*PARADE*" (and non-"*JUMP*") animation and the attacker is in
    // the +-90deg cone; there is no DEF_WINDOW sub-gate, so the whole parade animation blocks
    // (DEF_WINDOW on a parade is the cancel/combo window, not the block-active window).
    // OpenGothic gated the block on isDefWindow (DEF_WINDOW "0 12" of a 15-frame parade), so
    // the recovery tail (frames 12-15) left the defender taking full damage.
    if(i.seq->name.find("JUMP")==std::string::npos &&
       i.seq->name.find("PARADE")!=std::string::npos)
      return true;
    if(i.seq->isDefWindow(tickCount-i.sAnim))
      return true;
    }
  return false;
  }
```
