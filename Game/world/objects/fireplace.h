#pragma once

#include "interactive.h"
#include "vobbundle.h"

class FirePlace : public Interactive {
  public:
    FirePlace(Vob* parent, World& world, ZenLoad::zCVobData& vob, bool startup);

  protected:
    void  moveEvent() override;
    bool  setMobState(const char* scheme, int32_t st) override;

  private:
    VobBundle   fireVobtree;
    std::string fireVobtreeName;
    std::string fireSlot;
  };

