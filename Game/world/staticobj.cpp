#include "staticobj.h"

#include "world.h"
#include "utils/fileext.h"

StaticObj::StaticObj(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup)
  : Vob(parent, world, vob, startup) {
  if(!vob.showVisual)
    return;

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
      mesh = world.getDecalView(vob, transform(), *decalMesh);

      Tempest::Matrix4x4 m;
      m.identity();
      mesh.setObjMatrix(m);
      } else {
      pfx = world.getView(vob);
      pfx.setActive(true);
      pfx.setObjMatrix(transform());
      }
    } else {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;

    mesh = world.getStaticView(vob.visual.c_str());
    mesh.setObjMatrix(transform());

    if(vob.cdDyn || vob.cdStatic) {
      physic = PhysicMesh(*view,*world.physic());
      physic.setObjMatrix(transform());
      }
    }
  }

void StaticObj::moveEvent() {
  pfx   .setObjMatrix(transform());
  mesh  .setObjMatrix(transform());
  physic.setObjMatrix(transform());
  }
