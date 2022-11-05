#pragma once

#include "interactive.h"
#include "vobbundle.h"

class FirePlace : public Interactive {
  public:
    FirePlace(Vob* parent, World& world, const phoenix::vobs::mob_fire& vob, Flags flags);

  protected:
    void  load(Serialize& fin) override;
    void  moveEvent() override;
    void  onStateChanged() override;

  private:
    VobBundle   fireVobtree;
    std::string fireVobtreeName;
    std::string fireSlot;
  };

