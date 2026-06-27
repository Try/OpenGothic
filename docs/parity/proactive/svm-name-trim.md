# SVM voice-line name resolution drops the original's whitespace trim (and `$`-less direct-sound fallback)

**Confidence:** MEDIUM (the code-level divergence is certain; the vanilla trigger frequency is low,
but the fix is strictly safe and strictly closer to the original).

## Original function + address (prose)

Dialog SVM lines are resolved by `oCSVMManager::GetOU(zSTRING name, int voiceNr)` @ `0x00779e50`
(called from `oCNpc::EV_OutputSVM` @ `0x007571f0`, which passes the speaker's voice field at
`oCNpc+0x254`). Its logic:

- If `voiceNr >= moduleCount` (`*this`): report "U: SVM: Voice number too high" and return `-1`.
- If the name's first char is `'$'`: it makes a working copy, then **`zSTRING::Delete(0,1)`** (drop the
  `'$'`), **`zSTRING::Upper()`**, **`zSTRING::TrimRight(' ')`**, **`zSTRING::TrimLeft(' ')`** — i.e.
  it uppercases AND strips leading/trailing spaces — before calling `oCSVM::GetOU` @ `0x0077a540`,
  which walks the `C_SVM` instance members of module `SVM_<voiceNr>`, strips the `"C_SVM."` prefix
  from each member name, and string-compares against the cleaned name to find the sound.
- Else (name does **not** start with `'$'`): it falls through to `GetSoundID(name)` and treats the
  raw string as a direct sound/wav name (the `else` branch of `GetOU`).

The module instance for `voiceNr` is `SVM_<voiceNr>` (each `oCSVM::InitByScript(i)` @ `0x0077a290`
builds the name purely from its index `i`), so voice indexes the `SVM_<n>` instances directly — this
part matches OpenGothic.

## OpenGothic file:line

`game/game/definitions/svmdefinitions.cpp:10` — `SvmDefinitions::find(std::string_view speech, int intId)`,
specifically lines 25–26:

```cpp
speech = speech.substr(1);
name = string_frm("C_SVM.",speech);
```

## Divergence

OpenGothic strips only the leading `'$'` and then builds `"C_SVM.<speech>"` and calls
`vm.find_symbol_by_name`. Case is covered (`DaedalusScript::find_symbol_by_name`,
`lib/ZenKit/src/DaedalusScript.cc:133/159`, uppercases its query, matching the original `Upper()`),
but the original's **`TrimRight(' ')` / `TrimLeft(' ')`** has no counterpart. A script SVM constant
with a stray leading/trailing space (e.g. `"$DEAD "`) resolves to a valid line in `Gothic2.exe` and
to a null symbol (silent line) in OpenGothic.

Secondary: OpenGothic returns `""` for any name not starting with `'$'`, whereas the original treats a
`$`-less name as a direct sound id. This is essentially never exercised by vanilla content (all
vanilla SVM output uses `$`-prefixed module keys), so it is noted only for completeness and is **not**
part of the proposed patch.

## Proposed patch

Trim leading/trailing spaces after dropping the `'$'`, mirroring the original's
`TrimLeft(' ')`/`TrimRight(' ')`. `std::string_view::remove_prefix/remove_suffix` is already used in
the codebase (`game/world/objects/interactive.cpp:664`). Daedalus member identifiers can never
contain spaces, so trimming can only ever help a malformed key and never break a valid lookup.

OLD (`game/game/definitions/svmdefinitions.cpp:25-26`):
```cpp
    speech = speech.substr(1);
    name = string_frm("C_SVM.",speech);
```

NEW:
```cpp
    // NOTE: in original-game oCSVMManager::GetOU @0x00779e50 the '$'-stripped SVM key is run through
    // Upper()+TrimRight(' ')+TrimLeft(' ') before the C_SVM member lookup. find_symbol_by_name
    // already uppercases, but the surrounding-space trim was missing, so a key like "$DEAD " (stray
    // space) resolved to a line in Gothic2.exe yet to a silent/null symbol here.
    speech = speech.substr(1);
    while(!speech.empty() && speech.front()==' ')
      speech.remove_prefix(1);
    while(!speech.empty() && speech.back()==' ')
      speech.remove_suffix(1);
    name = string_frm("C_SVM.",speech);
```

Verified OG symbols: `SvmDefinitions::find` and the two surrounding lines exist as quoted;
`string_frm` (`game/utils/string_frm.h`), `vm.find_symbol_by_name` (uppercasing confirmed in ZenKit),
and `string_view::remove_prefix/remove_suffix` are all in use in the tree.
