#include "staticobj.h"

#include <Tempest/Log>

#include "world/world.h"
#include "utils/string_frm.h"

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
  string_frm name("S_S",st);
  if(visual.startAnimAndGet(name,world.tickCount())!=nullptr) {
    // state = st;
    return ret;
    }
  return false;
  }
