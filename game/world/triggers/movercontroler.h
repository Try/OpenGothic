#pragma once

#include "abstracttrigger.h"

class MoverControler : public AbstractTrigger {
  public:
    MoverControler(Vob* parent, World& world, const std::unique_ptr<phoenix::vob>& data, Flags flags);

  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
  public:
    phoenix::mover_message_type message;
    uint32_t                    key;
  };


