#pragma once

#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(Vob* parent, World& world, const phoenix::vobs::trigger_change_level& data, Flags flags);

    void onIntersect(Npc& n) override;

  private:
    std::string levelName;
    std::string startVobName;
  };
