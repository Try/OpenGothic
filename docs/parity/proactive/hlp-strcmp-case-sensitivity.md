# Hlp_StrCmp: OpenGothic compares case-sensitively; original is case-INsensitive

**Confidence:** High

## Original function + address

`Hlp_StrCmp` is handled by the external at `0x6eebe0` in `Gothic2.exe`
(bound by `DefineExternals_Ulfi`; source tag `Gothic\_ulf\oGameExternal.cpp`).

Behavior of the original handler, in prose:
1. It pops the two `string` parameters from the Daedalus stack.
2. It calls `zSTRING::Upper()` on **both** strings, i.e. it upper-cases each
   operand in place before any comparison.
3. It then performs a byte-wise lexicographic comparison of the two
   upper-cased buffers (the usual memcmp-style loop producing -1/0/+1).
4. It returns to the script `(comparisonResult == 0)` — i.e. it returns the
   boolean **TRUE (1) when the two strings are equal, FALSE (0) otherwise**.

Net semantics: `Hlp_StrCmp(a, b)` is a **case-insensitive equality test** that
returns `1` when `a` and `b` are equal ignoring letter case, and `0` when they
differ. (It is NOT a C `strcmp`-style tri-state and it is NOT case-sensitive.)

## OpenGothic file:line

`game/gothic.cpp:1041-1043`

```cpp
bool Gothic::hlp_strcmp(std::string_view a, std::string_view b) {
  return a == b;
  }
```

Bound at `game/gothic.cpp:977` (`vm.register_external("hlp_strcmp", ...)`);
declared at `game/gothic.h:284`.

## Divergence

OpenGothic's `hlp_strcmp` returns `a == b`, which is a **case-SENSITIVE**
byte-exact comparison. The original up-cases both operands first, so it is
**case-INsensitive**.

Consequence: any script that calls `Hlp_StrCmp` expecting case-insensitive
matching (e.g. comparing a runtime/instance name against a literal that differs
only in letter case) gets the wrong answer in OpenGothic. Example: original
`Hlp_StrCmp("Adanos", "ADANOS")` returns `TRUE`, OpenGothic returns `FALSE`.
The boolean polarity (1 == equal) already matches; only the case-folding is
missing.

This is distinct from previously fixed externals (Npc_GetNextTarget,
RemoveInvItem(s), KnowsInfo permanent, etc.).

## Proposed patch

`<cctype>` is already included in `game/gothic.cpp` (and `std::tolower` is
already used at `game/gothic.cpp:1196`), so no new includes are needed. ASCII
case-folding matches the original's effective ASCII/Windows-1252 `Upper()` for
the script identifiers these comparisons use.

OLD (`game/gothic.cpp:1041`):
```cpp
bool Gothic::hlp_strcmp(std::string_view a, std::string_view b) {
  return a == b;
  }
```

NEW:
```cpp
bool Gothic::hlp_strcmp(std::string_view a, std::string_view b) {
  // NOTE: in original-game Hlp_StrCmp @0x6eebe0 both operands are passed
  // through zSTRING::Upper() before comparison, so the test is
  // case-insensitive and returns TRUE when the strings are equal.
  if(a.size()!=b.size())
    return false;
  for(size_t i=0; i<a.size(); ++i) {
    if(std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
      return false;
    }
  return true;
  }
```

Grep-verified symbols: `Gothic::hlp_strcmp` (game/gothic.h:284, gothic.cpp:1041),
`std::tolower` already in use (gothic.cpp:1196), `<cctype>` included
(gothic.cpp).
