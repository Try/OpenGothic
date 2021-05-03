#include "fireplace.h"

FirePlace::FirePlace(Vob* parent, World& world, ZenLoad::zCVobData& vob, bool startup)
  :Interactive(parent,world,vob,startup){
  fireVobtreeName = std::move(vob.oCMobFire.fireVobtreeName);
  fireSlot        = std::move(vob.oCMobFire.fireSlot);
  }

void FirePlace::load(Serialize& fin) {
  Interactive::load(fin);
  onStateChanged();
  }

void FirePlace::moveEvent() {
  Interactive::moveEvent();

  auto at = this->nodeTranform(fireSlot.c_str());
  fireVobtree.setObjMatrix(at);
  }

void FirePlace::onStateChanged() {
  if(stateId()!=0) {
    auto at = this->nodeTranform(fireSlot.c_str());
    fireVobtree = VobBundle(world,fireVobtreeName);
    fireVobtree.setObjMatrix(at);
    } else {
    fireVobtree = VobBundle();
    }
  }
