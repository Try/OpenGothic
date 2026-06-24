# Pickpocket: engine-side steal-container / "caught" detection is unimplemented

**Confidence:** High (that the divergence exists); the fix itself is **DEFERRED** (whole-feature gap, not surgical).

## Original function + address (prose only)

In `Gothic2.exe` the engine-driven pickpocket flow lives in `oNpcInv.cpp` / `oInventory.cpp`:

- `oCNpc::OpenSteal` (entry `0x00762430`) is the engine entry point for pickpocketing a *living* focus NPC. It grabs the focus NPC as the victim, validates it (not dead, not the player's own guild value 0x3a, not in an interaction), sets the script `SELF`/`OTHER` instances, then calls the Daedalus function `G_CANSTEAL` (string at `0x008b9d98`) via `zCParser::CallFunc`. If `G_CANSTEAL` returns nonzero and the victim has not detected the thief, it allocates an `oCStealContainer`, calls `CreateList`, and opens a steal inventory view; otherwise it spawns a `T_DONTKNOW` conversation message and aborts.
- `oCNpc::IsVictimAwareOfTheft` (entry `0x00761ef0`) is the engine "caught" check. It returns 1 (the victim notices) when the victim `HasVobDetected` the thief while alive, or when the victim is not in a steal-permitting body state (it only tolerates `BS_STAND == 0` and `BS_SNEAK == 0xe`) and is neither unconscious nor dead.
- `oCStealContainer::CreateList` (entry `0x0070ade0`) builds the list of stealable items, skipping items that carry flag `0x40000000` or flag `0x10`.
- `oCNpc::AssessTheft_S` (entry `0x0075c6d0`) and the take path `oCNpc::DoTakeVob` (entry `0x007449c0`, broadcasting `CreatePassivePerception(this, 0x11 /*PERC_ASSESSTHEFT*/, ...)`) are the perception side of being seen taking an item.

The interesting parity point is that the entire `OpenSteal` / `oCStealContainer` / `IsVictimAwareOfTheft` machinery — the engine-side "open the victim's pocket, pick an item, and get caught if detected" mechanic — has **no counterpart in OpenGothic at all**.

## OpenGothic file:line

- `game/game/playercontrol.cpp:340` `PlayerControl::interact(Npc&)` — interacting with a living NPC only ever calls `other.startDialog(*pl)`; there is no steal branch. The loot/ransack branch (`inv.ransack`) is reached only when `other.isDown()` (steal-from-corpse), never for a conscious victim.
- Repo-wide: no symbol matches `OpenSteal`, `G_CanSteal`, `IsVictimAwareOfTheft`, `StealContainer`, or any "steal from living NPC" path (grep of `game/` for `steal`/`pickpocket` returns only `BS_PICKPOCKET`, `TALENT_PICKPOCKET`, `PERC_ASSESSTHEFT`, and the `directmemory.cpp` `oCStealContainer::CreateList` byte-patch addresses — no behavioral implementation).

## Divergence

OpenGothic implements pickpocketing **purely** through the Daedalus dialog flow (`B_Beklauen` → DEX/talent check → `Npc_RemoveInvItems`/`CreateInvItems`). It never invokes `G_CANSTEAL`, never opens a steal container, and — critically — never runs the engine "caught" detection `IsVictimAwareOfTheft`. In the original, even within the script-driven G2 pickpocket dialog, the engine `OpenSteal`/steal-container mechanic is a parallel, input-bindable way to rob a conscious NPC, gated by `G_CANSTEAL` and by `HasVobDetected`/body-state detection. In OpenGothic that engine path is simply absent, so:

- `G_CANSTEAL` is dead script that the engine never calls;
- there is no engine-side "victim noticed you" check tied to detection + body state;
- there is no steal-container item filter (original excludes flag `0x40000000` / `0x10` items).

## Proposed patch

**DEFERRED.** Reason: this is a whole-subsystem gap, not a one-line behavioral divergence. A faithful fix would require reimplementing `oCNpc::OpenSteal`, an `oCStealContainer` model (item list with the `0x40000000`/`0x10` flag filter), an `IsVictimAwareOfTheft`-equivalent detection (`HasVobDetected` + body-state gate), the `G_CANSTEAL` call site, and a steal inventory UI view — plus the wiring of an input action to trigger it on a living focus NPC. That spans `playercontrol.cpp`, `npc.cpp`, the inventory model, and the UI, and cannot be made surgical or build-verifiable as a single change. No grep-verified OG symbol exists to anchor a minimal patch.

Note (for any future implementation):
`// NOTE: in original-game oCNpc::OpenSteal @0x00762430 a living focus NPC can be pickpocketed via an engine steal-container gated by Daedalus G_CANSTEAL; detection of being caught is oCNpc::IsVictimAwareOfTheft @0x00761ef0 (victim HasVobDetected the thief while alive, or victim not in BS_STAND/BS_SNEAK and not unconscious/dead). OpenGothic has no engine-side equivalent; pickpocket is dialog-only.`

Adjacent already-known item: the `PERC_ASSESSTHEFT` `isPlayer()` gate at `game/world/objects/npc.cpp:3481` (deferred separately) is the only engine theft-perception code present, and is narrower than the original's unconditional `CreatePassivePerception(..., 0x11, ...)` in `oCNpc::DoTakeVob @0x007449c0`.
