# Log_AddEntry: missing duplicate-entry suppression

**Confidence:** High

## Original function + address

- Script external `log_addentry` is bound to the handler at `Gothic2.exe 0x006e3df0`
  (`oGameExternal.cpp`). It reads two parameters (topic name, entry text), walks the
  `oCLogManager` topic list, and for the topic whose name equals the first parameter it
  calls `oCLogTopic::AddEntry(entry)` (`Gothic2.exe 0x00663870`, `oLogManager.cpp`).
- `oCLogTopic::AddEntry` first iterates the topic's existing entry list. For every existing
  entry it does a full-string `compare` against the incoming entry text; if any existing
  entry is byte-for-byte equal to the new one, it returns immediately **without inserting**.
  Only when no equal entry exists does it allocate a list node and append the new entry to
  the tail of the list.

In other words, the original silently suppresses a `Log_AddEntry` call whose text exactly
duplicates an entry already present in that topic. This matters in practice because scripts
frequently re-issue the same `Log_AddEntry` on revisited dialogue branches / repeated
conditions, relying on the engine to de-duplicate so the journal does not accumulate
identical lines.

## OpenGothic file:line

`game/game/questlog.cpp:27-30` — `QuestLog::addEntry`

```cpp
void QuestLog::addEntry(std::string_view name, std::string_view entry) {
  if(auto m = find(name))
    m->entry.emplace_back(entry);
  }
```

The entry is appended unconditionally, with no comparison against existing entries. The
journal UI (`game/ui/gamemenu.cpp:104-106`) then renders every element of `entry`, so the
duplicates are user-visible as repeated paragraphs in the quest log.

## Divergence

Original: identical entry text already present in the topic -> the new `Log_AddEntry` is a
no-op. OpenGothic: identical entry text is appended again, producing duplicate journal lines
that never appear in the original game.

## Proposed patch

Grep-verified symbols: `QuestLog::Quest::entry` (`std::vector<std::string>`, `questlog.h:30`),
`QuestLog::find` (`questlog.cpp:32`). `std::string == std::string_view` comparison is the
exact-string equality the original uses.

OLD (`game/game/questlog.cpp:27-30`):
```cpp
void QuestLog::addEntry(std::string_view name, std::string_view entry) {
  if(auto m = find(name))
    m->entry.emplace_back(entry);
  }
```

NEW:
```cpp
void QuestLog::addEntry(std::string_view name, std::string_view entry) {
  // NOTE: in original-game oCLogTopic::AddEntry @0x00663870 (called from the
  // log_addentry external @0x006e3df0) suppresses the insert when an entry with
  // byte-identical text already exists in the topic; only unique entries are appended.
  auto m = find(name);
  if(m==nullptr)
    return;
  for(auto& e:m->entry)
    if(e==entry)
      return;
  m->entry.emplace_back(entry);
  }
```
