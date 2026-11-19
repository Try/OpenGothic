#pragma once

#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(Vob* parent, World& world, const zenkit::VTriggerChangeLevel& data, Flags flags);

    void onIntersect(Npc& n) override;

  private:
    void onTrigger(const TriggerEvent& evt) override;

    std::string levelName;
    std::string startVobName;
  };
