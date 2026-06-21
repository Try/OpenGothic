# FAI: enemy-prehit reaction uses a 30-degree cone instead of 90/50

**Confidence:** High

## Original function

`oCNpc::FindNextFightAction` (Gothic2.exe `0x0067d680`, oNpc_Fight.cpp). For
melee weapon modes the function builds three local gates before the FAI
situation switch:

- `local_8c = IsInFightRange(target, me)` -- I am inside the target's reach.
- `local_88 = IsInFightFocus(target, me)` -- the target is facing me (the
  narrow ~30deg fight-focus cone).
- `local_90` -- computed from `GetAngles(me, target)`: it is `1` only when
  `|yaw| < 90.0` AND `|pitch| < 50.0`. This is a WIDE 90deg-yaw / 50deg-pitch
  cone measured from ME toward the attacker.

The `enemy_prehit` (situation 0, `FA_ENEMY_PREHIT_*`) and `enemy_stormprehit`
(situation 1) blocks fire when `local_8c && local_88 && local_90` and the
attacker's current move is PREHIT / STORMPREHIT. There is **no** narrow
30deg "I am facing the target" requirement on the reacting NPC; the only
angular gate on the defender is the wide 90/50 cone `local_90`.

## OpenGothic

`game/game/fightalgo.cpp:43-51`

```
const bool focus = isInFocusAngle(npc,tg);            // 30deg narrow cone npc->tg
...
if(tg.isPrehit() && isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && focus){
```

- `isInWRange(tg,npc)` == `local_8c` (OK).
- `isInFocusAngle(tg,npc)` (30deg, target faces me) == `local_88` (OK).
- the trailing `&& focus` is `isInFocusAngle(npc,tg)` == a **30deg** narrow
  cone from me to the attacker.

## Divergence

The defender's angular gate is a 30deg cone in OG but a 90deg-yaw / 50deg-pitch
cone in the original. An NPC attacked from ~30-90deg off its facing will, in
the original, still run its `enemy_prehit` / `enemy_stormprehit` reaction
(parry / dodge / turn-to-hit), but in OG that condition fails and the NPC just
eats the hit. Gameplay-different: side attacks no longer provoke the scripted
defensive reaction.

`angleTest` only measures yaw, so the faithful analogue is a 90deg yaw cone
(the pitch term is not modelled, matching `isInJumpBackAngle`'s 90deg form).

## Proposed patch

`game/game/fightalgo.cpp`

OLD:
```
  if(tg.isPrehit() && isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && focus){
```
NEW:
```
  // NOTE: in original-game oCNpc::FindNextFightAction (0x0067d680) the defender's
  // angular gate for enemy_prehit/stormprehit is a WIDE 90deg-yaw / 50deg-pitch
  // cone (local_90 from GetAngles(me,target)), not the 30deg fight-focus cone.
  // angleTest is yaw-only, so use a 90deg cone here.
  if(tg.isPrehit() && isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) &&
     isInFocusAngle(npc,tg,90)){
```

This leaves `focus` (the 30deg cone) used for the my_w_* / my_g_* situation
selection further down, where the original genuinely requires `local_c0`
(IsInFightFocus(me,target), the narrow cone).
