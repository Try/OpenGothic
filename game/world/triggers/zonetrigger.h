#pragma once

#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(Vob* parent, World& world, const std::unique_ptr<phoenix::vob>& data, Flags flags);

    void onIntersect(Npc& n) override;

  private:
    std::string levelName;
    std::string startVobName;
  };
