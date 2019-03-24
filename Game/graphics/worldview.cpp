#include "worldview.h"

#include "world/world.h"
#include "rendererstorage.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const ZenLoad::PackedMesh &wmesh, const RendererStorage &storage)
  :owner(world),storage(storage),sky(storage),land(storage,wmesh),vobGroup(storage),objGroup(storage),itmGroup(storage) {
  sky.setWorld(owner);
  vobGroup.reserve(8192,0);
  objGroup.reserve(8192,2048);
  itmGroup.reserve(8192,0);
  }

WorldView::~WorldView() {
  // cmd buffers must not be in use
  storage.device.waitIdle();
  }

void WorldView::initPipeline(uint32_t w, uint32_t h) {
  proj.perspective(45.0f, float(w)/float(h), 0.05f, 100.0f);
  //projective.translate(0,0,0.5f);
  nToUpdateCmd=true;
  }

Matrix4x4 WorldView::viewProj(const Matrix4x4 &view) const {
  auto viewProj=proj;
  viewProj.mul(view);
  return viewProj;
  }

bool WorldView::needToUpdateCmd() const {
  return nToUpdateCmd || vobGroup.needToUpdateCommands() || objGroup.needToUpdateCommands() || itmGroup.needToUpdateCommands();
  }

void WorldView::updateCmd(const World &world) {
  if(needToUpdateCmd()){
    prebuiltCmdBuf(world);

    vobGroup.setAsUpdated();
    objGroup.setAsUpdated();
    itmGroup.setAsUpdated();
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
  itmGroup.setModelView(viewProj);
  itmGroup.updateUbo   (imgId);
  }

void WorldView::draw(CommandBuffer &cmd, FrameBuffer &fbo) {
  const uint32_t fId=storage.device.frameId();
  if(!cmdLand.empty()) {
    cmd.setSecondaryPass(fbo,storage.pass());
    cmd.exec(cmdLand[fId]);
    }
  }

void WorldView::resetCmd() {
  // cmd buffers must not be in use
  storage.device.waitIdle();
  cmdLand.clear();
  nToUpdateCmd=true;
  }


StaticObjects::Mesh WorldView::getView(const std::string &visual, int32_t headTex, int32_t teethTex, int32_t bodyColor) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh,headTex,teethTex,bodyColor);
  return StaticObjects::Mesh();
  }

StaticObjects::Mesh WorldView::getStaticView(const std::string &visual,int32_t material) {
  if(auto mesh=Resources::loadMesh(visual))
    return itmGroup.get(*mesh,material,0,material);
  return StaticObjects::Mesh();
  }

void WorldView::addStatic(const ZenLoad::zCVobData &vob) {
  auto mesh = Resources::loadMesh(vob.visual);
  if(!mesh)
    return;

  auto physic = (vob.cdDyn || vob.cdStatic) ? Resources::physicMesh(mesh) : nullptr;
  float v[16]={};
  std::memcpy(v,vob.worldMatrix.m,sizeof(v));
  auto objMat = Tempest::Matrix4x4(v);

  StaticObj obj;
  obj.mesh   = vobGroup.get(*mesh,0,0,0);
  obj.physic = owner.physic()->staticObj(physic,objMat);
  obj.mesh.setObjMatrix(objMat);

  objStatic.push_back(std::move(obj));
  }

std::shared_ptr<Pose> WorldView::get(const Skeleton *s, const Animation::Sequence *sq, uint64_t sT,std::shared_ptr<Pose>& prev) {
  return animPool.get(s,sq,sT,prev);
  }

std::shared_ptr<Pose> WorldView::get(const Skeleton *s, const Animation::Sequence *sq,
                                     const Animation::Sequence *sq1, uint64_t sT,std::shared_ptr<Pose>& prev) {
  return animPool.get(s,sq,sq1,sT,prev);
  }

void WorldView::updateAnimation(uint64_t tickCount) {
  animPool.updateAnimation(tickCount);
  }

void WorldView::prebuiltCmdBuf(const World &world) {
  auto& device=storage.device;

  resetCmd();

  for(size_t i=0;i<device.maxFramesInFlight();++i){
    auto cmd=device.commandSecondaryBuffer();

    sky     .commitUbo(i);
    land    .commitUbo(i);
    vobGroup.commitUbo(i);
    objGroup.commitUbo(i);
    itmGroup.commitUbo(i);

    cmd.begin(storage.pass());
    sky     .draw(cmd,i,world);
    land    .draw(cmd,i);
    vobGroup.draw(cmd,i);
    objGroup.draw(cmd,i);
    itmGroup.draw(cmd,i);
    cmd.end();

    cmdLand.emplace_back(std::move(cmd));
    }
  }
