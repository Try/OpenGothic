#pragma once

#include "trigger.h"

class World;

class CodeMaster : public Trigger {
  public:
    CodeMaster(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    World&            owner;
    std::vector<bool> keys;
  };

