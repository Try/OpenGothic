# Theft perception (PERC_ASSESSTHEFT) is gated on `isPlayer()`, but the original fires it for any taker

**Confidence:** Medium

## Original function + address

`oCNpc::DoTakeVob` (Gothic2.exe `@0x007449c0`) is the engine routine that runs when an
NPC physically picks a world `oCItem` off the floor. After the item is successfully moved
into the inventory (the `PutInInv` result is non-null and passes the `oCItem` class check),
the function sets the global `ITEM` Daedalus instance to the picked-up item and then calls
`oCNpc::CreatePassivePerception` (`@0x0075b270`) with perception id `0x11` (= `PERC_ASSESSTHEFT`,
17), passing the taker itself as the perception "other"/origin vob and a null victim vob.

Crucially, this `CreatePassivePerception` call sits on the generic inventory-pickup branch and
is **not** guarded by any "taker is the player" test. The original gates *nothing* on
who is taking the item; it always broadcasts the theft perception to nearby NPCs. The
recipient-side eligibility (skip the taker itself, recipient must be alive, must not be
unconscious/disabled, and must have `PERC_ASSESSTHEFT` registered) is done inside
`CreatePassivePerception`'s candidate loop, not at the send site.

## OpenGothic file:line

`game/world/objects/npc.cpp:3468-3469` (in `Npc::takeItem`):

```cpp
it = addItem(std::move(ptr));
if(isPlayer() && it!=nullptr)
  owner.sendPassivePerc(*this,*this,*it,PERC_ASSESSTHEFT);
```

The recipient-side filtering that mirrors the original's candidate loop already exists in
`WorldObjects::passivePerceptionProcess` (`game/world/worldobjects.cpp:939`): it skips
players/downed NPCs, requires `NpcProcessPolicy::AiNormal`, skips the sender
(`msg.self==&npc`), enforces the per-perception range, and only invokes a recipient that has
the perception registered. So the OpenGothic send site adds an *extra* sender-side
`isPlayer()` condition that has no counterpart in `oCNpc::DoTakeVob`.

## Divergence

In the original engine, whenever **any** NPC picks up a world item, every nearby NPC that has
`PERC_ASSESSTHEFT` registered receives the perception (with `other` = the taker and the `ITEM`
instance set to the taken item). In OpenGothic the perception is only ever emitted when the
**player** is the taker; an NPC looting an item from the ground broadcasts nothing.

For the stock Gothic II `B_AssessTheft` handler the visible effect is small, because that
script early-outs unless `other` is the player — so the most common consequence is just the
extra (suppressed) invocations. The behavioral divergence becomes observable when a script
(stock variants, total-conversion mods, or any guild whose `PERC_ASSESSTHEFT` reacts to
NPC-on-NPC or NPC-on-world theft) relies on witnessing *non-player* pickups: in OpenGothic
those witnesses are never notified, so the engine-level perception stream diverges from the
original. This is purely an over-restrictive sender-side guard, not a script difference.

## Proposed patch

Drop the sender-side `isPlayer()` gate to match `oCNpc::DoTakeVob`; keep the `it!=nullptr`
guard (the original equally requires a successful inventory insertion). All recipient
filtering is already handled in `WorldObjects::passivePerceptionProcess`.

```cpp
// OLD
  it = addItem(std::move(ptr));
  if(isPlayer() && it!=nullptr)
    owner.sendPassivePerc(*this,*this,*it,PERC_ASSESSTHEFT);

// NEW
  it = addItem(std::move(ptr));
  // NOTE: in original-game oCNpc::DoTakeVob @0x007449c0 the PERC_ASSESSTHEFT (0x11) passive
  // perception is broadcast for *any* taker, not only the player; recipient eligibility
  // (skip-self/alive/AiNormal/has-perc) is enforced in WorldObjects::passivePerceptionProcess.
  if(it!=nullptr)
    owner.sendPassivePerc(*this,*this,*it,PERC_ASSESSTHEFT);
```

Grep-verified symbols: `Npc::isPlayer` (`game/world/objects/npc.cpp:548`),
`WorldObjects::sendPassivePerc(Npc&,Npc&,Item&,int32_t)` overload routed via
`World::sendPassivePerc` (`game/world/world.cpp:718`), `PERC_ASSESSTHEFT`
(`game/game/constants.h:426`), `WorldObjects::passivePerceptionProcess`
(`game/world/worldobjects.cpp:939`).
