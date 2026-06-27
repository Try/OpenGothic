# Stance subsystem: strafe / turn locomotion anims are weapon-mode independent in the original

**Confidence:** Medium that a code-level divergence exists; Low that it produces a *vanilla-data* behavioral
difference (OpenGothic's fallback chain masks most of it). Filed as **DEFERRED** — see reason below.

## Original function + address (prose only)
The walk-mode locomotion-animation table is populated by `oCAniCtrl_Human::SetWalkMode`
(Gothic2.exe @ `0x006a9820`), with the per-weapon-mode base table built in
`oCAniCtrl_Human::InitAllAnis` (`0x006a5bf0`) and the runtime strafe step driven by
`oCNpc::EV_Strafe` (`0x00683de0`).

The key, decisive evidence is in the binary's string table. The ONLY strafe and in-place-turn
animation names the executable ever references are the weapon-mode-**independent** shallow-water-walk
("WALKW") forms:
- `T_WALKWSTRAFEL` / `T_WALKWSTRAFER`  (data addresses `0x008b19d4` / `0x008b19c4`)
- `T_WALKWTURNL`   / `T_WALKWTURNR`    (`0x008b19f4` / `0x008b19e4`)

These four are loaded by `SetWalkMode` (xref from `0x008b19d4` resolves to `SetWalkMode`). An exhaustive
string scan finds **no** weapon-prefixed strafe/turn names (`1H…`, `2H…`, `BOW…`, `CBOW…`, `MAG…`) and
**no** `RUNSTRAFE`, `SNEAKSTRAFE`, `RUNTURN`, `WALKTURN` (non-water) variants anywhere in Gothic2.exe.
In the original, strafing/turning therefore resolves through one small, weapon-independent family.

## OpenGothic file:line
`game/graphics/mesh/animationsolver.cpp`
- Strafe `MoveL`: lines 218-230  — `T_%sSNEAKSTRAFEL` (224), `T_%sWALKWSTRAFEL` (226/228), `T_%sRUNSTRAFEL` (229)
- Strafe `MoveR`: lines 231-243  — symmetric `…STRAFER`
- Turn `RotL`:   lines 260-272  — `T_%sWALKTURNL` (268), `T_%sWALKWTURNL` (270), `T_%sRUNTURNL` (271)
- Turn `RotR`:   lines 273-285  — symmetric `…TURNR`
- Weapon-prefix expansion: `AnimationSolver::solveFrm(fview, st)` lines 463-485 (maps `WeaponState` →
  `"" / FIST / 1H / 2H / BOW / CBOW / MAG`, with empty-prefix and FIST fallbacks).

## Divergence
OpenGothic constructs strafe/turn names *with* the weapon-mode prefix (`T_%s…`, e.g. `T_2HWALKWSTRAFEL`)
and *with* distinct `RUN…`/`WALK…`/`SNEAK…` infixes per walk-mode. The original game has none of those
variants and strafes/turns with a single unprefixed `T_WALKW{STRAFE,TURN}{L,R}` set. Two consequences:

1. Weapon prefix: if a loaded model defines, say, `t_2HWalkWStrafeL`, OpenGothic prefers it while the
   original always plays the unprefixed `t_WalkWStrafeL` — a different clip (potentially different move
   distance/speed) for the same stance.
2. Walk-vs-water collision and RUN/SNEAK infixes: OG maps `WM_Walk` strafe to the water form
   `WALKW` (226/239) — same as `WM_Water` — and emits `RUNSTRAFE`/`SNEAKSTRAFE`/`WALKTURN` names the
   original never requests. Whether these resolve to a real clip (vs. falling back) depends entirely on
   the MDS animation table.

## Proposed patch
DEFERRED.

Reason: the divergence is real at the name-construction level and is binary-confirmed, but the *observable*
behavior in vanilla data cannot be pinned down from the executable alone. OpenGothic's `solveFrm`
empty-prefix fallback (line 480) already collapses `T_2HWALKWSTRAFEL` → `T_WALKWSTRAFEL` whenever the
prefixed clip is absent, which is exactly the vanilla case — so for stock Humans.mds the strafe path most
likely resolves to the same animation as the original, yielding no speed difference. Confirming an actual
regression requires inspecting the loaded MDS animation set (which strafe/turn clips genuinely exist and at
what move-speed), and that data is not available in this environment. A blind "drop the prefix / collapse
RUN+SNEAK+WALK strafe to the single WALKW family" rewrite would risk breaking community models that *do*
ship prefixed locomotion overlays, so it does not meet the high-confidence, build-verifiable bar.

`// NOTE: in original-game oCAniCtrl_Human::SetWalkMode @0x006a9820 strafe/turn use the single
//       weapon-independent family T_WALKW{STRAFE,TURN}{L,R} (strings @0x008b19c4..0x008b19f4);
//       no weapon-prefixed or RUN/SNEAK/WALK(non-water) strafe/turn names exist in Gothic2.exe.`
