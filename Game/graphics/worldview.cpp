#include "worldview.h"

#include <Tempest/Application>

#include "world/world.h"
#include "rendererstorage.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const ZenLoad::PackedMesh &wmesh, const RendererStorage &storage)
  :owner(world),storage(storage),sky(storage),land(storage,wmesh),
    vobGroup(storage),objGroup(storage),itmGroup(storage),pfxGroup(storage) {
  sky.setWorld(owner);
  vobGroup.reserve(8192,0);
  objGroup.reserve(8192,2048);
  itmGroup.reserve(8192,0);

  sun.setDir(1,-1,1);

  const size_t count = storage.device.maxFramesInFlight();
  frame.reset(new PerFrame[count]);
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
  }

void WorldView::setupSunDir(float pulse,float ang) {
  float a  = 360-360*ang;
  a = a*float(M_PI/180.0);

  sun.setDir(std::cos(a),std::min(0.9f,-1.0f*pulse),std::sin(a));
  }

void WorldView::drawShadow(Encoder<PrimaryCommandBuffer> &cmd, uint8_t layer) {
  uint32_t fId = storage.device.frameId();
  if(!frame[fId].actual)
    return;
  cmd.exec(frame[fId].cmdShadow[layer]);
  }

void WorldView::drawMain(Encoder<PrimaryCommandBuffer> &cmd) {
  uint32_t fId = storage.device.frameId();
  if(!frame[fId].actual)
    return;
  cmd.exec(frame[fId].cmdMain);
  }

MeshObjects::Mesh WorldView::getView(const char* visual, int32_t headTex, int32_t teethTex, int32_t bodyColor) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh,headTex,teethTex,bodyColor);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::getStaticView(const char* visual, int32_t material) {
  if(auto mesh=Resources::loadMesh(visual))
    return itmGroup.get(*mesh,material,0,material);
  return MeshObjects::Mesh();
  }

PfxObjects::Emitter WorldView::getView(const ParticleFx *decl) {
  if(decl!=nullptr)
    return pfxGroup.get(*decl);
  return PfxObjects::Emitter();
  }

void WorldView::addStatic(const ZenLoad::zCVobData &vob) {
  if(vob.visual.rfind(".pfx")==vob.visual.size()-4)
    return addPfx(vob);
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

void WorldView::addPfx(const ZenLoad::zCVobData &vob) {
  float v[16]={};
  std::memcpy(v,vob.worldMatrix.m,sizeof(v));
  auto objMat = Tempest::Matrix4x4(v);

  StaticObj obj;
  if(vob.showVisual) {
    const ParticleFx* pfx = owner.script().getParticleFx(vob.visual.substr(0,vob.visual.size()-4).c_str());
    if(pfx==nullptr)
      return;
    float x=0,y=0,z=0;
    objMat.project(x,y,z);
    obj.pfx = pfxGroup.get(*pfx);
    //obj.pfx.setActive(true); // TODO
    obj.pfx.setPosition(x,y,z);
    }

  objStatic.push_back(std::move(obj));
  }

void WorldView::updateLight() {
  const int64_t rise     = gtime(3,1).toInt();
  const int64_t meridian = gtime(11,46).toInt();
  const int64_t set      = gtime(20,32).toInt();
  const int64_t midnight = gtime(1,0,0).toInt();
  const int64_t now      = owner.time().timeInDay().toInt();

  float pulse = 0.f;
  if(rise<=now && now<meridian){
    pulse =  0.f + float(now-rise)/float(meridian-rise);
    }
  else if(meridian<=now && now<set){
    pulse =  1.f - float(now-meridian)/float(set-meridian);
    }
  else if(set<=now){
    pulse =  0.f - float(now-set)/float(midnight-set);
    }
  else if(now<rise){
    pulse = -1.f + (float(now)/float(rise));
    }

  float k = float(now)/float(midnight);

  float a  = std::max(0.f,std::min(pulse*3.f,1.f));
  auto clr = Vec3(0.75f,0.75f,0.75f)*a;
  ambient  = Vec3(0.2f,0.2f,0.3f)*(1.f-a)+Vec3(0.25f,0.25f,0.25f)*a;

  setupSunDir(pulse,std::fmod(k+0.25f,1.f));
  sun.setColor(clr);
  sky.setDayNight(std::min(std::max(pulse*3.f,0.f),1.f));
  }

void WorldView::resetCmd() {
  // cmd buffers must not be in use
  storage.device.waitIdle();
  nToUpdateCmd=true;
  }

bool WorldView::needToUpdateCmd() const {
  return nToUpdateCmd ||
         vobGroup.needToUpdateCommands() ||
         objGroup.needToUpdateCommands() ||
         itmGroup.needToUpdateCommands() ||
         pfxGroup.needToUpdateCommands();
  }

void WorldView::updateCmd(const World &world,const Tempest::Texture2d& shadow,
                          const Tempest::FrameBufferLayout &mainLay, const Tempest::FrameBufferLayout &shadowLay) {
  if(this->mainLay  ==nullptr || mainLay  !=*this->mainLay ||
     this->shadowLay==nullptr || shadowLay!=*this->shadowLay) {
    this->mainLay   = &mainLay;
    this->shadowLay = &shadowLay;
    nToUpdateCmd    = true;
    }

  if(needToUpdateCmd()) {
    uint32_t count = storage.device.maxFramesInFlight();
    for(uint32_t i=0;i<count;++i) {
      frame[i].actual=false;
      }
    vobGroup.setAsUpdated();
    objGroup.setAsUpdated();
    itmGroup.setAsUpdated();
    pfxGroup.setAsUpdated();
    nToUpdateCmd=false;
    }

  builtCmdBuf(world,shadow,mainLay,shadowLay);
  }

void WorldView::updateUbo(const Matrix4x4& view,const Tempest::Matrix4x4* shadow,size_t shCount) {
  updateLight();

  uint32_t frameId = storage.device.frameId();

  auto viewProj=this->viewProj(view);
  sky .setMatrix(frameId,viewProj);
  sky .setLight (sun.dir());

  land.setMatrix(frameId,viewProj,shadow,shCount);
  land.setLight (sun,ambient);

  vobGroup.setModelView(viewProj,shadow,shCount);
  vobGroup.setLight    (sun,ambient);
  vobGroup.updateUbo   (frameId);
  objGroup.setModelView(viewProj,shadow,shCount);
  objGroup.setLight    (sun,ambient);
  objGroup.updateUbo   (frameId);
  itmGroup.setModelView(viewProj,shadow,shCount);
  itmGroup.setLight    (sun,ambient);
  itmGroup.updateUbo   (frameId);

  pfxGroup.setModelView(viewProj,shadow[0]);
  pfxGroup.setLight    (sun,ambient);
  pfxGroup.updateUbo   (frameId,owner.tickCount());
  }

void WorldView::builtCmdBuf(const World &world, const Texture2d& shadowMap,
                            const FrameBufferLayout& mainLay,const FrameBufferLayout& shadowLay) {
  auto&    device  = storage.device;
  uint32_t frameId = device.frameId();

  auto& pf = frame[device.frameId()];
  if(pf.actual)
    return;
  pf.actual=true;

  sky     .commitUbo(frameId);
  land    .commitUbo(frameId,shadowMap);
  vobGroup.commitUbo(frameId,shadowMap);
  objGroup.commitUbo(frameId,shadowMap);
  itmGroup.commitUbo(frameId,shadowMap);
  pfxGroup.commitUbo(frameId,shadowMap);

  // cascade#0 detail shadow
  {
  auto cmd = pf.cmdShadow[0].startEncoding(device,shadowLay);
  land    .drawShadow(cmd,frameId,0);
  vobGroup.drawShadow(cmd,frameId);
  objGroup.drawShadow(cmd,frameId);
  itmGroup.drawShadow(cmd,frameId);
  }

  // cascade#1 shadow
  {
  auto cmd = pf.cmdShadow[1].startEncoding(device,shadowLay);
  land.drawShadow(cmd,frameId,1);
  vobGroup.drawShadow(cmd,frameId,1);
  }

  // main draw
  {
  auto cmd = pf.cmdMain.startEncoding(device,mainLay);
  land    .draw(cmd,frameId);
  vobGroup.draw(cmd,frameId);
  objGroup.draw(cmd,frameId);
  itmGroup.draw(cmd,frameId);
  sky     .draw(cmd,frameId,world);
  pfxGroup.draw(cmd,frameId);
  }
  }
