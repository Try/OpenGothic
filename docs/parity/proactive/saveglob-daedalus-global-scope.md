# Savegame .dat-global persistence scope: OpenGothic persists FLOAT/STRING/INSTANCE globals the original never saves

**Confidence:** Medium (divergence is address-verified and high-confidence; player-observable impact in the
base game is low, and the parity-correct fix removes OpenGothic functionality — see DEFERRED below).

## Original function + address (prose only)

The original Gothic2.exe persists the script-global (.dat) variables of a savegame through
`zCParser::SaveGlobalVars` at `0x00796c40`, called once from `oCGame::WriteSavegame` at the call
site `0x006c5942`; the load side is `zCParser::LoadGlobalVars` at `0x00797170`, called from
`oCGame::LoadSavegame` at `0x006c6d5d`.

`SaveGlobalVars` walks the parser symbol table and writes a symbol **only** when all of the
following hold (read from the symbol property word at `+0x20` / flag byte at `+0x22`):

- the symbol type field (`& 0xf000`) equals `0x2000`, i.e. `zPAR_TYPE_INT` (INT) — floats
  (`0x1000`), strings (`0x3000`), classes/instances (`0x4000`/`0x7000`) are skipped;
- the element count (`& 0x0fff`) is non-zero;
- no symbol flag bit is set (`(byte[+0x22] & 0x3f) == 0`), i.e. not const, not class-member, not
  external, not return, not merged.

In other words, the original savegame persists **only non-const, non-member, non-external INT
globals**. Every runtime-mutated global FLOAT, global STRING and global INSTANCE variable is
**dropped on save** and therefore re-initialised to its compiled `.dat` default on the next load.
(Per-instance state such as the hero is restored by other means — `oCGame::InitNpcAttitudes`
`0x006c61d0`, instance re-resolution — not by `SaveGlobalVars`.)

## OpenGothic file:line

- `game/game/gamescript.cpp:704` — `GameScript::saveSym(...)`
- `game/game/gamescript.cpp:570` — `GameScript::saveVar(...)` (drives `saveSym` over every symbol)
- `game/game/gamescript.cpp:579` — `GameScript::loadVar(...)`

## Divergence

`GameScript::saveSym` persists not only INT globals but also FLOAT, STRING and INSTANCE globals
(each guarded by `!is_member() && !is_const() && count()>0`, INSTANCE additionally serialised by
NPC/item id). `loadVar` restores all four types. Consequently, after a save+load cycle OpenGothic
keeps the **runtime** value of every global float / string / instance variable, whereas the
original resets those globals to their `.dat` init values. The persisted set is a strict superset
of the original's, so this is an over-restore ("restores wrong") rather than a dropped field:
a script that mutates a global float/string/instance and later reads it without re-assigning it
will observe the carried-over value under OpenGothic but the init value under Gothic2.exe.

INT globals themselves match: OpenGothic's `count()>0 && !is_member() && !is_const()` guard
covers the same symbol set the original's INT-only filter selects, so no INT global is dropped.

## Proposed patch

**DEFERRED.**

Reason: the parity-correct change is to make `GameScript::saveSym` emit *only* non-const,
non-member INT globals (matching `zCParser::SaveGlobalVars @0x00796c40`), i.e. remove the FLOAT /
STRING / INSTANCE branches from the save path (load can stay tolerant for backward compatibility).
That is a *removal* of persisted state, not an addition, so it does not fit the "bump
Serialize::Current + guard on load" pattern, and it is high-risk:

1. The base game's observable impact is low — Gothic II's main scripts rely on INT globals (and
   per-instance/NPC state) for persistent quest/world bookkeeping, with very few runtime-mutated
   global floats or strings, so the divergence rarely surfaces in normal play.
2. OpenGothic's broader persistence is plausibly an intentional robustness enhancement, and
   silently dropping global string/instance persistence could regress mods or OpenGothic-specific
   behaviours that depend on it.

A safe fix therefore needs targeted validation (which, if any, base-game and common-mod globals of
non-INT type are read-after-load without re-assignment) before constraining the save scope. Until
that is established, no surgical high-confidence build-verifiable change is warranted.

If pursued, the change would be, in `GameScript::saveSym`:

```
// NOTE: in original-game zCParser::SaveGlobalVars @0x00796c40 (called from
// oCGame::WriteSavegame @0x006c5942) persists ONLY non-const, non-member INT globals; runtime
// float/string/instance globals are dropped on save and reset to .dat defaults on load.
case zenkit::DaedalusDataType::FLOAT:
case zenkit::DaedalusDataType::STRING:
case zenkit::DaedalusDataType::INSTANCE:
  break; // do not persist: matches original int-only global scope
```

(grep-verified OpenGothic symbols used above: `zenkit::DaedalusSymbol::type/name/count/is_member/`
`is_const/get_int/get_float/get_string/get_instance/is_instance_of` and
`zenkit::DaedalusDataType::{INT,FLOAT,STRING,INSTANCE}` all appear in
`game/game/gamescript.cpp` `saveSym`/`saveVar`/`loadVar`.)
