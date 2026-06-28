# AI_OutputSVM with a non-`$` argument is dropped instead of played as a direct output-unit name

**Confidence:** Low-Medium (the divergence is decompile-certain and the fix is surgical/safe; real-world trigger is low — vanilla Gothic II always passes `"$..."` to AI_OutputSVM, so this affects mod/edge content only)

## Original function + address

- `oCSVMManager::GetOU(const zSTRING&, int voice)` @ `0x00779e50` (called by both
  `oCNpc::EV_OutputSVM` @0x007571f0 and `oCNpc::EV_OutputSVM_Overlay` @0x00756a60).

`GetOU` branches on the **first character** of the requested output string:

- **If it starts with `$`** (the situational-SVM case): it strips the `$`, then
  `Upper()` + `TrimRight(' ')` + `TrimLeft(' ')`, and resolves the key against the
  per-voice voice-module via `oCSVM::GetOU` @0x0077a540, i.e. `C_SVM[voice].<key>`.
  This is the voice-set-dependent lookup.
- **If it does NOT start with `$`**: it does **not** touch the per-voice `C_SVM`
  table at all. It resolves the whole string *directly* as an output-unit / dialog
  name through the message manager (`ogame->GetSpeechMan()->GetIndex(string)`,
  vtable +0x50), voice-independently — exactly the same resolution that
  `oCNpc::EV_Output` (@0x007576f0, the plain `AI_Output`) uses. A valid name yields
  its output-unit index and plays; an unknown name returns `-1` and is dropped with a
  `"U: SVM: Output Unit ..."` warning.

So in the original engine, `AI_OutputSVM(self, other, "SOMEDIALOGNAME")` (no `$`)
plays `SOMEDIALOGNAME` as a normal output line.

## OpenGothic file:line

- `game/game/definitions/svmdefinitions.cpp:10-43` — `SvmDefinitions::find()`.
  The whole body is gated by `if(!speech.empty() && speech[0]=='$' && intId>=0)`;
  any string that does not begin with `$` falls straight through to `return "";`
  (line 42).
- `game/game/gamescript.cpp:1359-1361` — `messageFromSvm()` just forwards to
  `svm->find()`.
- `game/world/objects/npc.cpp:443-447` — `performOutput()`: the resolved (here empty)
  `svm` string is handed to `outputPipe->outputSvm()`. With an empty string the
  global path (`GameScript::aiOutputSvm`, `gamescript.cpp:1338-1340`) returns `true`
  without emitting anything — the line is silently swallowed.

## Divergence

For an `AI_OutputSVM`/`AI_OutputSVM_Overlay` whose argument does **not** start with
`$`, the original engine plays it as a direct (voice-independent) output-unit/dialog
line, identical to `AI_Output`. OpenGothic returns `""` from `SvmDefinitions::find`,
so `performOutput` drops the line with no voice and no subtitle. Vanilla G2 always
uses `"$..."` SVM keys, so this is a mod-/edge-content parity gap rather than a
vanilla regression.

## Proposed patch

`game/game/definitions/svmdefinitions.cpp`, at the end of `SvmDefinitions::find()`:

OLD:
```cpp
    auto* i = vm.find_symbol_by_name(name);
    if(i==nullptr)
      return "";
    return i->get_string(0,svm[size_t(id)].get());
    }

  return "";
  }
```

NEW:
```cpp
    auto* i = vm.find_symbol_by_name(name);
    if(i==nullptr)
      return "";
    return i->get_string(0,svm[size_t(id)].get());
    }

  // NOTE: in original-game oCSVMManager::GetOU @0x00779e50 an output string that does NOT
  // start with '$' is NOT resolved through the per-voice C_SVM table; it is looked up
  // directly as an output-unit / dialog name via the message manager (voice-independent),
  // exactly like AI_Output / oCNpc::EV_Output @0x007576f0. OpenGothic returned "" for any
  // non-'$' SVM argument, silently dropping such AI_OutputSVM lines.
  if(!speech.empty() && speech.front()!='$')
    return speech;

  return "";
  }
```

This makes `AI_OutputSVM("NAME")` behave like `AI_Output("NAME")` for non-`$`
arguments (the downstream `outputSvm` path already resolves a plain name via
`messageByName()` + `<name>.WAV`), matching the original's non-`$` `GetOU` branch.
Build-verifiable; no behavior change for the vanilla `"$..."` path.
