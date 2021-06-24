#include "staticobj.h"

#include <Tempest/Log>

#include "world/world.h"
#include "utils/fileext.h"

using namespace Tempest;

StaticObj::StaticObj(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup)
  : Vob(parent, world, vob, startup) {
  visual.setVisual(vob,world);
  visual.setObjMatrix(transform());

  scheme = std::move(vob.visual);
  }

void StaticObj::moveEvent() {
  Vob::moveEvent();
  visual.setObjMatrix(transform());
  }

bool StaticObj::setMobState(std::string_view sc, int32_t st) {
  const bool ret = Vob::setMobState(sc,st);

  if(scheme.find(sc)!=0)
    return ret;
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"S_S%d",st);
  if(visual.startAnimAndGet(buf,world.tickCount())!=nullptr) {
    // state = st;
    return ret;
    }
  return false;
  }
