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
    float x=0,y=0,z=0;
    objMat.project(x,y,z);
    pfx = owner.getView(view);
    //pfx.setActive(true); // TODO
    pfx.setPosition(x,y,z);
    } else
  if(FileExt::hasExt(vob.visual,"TGA")){
    if(vob.visualCamAlign==0) {
      float x = objMat.at(3,0);
      float y = objMat.at(3,1);
      float z = objMat.at(3,2);

      decalMesh = std::make_unique<ProtoMesh>(ZenLoad::PackedMesh(),"");
      mesh = owner.getDecalView(vob.visual.c_str(), x,y,z, *decalMesh);

      Tempest::Matrix4x4 m;
      m.identity();
      mesh.setObjMatrix(m);
      } else {
      // single particle
      }
    } else {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;

    mesh   = owner.getStaticView(vob.visual.c_str());
    mesh.setObjMatrix(objMat);

    if(vob.cdDyn || vob.cdStatic) {
      physic = PhysicMesh(*view,*owner.physic());
      physic.setObjMatrix(objMat);
      }
    }
  }
