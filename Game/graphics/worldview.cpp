#include "worldview.h"

#include <Tempest/Application>

#include "world/world.h"
#include "rendererstorage.h"
#include "graphics/submesh/packedmesh.h"
#include "graphics/dynamic/painter3d.h"
#include "utils/gthfont.h"

using namespace Tempest;

WorldView::WorldView(const World &world, const PackedMesh &wmesh, const RendererStorage &storage)
  : owner(world),storage(storage),sGlobal(storage),visuals(sGlobal),
    objGroup(visuals),pfxGroup(sGlobal,visuals),land(*this,visuals,wmesh) {
  visuals.setWorld(owner);
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
  resetCmd();
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
  Light l;
  l.setPosition(Vec3(vob.position.x,vob.position.y,vob.position.z));

  if(vob.zCVobLight.dynamic.rangeAniScale.size()>0) {
    l.setRange(vob.zCVobLight.dynamic.rangeAniScale,vob.zCVobLight.range,vob.zCVobLight.dynamic.rangeAniFPS,vob.zCVobLight.dynamic.rangeAniSmooth);
    } else {
    l.setRange(vob.zCVobLight.range);
    }

  if(vob.zCVobLight.dynamic.colorAniList.size()>0) {
    l.setColor(vob.zCVobLight.dynamic.colorAniList,vob.zCVobLight.dynamic.colorAniListFPS,vob.zCVobLight.dynamic.colorAniSmooth);
    } else {
    l.setColor(vob.zCVobLight.color);
    }

  pendingLights.push_back(std::move(l));
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
    visuals.setupUbo();
    }
  if(&shadow!=sGlobal.shadowMap) {
    // wait before update all descriptors
    sGlobal.storage.device.waitIdle();
    sGlobal.setShadowmMap(shadow);
    visuals.setupUbo();
    }
  pfxGroup.tick(tickCount);
  sGlobal.lights.tick(tickCount);
  sGlobal .setTime(tickCount);
  sGlobal .commitUbo(fId);

  sGlobal.lights.preFrameUpdate(fId);
  visuals .preFrameUpdate(fId);
  pfxGroup.preFrameUpdate(fId);
  }

void WorldView::setGbuffer(const Texture2d& diffuse, const Texture2d& norm, const Texture2d& depth) {
  if(sGlobal.gbufDiffuse == &diffuse ||
     sGlobal.gbufNormals == &norm    ||
     sGlobal.gbufDepth   == &depth)
    return;
  sGlobal.gbufDiffuse = &diffuse;
  sGlobal.gbufNormals = &norm;
  sGlobal.gbufDepth   = &depth;

  // wait before update all descriptors
  sGlobal.storage.device.waitIdle();
  sGlobal.lights.setupUbo();
  visuals.setupUbo();
  }

void WorldView::dbgLights(Painter& p) const {
  int cnt = 0;
  p.setBrush(Color(1,0,0,0.01f));
  //p.setBrush(Color(1,0,0,1.f));

  auto mv = sGlobal.modelView();
  for(auto& i:pendingLights) {
    float r  = i.range();
    auto  pt = i.position();
    Vec3 px[9] = {};
    px[0] = pt+Vec3(-r,-r,-r);
    px[1] = pt+Vec3( r,-r,-r);
    px[2] = pt+Vec3( r, r,-r);
    px[3] = pt+Vec3(-r, r,-r);
    px[4] = pt+Vec3(-r,-r, r);
    px[5] = pt+Vec3( r,-r, r);
    px[6] = pt+Vec3( r, r, r);
    px[7] = pt+Vec3(-r, r, r);
    px[8] = pt;

    for(auto& i:px) {
      mv.project(i.x,i.y,i.z);
      i.x = (i.x+1.f)*0.5f;
      i.y = (i.y+1.f)*0.5f;
      }

    int x = int(px[8].x*float(vpWidth ));
    int y = int(px[8].y*float(vpHeight));

    int x0 = x, x1 = x;
    int y0 = y, y1 = y;
    float z0=px[8].z, z1=px[8].z;

    for(auto& i:px) {
      int x = int(i.x*float(vpWidth ));
      int y = int(i.y*float(vpHeight));
      x0 = std::min(x0, x);
      y0 = std::min(y0, y);
      x1 = std::max(x1, x);
      y1 = std::max(y1, y);
      z0 = std::min(z0, i.z);
      z1 = std::max(z1, i.z);
      }

    if(z1<0.f || z0>1.f)
      continue;
    if(x1<0 || x0>int(vpWidth))
      continue;
    if(y1<0 || y0>int(vpHeight))
      continue;

    cnt++;
    p.drawRect(x0,y0,x1-x0,y1-y0);
    p.drawRect(x0,y0,3,3);
    }

  auto& fnt = Resources::font();
  char  buf[250]={};
  std::snprintf(buf,sizeof(buf),"light count = %d",cnt);
  fnt.drawText(p,10,50,buf);
  }

void WorldView::drawShadow(Tempest::Encoder<CommandBuffer>& cmd, Painter3d& painter, uint8_t fId, uint8_t layer) {
  visuals.drawShadow(cmd,painter,fId,layer);
  }

void WorldView::drawGBuffer(Tempest::Encoder<CommandBuffer>& cmd, Painter3d& painter, uint8_t fId) {
  visuals.drawGBuffer(cmd,painter,fId);
  }

void WorldView::drawMain(Tempest::Encoder<CommandBuffer>& cmd, Painter3d& painter, uint8_t fId) {
  visuals.draw(cmd,painter,fId);
  }

void WorldView::drawLights(Tempest::Encoder<CommandBuffer>& cmd, Painter3d&, uint8_t fId) {
  sGlobal.lights.draw(cmd,fId);
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

MeshObjects::Mesh WorldView::getAtachView(const ProtoMesh::Attach& visual, const int32_t version) {
  return objGroup.get(visual,version,false);
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

PfxObjects::Emitter WorldView::getView(const ZenLoad::zCVobData& vob) {
  return pfxGroup.get(vob);
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
  visuals.setDayNight(std::min(std::max(pulse*3.f,0.f),1.f));
  }

void WorldView::resetCmd() {
  // cmd buffers must not be in use
  visuals.setupUbo();
  sGlobal.lights.setupUbo();
  }

