#pragma once

#include "abstracttrigger.h"

class MessageFilter : public AbstractTrigger {
  public:
    MessageFilter(Vob* parent, World& world, const zenkit::VMessageFilter& data, Flags flags);

  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    // NOTE: in original-game zCMessageFilter::OnTouch @0x0060bcd0 and OnUntouch @0x0060bce0 are
    // empty overrides -- a message filter never reacts to NPC touch, only to OnTrigger/OnUntrigger.
    // OpenGothic inherited AbstractTrigger::onIntersect with reactToOnTouch defaulting true (the
    // ctor only seeds the trigger-family vob types), so an NPC crossing a filter's bbox spuriously
    // fired its trigger-action. Override as a no-op (same pattern as TouchDamage/ZoneTrigger).
    void onIntersect(Npc& n) override {}

    void exec(zenkit::MessageFilterAction type);

    zenkit::MessageFilterAction onUntriggerA;
    zenkit::MessageFilterAction onTriggerA;
  };
