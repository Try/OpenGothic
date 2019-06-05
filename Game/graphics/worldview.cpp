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

  sun.setDir(1,-2,1);
  }

WorldView::~WorldView() {
  // cmd buffers must not be in use
  storage.device.waitIdle();
  }

void WorldView::initPipeline(uint32_t w, uint32_t h) {
  proj.perspective(45.0f, float(w)/float(h), 0.05f, 100.0f);
  vpWidth  = w;
  vpHeight = h;
  nToUpdateCmd=true;
  }

Matrix4x4 WorldView::viewProj(const Matrix4x4 &view) const {
  auto viewProj=proj;
  viewProj.mul(view);
  return viewProj;
  }

const Light &WorldView::mainLight() const {
  return sun;
  }

bool WorldView::needToUpdateCmd() const {
  return nToUpdateCmd || vobGroup.needToUpdateCommands() || objGroup.needToUpdateCommands() || itmGroup.needToUpdateCommands();
  }

void WorldView::updateCmd(const World &world,const Tempest::Texture2d& shadow,const RenderPass& shadowPass) {
  if(!needToUpdateCmd())
    return;
  prebuiltCmdBuf(world,shadow,shadowPass);

  vobGroup.setAsUpdated();
  objGroup.setAsUpdated();
  itmGroup.setAsUpdated();
  nToUpdateCmd=false;
  }

void WorldView::updateUbo(const Matrix4x4& view,const Tempest::Matrix4x4 &shadow,uint32_t imgId) {
  auto viewProj=this->viewProj(view);

  sky .setMatrix(imgId,viewProj);
  sky .setLight (sun.dir());
  land.setMatrix(imgId,viewProj,shadow);
  land.setLight (sun.dir());

  vobGroup.setModelView(viewProj,shadow);
  vobGroup.setLight    (sun.dir());
  vobGroup.updateUbo   (imgId);
  objGroup.setModelView(viewProj,shadow);
  objGroup.setLight    (sun.dir());
  objGroup.updateUbo   (imgId);
  itmGroup.setModelView(viewProj,shadow);
  itmGroup.setLight    (sun.dir());
  itmGroup.updateUbo   (imgId);
  }

void WorldView::drawShadow(PrimaryCommandBuffer &cmd, FrameBuffer &fbo,const RenderPass &pass, uint32_t /*imgId*/) {
  if(!cmdShadow.empty()) {
    const uint32_t fId=storage.device.frameId();
    cmd.exec(fbo,pass,cmdShadow[fId]);
    }
  }

void WorldView::draw(PrimaryCommandBuffer &cmd, FrameBuffer &fbo, const RenderPass &pass, uint32_t /*imgId*/) {
  if(!cmdMain.empty()) {
    const uint32_t fId=storage.device.frameId();
    cmd.exec(fbo,pass,cmdMain[fId]);
    }
  }

void WorldView::resetCmd() {
  // cmd buffers must not be in use
  storage.device.waitIdle();
  cmdMain.clear();
  cmdShadow.clear();
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

void WorldView::prebuiltCmdBuf(const World &world,const Texture2d& shadowMap,const RenderPass& shadowPass) {
  auto&  device = storage.device;
  size_t count  = device.maxFramesInFlight();

  resetCmd();

  for(size_t i=0;i<count;++i){
    sky     .commitUbo(i);
    land    .commitUbo(i,shadowMap);
    vobGroup.commitUbo(i,shadowMap);
    objGroup.commitUbo(i,shadowMap);

    itmGroup.commitUbo(i,shadowMap);
    }

  for(size_t i=0;i<count;++i) {
    auto cmd=device.commandSecondaryBuffer(shadowPass,shadowMap.w(),shadowMap.h());

    cmd.begin();
    land    .drawShadow(cmd,i);
    vobGroup.drawShadow(cmd,i);
    objGroup.drawShadow(cmd,i);
    itmGroup.drawShadow(cmd,i);
    cmd.end();
    cmdShadow.emplace_back(std::move(cmd));
    }

  for(size_t i=0;i<count;++i) {
    auto cmd=device.commandSecondaryBuffer(storage.pass(),vpWidth,vpHeight);

    cmd.begin();
    land    .draw(cmd,i);
    vobGroup.draw(cmd,i);
    objGroup.draw(cmd,i);
    itmGroup.draw(cmd,i);
    sky     .draw(cmd,i,world);
    cmd.end();

    cmdMain.emplace_back(std::move(cmd));
    }
  }
