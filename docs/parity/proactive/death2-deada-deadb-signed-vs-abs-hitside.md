# Death animation side (DeadA vs DeadB) uses a symmetric hit-side test where the original uses a *signed* azimuth

**Confidence:** Medium (divergence is high-confidence and triple-cross-checked in the decompile; the corrective patch is **DEFERRED** because the sign convention cannot be verified — see below).

## Original function + address (prose)

`oCNpc::OnDamage_Anim` @ `0x00675bd0` is the routine that plays the reaction
animation for a damage event. Near its tail (after it has already called
`oCNpc::GetAngles` @ `0x006812b0`, which writes the signed azimuth-to-attacker
into the damage descriptor at offset `+0x94`, normalised into `[-180,180]`
degrees), it builds the death animation name. When the descriptor's death bit
(`desc[0x90] & 4`) is set and the NPC is not swimming/diving it appends `"DEAD"`
and then decides the side with a **signed** comparison:

- if `azimuth(+0x94) > 90.0f` → play `"T_DEAD"`  (OpenGothic `Anim::DeadA`)
- otherwise (`azimuth <= 90.0f`) → append `"B"` → play `"T_DEADB"` (`Anim::DeadB`)

Crucially, the *same* function selects the **stumble** side just above this with
an explicit absolute value: `if (ABS(azimuth(+0x94)) < 90.0) ... "B"` — so the
death comparison being signed (no `ABS`) is intentional, not a decompiler
artifact.

The two sibling routines that pick the *other* down/reaction sides both use the
**absolute** value of the very same `+0x94` azimuth:

- `oCNpc::DropUnconscious` @ `0x00735eb0` (called from `OnDamage_Events`
  @ `0x0067abe0` with `desc[0x94]` as its `float` argument): `if (abs((int)azimuth) < 91)`
  → `"T_STAND_2_WOUNDEDB"` (`UnconsciousB`), else `"T_STAND_2_WOUNDED"` (`UnconsciousA`).
- the stumble path in `OnDamage_Anim` itself: `abs(azimuth) < 90` → `STUMBLEB`.

So inside the original, **death is the lone reaction whose left/right side is
chosen with a signed test**; stumble, unconscious-drop and fly-back all use the
symmetric `abs` test.

## OG file:line

`game/world/objects/npc.cpp:625` (death side), fed by the hit-side value computed
at `game/world/objects/npc.cpp:2144-2148`:

```
float a  = angleDir(other.x-x,other.z-z);
float da = a-angle;
if(std::cos(da*M_PI/180.0)<0) lastHitType='A'; else lastHitType='B';
...
setAnim(lastHitType=='A' ? Anim::DeadA : Anim::DeadB);   // line 625
```

`Anim::DeadA`→`S_DEAD/T_DEAD`, `Anim::DeadB`→`S_DEADB/T_DEADB` in
`game/graphics/mesh/animationsolver.cpp:370-387`.

## Divergence

`cos(da) < 0` is exactly the **symmetric** test `|relative-angle| > 90°`.
OpenGothic reuses this single `lastHitType` for *every* reaction (stumble @2211,
fly @2224, fall @2266-2269, unconscious @626 and death @625). That is correct for
stumble/unconscious/fall — they match the original's `abs` tests. But for **death**
the original uses the asymmetric, signed test `azimuth > 90° → DeadA`.

Result: for a hit arriving from the rear on the negative-azimuth side
(`azimuth ∈ [-180°,-90°)` — i.e. `|angle| > 90` so OpenGothic picks `'A'`), the
original plays `T_DEADB` while OpenGothic plays `T_DEAD`. Roughly one rear
quadrant of incoming hit directions selects the wrong death-fall animation. The
front hemisphere (`|azimuth| < 90`) and the positive rear quadrant both still
agree, which is why the bug is easy to miss in casual play.

## Proposed patch — DEFERRED

The divergence (symmetric vs signed death-side test) is established with high
confidence from three decompiled functions. The fix itself is **deferred**
because the corrected condition needs the *sign* of the original `GetAngles`
azimuth, and that sign cannot be mapped to OpenGothic's `da = angleDir(...) - angle`:

- The original azimuth comes from `oCNpc::GetAngles` @ `0x006812b0`, a full 3-D
  azimuth/elevation computation off the model "at" vector (with bench/throne
  negation and an `fpatan` correction term), not a planar `angleDir`.
- Every reaction case where OpenGothic's convention is *confirmed* to match the
  original (stumble, unconscious) uses `abs()`, which discards the sign — so none
  of the verifiable cross-checks constrain whether `da > 90` or `da < -90`
  corresponds to the original `azimuth > 90`. Picking the wrong sign would make a
  second quadrant wrong instead of fixing the first.

Sketch of the intended change (do **not** apply until the azimuth sign is decoded
and verified against `GetAngles`): compute a dedicated signed death-side at hit
time (the signed `da` is already in hand at `npc.cpp:2144-2148`) and consume it at
`npc.cpp:625` for the `DeadA`/`DeadB` choice only, leaving `lastHitType` (the
`abs`/`cos<0` value) for stumble/fall/unconscious. Something like:

```
// OLD (npc.cpp:625)
setAnim(lastHitType=='A' ? Anim::DeadA : Anim::DeadB); else

// NEW (intended)
// NOTE: in original-game oCNpc::OnDamage_Anim @0x00675bd0 the death side is chosen by a
// SIGNED azimuth test (azimuth>90 -> T_DEAD, else T_DEADB), unlike the ABS test used for
// stumble (same fn) and oCNpc::DropUnconscious @0x00735eb0 (abs(azimuth)<91 -> WOUNDEDB).
setAnim(lastHitDeathSide=='A' ? Anim::DeadA : Anim::DeadB); else
```
where `lastHitDeathSide` is set from the normalised signed `da` (`da>90 → 'A'`)
**once the GetAngles sign convention is confirmed**.
