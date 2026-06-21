# Container `contains` parse drops items after a space-comma

**Confidence:** High

## Original function + address
`oCMobContainer::CreateContents(zSTRING const&)` at `0x00726190`
(`P:\dev\g2addon\release\Gothic\_ulf\oMobInter.cpp`).

The original splits the `contains` string into entries and, within each entry,
extracts the item-name word and the count word using `zSTRING::PickWordPos`
(`0x0046b1d0`). `PickWordPos` is given a separator set *and* a skip-character set.
The skip set causes leading/trailing whitespace around each token (notably the
space that commonly follows the comma) to be ignored, so the engine resolves the
clean instance name. It then looks the name up with `zCParser::GetIndex` +
`MatchClass(oCItem)` and, on success, creates the item.

## OpenGothic location
`game/world/objects/interactive.cpp:71-85` (split loop) and
`game/world/objects/interactive.cpp:651-661` (`implAddItem`).

The split loop cuts on `,` only:
```
else if(*i==','){ *i='\0'; implAddItem(it); it=i+1; }
```
`implAddItem` then calls `invent.addItem(name, ...)`, and the name is passed
verbatim to `GameScript::findSymbolIndex` -> `zenkit::find_symbol_by_name`
(`game/game/gamescript.cpp:835`). No whitespace is trimmed anywhere.

## Divergence
For an entry that follows a `", "` separator (e.g. the shipped string
`ItFo_Fish:3, ItMiNugget:5`), OpenGothic passes `" ItMiNugget"` — with a leading
space — to the symbol lookup. The lookup fails, `addItem` returns `nullptr`, and
the item is **silently dropped**. The original engine skips the space and loads
the item normally.

This is confirmed against shipped Gothic 2 data: `", It"` separators occur (e.g.
`ItFo_Fish:3, ItMi...`), so affected containers really do lose contents in
OpenGothic.

## Proposed patch
```cpp
// game/world/objects/interactive.cpp
```
OLD:
```cpp
void Interactive::implAddItem(std::string_view name) {
  size_t sep = name.find(':');
  if(sep!=std::string::npos) {
```
NEW:
```cpp
void Interactive::implAddItem(std::string_view name) {
  // NOTE: in original-game oCMobContainer::CreateContents (0x00726190) the
  // contains-string is tokenized via zSTRING::PickWordPos with a whitespace
  // skip-set, so spaces around each "Item:count" entry (e.g. "...:3, ItMi...")
  // are ignored. Trim them here to avoid a failed symbol lookup dropping items.
  while(!name.empty() && (name.front()==' ' || name.front()=='\t'))
    name.remove_prefix(1);
  while(!name.empty() && (name.back()==' ' || name.back()=='\t'))
    name.remove_suffix(1);
  size_t sep = name.find(':');
  if(sep!=std::string::npos) {
```
