# Doc_SetMargins: 7th argument is an apply-flag, not a margin multiplier

**Confidence:** Medium-High. The logic divergence is certain from the decompilation
(`oCViewDocument::SetMargins` gates on the 7th argument being non-zero and stores the
inset values verbatim — no scaling). Real-world impact is low for *vanilla* Gothic II
content, which always passes `1` for that argument (so the bug is dormant there); it
diverges for any mod / call that passes a value other than `1` (notably `0`).

## Original function + address (prose)

- `Doc_SetMargins` is registered in `DefineExternals_Ulfi` with 7 integer parameters
  (`handle, page, left, top, right, bottom, flag`); its handler thunk lives at
  `0x6d9db0`.
- The thunk reads the 7 integers and forwards them — building an upper-left
  `zCPosition(left, top)` and a lower-right `zCPosition(right, bottom)` — into the
  document-manager dispatch at `0x65f180` (`oCDocumentManager::SetMargins`), passing the
  7th integer through unchanged as a separate trailing argument.
- The work is done in `oCViewDocument::SetMargins` @ `0x68c7f0`. It only writes the
  margin insets (object offsets `0xdc/0xe0` = upper-left, `0xe4/0xe8` = lower-right)
  **when the trailing 7th argument is non-zero** (`if (... && in_stack_0000000c != 0)`).
  The inset values are stored **verbatim** — there is no multiplication anywhere on the
  path. `zCViewPrint::PrintLines` @ `0x693c00` then consumes those insets as
  `textLeft = left + marginUL`, `textRight = right - marginLR`, i.e. per-edge insets,
  which matches OpenGothic's `Tempest::Margin(left,right,top,bottom)` ordering.

So in the original the 7th parameter is a boolean "apply these margins" gate, and a `0`
makes the call a no-op (the previously-set / default margins are retained).

## OpenGothic file:line

`game/gothic.cpp:1140` — `Gothic::doc_setmargins(int handle, int page, int left, int top,
int right, int bottom, int mul)`.

## Divergence

OpenGothic treats the 7th argument (`mul`) as a **multiplier** and **always** applies the
result:

```cpp
bottom *= mul;  right *= mul;  top *= mul;  left *= mul;
... pg.margins = Tempest::Margin(left,right,top,bottom);
    pg.flg     = ... | DocumentMenu::F_Margin;   // always set
```

versus the original, where the argument is a non-zero apply-flag and the insets are used
as-is. Concrete differences:

- `mul == 1` (the vanilla case): identical results — bug is dormant.
- `mul == 0`: original is a **no-op** (keeps default/previous margins, falls back to the
  document-level margins). OpenGothic multiplies every inset to `0`, sets `F_Margin`, and
  applies a zero-margin box, so the page text spills to the full page rectangle instead of
  keeping the default inset.
- `mul > 1`: original applies the raw `left/top/right/bottom`; OpenGothic scales them
  (e.g. `mul == 2` doubles every margin).

## Proposed patch

Grep-verified OG symbols: `Gothic::doc_setmargins` (`game/gothic.cpp:1140`,
`game/gothic.h:300`), `DocumentMenu::F_Margin` and `Page::margins`/`flg`
(`game/ui/documentmenu.h:14-25`), `Tempest::Margin`.

```cpp
// OLD — game/gothic.cpp:1140
void Gothic::doc_setmargins(int handle, int page, int left, int top, int right, int bottom, int mul) {
  bottom *=  mul;
  right  *=  mul;
  top    *=  mul;
  left   *=  mul;

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  if(page>=0 && size_t(page)<doc->pages.size()){
    auto& pg = doc->pages[size_t(page)];
    pg.margins = Tempest::Margin(left,right,top,bottom);
    pg.flg     = DocumentMenu::Flags(pg.flg | DocumentMenu::F_Margin);
    } else {
    doc->margins = Tempest::Margin(left,right,top,bottom);
    }
  }
```

```cpp
// NEW
void Gothic::doc_setmargins(int handle, int page, int left, int top, int right, int bottom, int mul) {
  // NOTE: in original-game oCViewDocument::SetMargins @0x68c7f0 the 7th Doc_SetMargins
  // argument is a boolean apply-flag (checked `!= 0`), not a multiplier; the left/top/
  // right/bottom insets are stored verbatim. A zero flag is a no-op (defaults retained).
  if(mul==0)
    return;

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  if(page>=0 && size_t(page)<doc->pages.size()){
    auto& pg = doc->pages[size_t(page)];
    pg.margins = Tempest::Margin(left,right,top,bottom);
    pg.flg     = DocumentMenu::Flags(pg.flg | DocumentMenu::F_Margin);
    } else {
    doc->margins = Tempest::Margin(left,right,top,bottom);
    }
  }
```

The fix removes the spurious scaling (so non-`1` flag values no longer rescale the text
box) and restores the original "flag `0` ⇒ no-op" semantics. It is surgical and
build-verifiable.
