# Sleep / Wake / Bed-use parity sweep — NO FINDING

**Confidence:** N/A (NO FINDING — engine logic is faithful or behavior is script-driven)

## Scope investigated
NPC lying-in-bed (BS_LIE) state, the can't-perceive-while-asleep gate, the
wake-on-perception path, the standup/wake-from-lie transition, the bed-MOBSI
body-state derivation, and the lie-down physics/alignment.

## What was checked and why each is NOT a divergence

1. **"Can't-perceive-while-asleep" gate.**
   Original `oCNpc::CanSense` (Gothic2.exe @0x00740740) gates perception only on
   SENSE_SMELL (range) and SENSE_SEE (`oCNpc::CanSee` @0x00741c10) — there is NO
   body-state/asleep self-gate. A sleeping (BS_LIE) NPC still perceives; ZS_Sleep
   restricts *which* percepts fire via `Npc_PercEnable` (script). OpenGothic's
   `Npc::canSenseNpc` (`game/world/objects/npc.cpp:5180`) matches: smell-by-range +
   see-by-ray, no body gate. The world perception driver
   (`game/world/worldobjects.cpp:247`) skips only player/dead, not lying NPCs — same
   as original. Faithful.

2. **Perception early-out in `oCNpc::PerceptionCheck` (@0x0075dd30).**
   The function early-returns on the vob EventManager state (vtable slot 0x34;
   cutscene/active-message gate) BEFORE any body-state classification. This is a
   *global* perception throttle, not sleep-specific, and replicating it would shift
   perception timing for every NPC — not surgical. Out of scope.

3. **Bed body-state derivation.** `Interactive::stateMask` → `GameScript::schemeToBodystate`
   (`game/game/gamescript.cpp:1463`) maps a bed scheme ("BEDHIGH"/"BEDLOW", listed in
   `MOB_LIE`) to `BS_LIE`, applied through `Npc::bodyState` (`npc.cpp:3635`). `BS_LIE = 12`
   with no flags (`game/game/constants.h:177`) matches the original/SDK body-state enum.
   Faithful.

4. **Wake / standup-from-lie transition.** `AI_StandUp` for `BS_LIE`
   (`npc.cpp:2814`) plays `setAnim(Anim::Idle)`, which routes through OpenGothic's MDS
   transition solver (auto-selects the `T_*_2_*` get-up transition when defined).
   Original `oCNpc::StandUp` (@0x00682b40, via `EV_StandUp` @0x00683ce0) builds the
   `T_<state>_2_…` transition name from the active layer ani when the message's quick
   flag is set, else snaps to the base ani. The snap-vs-transition distinction belongs
   to the StandUp machinery already marked **deferred** (recover-standupquick,
   wait-standup-from-sit) and is excluded from this hunt. Bed sleep additionally exits
   via the `interactive()` detach branch (`npc.cpp:2808`), never reaching the BS_LIE
   arm. No fresh surgical sleep-specific divergence.

5. **Lie-down physics / alignment.** `Npc::isAlignedToGround` includes `isLie()`
   (`npc.cpp:5253`); `MoveAlgo::accessDamFly` and BS_LIE handling in `aiOutputForward`
   /`playercontrol` are consistent with the original's BS_LIE handling. No divergence
   found.

## Conclusion
NPC sleep/wake/bed-use behavior in OpenGothic is either faithful to Gothic2.exe at the
engine layer or is driven by the `ZS_Sleep` Daedalus state machine. The only standup
transition nuance overlaps already-deferred StandUp work. **NO FINDING.**
