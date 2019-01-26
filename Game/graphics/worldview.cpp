#include "worldview.h"

#include "world/world.h"
#include "rendererstorage.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const RendererStorage &storage)
  :storage(storage),land(storage),vobGroup(storage),objGroup(storage) {
  StaticObjects::Mesh obj;

  objStatic.clear();
  for(auto& v:world.staticObj) {
    obj = vobGroup.get(*v.mesh);
    obj.setObjMatrix(v.objMat);

    objStatic.push_back(std::move(obj));
    }
  }

void WorldView::initPipeline(uint32_t w, uint32_t h) {
  projective.perspective(45.0f, float(w)/float(h), 0.1f, 100.0f);
  nToUpdateCmd=true;
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
  auto viewProj=projective;
  viewProj.mul(view);

  land    .setMatrix   (imgId,viewProj);
  vobGroup.setModelView(viewProj);
  vobGroup.setMatrix   (imgId);
  objGroup.setModelView(viewProj);
  objGroup.setMatrix   (imgId);
  }

void WorldView::draw(CommandBuffer &cmd, FrameBuffer &fbo) {
  const uint32_t fId=storage.device.frameId();
  if(!cmdLand.empty()) {
    cmd.setSecondaryPass(fbo,storage.pass());
    cmd.exec(cmdLand[fId]);
    }
  }

StaticObjects::Mesh WorldView::getView(const std::string &visual) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh);
  return StaticObjects::Mesh();
  }

void WorldView::prebuiltCmdBuf(const World &world) {
  auto& device=storage.device;

  cmdLand.clear();

  for(size_t i=0;i<device.maxFramesInFlight();++i){
    auto cmd=device.commandSecondaryBuffer();

    land    .commitUbo(i);
    vobGroup.commitUbo(i);
    objGroup.commitUbo(i);

    cmd.begin(storage.pass());
    land    .draw(cmd,i,world);
    vobGroup.draw(cmd,i);
    objGroup.draw(cmd,i);
    cmd.end();

    cmdLand.emplace_back(std::move(cmd));
    }
  }
