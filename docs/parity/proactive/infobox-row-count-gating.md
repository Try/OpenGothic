# Info-box stat rows: value-column visibility gated by cost() instead of raw COUNT[], and empty-text rows suppressed

**Confidence:** Low

## Original function + address
`oCItemContainer::DrawItemInfo` (Gothic2.exe `0x00706e40`).

The original draws the description line, then iterates a fixed loop `row = 0 .. 5`
(loop bound is exactly 6, matching `Item::MAX_UI_ROWS`). For every row the engine does
two independent things:

1. It reads `oCItem::GetText(row)` (the script `TEXT[row]`) and prints that label on
   the left edge **unconditionally** — there is no test for an empty label string.
2. It reads `oCItem::GetCount(row)` (the script `COUNT[row]`). The numeric value on the
   right edge is printed **only when `GetCount(row) != 0`**. For rows `0..4` the number
   shown is the raw `COUNT[row]`; for the last row (`row == 5`) the number shown is
   `oCItem::GetValue()` (the condition-scaled value, with the mode-5 trade multiplier),
   but the visibility gate is still the raw `COUNT[5]`, *not* the computed value.

So the engine rule is: **left label always drawn; right number drawn iff `COUNT[row] != 0`,
for every row including the value row.**

## OpenGothic file:line
`game/ui/inventorymenu.cpp:739-769` (`InventoryMenu::drawInfo`).

## Divergence
OpenGothic's loop deviates from the original in two related ways:

```
int32_t val = r.uiValue(i);     // COUNT[i]
if(txt.empty())
  continue;                     // (A) whole row skipped when label empty
...
if(i+1==Item::MAX_UI_ROWS){
  if(state==State::Trade && player!=nullptr) val = r.sellCost();
  else                                       val = r.cost();   // (B) val overwritten
}
...
if(val!=0)                      // gate now uses cost()/sellCost(), not COUNT[5]
  fnt.drawText(...,vint);
```

- (A) The `if(txt.empty()) continue;` early-out is absent from the original. The original
  still prints the right-column number when `COUNT[i] != 0` even if `TEXT[i]` is empty
  (a bare value with no label). OpenGothic suppresses the entire row.
- (B) For the value row, `val` is reassigned to `cost()`/`sellCost()` *before* the
  `val != 0` visibility test, so the value column is gated by the computed cost rather
  than by raw `COUNT[5]`. This directly contradicts the function's own existing NOTE
  comment ("COUNT[5] only gates the row's visibility").

Note that for realistic vanilla scripts these two deviations largely cancel: items that
set up the value row use `TEXT[5]=NAME_Value, COUNT[5]=value`, so (A) hides the value row
exactly when it is not set up, and (B)'s `cost()` gate is non-zero exactly when
`COUNT[5]=value` is non-zero. The observable mismatch is confined to rows where a script
sets `COUNT[i] != 0` with an empty `TEXT[i]` (non-value rows), or sets the value-row label
but with `COUNT[5]` disagreeing with the item value. I could not confirm such an item in
vanilla content, hence the Low confidence.

## Proposed patch
Make the loop a 1:1 transcription of the original: always draw the label, gate the right
column on the raw `COUNT[i]` for every row, and keep `cost()`/`sellCost()` only as the
*number displayed* on the value row.

OLD (`game/ui/inventorymenu.cpp`):
```cpp
  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    auto    txt = r.uiText(i);
    int32_t val = r.uiValue(i);

    if(txt.empty())
      continue;

    // NOTE: ... (existing comments retained) ...
    if(i+1==Item::MAX_UI_ROWS){
      if(state==State::Trade && player!=nullptr)
        val = r.sellCost();
      else
        val = r.cost();
      }

    string_frm vint(val);
    int tw = fnt.textSize(vint).w;

    fnt.drawText(p, x+20,  y+int(i+2)*fnt.pixelSize(), txt);
    if(val!=0)
      fnt.drawText(p,x+dw-tw-20,y+int(i+2)*fnt.pixelSize(),vint);
    }
```

NEW:
```cpp
  for(size_t i=0;i<Item::MAX_UI_ROWS;++i){
    auto    txt = r.uiText(i);
    // NOTE: in original-game oCItemContainer::DrawItemInfo @0x00706e40 the right-column
    // number is gated by the raw COUNT[i] (oCItem::GetCount) for EVERY row, and the left
    // label is printed unconditionally (no empty-text skip). The value row (index 5)
    // displays oCItem::GetValue (cost()/sellCost()) but is still gated by COUNT[5].
    const int32_t cnt = r.uiValue(i);  // COUNT[i] -- visibility gate for the value column
    int32_t       val = cnt;

    if(i+1==Item::MAX_UI_ROWS){
      if(state==State::Trade && player!=nullptr)
        val = r.sellCost();
      else
        val = r.cost();
      }

    string_frm vint(val);
    int tw = fnt.textSize(vint).w;

    fnt.drawText(p, x+20,  y+int(i+2)*fnt.pixelSize(), txt);
    if(cnt!=0)
      fnt.drawText(p,x+dw-tw-20,y+int(i+2)*fnt.pixelSize(),vint);
    }
```

Box geometry is unaffected: `infoHeight()` (`inventorymenu.cpp:434`) already reserves
`MAX_UI_ROWS+2` lines unconditionally and rows are positioned by index `i`, so drawing
empty labels introduces no layout shift or overflow.
