# HUD focus enemy HP-bar: shown by script-dead state instead of current hitpoints

**Confidence:** Medium

## Original function + address (prose only)

The focused-target name and HP bar are drawn by `oCGame::UpdatePlayerStatus`
(Gothic2.exe @ `0x006c3140`), called every HUD frame from `oCGame::Render`
(@ `0x006c89d8`) while the on-screen status flag `oCGame[0xa0]` is set. For the
NPC branch (focus VOB resolved via `oCNpc::GetFocusVob`, RTTI-checked against
`oCNpc::classDef`) the original reads the focus NPC's current and max hitpoints:

- `cur = GetAttribute(npc, ATR_HITPOINTS)`  (attribute index 0)
- `max = GetAttribute(npc, ATR_HITPOINTSMAX)` (attribute index 1)

It then inserts and fills the enemy status bar (`oCViewStatusBar` at
`oCGame[0x98]`) only when **`0 < cur`** (current hitpoints strictly positive),
in addition to the option gate `show_FocusBar != 0`. The bar range is
`SetRange(0, max)` and the value is `SetValue(cur)`, i.e. fill = cur/max.

The key point: the original predicate for showing the focus HP bar is purely the
**current hitpoint count being above zero**, not the NPC's scripted life-state.

## OpenGothic file:line

`game/mainwindow.cpp:600-603` (`MainWindow::paintFocus`):

```cpp
if(focus.npc!=nullptr && !focus.npc->isDead()) {
  float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
  drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
  }
```

`Npc::isDead()` (`game/world/objects/npc.cpp:4195`) is **script-state based**:

```cpp
bool Npc::isDead() const {
  return owner.script().isDead(*this);   // ZS_DEAD state, not hitpoints
  }
```

## Divergence

The original gates the focus enemy HP bar on `currentHitpoints > 0`, whereas
OpenGothic gates it on `!isDead()`, where `isDead()` reflects the scripted
`ZS_DEAD` AI state rather than the hitpoint count. These can disagree:

- An NPC reduced to `HITPOINTS <= 0` in the current frame, before its AI has
  transitioned into `ZS_DEAD`, still has `isDead()==false`, so OpenGothic keeps
  drawing the focus HP bar for a frame (or longer if the corpse never enters
  `ZS_DEAD`, e.g. instantly-removed/despawned actors); the original hides it the
  moment hitpoints reach zero.
- Conversely, a scripted actor put into `ZS_DEAD` while still carrying positive
  hitpoints would be hidden by OpenGothic but shown by the original.

The fill computation itself (`cur/max` with range `[0,max]`) already matches.

## Proposed patch

`ATR_HITPOINTS` / `ATR_HITPOINTSMAX` are grep-verified in
`game/game/constants.h:473-474`; `Npc::attribute(Attribute)` is verified in
`game/world/objects/npc.h:216`.

OLD (`game/mainwindow.cpp:600`):
```cpp
  if(focus.npc!=nullptr && !focus.npc->isDead()) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }
```

NEW:
```cpp
  // NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140 the focus enemy
  // HP bar is gated on current hitpoints (0 < GetAttribute(npc,ATR_HITPOINTS)),
  // not on the scripted ZS_DEAD life-state.
  if(focus.npc!=nullptr && focus.npc->attribute(ATR_HITPOINTS)>0) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }
```

This is a one-line predicate change (`!isDead()` -> `attribute(ATR_HITPOINTS)>0`)
against grep-verified OpenGothic symbols, no layout/pixel change.
