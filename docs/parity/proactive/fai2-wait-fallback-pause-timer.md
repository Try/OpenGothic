# FAI: missing engine "wait-when-no-band" inter-action pause fallback

**Confidence:** Medium (the divergence is real and located; behavioural impact is
a combat-pacing nuance and OpenGothic already has a *different*, deliberately
tuned pause model, so this is **DEFERRED** — reason at the end).

## Original function + address (prose only)

`oCNpc::FindNextFightAction` (Gothic2.exe @0x0067d680, `oNpc_Fight.cpp`) walks the
fixed priority list of fight-move bands and returns the first one that yields a
move. Two of its exits are *not* a fight-AI bank at all but engine action codes,
and both are driven by a per-NPC fight-pause timer:

- The NPC carries a float pause timer (field `npc+0x9e0`) and a pending/forced
  action slot (`npc+0x9e4`, default -1).
- After the first three bands have been considered (loop index > 2), on every
  remaining band the function checks: if a forced action is pending
  (`npc+0x9e4 != -1`) it consumes/returns it (or returns engine code `0x16` if no
  band matched); otherwise, **if no band has matched yet AND the pause timer
  `npc+0x9e0` is still > 0, it returns the WAIT action (engine code `0x13`)**
  instead of forcing any move.

The executor `oCNpc::ThinkNextFightAction` (@0x0067e350) closes the loop: each
tick it decrements `npc+0x9e0` by the frame time and clamps to 0; after it
handles a WAIT (`0x13`) idle action it re-arms `npc+0x9e0` to `200.0` ms (and
holds spacing — steps `_Backward` if beyond ~0.25*reach, else `_Stand`); on its
default / fall-through idle path it re-arms `npc+0x9e0` to `400.0` ms. Net effect:
the engine injects an automatic inter-action pause whenever the NPC has no
higher-priority band to satisfy — even when the *scripts* never emitted a WAIT
move — so a vanilla fighter that is between useful actions stands / holds spacing
for 200-400 ms rather than immediately re-selecting another move.

## OpenGothic file:line

- `game/game/fightalgo.cpp:110` — `FightAlgo::fillQueue` ends with an
  unconditional `fillQueue(owner,ai.my_w_nofocus);`. When no earlier band matches,
  this catch-all bank (which normally contains turn/approach moves) always yields
  a move; there is no pause-timer fallback and no engine-`0x13`/`WAIT` equivalent.
- `game/world/objects/npc.cpp:1656` and `:1662` — the fight executor skips
  `fghAlgo.nextFromQueue` only while `faiWaitTime`/`waitTime` are in the future.
- `game/world/objects/npc.cpp:1919-1928` (`implFaiWait(200)` / `implFaiWait(300)`)
  and `:1967` (`faiWaitTime = owner.tickCount()+dt;`) — OpenGothic's only fight
  pacing: `faiWaitTime` is armed **solely** when a `WAIT` / `WAIT_LONGER` move is
  *drawn from a script bank* (200 / 300 ms). Grep-verified symbols:
  `faiWaitTime`, `implFaiWait`, `FightAlgo::MV_WAIT`, `ai.my_w_nofocus`.

## Divergence

In the original, the inter-action pause is an **engine-level** behaviour: any
frame in which `FindNextFightAction` finds no demanding band (and the pause timer
is running) returns WAIT and re-arms the timer (200 ms after a wait, 400 ms on the
idle fall-through). OpenGothic has no such fallback: when no band matches,
`fillQueue` always returns a `my_w_nofocus` move, and a pause only occurs if a
*script* bank happens to contain a `WAIT`/`WAIT_LONGER` entry. Consequently
OpenGothic NPCs that are between useful actions (e.g. just out of preferred range,
or after a sequence with no script WAIT) keep selecting moves and pace less than
vanilla — combat reads as slightly more relentless and the "stand / hold spacing"
beat the original inserts is absent.

## Proposed patch

**DEFERRED.** A faithful port is not a surgical one-liner: it requires (a) a new
per-NPC fight-pause timer mirroring `npc+0x9e0` with the 200 ms / 400 ms re-arm
points, (b) a WAIT fallback inside `FightAlgo::fillQueue` (or its caller) gated on
that timer instead of falling through to `my_w_nofocus`, and (c) executor changes
in `Npc::implTickFight` around `npc.cpp:1656/1662`. This intersects OpenGothic's
already-tuned `faiWaitTime` / `loopNextTime` pacing (the dev NOTE at
`npc.cpp:1918` shows this band has been hand-tuned for test cases such as wolves
jumping back), so changing it risks regressing combat feel and needs runtime A/B
validation rather than a static gate. Documented here for traceability; no code
change made.
