#pragma once

#include "abstracttrigger.h"

class World;

class TriggerScript : public AbstractTrigger {
  public:
    TriggerScript(Vob* parent, World& world, const phoenix::vobs::trigger_script& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    std::string function;
  };
