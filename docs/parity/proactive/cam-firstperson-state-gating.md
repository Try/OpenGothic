# cam-firstperson-state-gating

First-person camera is selected from the toggle flag alone, with no player-state
gating; the original suppresses first-person while swimming / diving (and other
states) and falls through to the situational camera instead.

**Confidence:** Medium (divergence is solid; a faithful surgical fix is not — see DEFERRED).

## Original fn + address

`oCAIHuman::ChangeCamModeBySituation` @ `0x0069cd60` (verified in
`~/gothic-re/out/functions.json`; the situational mode is finally committed via
`zCAICamera::SetMode`). The routine first computes an "allow first-person" predicate
(local `bVar14`, initialised `true`). Inside the `if(engineOption==0)` block it forces
`bVar14 = false` whenever any of the following hold: a weapon is drawn (GetWeaponMode!=0
without the special case), the npc has an interact-mob, holds a torch, is dead, is
unconscious, or its body-state is `5` (swim) or `7` (dive). `CamModFirstPerson` is then
selected **only** on the two branches guarded by `... && bVar14` (the "currently FP"
branch and the look/lookback branch). When `bVar14` is false the code does not pick
first-person; it drops through to the situational ladder
(Death -> Mob -> Dive(bodyState 7) -> Swim(waterLevel>1) -> Climb -> Fall ->
Inventory -> weapon mode). So in vanilla, toggling first-person on and then entering
water yields `CamModSwim` / `CamModDive`, not the first-person view.

## OG file:line

`game/mainwindow.cpp:1014` `MainWindow::solveCameraMode()`, specifically:

```
1022    if(camera!=nullptr && camera->isFirstPerson())
1023      return Camera::FirstPerson;
```

`isFirstPerson()` is the sticky toggle set in
`game/game/playercontrol.cpp:192` (`setFirstPerson(!isFirstPerson())`), with no
state condition anywhere. The dead/dive/swim/fall/weapon checks all live *below* this
return (`mainwindow.cpp:1034-1060`), so once the toggle is on, first-person wins over
every situational state.

## Divergence

Reachable case: toggle first-person on, then swim/dive. OpenGothic keeps
`Camera::FirstPerson` (camera pinned to the head bone underwater, `tickFirstPerson`
has no water handling); the original would have switched to the third-person
`CamModSwim` / `CamModDive`. The original additionally suppresses first-person while a
weapon is drawn, while holding a torch, and while at an interact-mob — none of which
OpenGothic gates. The discrete bug is "wrong mode selected for the swim/dive (and
weapon-drawn / torch) state while the first-person toggle is held."

## Proposed patch

DEFERRED.

Reason: the original's gate sits behind a cached `ENGINE`-section gothic.ini option
(`DAT_008b0e44`, read once via the generic `zCOption` reader `FUN_00462160` @
`0x00462160`). When that option is non-zero the entire gating block is skipped and
`bVar14` stays `true`, i.e. first-person is allowed in every state — making
OpenGothic's current behaviour correct for that configuration. The option's key string
and default value cannot be reconstructed from the decompile with high confidence, so
it is impossible to know whether the swim/dive suppression is the default behaviour.
A partial reorder (gating `Camera::FirstPerson` only behind dive/swim/dead) would also
risk regressing intended first-person aiming with a drawn bow, since the weapon-drawn
arm of the same predicate is equally config-dependent.

A faithful fix would require: (1) resolving the `ENGINE` option key + default behind
`DAT_008b0e44`, then (2) gating the `Camera::FirstPerson` return on
`!isDive() && !isSwim() && !isDead() && interactive()==nullptr && !hasTorch() &&
weaponDrawnPredicate` to mirror `bVar14`.

```
// NOTE: in original-game oCAIHuman::ChangeCamModeBySituation @0x0069cd60 first-person
// (CamModFirstPerson) is selected only when the "allow-FP" predicate bVar14 is true;
// bVar14 is forced false while swimming(bodyState5)/diving(bodyState7)/dead/unconscious/
// torch/interact-mob/weapon-drawn, behind the ENGINE option DAT_008b0e44.
```
