# ext7 — Hlp_StrCmp unbound: case-insensitive string equality always returns FALSE

**Confidence:** High

## Original fn + address
`Hlp_StrCmp` is registered by `oCGame::DefineExternals_Ulfi` (the `Hlp_*` block, between
`Hlp_GetInstanceID` and `Hlp_GetNpc`) with handler `FUN_006eebe0` (Gothic2.exe), signature
`(string, string) -> int`. The handler pops both string parameters, calls `zSTRING::Upper()` on
each, performs a byte-wise lexical compare of the upper-cased strings, and does
`SetReturn(cmp == 0)` — i.e. it returns **TRUE when the two strings are equal, case-insensitively**,
FALSE otherwise. It is a pure query: no engine state is touched.

## OG file:line
`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp` — `bindExternal` table, `hlp_*` group
at lines 111-116. There is **no** `hlp_strcmp` binding and **no** `GameScript::hlp_strcmp` method
(grep for `strcmp` in `gamescript.cpp`/`gamescript.h` returns nothing). Header decls for the other
`hlp_*` externals are at `gamescript.h:235-240`.

## Divergence
Because `hlp_strcmp` is never bound, the Daedalus VM falls through to ZenKit's default external,
which returns 0. For `Hlp_StrCmp` that means **every comparison reports "not equal" (FALSE)**, so
any script branch keyed on `Hlp_StrCmp(a, b)` always takes the inequality path — the exact opposite
of the original whenever the two strings actually match. The original implements a meaningful,
state-free return; OG silently returns the 0/default. (`<cctype>` is already included at
`gamescript.cpp:6`, so `std::toupper` is available with no new infrastructure; no serialize bump.)

## Proposed patch

### 1. Binding — `gamescript.cpp`, after line 116 (`hlp_getinstanceid`)

OLD:
```cpp
  bindExternal("hlp_getinstanceid",              &GameScript::hlp_getinstanceid);
```
NEW:
```cpp
  bindExternal("hlp_getinstanceid",              &GameScript::hlp_getinstanceid);
  bindExternal("hlp_strcmp",                     &GameScript::hlp_strcmp);
```

### 2. Header decl — `gamescript.h`, after line 240 (`hlp_getnpc`)

OLD:
```cpp
    auto hlp_getnpc          (int instanceSymbol) -> std::shared_ptr<zenkit::INpc>;
```
NEW:
```cpp
    auto hlp_getnpc          (int instanceSymbol) -> std::shared_ptr<zenkit::INpc>;
    bool hlp_strcmp          (std::string_view a, std::string_view b);
```

### 3. Method body — `gamescript.cpp`, near the other `hlp_*` bodies (e.g. after `hlp_random`, line ~3714)

NEW:
```cpp
bool GameScript::hlp_strcmp(std::string_view a, std::string_view b) {
  // NOTE: in original-game Hlp_StrCmp @0x006eebe0 upper-cases both args (zSTRING::Upper)
  // and returns TRUE on case-insensitive equality; unbound in OG it defaulted to FALSE.
  if(a.size()!=b.size())
    return false;
  for(size_t i=0; i<a.size(); ++i)
    if(std::toupper(uint8_t(a[i]))!=std::toupper(uint8_t(b[i])))
      return false;
  return true;
  }
```

NOTE: `std::string_view` is the established param type for string externals in OG
(`mdl_setvisual`, `log_createtopic`), and `bindExternal` already supports `bool`-returning
methods — so this needs no new plumbing. The ASCII `toupper` matches the original's behavior for
all script-relevant identifiers (German-codepage edge cases in `zSTRING::Upper` are not exercised by
stock Daedalus string keys).
