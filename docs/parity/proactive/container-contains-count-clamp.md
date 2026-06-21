# Container `contains` count of 0 / negative drops item instead of giving 1

**Confidence:** Medium

## Original function + address
`oCMobContainer::CreateContents(zSTRING const&)` at `0x00726190`
(`P:\dev\g2addon\release\Gothic\_ulf\oMobInter.cpp`).

After extracting the count word for an entry, the original does
`count = atol(word)` and then clamps: if `count < 1`, it forces `count = 1`.
It then creates exactly that many of the item. So an entry written as
`ItMiSword:0`, `ItMiSword:-3`, or with a non-numeric count, yields **one** item.
(A bare name with no `:count` also resolves to `atol("") == 0 -> 1`, matching the
no-colon path.)

## OpenGothic location
`game/world/objects/interactive.cpp:651-661`.

```cpp
long count = std::strtol(name.data()+sep+1,nullptr,10);
if(count>0)
  invent.addItem(itm,size_t(count),world);
```

## Divergence
When a `:count` is present but parses to `<= 0` (e.g. `ItMiSword:0`, a negative,
or a non-numeric value such as `ItMiSword:x`), OpenGothic adds **zero** items —
the `if(count>0)` guard skips it. The original engine clamps `count < 1` up to
`1` and adds one item. This is an inverted boundary: original floors at 1, OG
floors at 0 for the colon path.

Note: no shipped Gothic 2 container uses a `:0`/negative count, so the base game
is unaffected; this matters for mods and hand-authored data. Kept at Medium
because it is a concrete, unambiguous value divergence rather than architecture.

## Proposed patch
```cpp
// game/world/objects/interactive.cpp
```
OLD:
```cpp
    auto itm = name.substr(0,sep);
    long count = std::strtol(name.data()+sep+1,nullptr,10);
    if(count>0)
      invent.addItem(itm,size_t(count),world);
    } else {
```
NEW:
```cpp
    auto itm = name.substr(0,sep);
    long count = std::strtol(name.data()+sep+1,nullptr,10);
    // NOTE: in original-game oCMobContainer::CreateContents (0x00726190) the
    // count is clamped with `if(count < 1) count = 1`, so a 0/negative/non-numeric
    // count still spawns exactly one item rather than dropping the entry.
    if(count<1)
      count = 1;
    invent.addItem(itm,size_t(count),world);
    } else {
```
