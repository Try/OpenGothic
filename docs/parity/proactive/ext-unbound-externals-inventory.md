# Unbound Daedalus externals: inventory + stack-safety analysis (reference)

**Status:** Reference / mostly DEFERRED. One surgical fix already applied
(`Wld_InsertNpcAndRespawn`, see below); the rest are engine-handled void AI
actions or non-surgical feature gaps.

## Method

Diffed the original's external registrations (`oCGame::DefineExternals_Ulfi`
@0x006d4780, name strings in the binary) against OpenGothic's `bindExternal(...)`
table in `game/game/gamescript.cpp`. OpenGothic binds ~203 externals.

## Key finding: ZenKit balances the stack for unbound externals

`DaedalusVm::register_default_external` (`lib/ZenKit/src/DaedalusVm.cc:809-836`)
is **stack-safe**: for any unbound external it pops all declared parameters and
pushes a default return value (`0` / `0.0` / `""` / `null`) per the symbol's
declared signature, then calls the log-only `notImplementedRoutine`
(`game/gothic.cpp:964/1007`).

Consequence: an unbound external is a **benign no-op** at the VM level — it does
not corrupt the Daedalus stack. So:

- A **void** AI-action external (`ai_*`) that OpenGothic implements *engine-side*
  (through `AI_Attack`/FightAlgo, the interact pipeline, etc.) is safe to leave
  unbound — binding it speculatively risks **double-driving** the same behavior
  and is unsafe without runtime validation.
- A **return-value query** external (`func int` / `func string`) that is unbound
  silently returns `0`/`""`. That is only a bug when OpenGothic has no
  engine-side source for the correct value AND a script reads the result — the
  `Npc_GetHeightToNpc`-class wrong-default. None of the unbound query externals
  below have a *constant* correct answer that a one-line bind could supply; they
  need real state (see Hlp_CutscenePlayed).

## Notable unbound externals (not an exhaustive list)

Void AI actions (engine-handled or rarely-scripted; leave unbound — do NOT bind blind):
`ai_aimat`, `ai_shootat`, `ai_stopaim`, `ai_canseenpc`, `ai_dropmob`,
`ai_takemob`, `ai_quicklook`, `ai_playcutscene`, `ai_gotosound`,
`ai_turntosound`, `ai_whirlaroundtosource`, `ai_combatreacttodamage`,
`ai_ask`, `ai_asktext`, `ai_waitforquestion`, `ai_waittillend`, `ai_playfx`,
`ai_stopfx`, `ai_snd_play`, `ai_snd_play3d`, `ai_defend`, `ai_dropitem`,
`ai_mobinteract`.
- Ranged NPC combat (`ai_aimat`/`ai_shootat`/`ai_stopaim`) is driven engine-side
  in OpenGothic via the fight AI, so binding the script externals risks
  conflicting double-fire. DEFERRED pending a runtime check of whether stock G2
  ranged AI routes through these or through `AI_Attack`.

Feature gaps (need new state/subsystem — non-surgical, DEFERRED):
- `hlp_cutsceneplayed` (`func int`): returns whether a named cutscene has played.
  Unbound → always `0` ("never played"), which could let a script re-trigger a
  one-time `.CS` cutscene. OpenGothic has no cutscene-played registry, so there is
  no correct value to return without implementing that tracking. DEFERRED.
- `doc_*` map/document family: handled by OpenGothic's own document menu where
  implemented; the unimplemented ones are rendering features, not parity logic.

## Applied from this investigation

- **`Wld_InsertNpcAndRespawn`** — bound to spawn the NPC + record `spawn_delay`
  (see `spawn-wld-insertnpcandrespawn-unbound.md` and the committed fix). The
  original spawns the NPC; OpenGothic's unbound no-op did not, so the NPC never
  appeared. (Correction to that finding's rationale: the Daedalus stack is **not**
  corrupted by the unbound call — ZenKit's default external balances it, per the
  analysis above — so the *only* divergence was the missing spawn, which the fix
  resolves. The cull-and-respawn-timer half remains deferred.)
