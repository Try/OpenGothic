# Focus-name divergence: missing " (locked)" suffix on a fixated combat target

**Confidence:** High that the divergence is real; **DEFERRED** for the fix (requires a
gameplay subsystem OpenGothic does not implement).

## Original function + address (prose)

`oCGame::UpdatePlayerStatus` at **Gothic2.exe 0x006C3140** is the routine that builds and
prints the focus target's HUD name. After it has assembled the target's base name string
(for an NPC via `oCNpc::GetName` @0x0072F820 → `name[0]`; for a MOB via the virtual
`oCMOB::GetName` @0x0071BC30; for an item via `oCItem::GetName` @0x00712880), it consults
the static flag **`oCNpc::s_bTargetLocked`**. When that flag is non-zero (the player is
currently *target-locked* / fixated on a combat target), the routine appends a fixed suffix
string before calling `zCView::Print`. The appended literal lives in `.rdata`
(Ghidra-named `s___locked_`) and its exact content is **" (locked)"** (a leading space, then
the parenthesised word; verified by reading the bytes in `system/Gothic2.exe`). So in the
original, a locked target's HUD name reads e.g. `Scavenger (locked)`, while an un-locked
focus shows just `Scavenger`. This is purely a *which-name-string-is-shown* difference: the
same target shows two different name strings depending on the lock state.

## OpenGothic file:line

- `game/mainwindow.cpp:593` `MainWindow::paintFocus(...)` — draws the focus label.
- `game/mainwindow.cpp:616,622` — `focus.displayName()` is measured and drawn verbatim, with
  no lock-state suffix.
- `game/world/focus.cpp:30` `Focus::displayName()` — returns the bare NPC/MOB/item name.
- `game/world/objects/npc.cpp:794` `Npc::displayName()` returns `hnpc->name[0]` only.

## Divergence

The original game decorates the focus name with " (locked)" whenever the player has a locked
combat target (`oCNpc::s_bTargetLocked`). OpenGothic always renders the undecorated
`displayName()`, so a locked target never visually indicates the lock through its HUD name.

## Proposed patch — DEFERRED

DEFERRED. OpenGothic does not implement the original combat *target-lock* mechanism at all:
`KeyCodec::keyLockTarget` is parsed from settings (`game/gothic.cpp:168`,
`game/utils/keycodec.cpp:445`) but is **never mapped to a `KeyCodec::Action`**
(`game/utils/keycodec.h:22` enum has no lock action) and nothing sets or tracks a persistent
"locked target" state equivalent to `oCNpc::s_bTargetLocked`. There is therefore no faithful
boolean to gate the suffix on. Synthesising one from existing state (e.g.
`player.currentTarget()==focus.npc` while in fight stance) would fire in many situations the
original would not (the original suffix appears only after an explicit lock key-press), i.e. a
false-positive name change rather than a faithful reproduction. Appending " (locked)" without
the underlying lock subsystem would be guesswork, so per "empty beats false positives" no
surgical, build-verifiable name fix is proposed.

If/when target-locking is implemented, the one-line HUD change would be, in
`MainWindow::paintFocus` (`game/mainwindow.cpp:616-622`):

```
// OLD
auto tsize = fnt.textSize(focus.displayName());
...
fnt.drawText(p,ix,iy,focus.displayName());

// NEW
// NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140, when oCNpc::s_bTargetLocked
// is set the focus target's HUD name is suffixed with the literal " (locked)" (rdata string
// s___locked_) before zCView::Print; un-locked focus shows the bare name.
string_frm label(focus.displayName(), isTargetLocked ? " (locked)" : "");
auto tsize = fnt.textSize(label);
...
fnt.drawText(p,ix,iy,label);
```

(blocked on a real `isTargetLocked` source mirroring `oCNpc::s_bTargetLocked`).
