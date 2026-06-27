# zCMessageFilter reacts to NPC touch and emits a spurious trigger-action message

**Confidence:** Medium-High (divergence is certain; gameplay impact depends on whether a given message-filter vob has an NPC-traversable bbox)

## Scope note (the trigger/untrigger action MAPPING itself is correct)

I first verified the asked-for focus — the OnTrigger/OnUntrigger -> message-type mapping — and it is faithful, so reporting a mapping bug there would be a false positive:

- Original `zCMessageFilter::OnTrigger` @0x00618930 calls `ProcessMessage(this, byte[0x134] & 0x0f, ...)` (LOW nibble = trigger action); `zCMessageFilter::OnUntrigger` @0x00618950 calls `ProcessMessage(this, byte[0x134] >> 4, ...)` (HIGH nibble = untrigger action). The packed byte defaults to `0x21` (trigger=MT_TRIGGER=1, untrigger=MT_UNTRIGGER=2) per the constructor @0x006184e0, and `Archive` @0x00618970 writes the low nibble first (`onTrigger`) then the high nibble (`onUntrigger`).
- `ProcessMessage` @0x00618620 maps MT_NONE(0)=skip, MT_TRIGGER(1)=target EM OnTrigger, MT_UNTRIGGER(2)=target EM OnUntrigger, MT_ENABLE(3)/MT_DISABLE(4)/MT_TOGGLE_ENABLED(5)=posted zCEventCommon subtypes.
- OpenGothic matches this exactly: `MessageFilter::exec` keys `onTriggerA`/`onUntriggerA` (assigned from `filt.on_trigger`/`filt.on_untrigger`) onto T_Trigger/T_Untrigger/T_Enable/T_Disable/T_ToggleEnable; the zenkit `MessageFilterAction` enum values (NONE..TOGGLE = 0..5) and the parse order (`on_trigger` then `on_untrigger`, ZenKit `Misc.cc:107-108`) line up with the original.

So the per-direction action selection is correct. The real divergence is in *when* the filter fires.

## Original function + address

In the original, `zCMessageFilter::OnTouch` @0x0060bcd0 and `zCMessageFilter::OnUntouch` @0x0060bce0 are explicit empty overrides (each is a bare `return`). A `zCMessageFilter` therefore never reacts to physical touch/untouch from NPCs walking through its bbox; it acts only when it receives an `OnTrigger`/`OnUntrigger` message (`OnTrigger` @0x00618930 / `OnUntrigger` @0x00618950), which then run `ProcessMessage` @0x00618620 and forward the configured action to the trigger target.

## OpenGothic file:line

- `/Users/admin/Downloads/opengothic/game/world/triggers/messagefilter.cpp` (no `onIntersect` override) together with `/Users/admin/Downloads/opengothic/game/world/triggers/abstracttrigger.cpp:174-186` (`AbstractTrigger::onIntersect`) and `:117-128` (T_Touch path).

## Divergence

`MessageFilter` derives from `AbstractTrigger` and does **not** override `onIntersect`. The `AbstractTrigger` ctor only initializes the trigger react/respond flags for the trigger family vob types (`abstracttrigger.cpp:29-48`); `zCMessageFilter` is not in that list, so `reactToOnTouch`, `respondToNpc`, `respondToPlayer` all keep their `true` defaults (`abstracttrigger.h:91-96`). When a message filter has a non-zero bbox, `boxNpc` is created (`abstracttrigger.cpp:20-25`) and an NPC entering it drives `AbstractTrigger::onIntersect`, which — because `vobType` is `zCMessageFilter` (not excluded at line 175) and the react/respond flags are `true` — emits a `T_Touch` event into `processEvent`. `implProcessEvent`'s T_Touch case then calls `onTrigger(evt)`, i.e. `MessageFilter::onTrigger -> exec(onTriggerA)`, sending the configured trigger-action message to the filter target.

In the original this never happens: `OnTouch`/`OnUntouch` are no-ops, so a passing NPC produces no message at all. OpenGothic thus sends spurious trigger-action messages to the target vob (movers/doors/sounds/sub-triggers) that the original engine never sends.

## Proposed patch

Restore the original "message filters ignore touch" behavior by overriding `onIntersect` as a no-op, mirroring the empty `OnTouch`/`OnUntouch` overrides. This matches the existing pattern of touch-sensitive triggers overriding `onIntersect` (`touchdamage.h:13`, `zonetrigger.h:9`).

`game/world/triggers/messagefilter.h`

OLD:
```cpp
  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
```
NEW:
```cpp
  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    // NOTE: in original-game zCMessageFilter::OnTouch @0x0060bcd0 and OnUntouch @0x0060bce0 are
    // empty overrides; a message filter never reacts to NPC touch, only to OnTrigger/OnUntrigger.
    void onIntersect(Npc& n) override {}
```

Grep-verified symbols: `AbstractTrigger::onIntersect(Npc&)` is `virtual` (`abstracttrigger.h:57`); `class Npc;` is forward-declared in `abstracttrigger.h:11` (included by `messagefilter.h`); the `override` pattern is already used by `touchdamage`/`zonetrigger`.

(Equivalent alternative: define the override out-of-line in `messagefilter.cpp` with an empty body — same effect.)
