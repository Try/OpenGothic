#include "staticobj.h"

#include "world.h"

StaticObj::StaticObj(const ZenLoad::zCVobData& vob,World& owner) {
  if(!vob.showVisual)
    return;

  float v[16]={};
  std::memcpy(v,vob.worldMatrix.m,sizeof(v));
  auto objMat = Tempest::Matrix4x4(v);

  if(vob.visual.rfind(".pfx")==vob.visual.size()-4) {
    const ParticleFx* view = owner.script().getParticleFx(vob.visual.substr(0,vob.visual.size()-4).c_str());
    if(view==nullptr)
      return;
    float x=0,y=0,z=0;
    objMat.project(x,y,z);
    pfx = owner.getView(view);
    //pfx.setActive(true); // TODO
    pfx.setPosition(x,y,z);
    } else {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;

    mesh   = owner.getStaticView(vob.visual.c_str());
    mesh.setObjMatrix(objMat);

    if(vob.cdDyn || vob.cdStatic) {
      auto physicMesh = Resources::physicMesh(view);
      physic = owner.physic()->staticObj(physicMesh,objMat);
      }
    }
  }
