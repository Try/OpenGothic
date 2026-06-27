# Clock HUD: minute not zero-padded (shows "8:5" instead of "8:05")

**Confidence:** High

## Original function + address
`oCWorldTimer::GetTimeString` @ `0x00780ec0` builds the on-screen clock string.
It calls `oCWorldTimer::GetTime` (@ `0x00780df0`, which yields `hour` then `minute`),
converts the **minute** to a decimal string first, and — when that minute string is a
single digit (length 1) — prepends a literal `'0'` (0x30) to it. It then prepends the
**hour** (converted as-is, NOT zero-padded) followed by `':'`. The net format is
`H:MM`: the hour is unpadded (1 or 2 digits) and the minute is always zero-padded to
two digits, e.g. `8:05`, `13:07`, `0:00`.

The marvin "clock" toggle (strings `CLOCK TICK TOCK` / `CLOCK OFF`) sets `oCGame+0xb8`.
`oCGame::ShowDebugInfos` @ `0x006c85c3` reads that flag (`if (*(int*)(this+0xb8)!=0)`),
calls `GetTimeString` on `this+0x114` (the `oCWorldTimer`), and `zCView::Print`s it at
top-right (`0x1fff - textWidth`, y=0). This is the exact feature OpenGothic mirrors with
`Gothic::inst().doClock()`.

Corroboration inside OpenGothic itself: every other time render uses the same canonical
format. `game/ui/gamemenu.cpp:1203` (`"%d:%02d"`) and `game/ui/gamemenu.cpp:1075`
(`"%d - %d:%02d"`) both zero-pad the minute and leave the hour unpadded. Only the live
HUD clock diverges.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/mainwindow.cpp:280-284`

```cpp
auto hour = world->time().hour();
auto min  = world->time().minute();
auto& fnt = Resources::font(scale);
string_frm clockT(int(hour),":",int(min));
fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
```

## Divergence
`string_frm`'s `int` overload writes with `"%d"` (no padding — see
`game/utils/string_frm.h:126-130`). So for any minute below 10 the HUD prints `8:5`,
`13:7`, `0:0`, whereas the original game (and OpenGothic's own save-header/menu time
strings) print `8:05`, `13:07`, `0:00`. The hour matches (both leave it unpadded);
the bug is strictly the missing zero-pad on the minute. Visually the clock text also
shifts width frame-to-frame because it is right-aligned by `textSize().w`.

## Proposed patch
Grep-verified symbols: `world->time()` → `gtime` with `.hour()` / `.minute()`
(used at `game/mainwindow.cpp:280-281`); `std::snprintf` already used for time at
`game/ui/gamemenu.cpp:1203`; `string_frm` supports a `const char*` arg
(`game/utils/string_frm.h:111`).

OLD (`game/mainwindow.cpp:280-284`):
```cpp
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      string_frm clockT(int(hour),":",int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
```

NEW:
```cpp
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      // NOTE: in original-game oCWorldTimer::GetTimeString @0x00780ec0 the clock is
      // formatted "H:MM" - hour unpadded, minute zero-padded to two digits.
      string_frm clockT(int(hour),":",(min<10 ? "0" : ""),int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
```

(Equivalent alternative, matching the `gamemenu.cpp` idiom: build with
`std::snprintf(buf,sizeof(buf),"%d:%02d",int(hour),int(min))`.) Both produce the
original `H:MM` form. This is build-safe: `min` is the same `gtime` minute accessor
already in use, and `string_frm` already handles the extra `const char*` argument.
