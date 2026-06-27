# Item condition->value scaling uses integer (floor) division, not float-proportional

**Confidence:** High on the machine-code fact; **DEFERRED** as a fix (see reason).

## Original function + address
`oCItem::GetValue` (Gothic2.exe `0x00712650`). Decoded from the actual x86 of the
condition-scaling block, the original computes the item value as:

- start an FPU accumulator from `(value + valueExtra)`, where `value` is the script
  field at engine offset `0x160` and `valueExtra` (offset `0x33c`) is an engine-only
  field that `oCItem::Init` (`0x00711970`) sets to `0` and that no setter in the binary
  ever writes (so it is effectively always 0 in practice);
- when `hp_max` (offset `0x150`) `> 0`, it loads `hp` (offset `0x14c`), sign-extends it
  (`cdq`) and runs `idiv hp_max` — a **signed integer division** — yielding the integer
  quotient `floor(hp / hp_max)`, then multiplies the accumulator by that integer
  quotient (`fimul` of the stored quotient);
- finally `ceil(...)` then round-to-nearest.

So the real formula is `ceil( value * floor(hp / hp_max) )`. For two positive operands
with `hp < hp_max` the integer quotient is `0`, i.e. any sub-full-condition item is worth
**0**; at `hp == hp_max` it is `1` (full value); at `hp == 2*hp_max` it is `2`, etc. The
scaling is a step function, not a proportional ramp. (Field map cross-checked:
`GetDamageByIndex` reads the damage array at `0x16c`, `ApplyDamages`/`InitByScript` use
`0x164/0x168/0x16c`, and on_equip/on_unequip/on_state[0..3] sit at `0x1e8/0x1ec/0x1f0..0x1fc`,
which fixes `value` at `0x160`.)

## OpenGothic file:line
`game/world/objects/item.cpp:330-337` (`Item::cost()`):

```
if(hitem->hp_max>0 && hitem->hp<hitem->hp_max)
  return int32_t(std::ceil(float(hitem->value)*float(hitem->hp)/float(hitem->hp_max)));
return hitem->value;
```

## Divergence
The already-applied OpenGothic fix scales with **float-proportional** division
(`value * float(hp)/float(hp_max)`), giving a damaged item a smoothly reduced price. The
original engine uses **integer floor division** (`value * floor(hp/hp_max)`), which is a
hard step: full value at `hp >= hp_max`, **zero** for any `0 < hp < hp_max`. For a half-
condition item OpenGothic returns ~half the value, the original returns 0.

## Proposed patch — DEFERRED
Reasons for deferral (not a surgical, build-justified fix here):
1. This lands squarely inside the **"value*condition (hp/hp_max) cost"** area that the
   task brief lists as already-handled/off-limits; the divergence is a refinement of that
   existing fix rather than a new subsystem.
2. **Reachability in vanilla is ~nil.** Vanilla item instances overwhelmingly leave
   `hp_max == 0` (condition disabled -> both code paths return `value`), and no engine
   code path decrements item `hp` below `hp_max` (only `Load` writes item `hp`; there is no
   weapon/armor wear-on-use in either engine). The float-vs-integer difference only
   manifests for mod content that ships items with `0 < hp < hp_max`, so it is not an
   observable vanilla parity bug.

If parity for condition-bearing mod items is later desired, the exact-match form is
`hp_max>0 ? int32_t(std::ceil(float(hitem->value) * float(hitem->hp / hitem->hp_max))) : value`
(integer `hp/hp_max` inside the float multiply), with a NOTE citing oCItem::GetValue
@0x00712650 and the integer `idiv`/`fimul` sequence.

## Search outcome for a distinct, reachable bug
Adjacent paths were checked and found already-correct or non-divergent:
- `oCItem::GetFullDamage`/`GetDamageByMode` sum the 8 damage fields with **no** condition
  scaling (item condition does not affect weapon damage in the original).
- `oCItemContainer::Insert` (`0x00709360`) stack-merges purely by instance id, with no
  hp/condition gate (matches OpenGothic stacking).
- on_equip/on_unequip (AddItemEffects `0x007320f0` / RemoveItemEffects `0x00732270`,
  fields `0x1e8`/`0x1ec`) and on_state[0..3] (`0x1f0..0x1fc`, via GetStateEffectFunc
  `0x00712b80`) are wired in OpenGothic (`inventory.cpp:471,491,833,1018`).
The on_state multi-step cycling in `EV_UseItemToState` (`0x007558f0`) steps state one at a
time and fires `on_state[current]` per step; OpenGothic's `putState` fires `on_state[target]`
once. This is a plausible divergence but could not be pinned to high confidence (the initial
`0x96c` "current state" seed and per-step firing index were not fully resolved), and vanilla
multi-state items using non-empty intermediate `on_state[1..3]` appear rare-to-absent, so it
is left unreported per "empty beats false positives".
