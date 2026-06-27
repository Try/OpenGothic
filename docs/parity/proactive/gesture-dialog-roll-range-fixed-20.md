# Dialog talk-gesture roll range is a fixed 1..20 in the original, not the exact gesture count

**Confidence:** High

## Original function + address

`oCNpc::StartDialogAni` @ `0x00757de0` is the routine that picks the talk
gesture played while an NPC speaks a conversation line. It is invoked from
`oCNpc::EV_PlayAniSound` @ `0x007580ed` (the per-`AI_Output` message handler),
exactly once when a conversation message begins to play; the chosen
animation id is stashed in the `oCMsgConversation` and faded out when the
line ends.

Inside `StartDialogAni`, after its body-state / free-hands guards pass, the
original rolls a single random index with `rand() % 0x14 + 1`, i.e. a fixed
range of **1..20**. It then formats the name `T_DIALOGGESTURE_<nn>` (prefixing
a `0` for indices below 10) and calls `SearchAni` on the model prototype. If
that specific gesture animation does **not** exist, `SearchAni` returns null
and the code falls through to `StartAni(model, -1, 0)`, which plays nothing.

Crucial consequence: because the roll range (20) is larger than the number of
`T_DIALOGGESTURE_*` animations actually shipped in the Gothic II human model
set, a sizable fraction of rolls select a non-existent gesture and the NPC
plays **no** talk gesture for that line. The original therefore gestures on
only a fraction of conversation lines, by design of this fixed-range roll.

## OpenGothic file:line

- `game/utils/versioninfo.h:11` — `dialogGestureCount()` returns `11` for game 2.
- `game/graphics/mdlvisual.cpp:936-944` — `startAnimDialog` rolls
  `std::rand()%count + 1` where `count = dialogGestureCount()`.

```cpp
const uint16_t count = Gothic::inst().version().dialogGestureCount();
const int      id    = std::rand()%count + 1;
```

## Divergence

OpenGothic rolls `rand() % 11 + 1` (range 1..11) for Gothic II, and every
index 1..11 maps to an existing `T_DIALOGGESTURE_*` animation, so a talk
gesture is started on **every** spoken line. The original rolls `rand() % 20 + 1`
(range 1..20); rolls that land on non-existent indices (12..20) play no
gesture. Net effect: OpenGothic NPCs gesture on 100% of dialog lines, whereas
the original gestures on roughly 11/20 (~55%) of lines and stands neutrally
the rest of the time. This is a visible, persistent difference in dialog body
language for every conversation in the game.

Note the existing Gothic I value (`21`) already follows the "fixed roll range"
reading rather than an exact-count reading, which corroborates that this
constant is the random range, and that the Gothic II `11` is the outlier.

## Proposed patch

`startAnimDialog` already handles a non-resolving gesture name correctly: when
`solver.solveFrm("T_DIALOGGESTURE_12")` returns null, `skInst->startAnim`
returns false and no animation plays — exactly mirroring the original's
`StartAni(-1)` no-op. So restoring the original 1..20 range only requires
widening the roll range constant. `stopDlgAnim` (`mdlvisual.cpp:964`) also reads
this value to stop active gestures; looping up to 20 simply issues
`stopAnim("T_DIALOGGESTURE_12".."_20")` for names that were never started,
which is a harmless no-op.

OLD (`game/utils/versioninfo.h:11`):
```cpp
    uint16_t dialogGestureCount() const { return game==2 ? 11 : 21;   }
```

NEW:
```cpp
    // NOTE: in original-game oCNpc::StartDialogAni @0x00757de0 the talk-gesture
    // index is rolled as rand()%20+1 (a fixed range, not the count of existing
    // T_DIALOGGESTURE_* anims); indices that don't resolve play no gesture, so
    // the original gestures on only ~55% of dialog lines. Called once per
    // AI_Output from oCNpc::EV_PlayAniSound @0x007580ed.
    uint16_t dialogGestureCount() const { return game==2 ? 20 : 21;   }
```
