#include "staticobj.h"

#include <Tempest/Log>

#include "world/world.h"
#include "utils/fileext.h"

using namespace Tempest;

StaticObj::StaticObj(Vob* parent, World& world, const phoenix::vob& vob, Flags flags)
  : Vob(parent, world, vob, flags) {
  visual.setVisual(vob,world,(flags & Flags::Static));
  visual.setObjMatrix(transform());

  scheme = vob.visual_name;
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
