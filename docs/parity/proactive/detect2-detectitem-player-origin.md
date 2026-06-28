# Wld_DetectItem returns null when origin NPC is the player (missing IsSelfPlayer guard)

**Confidence:** HIGH

**Original fn + address:** `oCNpc::DetectItem` (Gothic2.exe @0x0073fd40), the `Wld_DetectItem`
external handler. The very first statement of the function calls the origin NPC's virtual
`IsSelfPlayer` (vtable+0x104, decompile-verified) and returns null immediately when it is non-zero:
the function is an NPC-AI sensing primitive that never operates for a hero origin. This is the exact
same entry guard already present in its siblings `oCNpc::FindNpc` @0x00740a80 (Wld_DetectNpc) and
`oCNpc::FindNpcEx` @0x00740b80 (Wld_DetectNpcEx), both of which OpenGothic has already fixed.

**OG file:line:** `/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:1940-1967`
(`GameScript::wld_detectitem`).

**Divergence:** `wld_detectitem` validates the origin `npc` for null but never excludes a player
origin. When the hero is the origin, OpenGothic walks the perception list and can return TRUE with
`item` set, whereas the original always returns FALSE (null) for a player origin. Same missing-guard
class as the applied Wld_DetectNpc/Ex player-origin fix; the per-candidate mask/no-detect filter
(0x800000, main_flag|flags) already matches the original and is unchanged.

**Proposed patch:**

OLD (gamescript.cpp, top of `wld_detectitem`):
```cpp
bool GameScript::wld_detectitem(std::shared_ptr<zenkit::INpc> npcRef, int flags) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }

  Item* ret =nullptr;
```

NEW:
```cpp
bool GameScript::wld_detectitem(std::shared_ptr<zenkit::INpc> npcRef, int flags) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return false;
    }
  // NOTE: in original-game oCNpc::DetectItem @0x0073fd40 (the Wld_DetectItem handler) the very first
  // statement returns null when the detecting/origin NPC is the player (vtable+0x104 = IsSelfPlayer,
  // decompile-verified) -- identical to the Wld_DetectNpc/Ex player-origin guard. Wld_DetectItem is an
  // NPC-AI sensing primitive and always returns FALSE for a hero origin. OpenGothic lacked the guard.
  if(npc->isPlayer())
    return false;

  Item* ret =nullptr;
```
