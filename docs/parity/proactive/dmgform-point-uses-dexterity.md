# Melee attribute boni: POINT damage gets DEXTERITY, not STRENGTH (and boni is split across active types)

**Confidence:** High

## Original function + address

`oCNpc::OnDamage_Hit` (Gothic2.exe @ 0x00666610), in the per-weapon "Attribute
Boni" block. After the engine has assembled the per-type weapon damage into the
damage-descriptor accumulators (Blunt @ desc+0x30, Edge @ desc+0x34, Point @
desc+0x44), and only on a physical (non-spell) NPC hit, it adds an *attribute
boni* to each active damage type.

Confirmed at the instruction level (the warm-decompiler folds the branch, so the
raw disassembly is what is authoritative here — described in prose, no code
pasted):

- At 0x0066ae53 the engine calls `oCNpc::GetAttribute(attacker, 4)` — attribute
  index 4 is STRENGTH — and `fild`/`fstp`-saves it as a float into a local slot.
- At 0x0066ae76 it calls `oCNpc::GetAttribute(attacker, 5)` — attribute index 5
  is DEXTERITY — and keeps that on the FP stack.
- It then counts the number of *active* boni-eligible damage types into a float
  `numTypes` (each present type contributes +1.0; the three flags come from the
  descriptor type mask at desc+0x24), guards against `numTypes == 0`, and forms
  the reciprocal `1.0 / numTypes` (`fdivr`, 0x0066aed0).
- It produces two scaled boni: `STRENGTH / numTypes` (stored to the local later
  consumed by the Blunt/Edge adds) and `DEXTERITY / numTypes` (stored to the
  local later consumed by the Point add).
- Blunt boni (desc+0x30, 0x0066aef1→__ftol→`add [edi+0x30],eax`) and Edge boni
  (desc+0x34, 0x0066b0e5) both consume the **STRENGTH/numTypes** slot.
- Point boni (desc+0x44, 0x0066b2e0) consumes the **DEXTERITY/numTypes** slot.

So the original per-type melee attribute boni is:
`BLUNT += STR/n`, `EDGE += STR/n`, `POINT += DEX/n`, where `n` = number of active
boni-eligible damage types. (Index map confirmed against
`game/game/constants.h`: `ATR_STRENGTH=4`, `ATR_DEXTERITY=5`; and against
`lib/ZenKit/.../daedalus.hh`: `DamageType::BLUNT=1, EDGE=2, POINT=6`.)

## OpenGothic file:line

`game/game/damagecalculator.cpp:159` and `:170-189` (`DamageCalculator::swordDamage`, the `version().game==2` branch).

## Divergence

OpenGothic reads a single `int str = nsrc.attribute(ATR_STRENGTH)` (line 159)
and adds it to **every** active damage type, including POINT:

```
int vd = std::max(str + src.damage[i] - other.protection[i], 0);
```

Two differences from the original:

1. **POINT damage type uses DEXTERITY in the original, not STRENGTH.** A
   melee/throwing weapon whose `damage_type` includes POINT (bit 6) should add
   the attacker's DEXTERITY to its point contribution. OpenGothic adds STRENGTH.
   For a high-STR/low-DEX attacker this over-counts point damage; for the
   reverse it under-counts.
2. **The boni is divided by the number of active boni-eligible types** (`/n`).
   For the overwhelmingly common single-type weapon (`n==1`) this is a no-op, so
   it does not affect normal swords/axes; it only matters for multi-type weapons
   and is therefore lower-impact and harder to scope precisely.

The surgical, build-verifiable fix below addresses (1) only — the per-type
attribute selection — which is the concrete attribute-to-damage divergence and
does not require reworking the type-count accounting. (2) is called out as
DEFERRED to keep the change high-confidence.

## Proposed patch

Select the boni attribute per damage type: DEXTERITY for the POINT type, STRENGTH
otherwise. Grep-verified OG symbols: `Npc::attribute(Attribute)`
(`game/world/objects/npc.h:216`), `Attribute::ATR_STRENGTH` / `ATR_DEXTERITY`
(`game/game/constants.h:477-478`), `zenkit::DamageType::POINT`
(`lib/ZenKit/.../daedalus.hh:67`). The pattern mirrors the existing DEX read at
`damagecalculator.cpp:239`.

OLD (`damagecalculator.cpp`, ~line 159 and the G2 loop body ~line 180):

```cpp
  const int dtype      = damageTypeMask(nsrc);
  Talent    tal        = TALENT_UNKNOWN;
  int       str        = nsrc.attribute(Attribute::ATR_STRENGTH);
  int       critChance = int(script.rand(100));
```

```cpp
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      int vd = std::max(str + src.damage[i] - other.protection[i],0);
```

NEW:

```cpp
  const int dtype      = damageTypeMask(nsrc);
  Talent    tal        = TALENT_UNKNOWN;
  int       str        = nsrc.attribute(Attribute::ATR_STRENGTH);
  int       dex        = nsrc.attribute(Attribute::ATR_DEXTERITY);
  int       critChance = int(script.rand(100));
```

```cpp
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe @0x00666610) the
      // per-type melee attribute boni adds STRENGTH to BLUNT/EDGE but DEXTERITY
      // to POINT (GetAttribute(5) is read for the point accumulator @desc+0x44,
      // GetAttribute(4) for the blunt/edge accumulators @desc+0x30/+0x34).
      const int atr = (i==zenkit::DamageType::POINT) ? dex : str;
      int vd = std::max(atr + src.damage[i] - other.protection[i],0);
```

(`dex` is unused for purely STR-based weapons, but the conditional keeps the
formula faithful for POINT-type weapons; if the compiler's `-Werror=unused` is a
concern, the read can be inlined into the ternary instead.)

DEFERRED (separate finding): the `/numTypes` division of the attribute boni
across active boni-eligible damage types is not modeled. It is a no-op for
single-type weapons (the common case) and would require introducing the type
count and the integer-truncation semantics of the original's per-type `__ftol`;
left out to keep this patch high-confidence.
