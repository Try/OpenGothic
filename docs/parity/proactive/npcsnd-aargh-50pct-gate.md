# NPC hurt-SVM ("aargh") suppressed 50% of the time by a phantom coin-flip

**Confidence:** High

## Original function + address (prose)

The per-hit pain/death voice line is emitted by `oCNpc::OnDamage_Sound`
(Gothic2.exe `0x0067a8a0`). It builds the name `"SVM_" + <voice> + ("_DEAD"|"_AARGH")`
(the `_DEAD` suffix is chosen when the damage-descriptor flag bit `0x04` at offset
`+0x90` is set, otherwise `_AARGH`) and then plays it directly through `zsound` at the
victim, caching the handle. The single `rand()` call in that routine is **not** a
playback gate: it computes `rand() % NPC_VOICE_VARIATION_MAX` (script constant, default
5 when the symbol is absent) and, only for the non-dead case and only when the modulus
is non-zero, appends a `"_<n>"` variation suffix. In other words the sound is always
emitted; the random number merely selects which voice variation is spoken.

`oCNpc::OnDamage_Sound` is invoked unconditionally from `oCNpc::OnDamage`
(Gothic2.exe `0x006660e0`) at the tail of the hit pipeline whenever the "hit registered"
flag — bit `0x01` of the descriptor flags at `+0x90`, the same bit that gates
`OnDamage_Anim` / `OnDamage_Effects_Start` / `OnDamage_Script` — is set. There is no
probability check anywhere on this path: a living NPC that takes a registered hit always
voices an aargh.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2206-2210`
(inside `Npc::assessDamage`, the alive `else` branch).

## Divergence

OpenGothic gates the aargh on a 50% coin flip:

```cpp
else {
  if(owner.script().rand(2)==0) {
    emitSoundSVM("SVM_%d_AARGH");
    }
  }
```

So roughly half of all qualifying hits produce no pain sound. The original has no such
gate — it voices an aargh on every registered alive hit (the only randomness there picks
the voice *variation*, never whether to play). This is distinct from the already-fixed
per-NPC SVM overlay barrier and footstep-in-water issues; this is a pure
emit-gating divergence in the hurt-line selection logic.

## Proposed patch

Remove the phantom 50% suppression so the hurt SVM fires on every qualifying hit, matching
the original. (Faithfully reproducing the `NPC_VOICE_VARIATION_MAX` voice-variation suffix
is a larger, separate feature — `emitSoundSVM` only formats a single `%d` and OpenGothic
carries no such constant — and is left as a DEFERRED follow-up; dropping the gate is the
high-confidence, build-verifiable parity fix and is a strict improvement either way.)

OLD:
```cpp
      else {
        if(owner.script().rand(2)==0) {
          emitSoundSVM("SVM_%d_AARGH");
          }
        }
```

NEW:
```cpp
      else {
        // NOTE: in original-game oCNpc::OnDamage_Sound (Gothic2.exe 0x0067a8a0), reached
        // unconditionally from oCNpc::OnDamage (0x006660e0) on every registered alive hit,
        // the hurt voice line is always emitted; its rand() only selects a voice variation
        // (rand()%NPC_VOICE_VARIATION_MAX), it never gates playback. Do not suppress half of them.
        emitSoundSVM("SVM_%d_AARGH");
        }
```

Grep-verified symbols: `Npc::emitSoundSVM` (npc.h:418 / npc.cpp:3217),
`GameScript::rand` (gamescript.h:86). The `_DEAD` variant on death is handled separately
in `Npc::onNoHealth` (npc.cpp:596-609) and is unaffected.
