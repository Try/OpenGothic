#include "staticobj.h"

#include "world.h"
#include "utils/fileext.h"

StaticObj::StaticObj(World& owner, ZenLoad::zCVobData&& vob, bool startup)
  : Vob(owner, vob, startup) {
  if(!vob.showVisual)
    return;

  if(FileExt::hasExt(vob.visual,"PFX")) {
    const ParticleFx* view = owner.script().getParticleFx(vob.visual.substr(0,vob.visual.size()-4).c_str());
    if(view==nullptr)
      return;
    pfx = owner.getView(view);
    pfx.setActive(true);
    pfx.setObjMatrix(transform());
    } else
  if(FileExt::hasExt(vob.visual,"TGA")){
    if(vob.visualCamAlign==0) {
      decalMesh = std::make_unique<ProtoMesh>(ZenLoad::PackedMesh(),"");
      mesh = owner.getDecalView(vob, transform(), *decalMesh);

      Tempest::Matrix4x4 m;
      m.identity();
      mesh.setObjMatrix(m);
      } else {
      pfx = owner.getView(vob);
      pfx.setActive(true);
      pfx.setObjMatrix(transform());
      }
    } else {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;

    mesh = owner.getStaticView(vob.visual.c_str());
    mesh.setObjMatrix(transform());

    if(vob.cdDyn || vob.cdStatic) {
      physic = PhysicMesh(*view,*owner.physic());
      physic.setObjMatrix(transform());
      }
    }
  }
