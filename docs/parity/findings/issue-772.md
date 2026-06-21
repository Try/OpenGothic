# Issue #772 — Ignore mouse-click when bringing window back into focus

- Category: UI / menu input
- Disposition: **FIX** (surgical, low-risk)

## Problem
Clicking the OpenGothic window in the taskbar to restore focus is registered as a
menu "select" click, causing unintended actions (start new game, overwrite a
save). The refocus click should be swallowed.

## OG files
- `game/ui/menuroot.cpp` — `MenuRoot::mouseDownEvent` (lines 129-139)
- `game/ui/menuroot.h` — class decl
- (in-game camera path `game/mainwindow.cpp:561 focusEvent` already resets dMouse,
  but the menu select path is separate and unguarded)

## OG current behavior / divergence
`MenuRoot::mouseDownEvent` invokes the confirm action on every left click:

```cpp
void MenuRoot::mouseDownEvent(MouseEvent& event) {
  if(current!=nullptr) {
    if(event.button==Event::ButtonRight) {
      popMenu();
      } else {
      current->onKeyboard(KeyCodec::ActionGeneric);   // <- select / confirm
      }
    ...
```

There is no notion of "this click only restored focus". Per the in-issue note,
on at least Windows the OS delivers the focus-in event *before* the mouseDown, so
a flag set on focus-in can suppress the immediately-following click.

## Proposed patch

### 1) `game/ui/menuroot.h`
Add a focus override and a one-shot guard flag.

OLD:
```cpp
  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;

  private:
    void initSettings();
```

NEW:
```cpp
  protected:
    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void focusEvent     (Tempest::FocusEvent& event) override;

  private:
    void initSettings();
    // NOTE: swallow the first menu click after the window regains focus, so that
    // a taskbar/refocus click is not interpreted as a menu confirm (issue #772).
    bool ignoreNextClick = false;
```

(ensure `#include <Tempest/FocusEvent>` is available; `<Tempest/Widget>` already
pulls in the event types in this codebase — add the include if compilation needs it.)

### 2) `game/ui/menuroot.cpp`
Set the flag on focus-in; consume it in mouseDownEvent.

OLD:
```cpp
void MenuRoot::mouseDownEvent(MouseEvent& event) {
  if(current!=nullptr) {
    if(event.button==Event::ButtonRight) {
      popMenu();
      } else {
      current->onKeyboard(KeyCodec::ActionGeneric);
      }
    } else {
    event.ignore();
    }
  }
```

NEW:
```cpp
void MenuRoot::focusEvent(Tempest::FocusEvent& event) {
  // NOTE: OS delivers focus-in before the restoring mouseDown (issue #772);
  // mark the next click to be ignored so refocusing never triggers a menu action.
  if(event.in)
    ignoreNextClick = true;
  }

void MenuRoot::mouseDownEvent(MouseEvent& event) {
  if(current!=nullptr) {
    if(ignoreNextClick && event.button!=Event::ButtonRight) {
      ignoreNextClick = false;
      event.accept();
      return;
      }
    ignoreNextClick = false;
    if(event.button==Event::ButtonRight) {
      popMenu();
      } else {
      current->onKeyboard(KeyCodec::ActionGeneric);
      }
    } else {
    event.ignore();
    }
  }
```

## Notes / risk
- Low risk: only the first left-click after a focus-in is swallowed; subsequent
  clicks behave normally. Right-click (back) is intentionally still processed.
- If `MenuRoot` does not currently have a focus policy that delivers FocusEvents,
  the guard simply never trips and behavior is unchanged — verify it receives
  focus events at runtime (DEFER-to-verify only this aspect). The maintainer's
  in-issue analysis confirms the engine raises `focusEvent` before the click.
