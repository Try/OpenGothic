# Issue #881 — Add `-c` arg to specify `Gothic.ini` location

**Category:** CLI/config · **Disposition:** FIX (surgical)

## Intended behavior
Allow `OpenGothic -c <path>` to point at an alternate user `Gothic.ini`, so a
launcher can keep multiple setting profiles. When absent, behavior is unchanged.

## OpenGothic — current state
- `game/commandline.cpp:43-156` parses args; there is no `-c`.
- `game/gothic.cpp:112` loads the user ini from a hard-coded relative name:
  `iniFile.reset(new IniFile(u"Gothic.ini"));` (resolved against cwd).
  `baseIniFile` (l.111) is the read-only `system/Gothic.ini` shipped with the
  game and is intentionally separate.

## Gap
No way to relocate the writable user `Gothic.ini`. The path is a literal at
gothic.cpp:112.

## Proposed patch

### 1) `game/commandline.h` — store + expose the override (after l.50)
```cpp
    std::string_view    defaultSave()      const { return saveDef;    }
    // NOTE: #881 — optional override for the user Gothic.ini location
    std::u16string_view configPath()       const { return gconfig;    }
```
And add the member near `gpath` (after l.59, `std::u16string gscript;`):
```cpp
    std::u16string      gscript;
    std::u16string      gconfig;   // NOTE: #881 -c <path>
```

### 2) `game/commandline.cpp` — parse `-c` (insert before the final `else` at l.153)
```cpp
    else if(arg=="-c") {
      // NOTE: #881 — path to the user Gothic.ini
      ++i;
      if(i<argc)
        gconfig = TextCodec::toUtf8ToUtf16(std::string_view(argv[i]));
      }
```
(`TextCodec` is already included; if a UTF-8→UTF-16 helper differs in the
local Tempest build, mirror the `gpath.assign(...)` byte-copy used at l.59.)

### 3) `game/gothic.cpp:112` — honor the override
OLD:
```cpp
  iniFile    .reset(new IniFile(u"Gothic.ini"));
```
NEW:
```cpp
  // NOTE: #881 — allow -c <path> to relocate the user Gothic.ini
  auto cfg = CommandLine::inst().configPath();
  iniFile    .reset(new IniFile(cfg.empty() ? std::u16string(u"Gothic.ini")
                                            : std::u16string(cfg)));
```

## Risk
Low. Default path unchanged when `-c` is absent. `IniFile` already tolerates a
missing file (logs and uses defaults), and `flush()` writes back to the same
`fileName`, so a custom profile is persisted correctly.
