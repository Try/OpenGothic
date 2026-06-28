# CodeMaster: untriggeredCancels / OnUntrigger key-cancel is unimplemented

**Confidence:** High

## Original fn + address
`zCCodeMaster::OnUntrigger` @ `0x00619500` (Gothic2.exe). The function gates on the
object flags byte at `this+0x1c0`: it returns immediately unless `orderRelevant` (bit `&1`)
is **clear** AND `untriggeredCancels` (bit `&2`) is **set** (and the emitter is a named vob).
The flag bits are assigned in `zCCodeMaster::Unarchive` @ `0x00619a80`: the first archived
bool -> `&1` (orderRelevant/ordered), the second -> `&4` (firstFalseIsFailure), and the bool
read after the failure-target string -> `&2` (untriggeredCancels). When the gate passes,
OnUntrigger scans the 6 slave slots (`this+0x134`, stride 0x14) for one whose name matches the
un-triggered vob; if that slave currently holds a recorded key pointer (`this+0x1c4 + idx*4`),
it releases the pointer and zeroes the slot. Net effect: in an unordered AND-gate with
`untriggeredCancels`, un-triggering a slave **removes** its contribution, so the gate
"un-completes" and will not fire its target again until that slave is re-triggered. The success
test in `zCCodeMaster::OnTrigger` @ `0x00619300` (unordered branch, flag `&1`==0) is "all
configured slave slots filled", so a cleared slot genuinely blocks success.

## OG file:line
`/Users/admin/Downloads/opengothic/game/world/triggers/codemaster.cpp:16-18` (ctor logs the
feature as unimplemented) and the class as a whole: `CodeMaster` overrides only `onTrigger`,
never `onUntrigger`, so `AbstractTrigger::onUntrigger` (the no-op default) handles every
`T_Untrigger` event routed at `abstracttrigger.cpp:158`. Header:
`/Users/admin/Downloads/opengothic/game/world/triggers/codemaster.h:11`.

## Divergence
With `untriggeredCancels` set and `ordered` off, OpenGothic ignores un-trigger events entirely:
once a slave's `keys[i]` is set true it is never cleared. Concrete observable case, slaves
`[A,B,C]`: trigger A then B (`keys=[T,T,F]`), un-trigger A, then trigger C. Original clears
A's key on the un-trigger, so after C the keys are `[F,T,T]` -> no success. OpenGothic leaves
A's key set, so after C the keys are `[T,T,T]` -> it fires the target. OpenGothic fires the
AND-gate's target when the original would not. The plumbing already exists
(`AbstractTrigger::onUntrigger` virtual, `TriggerEvent::emitter`); only the CodeMaster override
is missing.

## Proposed patch

codemaster.h (add the override next to `onTrigger`):

```
OLD:
    void onTrigger(const TriggerEvent& evt) override;

NEW:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
```

codemaster.cpp (drop the "not implemented" warning and implement the cancel):

```
OLD:
  untriggeredCancels  = cm.untriggered_cancels;
  if(untriggeredCancels)
    Tempest::Log::d("zCCodeMaster::untriggeredCancels is not implemented. Vob: \"", vobName, "\"");
  }

NEW:
  untriggeredCancels  = cm.untriggered_cancels;
  }

void CodeMaster::onUntrigger(const TriggerEvent& evt) {
  // NOTE: in original-game zCCodeMaster::OnUntrigger @0x00619500 the un-trigger is honoured
  // only when orderRelevant is OFF and untriggeredCancels is ON; it then clears the matching
  // slave's recorded key, so the unordered AND-gate "un-completes" until that slave fires again.
  if(ordered || !untriggeredCancels)
    return;
  for(size_t i=0;i<keys.size();++i)
    if(slaves[i]==evt.emitter) {
      keys[i] = false;
      return;
      }
  }
```

Notes on fidelity: OpenGothic's unordered success gate is `count>=keys.size()` followed by
`std::find(keys,false)==end`; clearing `keys[i]` alone is sufficient (no `count` change needed)
because once the gate has been crossed the `find(false)` test governs every subsequent
`onTrigger`, matching the original's "all slots filled" check. The original additionally
requires the slot to currently hold a key before clearing; setting an already-false `keys[i]`
to false is a no-op, so the simpler form is behaviourally equivalent.
