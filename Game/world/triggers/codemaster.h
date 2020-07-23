#pragma once

#include "abstracttrigger.h"

class World;

class CodeMaster : public AbstractTrigger {
  public:
    CodeMaster(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    void save(Serialize& fout) const override;
    void load(Serialize &fin) override;

    void onFailure();
    void zeroState();

    std::vector<bool> keys;
  };

