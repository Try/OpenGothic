#include "worldview.h"

#include "world/world.h"
#include "rendererstorage.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const RendererStorage &storage)
  :storage(storage),sky(storage),land(storage),vobGroup(storage),objGroup(storage) {
  sky.setWorld(world);

  StaticObj obj;

  objStatic.clear();
  for(auto& v:world.staticObj) {
    obj.mesh   = vobGroup.get(*v.mesh,0,0,0);
    obj.physic = world.physic()->staticObj(v.physic,v.objMat);
    obj.mesh.setObjMatrix(v.objMat);

    objStatic.push_back(std::move(obj));
    }
  for(auto& v:world.interactiveObj) {
    obj.mesh = vobGroup.get(*v.mesh,0,0,0);
    obj.physic = world.physic()->staticObj(v.physic,v.objMat);
    obj.mesh.setObjMatrix(v.objMat);

    objStatic.push_back(std::move(obj));
    }
  }

void WorldView::initPipeline(uint32_t w, uint32_t h) {
  proj.perspective(45.0f, float(w)/float(h), 0.1f, 100.0f);
  //projective.translate(0,0,0.5f);
  nToUpdateCmd=true;
  }

Matrix4x4 WorldView::viewProj(const Matrix4x4 &view) const {
  auto viewProj=proj;
  viewProj.mul(view);
  return viewProj;
  }

void WorldView::updateCmd(const World &world) {
  if(nToUpdateCmd || vobGroup.needToUpdateCommands() || objGroup.needToUpdateCommands()){
    storage.device.waitIdle();
    prebuiltCmdBuf(world);

    vobGroup.setAsUpdated();
    objGroup.setAsUpdated();
    nToUpdateCmd=false;
    }
  }

void WorldView::updateUbo(const Matrix4x4& view,uint32_t imgId) {
  auto viewProj=this->viewProj(view);

  sky     .setMatrix   (imgId,viewProj);
  land    .setMatrix   (imgId,viewProj);

  vobGroup.setModelView(viewProj);
  vobGroup.updateUbo   (imgId);
  objGroup.setModelView(viewProj);
  objGroup.updateUbo   (imgId);
  }

void WorldView::draw(CommandBuffer &cmd, FrameBuffer &fbo) {
  const uint32_t fId=storage.device.frameId();
  if(!cmdLand.empty()) {
    cmd.setSecondaryPass(fbo,storage.pass());
    cmd.exec(cmdLand[fId]);
    }
  }


StaticObjects::Mesh WorldView::getView(const std::string &visual, int32_t headTex, int32_t teethTex, int32_t bodyColor) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh,headTex,teethTex,bodyColor);
  return StaticObjects::Mesh();
  }

void WorldView::prebuiltCmdBuf(const World &world) {
  auto& device=storage.device;

  cmdLand.clear();

  for(size_t i=0;i<device.maxFramesInFlight();++i){
    auto cmd=device.commandSecondaryBuffer();

    sky     .commitUbo(i);
    land    .commitUbo(i);
    vobGroup.commitUbo(i);
    objGroup.commitUbo(i);

    cmd.begin(storage.pass());
    sky     .draw(cmd,i,world);
    land    .draw(cmd,i,world);
    vobGroup.draw(cmd,i);
    objGroup.draw(cmd,i);
    cmd.end();

    cmdLand.emplace_back(std::move(cmd));
    }
  }
