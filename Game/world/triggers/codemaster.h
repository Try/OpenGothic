#pragma once

#include "abstracttrigger.h"

class World;

class CodeMaster : public AbstractTrigger {
  public:
    CodeMaster(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    std::vector<bool> keys;
  };

