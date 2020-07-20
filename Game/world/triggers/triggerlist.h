#pragma once

#include "abstracttrigger.h"

class World;

class TriggerList : public AbstractTrigger {
  public:
    TriggerList(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    enum ProcessMode : uint8_t {
      LP_ALL  = 0,
      LP_NEXT = 1,
      LP_RAND = 2
      };
    uint32_t next=0;
  };
