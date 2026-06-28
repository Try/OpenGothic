# HUD player status-bars & focus not hidden during a global cutscene

**Confidence:** Medium-High

## Original fn + address

`oCGame::UpdatePlayerStatus` @ `0x006c3140` (`P:\dev\g2addon\release\Gothic\_ulf\oGame.cpp`)
is the routine that draws every on-screen status view: the player HP bar
(`this+0x8c`), the swim/breath bar (`this+0x90`), the player mana bar
(`this+0x94`) and the focus name + focus HP bar (`this+0x98`).

Near the very top of the function — after the `oCNpc::player != null` and the
`SetShowPlayerStatus`-flag (`this+0x9c`) checks, and *before* any view is
re-inserted — it does a second, independent guard:

- It fetches the session's cutscene player (oCGame vtable getter, member
  `+0x64` is the `zCCSPlayer*`) and calls
  `zCCSPlayer::GetPlayingGlobalCutscene` @ `0x00420770`.
- If a **global cutscene is currently playing**, the function early-returns
  immediately. Because the four `zCView` items were already removed at the
  top of the call, the result is that **all player status bars and the focus
  name/HP bar are hidden for the entire duration of a global (in-world,
  non-dialog) cutscene.**

This is a separate guard from the dialog gate: regular conversations are
suppressed by the `SetShowPlayerStatus(0)` flag at `this+0x9c` (already handled
in `dlgexit-playerstatus-bars-hidden.md`); the cutscene guard additionally
covers scripted story/camera cutscenes, which do not toggle that flag.

## OG file:line

`/Users/admin/Downloads/opengothic/game/mainwindow.cpp:206-253`
(`MainWindow::paintEvent`, the in-world HUD block) and the
`paintFocus(p,focus,vp)` call at line 215.

- `camera` is already in scope at line 207 (`auto& camera = *Gothic::inst().camera();`).
- The bars block (lines 217-253) gates each bar only on `!inDialog`
  (`dialogs.isActive()`); there is **no cutscene gate**.
- `paintFocus` (line 584) early-returns only on `dialogs.isActive()`, never on
  a cutscene.

OpenGothic already tracks cutscene state: `Camera::isCutscene()`
(`game/camera.cpp:261`) returns true while the camera mode is
`Camera::Mode::Cutscene`, which is set exclusively by `CsCamera`
(`game/world/triggers/cscamera.cpp:167`) for in-world scripted cutscene
cameras — the OpenGothic analog of a playing global cutscene.

## Divergence

During an in-world scripted cutscene (a `CsCamera` is active, camera mode =
`Cutscene`), OpenGothic keeps drawing the player HP bar (shown by default),
plus the mana/swim bars and the floating focus name + focus HP bar when their
conditions hold. The original game removes and withholds all of these views for
the whole cutscene. The bars/focus therefore remain visible in OpenGothic where
the original shows a clean cinematic frame.

(Scope note: `isCutscene()` covers cutscene-camera-driven global cutscenes,
which is the common observable case; a purely script-driven global cutscene with
no camera vob would not be caught. The fix is a faithful, conservative subset
and never over-hides during ordinary dialog, whose camera mode is
`Camera::Dialog`, not `Cutscene`.)

## Proposed patch

`game/mainwindow.cpp`, in `MainWindow::paintEvent`:

OLD:
```cpp
      auto focus = world->validateFocus(player.focus());
      paintFocus(p,focus,vp);

      if(auto pl = Gothic::inst().player()){
        if (!Gothic::inst().isDesktop()) {
          auto& opt = Gothic::options();
```

NEW:
```cpp
      // NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140 a second guard
      // (after the SetShowPlayerStatus flag check) calls zCCSPlayer::GetPlayingGlobalCutscene
      // @0x00420770 and early-returns when a global cutscene is playing, removing every status
      // view -- the HP/mana/swim bars and the focus name+HP bar -- for the whole cutscene.
      const bool inCutscene = camera.isCutscene();

      auto focus = world->validateFocus(player.focus());
      if(!inCutscene)
        paintFocus(p,focus,vp);

      if(auto pl = Gothic::inst().player()){
        if (!Gothic::inst().isDesktop() && !inCutscene) {
          auto& opt = Gothic::options();
```

(The added `&& !inCutscene` on the bars guard and the `if(!inCutscene)` around
`paintFocus` together mirror the original's single early-return that withholds
all four status views during a global cutscene.)
