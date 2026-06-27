# Npc_GetPermAttitude consults the NPC's perm attitude even when the *subject* is the player

**Confidence:** Medium-High

## Original function + address

`oCNpc::GetPermAttitude(oCNpc*)` at **Gothic2.exe `0x0072fb30`**, reached from the
Daedalus external `Npc_GetPermAttitude(npc, other)` as `npc->GetPermAttitude(other)`
(external wrapper `FUN_006e8ec0`; pops `other` first then `npc`, then calls
`oCNpc::GetPermAttitude(npc, other)` — so `this = npc` (subject), `param_1 = other`).

The handler is short and **asymmetric** with respect to who the player is:

- It tests `other->IsAPlayer()` (virtual via `param_1` vtable +0x100).
- If `other` **is** the player: return the SUBJECT npc's permanent-attitude field
  (`oCNpc+0x7e4`) directly. (Perm-only; the temp field `+0x7e8` is never read here, unlike
  `oCNpc::GetAttitude` @ `0x0072fab0`.)
- Otherwise (`other` is not the player): return
  `oCGuilds::GetAttitude(npc.trueGuild (+0x766), other.guild (+0x230))` — the pure
  guild-matrix value. The NPC's personal perm attitude is **never** consulted in this branch.

Decisive case: when the **subject is the player** and `other` is a regular NPC,
`other->IsAPlayer()` is false, so the engine returns the guild-matrix value and does **not**
look at the NPC's own perm attitude. This mirrors the sibling `oCNpc::GetAttitude`
(`0x0072fab0`) gate, whose subject-is-player asymmetry was already fixed for OpenGothic's
`personAttitude` (gamescript.cpp:1437, keyed on `p1.isPlayer()`).

## OG file:line

`game/game/gamescript.cpp:2971` — `GameScript::npc_getpermattitude`, specifically the
selector at line 2982:

```cpp
const Npc& npc = a->isPlayer() ? *b : *a;
```

This handler was rewritten directly (rather than calling a helper) to drop the temp-attitude
precedence, but it kept the old "whichever of the two is not the player" selector. That is the
same asymmetry that was corrected in `personAttitude` (line 1437) — so the perm handler now
diverges from both the original engine and its own sibling.

## Divergence

For `Npc_GetPermAttitude(hero, someNpc)` (subject = player, `other` = NPC):

- **Original:** `other` (the NPC) is not the player → returns
  `guildAttitude(player.guild, npc.guild)` (typically `ATT_NEUTRAL`); the NPC's personal perm
  attitude is ignored.
- **OpenGothic (current):** `!a->isPlayer()` is false, so it falls to the selector with
  `a->isPlayer()` true → `npc = *b` (the NPC) → returns *that NPC's* personal perm attitude
  (`b.attitude()`), e.g. an explicitly-set `ATT_HOSTILE`, instead of the guild value.

The common in-engine path `Npc_GetPermAttitude(self_npc, hero)` (subject = NPC) is unaffected
and stays correct; only the rarer subject-is-player call differs.

## Proposed patch

Gate the perm consultation on the OTHER argument `b` being the player, matching the original's
`other->IsAPlayer()` test and the already-applied `personAttitude` fix at line 1437.

OLD (`game/game/gamescript.cpp:2975`):
```cpp
  if(a!=nullptr && b!=nullptr){
    // NOTE: in original-game oCNpc::GetPermAttitude (Gothic2.exe 0x0072fb30) is PERM-only and
    // never consults the temp attitude (that is Npc_GetAttitude/personAttitude's job). Routing
    // this through the now temp-aware personAttitude wrongly returned a temp-angered NPC's temp
    // attitude here, so resolve perm + guild fallback directly.
    if(!a->isPlayer() && !b->isPlayer())
      return guildAttitude(*a,*b);
    const Npc& npc = a->isPlayer() ? *b : *a;
    Attitude att = npc.attitude();
    if(att!=ATT_NULL)
      return att;
    return guildAttitude(*a,*b);
    }
```

NEW:
```cpp
  if(a!=nullptr && b!=nullptr){
    // NOTE: in original-game oCNpc::GetPermAttitude (Gothic2.exe 0x0072fb30) is a->GetPermAttitude(b):
    // it returns subject a's perm-attitude field ONLY when the OTHER argument b is the player, and the
    // pure guild-matrix value guildAttitude(a.guild,b.guild) otherwise. It is PERM-only (never reads
    // temp). Keying on "whichever is not the player" wrongly surfaced an NPC's perm attitude for
    // Npc_GetPermAttitude(hero,npc) (subject=player), which the original answers from the guild matrix.
    // Mirror personAttitude's "other is the player" gate (gamescript.cpp:1437).
    if(!b->isPlayer())
      return guildAttitude(*a,*b);
    Attitude att = a->attitude();
    if(att!=ATT_NULL)
      return att;
    return guildAttitude(*a,*b);
    }
```

Grep-verified OG symbols: `Npc::attitude()` / `Npc::isPlayer()` (used throughout this file),
`GameScript::guildAttitude` (`gamescript.cpp:1423`), `Attitude`/`ATT_NULL`/`ATT_NEUTRAL`. The
`ATT_NULL → guildAttitude` fallback is kept for parity with the applied `personAttitude` perm
path (line 1445-1448); the original's raw `+0x7e4` field defaults to `ATT_NEUTRAL`, so the
fallback only differs from the original when an NPC's perm attitude is genuinely unset.

Caveat / why Medium-High: identical reasoning and confidence to the already-applied
`att-personattitude-self-is-player-asymmetry` finding; the surface is narrow (only the
`Npc_GetPermAttitude(hero, npc)` ordering changes — the typical `(self_npc, hero)` callers are
unaffected and already correct).
