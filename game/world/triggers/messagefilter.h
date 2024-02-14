#pragma once

#include "abstracttrigger.h"

class MessageFilter : public AbstractTrigger {
  public:
    MessageFilter(Vob* parent, World& world, const zenkit::VMessageFilter& data, Flags flags);

  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;

    void exec(zenkit::MessageFilterAction type);

    zenkit::MessageFilterAction onUntriggerA;
    zenkit::MessageFilterAction onTriggerA;
  };
