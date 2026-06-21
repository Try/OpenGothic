# Mobsi missing 20s "reserved-by-NPC" walk-to-mob lock

**Confidence:** High (root cause confirmed in disassembly and OG source; fix touches save/load, see risk note).

## Original function + address (prose only)
- `oCMobInter::MarkAsUsed` (Gothic2.exe @0x00720f20): stores `usedBy = npc` into the mob
  field at +0x22c and `usedUntil = currentTime + timeout` into the float field at +0x230.
- `oCNpc::EV_UseMob` (Gothic2.exe @0x00754290): when an NPC begins the AI_UseMob action and
  has located the target mob via `FindMobInter`, it issues the movement message that sends the
  NPC walking to a free position and then calls `MarkAsUsed(mob, 20000.0, npc)` — i.e. it
  reserves that mob for this NPC for 20 seconds, **before** the NPC has attached to any slot.
- `oCMobInter::CanInteractWith` (Gothic2.exe @0x00720f40) and `oCMobInter::IsAvailable`
  (@0x00720ec0) both contain the same gate: if the requesting actor is **not** the player
  (vtbl `IsAPlayer` == 0), and `usedBy != 0 && usedBy != npc && currentTime < usedUntil`, the
  mob reports itself unavailable and the function returns 0. An expired reservation
  (`currentTime >= usedUntil`) is cleared (`usedBy = 0`) and the actor is allowed through. The
  player is always exempt from this gate.

Net behavior: once NPC A commits to walking toward a bench/bed/forge, that mob is locked to A
for 20s so a second NPC B cannot also target it during the walk — even though no attach slot is
occupied yet. The player ignores the reservation entirely.

## OpenGothic file:line
- `game/world/worldobjects.cpp:859` `WorldObjects::availableMob` — candidate filter uses only
  `Interactive::isAvailable()`.
- `game/world/objects/interactive.cpp:759` `Interactive::isAvailable()` — checks only
  `attPos[].user`; there is no temporal reservation field anywhere on `Interactive`
  (grep of `interactive.h` shows no `usedBy`/`reservedBy`/`usedUntil`).
- `game/world/objects/npc.cpp:2641` `AI_UseMob` — when the mob is too far the NPC sets
  `go2.set(pos)` and re-queues the action (npc.cpp:2674) without reserving the mob.

## Divergence
While an NPC is walking to a mob (`go2` set, action re-queued, not yet attached), the mob's
`attPos[].user` slots are all still null, so `isAvailable()` returns true. A second NPC running
its routine and calling `availableMob` for the same scheme will therefore also select that mob
and walk to it. The original's 20s reservation prevents exactly this two-NPC race. Visible as
two NPCs converging on the same bench/bed and one of them failing to attach (or thrashing) once
the first arrives — the original instead steers the second NPC away (mob reports unavailable),
or lets it wait for the reservation to lapse.

## Proposed patch
Add a non-player-only temporal reservation mirroring `usedBy`/`usedUntil`.

`game/world/objects/interactive.h` — new private fields next to `attPos`:
```
// NOTE: in original-game oCMobInter::MarkAsUsed (Gothic2.exe @0x00720f20) the mob stores a
// "reserved by NPC until time" pair (+0x22c/+0x230); EV_UseMob (@0x00754290) sets it to 20000ms
// when an NPC starts walking toward the mob, and CanInteractWith/IsAvailable (@0x00720f40/
// 0x00720ec0) reject a *different* NPC (player exempt) while the reservation is live.
Npc*      reservedBy    = nullptr;
uint64_t  reservedUntil = 0;
```
New public API on `Interactive`:
```
void reserveFor(Npc& npc);              // sets reservedBy=&npc, reservedUntil=tick+20000
bool isReservedForOther(const Npc&) const;
```
`game/world/objects/interactive.cpp` — implementations and `isAvailable` gate are kept where the
existing reservation semantics live; gate only the **walk-time** candidate search, not attach:
```
void Interactive::reserveFor(Npc& npc) {
  reservedBy    = &npc;
  reservedUntil = world.tickCount() + 20000;
  }
bool Interactive::isReservedForOther(const Npc& npc) const {
  if(npc.isPlayer())
    return false;                                   // player exempt (original)
  if(reservedBy==nullptr || reservedBy==&npc)
    return false;
  return world.tickCount() < reservedUntil;          // live reservation by someone else
  }
```
`game/world/worldobjects.cpp:872` (availableMob) — OLD:
```
    if(i.isAvailable() && i.checkMobName(dest)) {
```
NEW:
```
    if(i.isAvailable() && !i.isReservedForOther(pl) && i.checkMobName(dest)) {
```
`game/world/objects/npc.cpp` AI_UseMob, at the too-far branch (npc.cpp:2673-2677) — reserve the
mob the moment this NPC commits to walking to it (matching EV_UseMob's MarkAsUsed):
```
        if(currentInteract==nullptr && !MoveAlgo::isClose(*this, pos, MAX_AI_USE_DISTANCE)) {
          inter->reserveFor(*this);   // NOTE: original EV_UseMob @0x00754290 MarkAsUsed(20000ms)
          go2.set(pos);
          queue.pushFront(std::move(act));
          return;
          }
```
Stale-pointer safety: extend `Interactive::postValidate` (interactive.cpp:172) to null
`reservedBy` if it no longer points at a live user, same pattern already used for `attPos[].user`.

### Risk / open item
The reservation is transient (clears within 20s) and is **not** archived in the original
(`oCMobInter::Archive` does not persist +0x22c/+0x230). So `reservedBy`/`reservedUntil` should
**not** be added to `Interactive::save/load`; this keeps the save format unchanged and avoids a
serialized raw `Npc*`. The only persistence concern is `postValidate` after load, handled above.
Because this adds a behavior (not just a numeric tweak) the patch is offered as high-confidence
but should be build- and play-verified; if a strictly numeric/single-site fix is required by the
"surgical only" rule, mark **DEFERRED** pending owner decision on the new field + API surface.
