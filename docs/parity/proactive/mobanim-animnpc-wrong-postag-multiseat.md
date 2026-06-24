# Mobsi transition anim: `animNpc` uses the *last* occupied seat's position tag, not the animating NPC's

**Confidence:** Medium

## Original function + address (prose only)

In `Gothic2.exe`, the per-NPC mob-interaction transition animation is selected through
`oCMobInter::GetTransitionNames` (0x0071f5e0) — called from `oCMobInter::StartStateChange`
(0x0071fea0) via vtable slot +0x108 — and the resulting per-NPC name is then played by
`oCMobInter::StartTransitionAniNpc` (0x00720150). The transition name is resolved against the
*NPC's own model / AnimationSolver* (the NPC passed to `StartStateChange`/`StartTransitionAniNpc`),
so each NPC seated at a multi-position mobsi resolves the animation for the slot it actually
occupies. The slot/position itself is bound per-NPC in `oCMobInter::SetIdealPosition`
(0x0071e240) / `SearchFreePosition` (0x0071dfc0); there is no global "current position"
shared between simultaneously-seated NPCs.

## OpenGothic file:line

`game/world/objects/interactive.cpp:1103-1106` (inside `Interactive::animNpc`,
`game/world/objects/interactive.cpp:1087`).

## Divergence

`Interactive::animNpc(solver, t)` builds the position-tagged transition name
(`T_<scheme>_<pos>_S<a>_2_S<b>` / `S_<scheme>_<pos>S<a>`) but takes no NPC argument. To pick the
position tag it loops over *all* attach slots and keeps the tag of the **last** occupied one:

```cpp
for(auto& i:attPos)
  if(i.user!=nullptr) {
    point = string_frm("_",i.posTag());   // overwritten on every occupied slot -> keeps the LAST
    }
```

Two concrete problems, both for multi-seat mobsis (a bench/table exposing e.g.
`ZS_POS0_FRONT` + `ZS_POS0_BACK`, or `ZS_POS0`/`ZS_POS1`):

1. **Wrong seat used.** When two NPCs are seated, resolving the transition for the FRONT NPC
   picks `_BACK` (the last occupied slot), so the FRONT NPC plays the BACK-seat transition
   (wrong facing/offset), and vice-versa. Vanilla resolves each NPC against its own occupied
   slot, so this cross-talk cannot happen.

2. **Internal inconsistency with the rest of the file.** The sibling helpers use the **first**
   occupied slot: `posSchemeName()` (`interactive.cpp:542`) returns on the first `user!=nullptr`,
   and `canQuitAtState()` (`interactive.cpp:803`) feeds that first-slot tag into the
   `T_<scheme>_<pos>_S<state>_2_STAND` name it must match (`// should match with this->animNpc(...)`
   at `interactive.cpp:815`). For multiple occupants `animNpc` (last) and `canQuitAtState`
   (first) can therefore build *different* position tags, breaking the "to-stand exists" gate
   that `detach()` (`interactive.cpp:906`) relies on.

`animNpc` also constructs `point` as `string_frm("_", posTag())`, which becomes a bare `"_"`
when `posTag()` is empty (untagged slot), yielding a malformed `T_<scheme>__S0_2_S1` as the
*first* lookup; it self-heals only because the loop retries with an empty tag. `posSchemeName()`
already returns `""` for untagged slots, and `canQuitAtState` guards with `if(pos.empty())`,
so routing `animNpc` through `posSchemeName()` removes the stray-underscore probe too.

## Proposed patch

Make `animNpc` use the same first-occupied-slot tag as `posSchemeName()`/`canQuitAtState()`,
which fixes the multi-seat cross-talk, the first/last inconsistency, and the bare-`"_"` probe in
one place. `posSchemeName()` is already declared (`interactive.h:60`) and returns the un-prefixed
tag (`"FRONT"`/`"BACK"`/`""`).

OLD (`game/world/objects/interactive.cpp:1090-1106`):
```cpp
  string_frm<12>   ss[2]    = {};
  string_frm       pointBuf = {};
  string_frm       point    = {};

  if(t==Anim::FromStand) {
    st[0] = -1;
    st[1] = state<1 ? 0 : stateNum - 1;
    }
  else if(t==Anim::ToStand) {
    st[0] = state<1 ? 0 : state;
    st[1] = -1;
    }

  for(auto& i:attPos)
    if(i.user!=nullptr) {
      point = string_frm("_",i.posTag());
      }
```

NEW:
```cpp
  string_frm<12>   ss[2]    = {};
  string_frm       point    = {};

  if(t==Anim::FromStand) {
    st[0] = -1;
    st[1] = state<1 ? 0 : stateNum - 1;
    }
  else if(t==Anim::ToStand) {
    st[0] = state<1 ? 0 : state;
    st[1] = -1;
    }

  // NOTE: in original-game oCMobInter::StartTransitionAniNpc (Gothic2.exe 0x00720150) the
  // per-NPC transition ani is resolved against the NPC's own occupied slot, never the last
  // occupied one. Reuse posSchemeName() (the first occupied slot, as canQuitAtState() does)
  // so a multi-seat mobsi does not borrow another NPC's seat tag, and so the to-stand gate
  // in canQuitAtState() stays in sync.
  auto posSc = posSchemeName();
  if(!posSc.empty())
    point = string_frm("_",posSc);
```

Notes:
- `pointBuf` (`interactive.cpp:1091`) is dead and is removed by this edit.
- The downstream loop at `interactive.cpp:1117` already falls back to the empty tag, so untagged
  mobsis are unaffected; the change only removes the spurious `"_"`-prefixed first probe and the
  last-vs-first seat mismatch.

DEFERRED beyond this: threading the actual `Npc*` into `animNpc`/`solveAnim` would be the fully
faithful fix (it would also disambiguate two NPCs on slots that share a tag, e.g. two `ZS_POS`
slots both untagged), but that requires touching `AnimationSolver::solveAnim`
(`animationsolver.cpp:446`) and its callers and is out of scope for a surgical patch.
