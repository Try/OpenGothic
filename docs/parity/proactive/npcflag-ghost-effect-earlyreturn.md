# NPC_FLAG_GHOST render toggle skipped by C_NPC.effect VFX early-return

**Confidence:** Low (DEFERRED — sub-frame, self-correcting impact)

## Original function + address
The ghost look (semi-transparent NPC body) and the per-NPC visual-FX string
(`C_NPC.effect`) are two independent systems in the original engine. Damage /
immortality / attitude live in `oCNpc::ChangeAttribute` (Gothic2.exe 0x0072ff60),
`oCNpc::OnDamage_Hit` (0x00666610, immortal = `flags & 2` zeroes the descriptor
damage), `oCNpc::IsFriendly` (0x0072f900), `oCNpc::IsHostile` (0x0072f870) and
`oCNpc::IsEnemyBehindFriend` (0x00741600) — all three of which resolve via the
guild attitude matrix (`oCGuilds::GetAttitude`) plus the player perm/temp
attitude, **never** via the `NPC_FLAG_FRIENDS` (bit 0) flag. The GHOST flag
(bit 2) drives only the ghost rendering; it is not consulted by collision,
focus collection (`oCNpc::CollectFocusVob` 0x00733a10 checks only item flags),
targeting or perception. The ghost render state is therefore independent of
whether the NPC currently has a loadable `effect` VFX.

## OpenGothic file:line
`game/graphics/mdlvisual.cpp:373-396` (`MdlVisual::setNpcEffect`), called every
tick from `game/world/objects/npc.cpp:2446` (`Npc::tickAnimationTags`).

## Divergence
`setNpcEffect` couples the two independent systems: when the `effect` string
changes to a value whose VFX fails to load (`vfx==nullptr`), the function does
`hnpcVisual.view = Effect(); return;` at line 379 — returning **before** the
GHOST-flag block at lines 389-396. On the tick where the effect string changes
to a non-loadable value, a simultaneous change of the GHOST flag is not applied
to `view`/`head`/`attach`. It self-corrects on the next tick (now
`hnpcVisualName==s`, the `if` body is skipped, and the ghost block is reached),
so the worst-case observable impact is a single frame of stale ghost state, and
only in the rare case where the effect string and the ghost flag change on the
same tick with a failing VFX load. The common ghost monster (empty `effect`
string) is unaffected: the string-change `if` is never entered, so the ghost
block always runs.

## Proposed patch (DEFERRED — do not apply)
Reason for DEFERRAL: the divergence is sub-frame and self-correcting; it does not
clear the "high-confidence behavioral divergence" bar. Recorded so a future pass
need not re-derive that the GHOST/FRIENDS flag handling is otherwise faithful.

If ever applied, the surgical fix is to decouple the ghost toggle from the
effect-string early-return by hoisting the GHOST block above the string block
(the `view`/`head`/`attach` meshes exist regardless of the effect VFX):

```cpp
// NOTE: in original-game the ghost look (NPC_FLAG_GHOST) and the C_NPC.effect VFX are
// independent; ghost rendering must not be skipped by a failed effect-VFX load.
void MdlVisual::setNpcEffect(World& owner, Npc& npc, std::string_view s, zenkit::NpcFlag flags) {
  const bool nextGhost = (flags & zenkit::NpcFlag::GHOST);
  if(hnpcFlagGhost!=nextGhost) {
    hnpcFlagGhost=nextGhost;
    view.setAsGhost(hnpcFlagGhost);
    head.view.setAsGhost(hnpcFlagGhost);
    for(auto& i:attach)
      i.view.setAsGhost(hnpcFlagGhost);
    }

  if(hnpcVisualName!=s) {
    ... // unchanged effect-string body, incl. the vfx==nullptr early return
    }
  }
```

Grep-verified OG symbols: `hnpcFlagGhost`, `hnpcVisualName`, `view`, `head`,
`attach`, `setAsGhost(bool)` (all in `game/graphics/mdlvisual.{h,cpp}`);
`zenkit::NpcFlag::GHOST` = `1U<<2U` (`lib/ZenKit/include/zenkit/addon/daedalus.hh:167`).
