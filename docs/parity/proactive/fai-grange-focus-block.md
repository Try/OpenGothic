# FAI: in-G-range focused NPC picks my_g_focus instead of my_fk_focus

> DEFER: my_fk_focus/my_g_fk_nofocus block selection is entangled with the original's real-time ani-state probe (OG abstracts it); agent flagged the standing-vs-walking mapping uncertain. Needs runtime.

**Confidence:** Medium

## Original function

`oCNpc::FindNextFightAction` (Gothic2.exe `0x0067d680`, oNpc_Fight.cpp).
The G-range situations are evaluated in this fixed order, each reading the
NPC's CURRENT move-state (`FUN_0067ce70`, returns PREHIT/COMBO/RUN/STRAFE/...):

- 8  my_g_runto   : inGRange && focus && myMove==COMBO(0xd)
- 9  my_g_strafe  : inGRange && focus && myMove==RUN(1)
- 10 my_g_focus   : inGRange && focus && myMove==STRAFE(5)
- 0xb my_fk_focus : inGRange && focus            (focused catch-all)
- 0xc my_g_fk_nofocus : inGRange                 (non-focus catch-all)

`my_g_focus` (situation 10) requires the NPC to already be in a STRAFE
animation (move code 5, set only while a t_runl/runr strafe ani is active).
For a NPC that is simply standing/idle and focused at G-range, situations
8/9/10 all fail, so the original deterministically selects **situation 0xb
`my_fk_focus`** (`FA_MY_FK_FOCUS_*`).

## OpenGothic

`game/game/fightalgo.cpp:65-72`

```
if(isInGRange(npc,tg,owner)) {
  if(focus && npc.bodyStateMasked()==BS_RUN)
    if(fillQueue(owner,ai.my_g_runto))
      return;
  if(focus && npc.bodyStateMasked()!=BS_RUN)
    if(fillQueue(owner,ai.my_g_focus))
      return;
  }
```

For a focused, non-running (standing) NPC in G-range, OG fires
`ai.my_g_focus`. It never references `my_fk_focus` (situation 0xb) nor
`my_g_fk_nofocus` (situation 0xc) anywhere.

## Divergence

Common case (focused, standing, in G-range but out of W-range):
original selects `my_fk_focus`, OG selects `my_g_focus`. These are different
FAI script blocks (e.g. close-the-gap RUN vs a focus action), so the NPC
queues a different move-sequence. Additionally the non-focus G-range catch-all
`my_g_fk_nofocus` is never used by OG; an unfocused in-G-range NPC falls
through to the FK-far blocks instead of running its dedicated G-nofocus block.

## Proposed patch

`game/game/fightalgo.cpp`

OLD:
```
    if(isInGRange(npc,tg,owner)) {
      if(focus && npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_g_runto))
          return;
      if(focus && npc.bodyStateMasked()!=BS_RUN)
        if(fillQueue(owner,ai.my_g_focus))
          return;
      }
```
NEW:
```
    if(isInGRange(npc,tg,owner)) {
      // NOTE: in original-game oCNpc::FindNextFightAction (0x0067d680) the G-range
      // situations are: my_g_runto (running) -> my_g_focus (only while strafing)
      // -> my_fk_focus (focused catch-all) -> my_g_fk_nofocus (non-focus catch-all).
      // A standing focused NPC lands on my_fk_focus, not my_g_focus.
      if(focus && npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_g_runto))
          return;
      if(focus && npc.bodyStateMasked()!=BS_RUN)
        if(fillQueue(owner,ai.my_g_focus))
          return;
      if(focus)
        if(fillQueue(owner,ai.my_fk_focus))
          return;
      if(fillQueue(owner,ai.my_g_fk_nofocus))
        return;
      }
```
