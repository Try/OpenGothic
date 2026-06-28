# Wld_DetectNpc / Wld_DetectNpcEx ignore the player as the *origin* of detection

**Confidence:** High

## Original function + address (prose)

Both world-detection externals forward to engine helpers on the *origin* NPC
(the `self`/first argument):

- `Wld_DetectNpc`   handler (`FUN_006e...`) → `oCNpc::FindNpc`   @ `0x00740a80`
- `Wld_DetectNpcEx` handler (`FUN_006e15c0`) → `oCNpc::FindNpcEx` @ `0x00740b80`

The very first thing **both** `FindNpc` and `FindNpcEx` do, before touching the
nearby-NPC list, is call the origin's virtual `IsAPlayer()` (vtable slot
`0x104`, body @ `0x007425a0`, which is simply `this == player`) and **return
`NULL` immediately when the origin is the player hero**:

```
iVar = this->IsAPlayer();   // vtable[0x104]
if (iVar != 0) return 0;     // origin is the player → no detection at all
```

That the same vtable slot `0x104` is reused later as the per-candidate
player-exclusion test (`excludePlayer` path, `param_5`) confirms it is
`IsAPlayer`. So in the original engine, `Wld_DetectNpc(hero, …)` and
`Wld_DetectNpcEx(hero, …)` **always return FALSE** — these queries are
AI-origin-only and never report anything when the detecting NPC is the player.

## OG file:line

`game/game/gamescript.cpp`
- `wld_detectnpc`   @ ~1866 (`GameScript::wld_detectnpc`)
- `wld_detectnpcex` @ ~1896 (`GameScript::wld_detectnpcex`)

Both early-out only on `npc==nullptr`; neither has a player-origin guard. The
detection lambda excludes *self* (`&n!=npc`) and (for Ex) the player as a
*candidate*, but nothing prevents the player from being the *origin*. With the
hero as origin, OpenGothic happily returns a nearby NPC where the original
returns false. (`Npc::isPlayer()` exists — `game/world/objects/npc.h:116`.)

## Divergence

When a script calls `Wld_DetectNpc(self,…)` / `Wld_DetectNpcEx(self,…)` with the
player hero as `self`, OpenGothic performs the search and can return `TRUE` with
`other` set to a found NPC, while the original engine unconditionally returns
`FALSE` and leaves `other` untouched. The omitted guard is identical in both
handlers and is the *first* statement of each engine helper.

## Proposed patch (OLD/NEW)

`wld_detectnpc`:

```
OLD:
bool GameScript::wld_detectnpc(std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }

NEW:
bool GameScript::wld_detectnpc(std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  // NOTE: in original-game oCNpc::FindNpc @0x00740a80 (the Wld_DetectNpc handler) the very first
  // statement returns NULL when the ORIGIN npc is the player (this->IsAPlayer(), vtable 0x104 @
  // 0x007425a0 == `this==player`). These detections are AI-origin-only; with the hero as origin the
  // original always reports false. OpenGothic omitted the guard and detected NPCs around the player.
  if(npc->isPlayer())
    return false;
```

`wld_detectnpcex`:

```
OLD:
bool GameScript::wld_detectnpcex(std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild, int player) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  Npc*  ret =nullptr;

NEW:
bool GameScript::wld_detectnpcex(std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild, int player) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  // NOTE: in original-game oCNpc::FindNpcEx @0x00740b80 (the Wld_DetectNpcEx handler) the very first
  // statement returns NULL when the ORIGIN npc is the player (this->IsAPlayer(), vtable 0x104 @
  // 0x007425a0 == `this==player`). Same origin-only guard as Wld_DetectNpc/FindNpc @0x00740a80.
  if(npc->isPlayer())
    return false;
  Npc*  ret =nullptr;
```
