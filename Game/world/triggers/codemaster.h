#pragma once

#include "abstracttrigger.h"

class World;

class CodeMaster : public AbstractTrigger {
  public:
    CodeMaster(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    std::vector<bool> keys;
  };

