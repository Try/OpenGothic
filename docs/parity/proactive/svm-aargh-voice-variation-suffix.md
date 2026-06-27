# SVM hurt voice line never picks one of the NPC_VOICE_VARIATION_MAX variants

**Confidence:** High

## Original function + address

`oCNpc::OnDamage_Sound` (Gothic2.exe `0x0067a8a0`), reached unconditionally from
`oCNpc::OnDamage` (`0x006660e0`) on every registered hit, builds the hurt/death
voice line as follows:

1. It seeds a variation count from the integer parser constant
   `NPC_VOICE_VARIATION_MAX` (string at `0x008b031c`), defaulting to `5` and
   overriding it with the symbol's value when that symbol exists.
2. It draws `v = rand() % NPC_VOICE_VARIATION_MAX`.
3. It assembles the base name `"SVM_" + voice` (the NPC voice number at
   `this+0x254`), then appends `"_DEAD"` when the damage descriptor's lethal flag
   (`desc[0x90] & 4`) is set, otherwise `"_AARGH"`.
4. **Only on the non-lethal path, and only when `v != 0`, it appends `"_" + v`**,
   yielding `SVM_<voice>_AARGH_1` .. `SVM_<voice>_AARGH_4`. When `v == 0` the bare
   `SVM_<voice>_AARGH` is used. The lethal `SVM_<voice>_DEAD` line is never varied.

So the original plays one of five distinct hurt grunts per hit (1-in-5 chance for
the bare `_AARGH`, 1-in-5 each for `_AARGH_1`.._AARGH_4`).

## OpenGothic file:line

`game/world/objects/npc.cpp:3274` (`Npc::emitSoundSVM`), called for the AARGH path
at `npc.cpp:610` (non-death branch of `onNoHealth`), `npc.cpp:2252`
(`OnDamage`/hit reaction), and `npc.cpp:2283` (fall damage).

## Divergence

`Npc::emitSoundSVM` formats exactly `SVM_<voice>_AARGH` (or `SVM_<voice>_DEAD`) and
plays it verbatim. It never computes `rand() % NPC_VOICE_VARIATION_MAX` and never
appends the `_1`.._4` variant suffix. As a result OpenGothic always emits the
single base `SVM_<voice>_AARGH` grunt and the four scripted hurt variants
(`SVM_<voice>_AARGH_1`..`_4`, which exist in the Gothic2 SFX/voice definitions) are
never heard, making pain reactions audibly monotone versus the original's
five-way variety. The in-code comment at `npc.cpp:2249-2251` already acknowledges
"its rand() only selects a voice variation (rand()%NPC_VOICE_VARIATION_MAX)", but
the implementation in `emitSoundSVM` omits that variation entirely.

This is distinct from the already-fixed AARGH coin-flip (which concerned *whether*
to play); this concerns *which* of the five variants is selected.

## Proposed patch

Grep-verified OG symbols used: `hnpc->voice`, `owner.script()`,
`GameScript::rand(uint32_t)` (`game/game/gamescript.h:86`),
`Npc::emitSoundEffect(std::string_view,float,bool)` (`npc.cpp:3262`).

OLD (`game/world/objects/npc.cpp:3274`):
```cpp
void Npc::emitSoundSVM(std::string_view svm) {
  if(hnpc->voice==0)
    return;
  char frm [32]={};
  std::snprintf(frm,sizeof(frm),"%.*s",int(svm.size()),svm.data());

  char name[32]={};
  std::snprintf(name,sizeof(name),frm,int(hnpc->voice));
  emitSoundEffect(name,2500,true);
  }
```

NEW:
```cpp
void Npc::emitSoundSVM(std::string_view svm) {
  if(hnpc->voice==0)
    return;
  char frm [32]={};
  std::snprintf(frm,sizeof(frm),"%.*s",int(svm.size()),svm.data());

  char name[64]={};
  int  len = std::snprintf(name,sizeof(name),frm,int(hnpc->voice));

  // NOTE: in original-game oCNpc::OnDamage_Sound @0x0067a8a0 the non-lethal hurt voice line picks
  // one of NPC_VOICE_VARIATION_MAX (default 5) variants: v = rand()%MAX, and when v!=0 it appends
  // "_<v>" -> SVM_<voice>_AARGH_1 .. _4. The bare SVM_<voice>_AARGH is used only when v==0; the
  // lethal "_DEAD" line is never varied.
  const bool death = (svm.find("DEAD")!=std::string_view::npos);
  if(!death && len>0 && len<int(sizeof(name))) {
    const uint32_t NPC_VOICE_VARIATION_MAX = 5; // Gothic2 Constants.d default for NPC_VOICE_VARIATION_MAX
    const uint32_t v = owner.script().rand(NPC_VOICE_VARIATION_MAX);
    if(v!=0)
      std::snprintf(name+len,sizeof(name)-size_t(len),"_%u",v);
    }
  emitSoundEffect(name,2500,true);
  }
```

Notes / caveats:
- The five hurt-variant sound effects (`SVM_<voice>_AARGH_1`..`_4`) must be present
  in the loaded SFX definitions; missing names fall through `emitSoundEffect`
  silently, so the worst case for an absent variant is the pre-fix behavior, never
  a regression.
- `NPC_VOICE_VARIATION_MAX` is hard-coded to the shipped Gothic2 value (5). The
  original additionally overrides it from the parser symbol of the same name when
  present; a fully faithful version would read that global int and fall back to 5.
- The lethal path here does *not* consume an RNG draw, whereas the original calls
  `rand()` on both paths; this is an RNG-stream nuance, not an audible difference.
