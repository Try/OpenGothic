#include "worldview.h"

#include <Tempest/Application>

#include "world/world.h"
#include "rendererstorage.h"
#include "graphics/submesh/packedmesh.h"
#include "graphics/dynamic/painter3d.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const PackedMesh &wmesh, const RendererStorage &storage)
  :owner(world),storage(storage),sky(storage),land(storage,wmesh),
    vobGroup(storage),objGroup(storage),itmGroup(storage),decGroup(storage),pfxGroup(storage) {
  sky.setWorld(owner);
  vobGroup.reserve(8192,0);
  objGroup.reserve(8192,2048);
  itmGroup.reserve(8192,0);
  pfxGroup.resetTicks();

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
  invalidateCmd();
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
  auto pl = owner.player();
  if(pl!=nullptr) {
    pfxGroup.setViewerPos(pl->position());
    }
  }

void WorldView::setupSunDir(float pulse,float ang) {
  float a  = 360-360*ang;
  a = a*float(M_PI/180.0);

  sun.setDir(std::cos(a),std::min(0.9f,-1.0f*pulse),std::sin(a));
  }

void WorldView::drawShadow(Encoder<PrimaryCommandBuffer> &cmd, Painter3d& painter, uint8_t fId, uint8_t layer) {
  if(!frame[fId].actual)
    return;
  cmd.exec(frame[fId].cmdShadow   [layer]);

  land    .drawShadow(painter,fId,layer);
  vobGroup.drawShadow(painter,fId,layer);
  objGroup.drawShadow(painter,fId,layer);
  if(layer==0) {
    itmGroup.drawShadow(painter,fId,layer);
    decGroup.drawShadow(painter,fId,layer);
    }
  painter.commit(cmd);
  }

void WorldView::drawMain(Encoder<PrimaryCommandBuffer> &cmd, Painter3d& painter, uint8_t fId) {
  if(!frame[fId].actual)
    return;

  land    .draw(painter,fId);
  vobGroup.draw(painter,fId);
  objGroup.draw(painter,fId);
  itmGroup.draw(painter,fId);
  decGroup.draw(painter,fId);
  painter.commit(cmd);

  cmd.exec(frame[fId].cmdMain);
  }

MeshObjects::Mesh WorldView::getView(const char* visual, int32_t headTex, int32_t teethTex, int32_t bodyColor) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh,headTex,teethTex,bodyColor);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::getItmView(const char* visual, int32_t material) {
  if(auto mesh=Resources::loadMesh(visual))
    return itmGroup.get(*mesh,material,0,0);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::getAtachView(const ProtoMesh::Attach& visual) {
  return objGroup.get(visual);
  }

MeshObjects::Mesh WorldView::getStaticView(const char* visual) {
  if(auto mesh=Resources::loadMesh(visual))
    return vobGroup.get(*mesh,0,0,0);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::getDecalView(const ZenLoad::zCVobData& vob,
                                          const Tempest::Matrix4x4& obj, ProtoMesh& out) {
  out = owner.physic()->decalMesh(vob,obj);
  return decGroup.get(out,0,0,0);
  }

PfxObjects::Emitter WorldView::getView(const ParticleFx *decl) {
  if(decl!=nullptr)
    return pfxGroup.get(*decl);
  return PfxObjects::Emitter();
  }

PfxObjects::Emitter WorldView::getView(const Texture2d* spr,const ZenLoad::zCVobData& vob) {
  if(spr!=nullptr)
    return pfxGroup.get(spr,vob);
  return PfxObjects::Emitter();
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
  for(auto& i:frame)
    i.actual=false;
  land    .invalidateCmd();
  vobGroup.invalidateCmd();
  objGroup.invalidateCmd();
  itmGroup.invalidateCmd();
  decGroup.invalidateCmd();
  pfxGroup.invalidateCmd();
  sky     .invalidateCmd();

  mainLay   = nullptr;
  shadowLay = nullptr;
  }

bool WorldView::needToUpdateCmd(uint8_t frameId) const {
  return land    .needToUpdateCommands(frameId) ||
         vobGroup.needToUpdateCommands(frameId) ||
         objGroup.needToUpdateCommands(frameId) ||
         itmGroup.needToUpdateCommands(frameId) ||
         decGroup.needToUpdateCommands(frameId) ||
         pfxGroup.needToUpdateCommands(frameId) ||
         sky     .needToUpdateCommands(frameId);
  }

void WorldView::invalidateCmd() {
  for(auto& i:frame)
    i.actual=false;
  }

void WorldView::updateCmd(uint8_t frameId, const World &world,
                          const Attachment& main, const Tempest::Attachment& shadow,
                          const Tempest::FrameBufferLayout &mainLay, const Tempest::FrameBufferLayout &shadowLay) {
  if(this->mainLay  ==nullptr || mainLay  !=*this->mainLay ||
     this->shadowLay==nullptr || shadowLay!=*this->shadowLay) {
    this->mainLay   = &mainLay;
    this->shadowLay = &shadowLay;
    invalidateCmd();
    }

  pfxGroup.tick(world.tickCount());
  builtCmdBuf(frameId,world,main,shadow,mainLay,shadowLay);
  }

void WorldView::updateUbo(uint8_t frameId, const Matrix4x4& view, const Tempest::Matrix4x4* shadow, size_t shCount) {
  updateLight();

  auto viewProj=this->viewProj(view);
  sky .setMatrix(frameId,viewProj);
  sky .setLight (sun.dir());

  land.setMatrix(frameId,viewProj,shadow,shCount);
  land.setLight (sun,ambient);

  vobGroup.setModelView(viewProj,shadow,shCount);
  vobGroup.setLight    (sun,ambient);
  objGroup.setModelView(viewProj,shadow,shCount);
  objGroup.setLight    (sun,ambient);
  itmGroup.setModelView(viewProj,shadow,shCount);
  itmGroup.setLight    (sun,ambient);
  decGroup.setModelView(viewProj,shadow,shCount);
  decGroup.setLight    (sun,ambient);
  pfxGroup.setModelView(viewProj,shadow[0]);
  pfxGroup.setLight    (sun,ambient);
  }

void WorldView::builtCmdBuf(uint8_t frameId, const World &world,
                            const Attachment& main, const Attachment& shadowMap,
                            const FrameBufferLayout& mainLay,const FrameBufferLayout& shadowLay) {
  (void)shadowLay;

  auto&      device    = storage.device;
  auto&      pf        = frame[frameId];
  const bool nToUpdate = needToUpdateCmd(frameId);
  auto&      smTexture = textureCast(shadowMap);

  sky     .commitUbo(frameId);
  land    .commitUbo(frameId,smTexture);

  vobGroup.commitUbo(frameId,smTexture);
  objGroup.commitUbo(frameId,smTexture);
  itmGroup.commitUbo(frameId,smTexture);
  decGroup.commitUbo(frameId,smTexture);
  pfxGroup.commitUbo(frameId,smTexture);

  if(!pf.actual || nToUpdate) {
    pf.actual=true;

    // main draw
    {
    auto cmd    = pf.cmdMain.startEncoding(device,mainLay,main.w(),main.h());
    sky     .draw(cmd,frameId,world);
    pfxGroup.draw(cmd,frameId);
    }
    }

  sky     .setAsUpdated(frameId);
  land    .setAsUpdated(frameId);
  vobGroup.setAsUpdated(frameId);
  objGroup.setAsUpdated(frameId);
  itmGroup.setAsUpdated(frameId);
  decGroup.setAsUpdated(frameId);
  pfxGroup.setAsUpdated(frameId);
  }
