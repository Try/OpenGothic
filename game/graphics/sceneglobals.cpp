#include "sceneglobals.h"

#include "graphics/shaders.h"
#include "gothic.h"

#include <cassert>

static uint32_t nextPot(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
  }

SceneGlobals::SceneGlobals()
  :lights(*this) {
  auto& device = Resources::device();

  Gothic::inst().onSettingsChanged.bind(this,&SceneGlobals::initSettings);
  initSettings();

  uboGlobalCpu.sunDir=Tempest::Vec3::normalize({1,1,-1});
  uboGlobalCpu.viewProject.identity();
  uboGlobalCpu.viewProjectInv.identity();
  for(auto& s:uboGlobalCpu.viewShadow)
    s.identity();

  for(auto& i:shadowMap)
    i = &Resources::fallbackBlack();

  for(uint8_t lay=0; lay<V_Count; ++lay) {
    uboGlobal[lay] = device.ssbo(nullptr,sizeof(UboGlobal));
    }

  auto& copy = Shaders::inst().copyBuf;
  for(uint8_t fId=0; fId<Resources::MaxFramesInFlight; ++fId)
    for(uint8_t lay=0; lay<V_Count; ++lay) {
      uboGlobalPf[fId][lay] = device.ubo<UboGlobal>(nullptr,1);
      uboCopy[fId][lay] = device.descriptors(copy);
      uboCopy[fId][lay].set(0, uboGlobal[lay]);
      uboCopy[fId][lay].set(1, uboGlobalPf[fId][lay]);
      }
  }

SceneGlobals::~SceneGlobals() {
  Gothic::inst().onSettingsChanged.ubind(this,&SceneGlobals::initSettings);
  }

void SceneGlobals::initSettings() {
  zWindEnabled = Gothic::inst().settingsGetI("ENGINE","zWindEnabled")!=0;

  float peroid  = Gothic::inst().settingsGetF("ENGINE","zWindCycleTime");
  float peroidV = Gothic::inst().settingsGetF("ENGINE","zWindCycleTimeVar");
  windPeriod = uint64_t((peroid+peroidV)*1000.f);
  if(windPeriod<=0) {
    windPeriod   = 1;
    zWindEnabled = false;
    }
  }

void SceneGlobals::setViewProject(const Tempest::Matrix4x4& v, const Tempest::Matrix4x4& p,
                                  float zNear, float zFar,
                                  const Tempest::Matrix4x4* sh) {
  view = v;
  proj = p;

  auto vp = p;
  vp.mul(v);

  uboGlobalCpu.viewProject    = vp;
  uboGlobalCpu.viewProjectInv = vp;
  uboGlobalCpu.viewProjectInv.inverse();
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    uboGlobalCpu.viewShadow[i] = sh[i];

  uboGlobalCpu.clipInfo.x = zNear*zFar;
  uboGlobalCpu.clipInfo.y = zNear-zFar;
  uboGlobalCpu.clipInfo.z = zFar;

  uboGlobalCpu.camPos = Tempest::Vec3(0,0,1);
  uboGlobalCpu.viewProjectInv.project(uboGlobalCpu.camPos);

  Tempest::Vec3 min = {0,0.75,0}, max = {0, 0.75f, 0.9f};
  auto inv = uboGlobalCpu.viewShadow[0]; inv.inverse();
  inv.project(min);
  inv.project(max);

  uboGlobalCpu.viewShadow[1].project(min);
  uboGlobalCpu.viewShadow[1].project(max);
  uboGlobalCpu.closeupShadowSlice = Tempest::Vec2(min.z,max.z);

  uboGlobalCpu.pfxLeft  = Tempest::Vec3::normalize({vp.at(0,0), vp.at(1,0), vp.at(2,0)});
  uboGlobalCpu.pfxTop   = Tempest::Vec3::normalize({vp.at(0,1), vp.at(1,1), vp.at(2,1)});
  uboGlobalCpu.pfxDepth = Tempest::Vec3::normalize({vp.at(0,2), vp.at(1,2), vp.at(2,2)});
  }

void SceneGlobals::setViewLwc(const Tempest::Matrix4x4& view, const Tempest::Matrix4x4& proj, const Tempest::Matrix4x4* sh) {
  viewLwc = view;

  auto m = proj;
  m.mul(viewLwc);
  m.inverse();
  uboGlobalCpu.viewProjectLwcInv = m;
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    uboGlobalCpu.viewShadowLwc[i] = sh[i];
  }

void SceneGlobals::setSky(const Sky& s) {
  uboGlobalCpu.sunDir        = s.sunLight().dir();
  uboGlobalCpu.lightCl       = s.sunLight().color();
  uboGlobalCpu.GSunIntensity = s.sunIntensity();
  uboGlobalCpu.lightAmb      = s.ambientLight();
  uboGlobalCpu.cloudsDir[0]  = s.cloudsOffset(0);
  uboGlobalCpu.cloudsDir[1]  = s.cloudsOffset(1);
  uboGlobalCpu.isNight       = s.isNight();
  uboGlobalCpu.exposureInv   = s.autoExposure();
  }

void SceneGlobals::setUnderWater(bool w) {
  uboGlobalCpu.underWater = w ? 1 : 0;
  }

void SceneGlobals::setTime(uint64_t time) {
  tickCount                = time;
  uboGlobalCpu.waveAnim    = 2.f*float(M_PI)*float(tickCount%3000)/3000.f;
  uboGlobalCpu.tickCount32 = uint32_t(tickCount);

  if(zWindEnabled)
    windDir = Tempest::Vec2(0.f,1.f)*1.f; else
    windDir = Tempest::Vec2(0,0);
  }

void SceneGlobals::commitUbo(uint8_t fId) {
  UboGlobal perView[V_Count];
  uboGlobalPf[fId][V_Main].update(&uboGlobalCpu,0,1);

  for(size_t i=V_Shadow0; i<V_Count; ++i) {
    auto& ubo = perView[i];
    ubo = uboGlobalCpu;
    if(i!=V_Main)
      ubo.viewProject = uboGlobalCpu.viewShadow[i-V_Shadow0];
    std::memcpy(ubo.frustrum, frustrum[i].f, sizeof(ubo.frustrum));
    }

  for(size_t i=0; i<V_Count; ++i) {
    uboGlobalPf[fId][i].update(&perView[i],0,1);
    }
  }

void SceneGlobals::prepareGlobals(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  static_assert(sizeof(UboGlobal)%sizeof(uint32_t)==0);

  auto& pso = Shaders::inst().copyBuf;
  const uint32_t size = sizeof(UboGlobal)/sizeof(uint32_t);
  for(uint8_t lay=0; lay<V_Count; ++lay) {
    cmd.setUniforms(pso, uboCopy[fId][lay], &size, sizeof(size));
    cmd.dispatchThreads(size);
    }
  }

void SceneGlobals::setResolution(uint32_t w, uint32_t h) {
  if(w==0)
    w = 1;
  if(h==0)
    h = 1;
  uboGlobalCpu.screenResInv = Tempest::Vec2(1.f/float(w), 1.f/float(h));
  uboGlobalCpu.screenRes    = Tempest::Point(int(w),int(h));
  if(hiZ!=nullptr && !hiZ->isEmpty()) {
    uint32_t hw = nextPot(w);
    uint32_t hh = nextPot(h);

    uboGlobalCpu.hiZTileSize = Tempest::Point(int(hw)/hiZ->w(),int(hh)/hiZ->h());
    }
  }

void SceneGlobals::setHiZ(const Tempest::Texture2d& t) {
  hiZ = &t;
  }

void SceneGlobals::setShadowMap(const Tempest::Texture2d* tex[]) {
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    shadowMap[i] = tex[i];
  }

const Tempest::Matrix4x4& SceneGlobals::viewProject() const {
  return uboGlobalCpu.viewProject;
  }

const Tempest::Matrix4x4& SceneGlobals::viewProjectInv() const {
  return uboGlobalCpu.viewProjectInv;
  }

const Tempest::Matrix4x4& SceneGlobals::viewShadow(uint8_t view) const {
  return uboGlobalCpu.viewShadow[view];
  }

const Tempest::Vec3 SceneGlobals::clipInfo() const {
  return uboGlobalCpu.clipInfo;
  }

const Tempest::Matrix4x4  SceneGlobals::viewProjectLwcInv() const {
  auto m = proj;
  m.mul(viewLwc);
  m.inverse();
  return m;
  }
