#include "staticobj.h"

#include "world.h"
#include "utils/fileext.h"

StaticObj::StaticObj(const ZenLoad::zCVobData& vob,World& owner) {
  if(!vob.showVisual)
    return;

  float v[16]={};
  std::memcpy(v,vob.worldMatrix.m,sizeof(v));
  auto objMat = Tempest::Matrix4x4(v);

  if(FileExt::hasExt(vob.visual,"PFX")) {
    const ParticleFx* view = owner.script().getParticleFx(vob.visual.substr(0,vob.visual.size()-4).c_str());
    if(view==nullptr)
      return;
    pfx = owner.getView(view);
    pfx.setActive(true);
    pfx.setObjMatrix(objMat);
    } else
  if(FileExt::hasExt(vob.visual,"TGA")){
    if(vob.visualCamAlign==0) {
      decalMesh = std::make_unique<ProtoMesh>(ZenLoad::PackedMesh(),"");
      mesh = owner.getDecalView(vob, objMat, *decalMesh);

      Tempest::Matrix4x4 m;
      m.identity();
      mesh.setObjMatrix(m);
      } else {
      const Tempest::Texture2d* view = Resources::loadTexture(vob.visual);
      if(view==nullptr)
        return;
      pfx = owner.getView(view,vob);
      pfx.setActive(true);
      pfx.setObjMatrix(objMat);
      }
    } else {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;

    mesh = owner.getStaticView(vob.visual.c_str());
    mesh.setObjMatrix(objMat);

    if(vob.cdDyn || vob.cdStatic) {
      physic = PhysicMesh(*view,*owner.physic());
      physic.setObjMatrix(objMat);
      }
    }
  }
