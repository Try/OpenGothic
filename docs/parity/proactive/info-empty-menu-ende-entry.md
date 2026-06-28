# Info menu: missing explicit "end" entry when an NPC has no remaining infos

**Confidence:** Low–Medium (decompile-confirmed structural divergence; trigger is rare in shipped Gothic 2 content because most NPCs define a permanent EXIT topic, so a verifiable, surgical one-liner is not available — see DEFERRED).

## Original function + address (prose)

`oCInformationManager::CollectInfos` @ `0x00661aa0` builds the un-important dialog
choice list each time the menu is (re)shown. Its logic:

1. `zCViewDialogChoice::RemoveAllChoices()`.
2. `iVar4 = oCInfoManager::GetInfoCountUnimportant(self,player)` (@ `0x00702c00`).
3. For `i = 0 .. iVar4-1`: `oCInfo::GetText(GetInfoUnimportant(self,player,i))` is added via
   `AddChoice` — i.e. all currently-available un-important infos, already in `nr` order
   (`oCInfoManager::CompareInfos` @ `0x007026f0`, a stable ascending-by-`nr` insertion in the
   ctor @ `0x007023f0`).
4. **When `iVar4 == 0` (no un-important infos left):**
   - if `this->importantCount` (field `+0x50`, the `GetInfoCountImportant` value captured at
     session start in `oCInformationManager::Update` @ `0x00660bb0`, line writing `+0x50`) is
     `> 0` → transition to **`OnExit`** (the dialog auto-closes; this is the "NPC walked up,
     said its important line, and leaves" path).
   - else (`importantCount == 0`, i.e. a *player-initiated* dialog that has no topics at all)
     → it appends **one explicit end choice** (`AddChoice(s_ENDE_)`) and keeps the menu open;
     the dialog is only closed once the player clicks that single entry.

So the engine distinguishes *NPC-initiated* exhaustion (silent auto-close) from
*player-initiated* "nothing to say" (open a one-line menu the player must dismiss).

`oCInfo` field map confirmed while tracing this: `+0x1c` npc-instance, `+0x20` `nr`,
`+0x24` important, `+0x28` condition-fn, `+0x2c` info-fn, `+0x48` permanent, `+0x4c` told
(`oCInfo::SetTold` @ `0x00703910`; `oCInfo::Info` @ `0x00703970` sets `+0x4c = 1` right after
calling the info function). The menu/count filters all keep an info when
`permanent != 0 || told == 0` — matching OpenGothic's `npcKnowsInfo && !permanent` skip, so the
told/permanent gating itself is faithful.

## OpenGothic file:line

- `game/game/gamescript.cpp:885` `GameScript::dialogChoices` — returns an empty list when no
  important and no un-important infos qualify.
- `game/ui/dialogmenu.cpp:102-103`:
  ```cpp
  else if(choice.size()==0 && state!=State::Idle && !haveToWaitOutput()) {
    close();
    }
  ```
  OpenGothic *always* `close()`s when the choice list is empty. It never reproduces the
  `importantCount == 0` branch that keeps the menu open with a single explicit "end" entry.

## Divergence

For an NPC that has **no permanent EXIT info** and whose one-shot topics are all exhausted (or
who simply had no qualifying topics): re-initiating dialog in the original opens a dialog window
showing a single end entry that the player clicks to leave; OpenGothic instead returns an empty
choice list and immediately closes (the dialog effectively never opens). The two engines also
differ in *who* closes the dialog — original distinguishes auto-close (had important infos) from
the explicit end-entry (purely player-initiated, no infos), whereas OpenGothic collapses both to
an immediate `close()`.

## Proposed patch — DEFERRED

DEFERRED, reasons:
1. **Rare trigger / false-positive risk.** Virtually all shipped Gothic 2 NPCs define a permanent
   `DIA_*_EXIT` (permanent=TRUE) info that keeps `GetInfoCountUnimportant >= 1`, so the
   `iVar4 == 0` branch — and hence the divergence — almost never fires in real content. Honoring
   "Empty beats false positives," this does not warrant changing the common-path close logic.
2. **Not a surgical, build-verifiable one-liner.** A faithful fix needs (a) the localized text
   `s_ENDE_` resolves to (not yet identified — the warm decompiler exposes it only as an
   auto-named string symbol), and (b) wiring a synthetic "end" `DlgChoice` with a handler that
   closes the dialog, plus tracking whether the just-finished session contained important infos
   (the `+0x50` `importantCount` semantics) to decide auto-close vs. explicit-entry. That is
   menu-infrastructure work, not a localized edit, and cannot be verified by a build alone.

If pursued later: in `DialogMenu`, when `choice.size()==0` and the session had **no** important
infos, push a single end entry (text = resolved `s_ENDE_`) instead of calling `close()`, and only
auto-`close()` when the exhausted session *did* play important infos.
`// NOTE: in original-game oCInformationManager::CollectInfos @0x00661aa0, an empty un-important`
`// list yields OnExit (auto-close) iff GetInfoCountImportant>0 this session, else an explicit`
`// AddChoice(s_ENDE_) keeping the menu open.`
