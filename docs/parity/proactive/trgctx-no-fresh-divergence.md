# Trigger-script Daedalus SELF/OTHER/ITEM context — NO FINDING

**Confidence:** High (NO FINDING)

**Original fn + address:** The only trigger-class vob that sets Daedalus VM globals and
fires a script is `oCTriggerScript::TriggerTarget(zCVob*)` @ `0x0043c4c0`. After chaining
to the base `zCTrigger::TriggerTarget`, it performs exactly three `zCParser::SetInstance`
calls — `SELF = RTDynamicCast(param_1 -> oCNpc)` (i.e. the activating vob, or null when the
activator is not an NPC), `OTHER = null`, `ITEM = null` — and then `zCParser::CallFunc` on
the stored function name (`this + 0x168`). It does NOT touch `VICTIM`, does NOT save/restore
the globals afterward, and sets them in the order SELF, OTHER, ITEM. The other trigger vobs
(`zCCodeMaster`, `zCMessageFilter`, `zCMoverControler`, `zCTriggerList`, `zCTouchDamage`,
`zCTriggerWorldStart`) only forward trigger messages and never enter the Daedalus VM.

**OG file:line:** `game/world/triggers/triggerscript.cpp:23-26` — clears
`global_self`/`global_other`/`global_item` to null, then `vm.call_function(function)`. A
repo-wide grep of `game/world/triggers/*.cpp` confirms `triggerscript.cpp` is the sole
Daedalus-firing trigger vob.

**Divergence:** None remaining within scope. The current implementation matches the original
for every activation where the trigger source is a non-NPC vob (the dominant trigger-chain
case): OTHER=null, ITEM=null, SELF=null are all faithful. The original sets no `VICTIM` and
performs no post-call restore, both matching OpenGothic. The single residual difference is
the direct-NPC-touch activation, where the original would set SELF to the touching oCNpc
while OpenGothic leaves it null — but that is the explicitly-excluded `trgscript-self-other-context`
deferred sub-case (onIntersect drops the Npc pointer before onTrigger), not a fresh finding.

**Proposed patch:** NO FINDING.

NOTE: in original-game `oCTriggerScript::TriggerTarget` @0x0043c4c0 the SELF/OTHER/ITEM clear
plus non-NPC SELF is already reproduced; the NPC-touch SELF is the deferred excluded case.
