#include "worldview.h"

#include <Tempest/Application>

#include "world/world.h"
#include "rendererstorage.h"
#include "graphics/submesh/packedmesh.h"
#include "graphics/dynamic/painter3d.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const PackedMesh &wmesh, const RendererStorage &storage)
  : owner(world),storage(storage),sGlobal(storage),
    sky(sGlobal),objGroup(sGlobal),pfxGroup(sGlobal),land(*this,sGlobal,wmesh) {
  sky.setWorld(owner);
  pfxGroup.resetTicks();
  }

WorldView::~WorldView() {
  // cmd buffers must not be in use
  storage.device.waitIdle();
  }

void WorldView::initPipeline(uint32_t w, uint32_t h) {
  proj.perspective(45.0f, float(w)/float(h), 0.05f, 100.0f);
  vpWidth  = w;
  vpHeight = h;
  }

Matrix4x4 WorldView::viewProj(const Matrix4x4 &view) const {
  auto viewProj=proj;
  viewProj.mul(view);
  return viewProj;
  }

const Light &WorldView::mainLight() const {
  return sGlobal.sun;
  }

void WorldView::tick(uint64_t /*dt*/) {
  auto pl = owner.player();
  if(pl!=nullptr) {
    pfxGroup.setViewerPos(pl->position());
    }
  }

void WorldView::addLight(const ZenLoad::zCVobData& vob) {
  uint8_t cl[4];
  std::memcpy(cl,&vob.zCVobLight.color,4);

  Light l;
  l.setPosition(Vec3(vob.position.x,vob.position.y,vob.position.z));
  l.setColor   (Vec3(cl[2]/255.f,cl[1]/255.f,cl[1]/255.f));
  l.setRange   (vob.zCVobLight.range);

  pendingLights.push_back(l);
  }

void WorldView::setupSunDir(float pulse,float ang) {
  float a  = 360-360*ang;
  a = a*float(M_PI/180.0);

  sGlobal.sun.setDir(std::cos(a),std::min(0.9f,-1.0f*pulse),std::sin(a));
  }

void WorldView::setModelView(const Matrix4x4& view, const Tempest::Matrix4x4* shadow, size_t shCount) {
  updateLight();
  sGlobal.setModelView(viewProj(view),shadow,shCount);
  }

void WorldView::setFrameGlobals(const Texture2d& shadow, uint64_t tickCount, uint8_t fId) {
  if(pendingLights.size()!=sGlobal.lights.size()) {
    sGlobal.lights.set(pendingLights);
    // wait before update all descriptors
    sGlobal.storage.device.waitIdle();
    objGroup.setupUbo();
    pfxGroup.setupUbo();
    }
  if(&shadow!=sGlobal.shadowMap) {
    // wait before update all descriptors
    sGlobal.storage.device.waitIdle();
    sGlobal.setShadowmMap(shadow);

    objGroup.setupUbo();
    pfxGroup.setupUbo();
    }
  sGlobal.tickCount = tickCount;
  pfxGroup.tick(tickCount);
  sGlobal.commitUbo(fId);
  }

void WorldView::drawShadow(Tempest::Encoder<CommandBuffer>& cmd, Painter3d& painter, uint8_t fId, uint8_t layer) {
  objGroup.drawShadow(painter,cmd,fId,layer);
  }

void WorldView::drawMain(Tempest::Encoder<CommandBuffer>& cmd, Painter3d& painter, uint8_t fId) {
  objGroup.draw(painter,cmd,fId);
  sky     .draw(painter,fId);
  pfxGroup.draw(painter,fId);
  }

MeshObjects::Mesh WorldView::getLand(Tempest::VertexBuffer<Resources::Vertex>& vbo,
                                     Tempest::IndexBuffer<uint32_t>& ibo,
                                     const Material& mat,
                                     const Bounds& bbox) {
  return objGroup.get(vbo,ibo,mat,bbox);
  }

MeshObjects::Mesh WorldView::getView(const char* visual, int32_t headTex, int32_t teethTex, int32_t bodyColor) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh,headTex,teethTex,bodyColor,false);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::getItmView(const char* visual, int32_t material) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh,material,0,0,true);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::getAtachView(const ProtoMesh::Attach& visual) {
  return objGroup.get(visual,false);
  }

MeshObjects::Mesh WorldView::getStaticView(const char* visual) {
  if(auto mesh=Resources::loadMesh(visual))
    return objGroup.get(*mesh,0,0,0,true);
  return MeshObjects::Mesh();
  }

MeshObjects::Mesh WorldView::getDecalView(const ZenLoad::zCVobData& vob,
                                          const Tempest::Matrix4x4& obj, ProtoMesh& out) {
  out = owner.physic()->decalMesh(vob,obj);
  return objGroup.get(out,0,0,0,true);
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
  sGlobal.ambient  = Vec3(0.2f,0.2f,0.3f)*(1.f-a)+Vec3(0.25f,0.25f,0.25f)*a;

  setupSunDir(pulse,std::fmod(k+0.25f,1.f));
  sGlobal.sun.setColor(clr);
  sky.setDayNight(std::min(std::max(pulse*3.f,0.f),1.f));
  }

void WorldView::resetCmd() {
  // cmd buffers must not be in use
  storage.device.waitIdle();
  mainLay   = nullptr;
  shadowLay = nullptr;
  }

