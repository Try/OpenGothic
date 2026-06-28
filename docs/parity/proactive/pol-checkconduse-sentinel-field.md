# Item::checkCondUse gates the use-condition on the wrong field (cond_value vs cond_atr)

**Confidence:** Medium (divergence is decompile-verified / high confidence; real-world trigger is narrow — see Impact).

## Original fn + address
`oCNpc::CanUse` (Gothic2.exe @0x007319b0), the stat-gate the equip path
(`oCNpc::Equip` @0x00739c90 -> EquipWeapon/EquipArmor) calls before allowing an item.

For each of the 3 condition slots the original walks `cond_atr[i]` (the attribute
*index*, item offset 0x1b4) and `cond_value[i]` (offset 0x1c0). A slot is treated as
**satisfied / inactive** when:

```
cond_atr[i] < 1   ||   cond_value[i] <= npc->attribute[ cond_atr[i] ]
```

i.e. the slot is an *active requirement* only when `cond_atr[i] >= 1`, and it then
fails when `attribute < cond_value`. The "is there a requirement here?" sentinel is the
attribute index `cond_atr`, NOT `cond_value`. Because attribute index 0 is
`ATR_HITPOINTS`, `cond_atr < 1` deliberately makes a HITPOINTS-keyed (index 0) condition
a no-op sentinel.

## OG file:line
`/Users/admin/Downloads/opengothic/game/world/objects/item.cpp:358-368`
(`Item::checkCondUse`), reached from `Inventory::setSlot`
(`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:436`) and the
`checkCond` users at inventory.cpp:943/1006/1065/1131.

## Divergence
OG keys slot activeness on the **wrong struct field**: it fails when
`other.attribute(cond_atr[i]) < cond_value[i] && cond_value[i] != 0` — i.e. it uses
`cond_value != 0` as the "requirement present" sentinel and never tests `cond_atr`.

* For every slot with `cond_atr[i] >= 1` the two formulations are identical (when
  `cond_value==0` both pass; when `cond_value>0` both fail iff `attribute<cond_value`),
  so the common case matches the original — the operator direction (`<`) is correct.
* They diverge only for a slot with `cond_atr[i] == 0` and `cond_value[i] > 0`:
  - Original: `cond_atr < 1` -> slot inactive -> item usable.
  - OG: `atr = Attribute(0) = ATR_HITPOINTS`; blocks the item whenever the NPC's current
    HP is below `cond_value[i]`, i.e. it invents a phantom HITPOINTS requirement (and
    surfaces a spurious "cannot use" error via `printCannotUseError`).

**Impact:** vanilla item instances normally leave unused condition slots at
`cond_atr=0, cond_value=0` (no divergence), so this rarely fires in stock G2. It bites
modded/edge items that carry a `cond_value` with `cond_atr` left at 0, where OG wrongly
gates equipping on the wearer's hitpoints.

## Proposed patch
`game/world/objects/item.cpp`, `Item::checkCondUse`:

OLD:
```cpp
bool Item::checkCondUse(const Npc &other, int32_t &a, int32_t &nv) const {
  for(size_t i=0;i<zenkit::IItem::condition_count;++i){
    auto atr = Attribute(hitem->cond_atr[i]);
    if(other.attribute(atr)<hitem->cond_value[i] && hitem->cond_value[i]!=0) {
      a  = atr;
      nv = hitem->cond_value[i];
      return false;
      }
    }
  return true;
  }
```

NEW:
```cpp
bool Item::checkCondUse(const Npc &other, int32_t &a, int32_t &nv) const {
  // NOTE: in original-game oCNpc::CanUse (Gothic2.exe 0x007319b0) a condition slot is an
  // active requirement only when its attribute index cond_atr>=1 (slot passes on
  // cond_atr<1 || cond_value<=attribute); the "is there a requirement" sentinel is
  // cond_atr, not cond_value. attribute index 0 is ATR_HITPOINTS, so cond_atr<1 is the
  // engine's "no condition" marker. OpenGothic keyed activeness on cond_value!=0 and
  // never tested cond_atr, so a slot with cond_atr==0 & cond_value>0 wrongly imposed a
  // phantom HITPOINTS minimum (item blocked when the wearer's HP was below cond_value).
  for(size_t i=0;i<zenkit::IItem::condition_count;++i){
    if(hitem->cond_atr[i]<1)
      continue;
    auto atr = Attribute(hitem->cond_atr[i]);
    if(other.attribute(atr)<hitem->cond_value[i]) {
      a  = atr;
      nv = hitem->cond_value[i];
      return false;
      }
    }
  return true;
  }
```

Behaviour is unchanged for every `cond_atr>=1` slot; it only stops OG from enforcing a
spurious HITPOINTS gate on `cond_atr==0` slots, matching `CanUse`'s `cond_atr<1` sentinel.
