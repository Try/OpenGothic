#include "worldview.h"

#include <Tempest/Application>

#include "world/world.h"
#include "rendererstorage.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const ZenLoad::PackedMesh &wmesh, const RendererStorage &storage)
  :owner(world),storage(storage),sky(storage),land(storage,wmesh),vobGroup(storage),objGroup(storage),itmGroup(storage) {
  sky.setWorld(owner);
  vobGroup.reserve(8192,0);
  objGroup.reserve(8192,2048);
  itmGroup.reserve(8192,0);

  sun.setDir(1,-1,1);
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

void WorldView::tick(uint64_t /*dt*/) {
  float t = float(double(owner.time().timeInDay().toInt())/double(gtime(1,0,0).toInt()));
  t = std::fmod(t+0.5f,1.f);
  //float k = 1.f-std::fabs(t-0.5f)*2.f;

  //float  t = (Application::tickCount()%40000)/40000.f; // [0-1]
  float  k = std::fabs(t*2.f-1.f)*2.f-1.f;// [-1 - 0 - 1 - 0 - -1]

  //static float k=0.2f;

  float a  = std::max(0.f,k);
  auto clr = Vec3(0.75f,0.75f,0.75f)*a;
  ambient  = Vec3(0.2f,0.2f,0.3f)*(1.f-a)+Vec3(0.25f,0.25f,0.25f)*a;

  setupSunDir(k,t);
  sun.setColor(clr);
  }

void WorldView::setupSunDir(float t,float pulse) {
  float a  = 360-360*pulse;
  a = a*float(M_PI/180.0);

  sun.setDir(std::cos(a),std::min(0.9f,-1.0f*t),std::sin(a));
  }

bool WorldView::needToUpdateCmd() const {
  return nToUpdateCmd ||
         vobGroup.needToUpdateCommands() ||
         objGroup.needToUpdateCommands() ||
         itmGroup.needToUpdateCommands();
  }

void WorldView::updateCmd(const World &world,const Tempest::Texture2d& shadow,
                          const RenderPass& mainPass,const RenderPass& shadowPass) {
  if(!needToUpdateCmd())
    return;
  prebuiltCmdBuf(world,shadow,mainPass,shadowPass);

  vobGroup.setAsUpdated();
  objGroup.setAsUpdated();
  itmGroup.setAsUpdated();
  nToUpdateCmd=false;
  }

void WorldView::updateUbo(const Matrix4x4& view,const Tempest::Matrix4x4* shadow,size_t shCount,uint32_t imgId) {
  auto viewProj=this->viewProj(view);

  sky .setMatrix(imgId,viewProj);
  sky .setLight (sun.dir());
  land.setMatrix(imgId,viewProj,shadow,shCount);
  land.setLight (sun,ambient);

  vobGroup.setModelView(viewProj,shadow[0]);
  vobGroup.setLight    (sun,ambient);
  vobGroup.updateUbo   (imgId);
  objGroup.setModelView(viewProj,shadow[0]);
  objGroup.setLight    (sun,ambient);
  objGroup.updateUbo   (imgId);
  itmGroup.setModelView(viewProj,shadow[0]);
  itmGroup.setLight    (sun,ambient);
  itmGroup.updateUbo   (imgId);
  }

void WorldView::drawShadow(PrimaryCommandBuffer &cmd, FrameBuffer &fbo,const RenderPass &pass,
                           uint32_t /*imgId*/, uint8_t layer) {
  if(!cmdShadow[layer].empty()) {
    const uint32_t fId=storage.device.frameId();
    cmd.exec(fbo,pass,cmdShadow[layer][fId]);
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
  cmdShadow[0].clear();
  cmdShadow[1].clear();
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
  if(vob.showVisual)
    obj.mesh = vobGroup.get(*mesh,0,0,0);
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

void WorldView::prebuiltCmdBuf(const World &world, const Texture2d& shadowMap,
                               const RenderPass& mainPass, const RenderPass& shadowPass) {
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

  // cascade#0 detail shadow
  for(size_t i=0;i<count;++i) {
    auto cmd=device.commandSecondaryBuffer(shadowPass,shadowMap.w(),shadowMap.h());

    cmd.begin();
    land    .drawShadow(cmd,i,0);
    vobGroup.drawShadow(cmd,i);
    objGroup.drawShadow(cmd,i);
    itmGroup.drawShadow(cmd,i);
    cmd.end();
    cmdShadow[0].emplace_back(std::move(cmd));
    }

  // cascade#1 shadow
  for(size_t i=0;i<count;++i) {
    auto cmd=device.commandSecondaryBuffer(shadowPass,shadowMap.w(),shadowMap.h());
    cmd.begin();
    land.drawShadow(cmd,i,1);
    cmd.end();
    cmdShadow[1].emplace_back(std::move(cmd));
    }

  for(size_t i=0;i<count;++i) {
    auto cmd=device.commandSecondaryBuffer(mainPass,vpWidth,vpHeight);

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
