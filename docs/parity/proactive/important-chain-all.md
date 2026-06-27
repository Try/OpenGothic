# Important-info auto-play: original chains ALL eligible important infos, OpenGothic plays only the first

**Confidence:** High

## Original function + address (prose only)

The dialog auto-trigger lives in `oCInformationManager::Update` @ `0x00660bb0`. It is a state
machine whose phase is held in an instance field at offset `+0x54`:

- phase `0` = **important-info auto-play**;
- phase `1` = unimportant player-choice list (a selection just chosen);
- phase `2` = sub-choice list;
- phase `3` = trade.

When a dialog opens, phase is `0`. The helper `ProcessImportant` @ `0x006615b0` resets the running
index field `+0x4c` to `0` and computes the eligible-important count into `+0x50` via
`oCInfoManager::GetInfoCountImportant` @ `0x00702aa0`. On each Update tick the machine plays the
`+0x4c`-th important info via `oCInfoManager::GetInfoImportant` @ `0x00702ec0`, then increments
`+0x4c` (`ProcessNextImportant` @ `0x006617b0`). It stays in phase `0`, chaining through **every**
eligible important info, until `+0x4c >= +0x50`; only then does `OnImportantEnd` set `+0x54 = 1` and
call `CollectInfos` @ `0x00661aa0` to build the player choice list. Once in phase `1` the machine
never returns to phase `0` for the rest of the conversation (verified across the phase-1/2/3
branches of Update).

`GetInfoImportant`/`GetInfoCountImportant` walk the manager's info list and select by index; the
list is kept sorted ascending-stable by `nr` at insertion time (`oCInfoManager::CompareInfos`
@ `0x007026f0`, wired in the `oCInfoManager` ctor). Eligibility per info is: `important==1`, npc
match, `(permanent || !told)` (i.e. `oCInfo::WasTold` @ `0x00703900`, the engine equivalent of
OpenGothic's `doesNpcKnowInfo && !permanent` check), and the condition function returning `1`.
**This ordering/known-filter already matches OpenGothic and is NOT the divergence.**

## OpenGothic file:line

`game/ui/dialogmenu.cpp:272` (`DialogMenu::onStart`), gating
`game/game/gamescript.cpp:889` (`GameScript::dialogChoices`, the `includeImp` loop).

## Divergence

OpenGothic only auto-plays the **first** important info per conversation. `onStart` passes
`includeImp = (state==State::PreStart)` to `dialogChoices`. `state` is `PreStart` only on the very
first `onStart` of a dialog session (set in `openPipe`); `onStart` immediately sets it to `Active`
(dialogmenu.cpp:273). After the first important info auto-plays (`onEntry` -> `dialogExec` ->
`onDoneText` -> `onStart` again because `depth>0`), the second `onStart` runs with `state==Active`,
so `includeImp==false` and `dialogChoices` skips the important loop entirely. Any further eligible
important infos are therefore not played in this conversation — they are deferred until the next
time the player opens the dialog.

Example: NPC has two simultaneously-eligible important infos A (`nr=1`) and B (`nr=2`). Original
plays A then B then shows choices, all in one conversation. OpenGothic plays only A, then jumps
straight to the choice list; B plays only on the next conversation.

The chain is self-terminating and cannot loop: each auto-played top-level important info calls
`setNpcInfoKnown` (gamescript.cpp:971), so `doesNpcKnowInfo` (gamescript.cpp:894) then skips it;
permanent important infos are instead suppressed by the `except` list (gamescript.cpp:898-907,
fed by `onEntry`'s `except.push_back`, dialogmenu.cpp:408). When no important info remains,
`dialogChoices` returns unimportant infos whose `title` is non-empty, so the auto-play check
(`choice[0].title.empty()`) is false and the normal choice list is shown.

## Proposed patch

Introduce a small phase flag `autoImportant` that stays true while we are still auto-playing
important infos and is cleared the moment a player-facing menu is presented (mirroring the original
leaving phase `+0x54==0`). All referenced symbols are grep-verified to exist: `state`,
`State::PreStart`, `choice`, `selected`, `depth`, `except` (dialogmenu.h:126-138);
`dialogChoices(... bool includeImp)` (gamescript.h:120).

### `game/ui/dialogmenu.h` (after line 138, `bool dlgTrade=false;`)

NEW (add member):
```cpp
    bool                                autoImportant=false;
```

### `game/ui/dialogmenu.cpp` — `tick`, PreStart block (lines 67-73)

OLD:
```cpp
  if(state==State::PreStart) {
    except.clear();
    dlgTrade=false;
    trade.close();
    onStart(*this->pl,*this->other);
    return;
    }
```
NEW:
```cpp
  if(state==State::PreStart) {
    except.clear();
    dlgTrade=false;
    autoImportant=true;
    trade.close();
    onStart(*this->pl,*this->other);
    return;
    }
```

### `game/ui/dialogmenu.cpp` — `onStart` (lines 271-286)

OLD:
```cpp
bool DialogMenu::onStart(Npc &p, Npc &ot) {
  choice         = ot.dialogChoices(p,except,state==State::PreStart);
  state          = State::Active;
  depth          = 0;
  curentIsPl     = true;
  choiceAnimTime = 0;

  if(choice.size()>0 && choice[0].title.size()==0){
    // important dialog
    onEntry(choice[0]);
    }

  dlgSel=0;
  update();
  return true;
  }
```
NEW:
```cpp
bool DialogMenu::onStart(Npc &p, Npc &ot) {
  choice         = ot.dialogChoices(p,except,autoImportant);
  state          = State::Active;
  depth          = 0;
  curentIsPl     = true;
  choiceAnimTime = 0;

  // NOTE: in original-game oCInformationManager::Update @0x00660bb0 the important-info auto-play is
  // a dedicated state-machine phase (field +0x54==0) that chains through EVERY eligible important
  // info (index +0x4c from 0 to count +0x50) before switching to the player choice list (+0x54==1),
  // and never re-enters that phase mid-conversation. Keep including important infos while we are
  // still auto-playing them; leave the phase as soon as a real choice list is presented.
  const bool isImportant = (choice.size()>0 && choice[0].title.size()==0);
  autoImportant = isImportant;
  if(isImportant){
    // important dialog
    onEntry(choice[0]);
    }

  dlgSel=0;
  update();
  return true;
  }
```

### `game/ui/dialogmenu.cpp` — `onDoneText` (lines 333-343)

OLD:
```cpp
void DialogMenu::onDoneText() {
  choice = Gothic::inst().updateDialog(selected,*pl,*other);
  dlgSel = 0;
  if(choice.size()==0){
    if(depth>0) {
      onStart(*pl,*other);
      } else {
      close();
      }
    }
  }
```
NEW:
```cpp
void DialogMenu::onDoneText() {
  choice = Gothic::inst().updateDialog(selected,*pl,*other);
  dlgSel = 0;
  if(choice.size()==0){
    if(depth>0) {
      onStart(*pl,*other);
      } else {
      close();
      }
    } else {
    // NOTE: in original-game oCInformationManager::Update @0x00660bb0 an important info that opens a
    // sub-choice list ends the auto-play phase (state +0x54 goes 0->2->1, never back to 0). The
    // sub-choices are player-facing, so stop auto-including important infos.
    autoImportant=false;
    }
  }
```
