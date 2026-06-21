# Log_SetTopicStatus creates a phantom topic when the topic does not exist

**Confidence:** High

## Original function (Gothic2.exe)

`Log_SetTopicStatus` external handler at **0x6e3f10** (binding registered in
`DefineExternals_Ulfi`, handler addr from the `DefineExternal` call for
`s_Log_SetTopicStatus`).

Behavior in prose: the handler pops the two Daedalus arguments (status int and
topic name), then walks the global `oCLogManager` topic list. For each existing
topic whose name string compares equal to the requested name, it writes the
status int into the topic's status field and stops. If no topic with that name
is found, the loop simply reaches the end of the list and the function returns
having done nothing. The original NEVER allocates or inserts a new topic from
`Log_SetTopicStatus`; topic creation only happens in the separate
`Log_CreateTopic` handler (0x6e3ca0), which is the only place that calls
`operator new` / `zCList::Insert`.

## OpenGothic

`game/game/questlog.cpp:17-23` (`QuestLog::setStatus`), reached from
`game/game/gamescript.cpp:3290-3296` (`GameScript::log_settopicstatus`).

```
void QuestLog::setStatus(std::string_view name, QuestLog::Status s) {
  auto m = find(name);
  if(m==nullptr && s==Status::Obsolete)
    return;
  auto& q  = add(name,Mission);   // <-- creates the topic when missing
  q.status = s;
  }
```

## Divergence

When a script calls `Log_SetTopicStatus` for a topic that was never created,
OpenGothic creates a brand-new `Mission` topic (with an empty entry list) for
every status value except `Obsolete`. The original silently ignores the call
and creates nothing. Gameplay effect: a phantom, empty quest appears in the
player's log (running/success/failed) that the original would never show. This
can happen with mod scripts (or buggy stock scripts) that set a status before
creating the topic, and the entry would persist into save games.

## Proposed patch

```
File: game/game/questlog.cpp
```

OLD:
```cpp
void QuestLog::setStatus(std::string_view name, QuestLog::Status s) {
  auto m = find(name);
  if(m==nullptr && s==Status::Obsolete)
    return;
  auto& q  = add(name,Mission);
  q.status = s;
  }
```

NEW:
```cpp
void QuestLog::setStatus(std::string_view name, QuestLog::Status s) {
  // NOTE: in original-game Log_SetTopicStatus (0x6e3f10) walks the topic list
  // and only updates the status of an EXISTING topic; if the topic is not found
  // it does nothing. It never creates a topic (creation happens only in
  // Log_CreateTopic). Do the same here instead of add()-ing a phantom topic.
  auto m = find(name);
  if(m==nullptr)
    return;
  m->status = s;
  }
```
