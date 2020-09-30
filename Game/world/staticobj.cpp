#include "staticobj.h"

#include <Tempest/Log>

#include "world.h"
#include "utils/fileext.h"

using namespace Tempest;

StaticObj::StaticObj(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup)
  : Vob(parent, world, vob, startup) {
  if(!vob.showVisual)
    return;

  if(vob.visual.find("FIREPLACE")==0)
    Log::d("");

  if(FileExt::hasExt(vob.visual,"PFX")) {
    const ParticleFx* view = world.script().getParticleFx(vob.visual.c_str());
    if(view==nullptr)
      return;
    pfx = world.getView(view);
    pfx.setActive(true);
    pfx.setObjMatrix(transform());
    } else
  if(FileExt::hasExt(vob.visual,"TGA")){
    if(vob.visualCamAlign==0) {
      decalMesh = std::make_unique<ProtoMesh>(ZenLoad::PackedMesh(),"");
      auto mesh = world.getDecalView(vob, transform(), *decalMesh);
      visual.setVisualBody(std::move(mesh),world);
      } else {
      pfx = world.getView(vob);
      pfx.setActive(true);
      pfx.setObjMatrix(transform());
      }
    } else {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;

    auto sk   = Resources::loadSkeleton(vob.visual.c_str());
    auto mesh = world.getStaticView(vob.visual.c_str());
    visual.setVisual(sk);
    visual.setVisualBody(std::move(mesh),world);

    if(vob.cdDyn || vob.cdStatic) {
      physic = PhysicMesh(*view,*world.physic());
      physic.setObjMatrix(transform());
      }
    visual.setPos(transform());
    }

  scheme = std::move(vob.visual);
  visual.setYTranslationEnable(false);
  }

void StaticObj::moveEvent() {
  Vob::moveEvent();
  pfx   .setObjMatrix(transform());
  physic.setObjMatrix(transform());
  visual.setPos(transform());
  }

bool StaticObj::setMobState(const char* sc, int32_t st) {
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
