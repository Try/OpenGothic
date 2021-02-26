#include "fireplace.h"

FirePlace::FirePlace(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup)
  :Interactive(parent,world,std::move(vob),startup){

  }
