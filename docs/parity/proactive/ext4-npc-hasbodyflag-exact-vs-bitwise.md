# Npc_HasBodyFlag uses exact-equality instead of bitwise overlap

**Confidence:** Medium-High

## Original function + address

`Npc_HasBodyFlag` external (Gothic2.exe `FUN_006f38b0`, in `oGameExternal.cpp`) pops the
NPC self-pointer and the integer `bodyflag` argument, then computes the result as the
**bitwise-overlap test** `(bodyflag & oCNpc::GetFullBodyState()) > 0`. The state word it
ANDs against comes from `oCNpc::GetFullBodyState` (`0x0075eaf0`), which returns the NPC's
composite bodystate field (`oCNpc+0x76C`) masked with `0xFFFFC07F`: it keeps the base
state bits (`0x7F`) and the flag bits `BS_FLAG_INTERRUPTABLE` (bit 15) / `BS_FLAG_FREEHANDS`
(bit 16), and strips the `BS_MOD_*` modifier bits (7..13). So the external returns true
whenever **any** queried bit overlaps the (base + flag) state — e.g. a standing NPC
(`BS_STAND`, which carries *both* `BS_FLAG_INTERRUPTABLE` and `BS_FLAG_FREEHANDS`) returns
true for `Npc_HasBodyFlag(self, BS_FLAG_INTERRUPTABLE)`.

## OpenGothic file:line

- `game/game/gamescript.cpp:2994-2999` — `GameScript::npc_hasbodyflag` returns
  `npc->hasStateFlag(BodyState(bodyflag))`.
- `game/world/objects/npc.cpp:3555-3561` — `Npc::hasStateFlag` and
  `game/graphics/mesh/pose.cpp:131-136` — `Pose::hasStateFlag`, both do an **exact-equality**
  compare `(i.bs & (BS_FLAG_MASK|BS_MOD_MASK)) == flg`.
- `Npc::hasStateFlag` is consumed *only* by this external (verified: sole caller is
  `gamescript.cpp:2998`), so the implementation can be matched to the binary safely.

## Divergence

OpenGothic tests `(state & (BS_FLAG_MASK|BS_MOD_MASK)) == bodyflag` (exact equality of the
flag/mod portion), whereas the original tests `(bodyflag & fullBodyState) > 0` (any bit
overlaps). The two disagree whenever the NPC's state carries **more** flag bits than the
single bit queried. The common case: an idle/standing NPC has `BS_STAND` =
`BS_FLAG_INTERRUPTABLE | BS_FLAG_FREEHANDS` (`0x18000`), so:

- `Npc_HasBodyFlag(self, BS_FLAG_INTERRUPTABLE)` → original: `0x8000 & 0x18000 = 0x8000 > 0`
  → **TRUE**; OpenGothic: `0x18000 == 0x8000` → **FALSE**.

This makes every single-flag query against a multi-flag state return the wrong answer, and
unlike the deferred `stagger-bodymod-interrupt-suppression` finding it does **not** depend on
any `BS_MOD_*` bit being populated — it is purely about the `BS_FLAG_*` bits and is hit by
the most common (standing) body state. (It also incidentally misses the original's inclusion
of base-state bits in the AND, but scripts query `Npc_HasBodyFlag` with `BS_FLAG_*` values.)

## Proposed patch

`game/game/gamescript.cpp` — replace the exact-equality helper call with the original's
bitwise-overlap test against `bodyStateMasked()` (which keeps base + `BS_FLAG_MASK` and
strips `BS_MOD_*`, exactly mirroring `GetFullBodyState`'s `0xFFFFC07F`):

OLD:
```cpp
bool GameScript::npc_hasbodyflag(std::shared_ptr<zenkit::INpc> npcRef, int bodyflag) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;
  return npc->hasStateFlag(BodyState(bodyflag));
  }
```

NEW:
```cpp
bool GameScript::npc_hasbodyflag(std::shared_ptr<zenkit::INpc> npcRef, int bodyflag) {
  // NOTE: in original-game Npc_HasBodyFlag (Gothic2.exe FUN_006f38b0) returns
  // (bodyflag & oCNpc::GetFullBodyState() @0x0075eaf0) > 0 -- a bitwise-overlap test, not an
  // exact-equality compare. GetFullBodyState masks with 0xFFFFC07F (base + BS_FLAG_*, strips
  // BS_MOD_*), which OpenGothic's bodyStateMasked() reproduces. The old hasStateFlag() did an
  // exact `(bs & (BS_FLAG_MASK|BS_MOD_MASK))==flg` compare, so querying one flag (e.g.
  // BS_FLAG_INTERRUPTABLE) on a standing NPC that has both flags set wrongly returned false.
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;
  return (uint32_t(bodyflag) & uint32_t(npc->bodyStateMasked()))!=0;
  }
```

Grep-verified symbols: `Npc::bodyStateMasked()` (`game/world/objects/npc.h:247`,
`game/world/objects/npc.cpp:3542`), `findNpc`, `BodyState`. `Npc::hasStateFlag` has no other
caller, so leaving it in place is harmless (or it may be removed).
