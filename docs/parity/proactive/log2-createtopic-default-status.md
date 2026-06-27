# Log_CreateTopic: newly-created topic gets status Running instead of "unset" (0)

**Confidence:** Medium-High (code divergence verified against original; user-visible only for the
edge case of a MISSION topic created without a following `Log_SetTopicStatus`).

## Original function + address

`Log_CreateTopic` external — `FUN_006e3ca0` (`P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`).
The external pops the section int and the topic name, scans the `oCLogManager` topic list for an
existing topic of the same name (early-out if found, so re-creating an existing topic is a no-op),
otherwise allocates a fresh `oCLogTopic` (0x24 bytes) and initializes its fields: name at +0x04,
the entry-list head/tail at +0x1c/+0x20 to null, the **section at +0x14** to the script int, and
the **status at +0x18 to literal `0`**. It does NOT default the status to `LOG_RUNNING` (1).

The status field meaning is fixed by `oCMenu_Log::SetLogTopics` @0x0047bf90: a MISSION topic
(+0x14 == 0) is routed to a listbox only when its status (+0x18) equals 1 (current), 2 (old/success)
or 3 (failed); any other status value — including the default 0 — matches no listbox and the topic
is therefore not shown anywhere. NOTE topics (+0x14 == 1) are shown regardless of status.

## OpenGothic file:line

- `game/game/questlog.h:28` — `Status status = Status::Running;` (default member initializer)
- `game/game/questlog.h:13-18` — `enum class Status` has no `0` value (Running=1..Obsolete=4)
- `game/game/questlog.cpp:7-15` — `QuestLog::add()` leaves `status` at its default
- consumer: `game/ui/gamemenu.cpp:341` — `uint8_t(q.status)==uint8_t(st)` decides tab membership

## Divergence

In the original a freshly created topic has status 0, which deliberately maps to "shown in no
mission tab" until script code calls `Log_SetTopicStatus(..., LOG_RUNNING)`. OpenGothic's
`QuestLog::Quest` defaults `status` to `Status::Running` (1), so a MISSION topic that is created
but never given an explicit status is treated as an active quest and appears under "Current
Missions", whereas the original hides it. The standard `B_LogEntry` helper always sets RUNNING, so
this is masked for the common path, but any script that calls `Log_CreateTopic(topic, LOG_MISSION)`
without a paired `Log_SetTopicStatus` (or that relies on the topic staying invisible until a later
status call) diverges, and the phantom status is also written into savegames
(`questlog.cpp:52`).

## Proposed patch

Mirror the original's "unset" default by introducing a `None = 0` status and using it as the
Quest default. `GameMenu::isCompatible` (gamemenu.cpp:341) already hides any status that does not
equal a `QuestStat` tab value (Current=1/Old=2/Failed=3/Log=5), so `None` is automatically
invisible for missions while notes continue to show via the section==Note branch. `setStatus`,
`save`/`load` and `log_settopicstatus` are unaffected (load overwrites the field; the status
validation in `log_settopicstatus` already rejects 0 as an explicit script argument).

OLD (`game/game/questlog.h`):
```cpp
    enum class Status : uint8_t {
      Running  = 1,
      Success  = 2,
      Failed   = 3,
      Obsolete = 4
      };
```
NEW:
```cpp
    enum class Status : uint8_t {
      // NOTE: in original-game Log_CreateTopic @0x006e3ca0 a new oCLogTopic is initialized with
      // status 0 (field +0x18), which oCMenu_Log::SetLogTopics @0x0047bf90 maps to no mission tab
      // until Log_SetTopicStatus sets RUNNING/SUCCESS/FAILED. OpenGothic defaulted to Running, so
      // a mission created without an explicit status was wrongly shown as a current quest.
      None     = 0,
      Running  = 1,
      Success  = 2,
      Failed   = 3,
      Obsolete = 4
      };
```

OLD (`game/game/questlog.h`):
```cpp
      Status      status =Status::Running;
```
NEW:
```cpp
      Status      status =Status::None;
```

Grep-verified symbols: `QuestLog::Status` / `Status::Running` (questlog.h:13, gamemenu.h:46,
gamescript.cpp:3551), `QuestLog::Quest::status` (questlog.h:28), sole runtime consumer
`GameMenu::isCompatible` (gamemenu.cpp:338-342). No code path treats `Running` as a sentinel for
"newly created", so the change is localized.
