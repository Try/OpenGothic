#include "fireplace.h"

FirePlace::FirePlace(Vob* parent, World& world, ZenLoad::zCVobData& vob, bool startup)
  :Interactive(parent,world,vob,startup){
  fireVobtreeName = std::move(vob.oCMobFire.fireVobtreeName);
  fireSlot        = std::move(vob.oCMobFire.fireSlot);

  fireVobtree     = VobBundle(world,fireVobtreeName);
  moveEvent();
  }

void FirePlace::moveEvent() {
  Interactive::moveEvent();

  auto at = this->nodeTranform(fireSlot.c_str());
  at.set(0,0, 1);
  at.set(2,2, 1);
  fireVobtree.setObjMatrix(at);
  }

bool FirePlace::setMobState(const char* scheme, int32_t st) {
  bool ret = Interactive::setMobState(scheme,st);
  if(std::strcmp(schemeName(),scheme)!=0)
    return ret;

  return ret;
  }
