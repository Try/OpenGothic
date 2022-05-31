#pragma once

#include "abstracttrigger.h"

class MessageFilter : public AbstractTrigger {
  public:
    MessageFilter(Vob* parent, World& world, const std::unique_ptr<phoenix::vobs::vob>& data, Flags flags);

  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;

    void exec(phoenix::message_filter_action type);

    phoenix::message_filter_action onUntriggerA;
    phoenix::message_filter_action onTriggerA;
  };
