# Selected sub-choice removal: by-text-single vs by-function-all

**Confidence:** High

## Original function + address

The selection handler for a clicked dialog choice is
`oCInformationManager::OnChoice(class oCInfoChoice*)` at **0x006629a0** (the
overload taking the selected `oCInfoChoice*`, distinct from the index-based
`OnChoice(int)` at 0x00662780). When a choice is selected it:

1. Copies the **text** of the selected `oCInfoChoice` (the choice's display
   string, at choice-object offset +4).
2. Calls `oCInfo::RemoveChoice(currentInfo)` at **0x00703c20**, passing that
   text. `RemoveChoice` walks the current info's choice list (the singly-linked
   list at info offset +0x58, built by prepend in `oCInfo::AddChoice` @0x00703b20)
   and removes **exactly the first node whose text compares equal** to the
   passed string, unlinks/frees that one node, and returns. It does not continue
   scanning after the first match.
3. Sets SELF/OTHER and invokes the choice's function via `zCParser::CallFunc`.

So the original removes **one** sub-choice, selected by **text equality**, namely
the entry the player actually clicked, and only that entry.

(The index-based `OnChoice(int)` @0x00662780 and `CollectChoices` @0x00661cd0
confirm the choice list is displayed head-to-tail in prepend/LIFO order with no
sort, which OpenGothic already matches: `add_choice` prepends and `updateDialog`
assigns a strictly increasing `sort`.)

## OpenGothic file:line

`game/game/gamescript.cpp:963-971` (`GameScript::exec`).

```cpp
if(info.information==int(dlg.scriptFn)) {
  setNpcInfoKnown(pl,info);
  } else {
  for(size_t i=0;i<info.choices.size();){
    if(info.choices[i].function==int(dlg.scriptFn))
      info.choices.erase(info.choices.begin()+int(i)); else
      ++i;
    }
  }
```

## Divergence

OpenGothic removes the selected sub-choice by matching **`function` (scriptFn)
index**, and the loop has **no break** — it erases **every** choice whose
`function` equals the selected one. The original removes a **single** choice
matched by **text**.

These differ whenever two or more sub-choices of the same info share the same
handler function but have different display text (a common pattern: several
"ask about X / ask about Y" sub-questions wired to one dispatcher function, or a
repeated "Anything else?" entry). On selecting one such choice:

- Original: only the clicked entry (matched by its text) disappears; the sibling
  choices that share the function remain in the list and are re-displayed by the
  subsequent `updateDialog`/CollectChoices pass.
- OpenGothic: all siblings sharing that function are erased at once, so they
  vanish from the menu after a single pick.

(The text-vs-function key only diverges in this duplicate-function case; with
distinct functions both keys select the same single entry, so the loop's
"remove-all + no-break" is the operative defect, made worse by keying on
function instead of text.)

## Proposed patch

`dlg.title` carries the selected choice's display text (`updateDialog` sets
`ch.title = sub.text`), and `IInfoChoice.text` is the per-choice text; both are
grep-verified to exist (`game/game/gamescript.h:51`,
`lib/ZenKit/include/zenkit/addon/daedalus.hh:344`). Match by text and remove only
the first hit.

OLD (`game/game/gamescript.cpp:965-971`):
```cpp
    } else {
    for(size_t i=0;i<info.choices.size();){
      if(info.choices[i].function==int(dlg.scriptFn))
        info.choices.erase(info.choices.begin()+int(i)); else
        ++i;
      }
    }
```

NEW:
```cpp
    } else {
    // NOTE: in original-game oCInformationManager::OnChoice(oCInfoChoice*) @0x006629a0
    // the selected sub-choice is removed via oCInfo::RemoveChoice @0x00703c20, which
    // deletes exactly the FIRST list entry whose TEXT matches the clicked choice and
    // then stops. Keying on function index and erasing every match wrongly removes
    // sibling choices that share a handler function but have different text.
    for(size_t i=0;i<info.choices.size();++i){
      if(info.choices[i].text==dlg.title){
        info.choices.erase(info.choices.begin()+int(i));
        break;
        }
      }
    }
```
