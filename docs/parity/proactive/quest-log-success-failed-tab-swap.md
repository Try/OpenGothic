# Quest log: successful vs failed missions shown under swapped tabs

**Confidence:** High

## Original function + address

`oCMenu_Log::SetLogTopics` (Gothic2.exe @ `0x0047bf90`) is the routine that
distributes every `oCLogTopic` into the log-screen listboxes. For a topic whose
*section* field (topic+0x14) is `MISSION` (== 0) it reads the *status* field
(topic+0x18) and inserts the topic into one of three listbox menu items it has
looked up by name via `zCMenuItem::GetByName`:

- status `1` (running)  -> listbox `MENU_ITEM_LIST_MISSIONS_ACT`
- status `2` (success)  -> listbox `MENU_ITEM_LIST_MISSIONS_OLD`
- status `3` (failed)   -> listbox `MENU_ITEM_LIST_MISSIONS_FAILED`
- status `4` (obsolete) / `0` -> inserted into no list (hidden)

Topics with section `NOTE` (== 1) always go to `MENU_ITEM_LIST_LOG`.

The name->DAT->listbox identity was confirmed against the warm decompiler: the
three mission listboxes carry the script `userString[0]` identifiers
`CURRENTMISSIONS`, `OLDMISSIONS`, `FAILEDMISSIONS` (set in MENU.DAT). So in the
original game **completed (success) quests appear under the OLDMISSIONS tab and
failed quests under the FAILEDMISSIONS tab** — the intuitive mapping.

(The string literals `MENU_ITEM_LIST_MISSIONS_ACT/_OLD/_FAILED/_LOG` and their
zSTRING globals `DAT_008ce270/008ce2d4/008ce178/008ce164` were tied to their
constructors at `0x0047b810/840/870/8a0` and cross-checked against the
`GetByName` order used in both `ScreenInit` @ `0x0047b9b0` and `SetLogTopics`.)

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/ui/gamemenu.h:41-46`
(consumed by `GameMenu::isCompatible` / `toStatus` in
`/Users/admin/Downloads/opengothic/game/ui/gamemenu.cpp:326-342`, fed by
`status = toStatus(list.handle->user_string[0])` at `gamemenu.cpp:81`).

## Divergence

OpenGothic's `QuestStat` enum binds the `OLDMISSIONS` and `FAILEDMISSIONS`
listboxes to the wrong status values:

```
Old       = uint8_t(QuestLog::Status::Failed),   // 3
Failed    = uint8_t(QuestLog::Status::Success),   // 2
```

`isCompatible()` compares `uint8_t(q.status) == uint8_t(st)`, so with the
`userString[0]` identifiers reported by zenkit:

- `OLDMISSIONS` tab (st == Old == 3) shows quests with status **Failed**
- `FAILEDMISSIONS` tab (st == Failed == 2) shows quests with status **Success**

i.e. exactly swapped versus the original. In OpenGothic a successfully completed
quest is listed under "Failed Missions" and a failed quest under "Old Missions".
(`Current`->Running and the implicit hiding of Obsolete/0 both already match the
original; only the success<->failed pair is swapped.)

## Proposed patch

`game/ui/gamemenu.h`

OLD:
```cpp
    enum class QuestStat : uint8_t {
      Current   = uint8_t(QuestLog::Status::Running),
      Old       = uint8_t(QuestLog::Status::Failed),
      Failed    = uint8_t(QuestLog::Status::Success),
      Log       = uint8_t(5),
      };
```

NEW:
```cpp
    // NOTE: in original-game oCMenu_Log::SetLogTopics @0x0047bf90 a MISSION topic
    // with status 2 (success) is inserted into the OLDMISSIONS listbox and status 3
    // (failed) into the FAILEDMISSIONS listbox. OpenGothic had these two mapped the
    // other way round, so completed quests appeared under "Failed" and failed quests
    // under "Old".
    enum class QuestStat : uint8_t {
      Current   = uint8_t(QuestLog::Status::Running),
      Old       = uint8_t(QuestLog::Status::Success),
      Failed    = uint8_t(QuestLog::Status::Failed),
      Log       = uint8_t(5),
      };
```

Grep-verified symbols: `QuestLog::Status::Running/Success/Failed/Obsolete`
(`game/game/questlog.h:21-26`); `QuestStat`, `toStatus`, `isCompatible`,
`numQuests` (`game/ui/gamemenu.h`, `game/ui/gamemenu.cpp`). No other code
depends on the numeric ordering of `QuestStat::Old`/`Failed`.
