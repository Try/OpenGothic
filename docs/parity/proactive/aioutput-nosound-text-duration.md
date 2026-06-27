# AI_Output no-voice subtitle/line-duration formula diverges from MD_GetMinTime

**Confidence:** High (concrete constant/formula mismatch; structure and zSTRING offsets cross-verified)

## Original function + address
`oCMsgConversation::MD_GetMinTime` @ `0x0076af50` (`oNpcMessages.cpp`). This virtual returns the
duration (in **seconds**) of a conversation event-message and is the value the event/message
manager uses to decide how long an `OUTPUT`-type dialog line (subtype 0 / 2 / 0x13) occupies the
NPC before the line advances. Its OUTPUT branch does:

- It asks the sound system for the length of the voice sample named by the message
  (`this+0x58`, the output-unit / WAV name) and converts ms->s by `* 0.001` (constant
  `0x3a83126f` == 0.001).
- If that sample length is `<= 0` (no voice clip found), it falls back to a **text-length**
  formula: `charCount * (1/6) + 1.0` seconds, i.e. `charCount * 166.667 ms + 1000 ms`, with
  **no upper clamp**. Constants are hard-coded literals: `0x3e2aaaab` == `1/6` and
  `0x3f800000` == `1.0`.
- `charCount` is `*(int*)(this+0x50)`. The displayed-text zSTRING lives at `this+0x44`, and a
  zSTRING stores its length at offset `+0xC` (independently confirmed: `zCView::PrintTimed`
  @ `0x007a7d20` reads its text length at `zSTRING+0xC`). `0x44 + 0xC = 0x50`, so `this+0x50`
  is the subtitle character count.

Note the additive `+1.0 s` base and the per-char constant `1/6 s` are **hard-coded** here; they
are *not* `VIEW_TIME_PER_CHAR`. `VIEW_TIME_PER_CHAR` is read once in `oCGame::Init` @ `0x006c1060`
into the zCView print-timer global and only feeds `zCView::PrintTimed`'s auto-timed text path
(`time = textLen * VIEW_TIME_PER_CHAR` when called with `time == -1`); it does not drive the
AI_Output line lifetime.

## OpenGothic file:line
`game/game/gamescript.cpp:1352` (`GameScript::messageTime`), specifically lines 1364-1367.

`messageTime` is the single value OpenGothic feeds to the AI_Output barrier
(`npc.setAiOutputBarrier`, `gamescript.cpp:1299`), the VISEME face-anim duration
(`npc.cpp:2859/2862`), and the dialog subtitle visibility (`dialogmenu.cpp:232-238`), so it is the
functional equivalent of `MD_GetMinTime` for the no-voice case.

## Divergence
For a dialog line with **no voice clip** (subtitle-only), the two engines compute very different
durations:

| | per-char | additive base | upper clamp |
|---|---|---|---|
| Original `MD_GetMinTime` | 166.667 ms (1/6 s) | +1000 ms | none |
| OpenGothic `messageTime` | 550 ms (`viewTimePerChar`) | 0 | 16000 ms |

Examples: 10-char line -> original 2667 ms vs OG 5500 ms; 30-char -> original 6000 ms vs OG
16000 ms (OG saturates its cap); 60-char -> original 11000 ms vs OG 16000 ms. OpenGothic subtitle
lines without audio stay up roughly 2-3x too long per character, omit the original's flat +1 s
floor, and clamp at 16 s where the original has no clamp. The voice-clip path already matches
(both use WAV length).

## Proposed patch
`game/game/gamescript.cpp`, in `GameScript::messageTime`:

```cpp
// OLD
    auto txt = messageByName(id);
    time = uint32_t(float(txt.length())*viewTimePerChar);
    time = std::min(time, 16000u);

// NEW
    // NOTE: in original-game oCMsgConversation::MD_GetMinTime @0x0076af50 the OUTPUT-message
    // (subtype 0/2/0x13) duration, when no voice sample is present, is hard-coded as
    // (charCount/6.0 + 1.0) seconds with NO upper clamp == charCount*166.667ms + 1000ms.
    // The 1/6s per-char and 1s base are literals (0x3e2aaaab, 0x3f800000), NOT VIEW_TIME_PER_CHAR;
    // VIEW_TIME_PER_CHAR (oCGame::Init @0x006c1060) only governs zCView::PrintTimed auto-timed
    // text, not the AI_Output line lifetime.
    auto txt = messageByName(id);
    time = uint32_t(float(txt.length())*(1000.f/6.f)) + 1000u;
```

This leaves the voice-clip branch (`s.timeLength()`) untouched, since it already matches the
original's `sampleLength` path. `viewTimePerChar` (lines 367-382) becomes unused on this path; it
can be left in place (still loaded harmlessly) or removed in a follow-up, but removing it is out of
scope for this surgical fix.

Grep-verified OpenGothic symbols: `GameScript::messageTime`, `messageByName`, `msgTimings`,
`Resources::loadSoundBuffer`, `Sound::timeLength`, `viewTimePerChar` (all present in
`game/game/gamescript.cpp`).
