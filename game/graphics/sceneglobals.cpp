#include "sceneglobals.h"

#include "objectsbucket.h"
#include "gothic.h"

#include <cassert>

SceneGlobals::SceneGlobals()
  :lights(*this) {
  auto& device = Resources::device();

  Gothic::inst().onSettingsChanged.bind(this,&SceneGlobals::initSettings);
  initSettings();

  uboGlobal.sunDir=Tempest::Vec3::normalize({1,1,-1});

  uboGlobal.viewProject.identity();
  uboGlobal.viewProjectInv.identity();
  for(auto& s:uboGlobal.shadowView)
    s.identity();

  for(auto& i:shadowMap)
    i = &Resources::fallbackBlack();

  sun.setDir(1,-1,1);

  for(uint8_t fId=0; fId<Resources::MaxFramesInFlight; ++fId)
    for(uint8_t lay=0; lay<V_Count; ++lay) {
      uboGlobalPf[fId][lay] = device.ubo<UboGlobal>(nullptr,1);
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

  uboGlobal.viewProject    = vp;
  uboGlobal.viewProjectInv = vp;
  uboGlobal.viewProjectInv.inverse();
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    uboGlobal.shadowView[i] = sh[i];

  uboGlobal.clipInfo.x = zNear*zFar;
  uboGlobal.clipInfo.y = zNear-zFar;
  uboGlobal.clipInfo.z = zFar;

  uboGlobal.camPos = Tempest::Vec3(0,0,1);
  uboGlobal.viewProjectInv.project(uboGlobal.camPos);
  }

void SceneGlobals::setTime(uint64_t time) {
  tickCount            = time;

  if(zWindEnabled)
    windDir = Tempest::Vec2(0.f,1.f)*1.f; else
    windDir = Tempest::Vec2(0,0);
  }

void SceneGlobals::commitUbo(uint8_t fId) {
  auto& c = sun.color();

  uboGlobal.sunDir   = sun.dir();
  uboGlobal.lightCl  = {c.x,c.y,c.z,0.f};
  uboGlobal.lightAmb = {ambient.x,ambient.y,ambient.z,0.f};

  UboGlobal perView[V_Count];
  uboGlobalPf[fId][V_Main].update(&uboGlobal,0,1);

  for(size_t i=V_Shadow0; i<V_Count; ++i) {
    auto& ubo = perView[i];
    ubo = uboGlobal;
    if(i!=V_Main)
      ubo.viewProject = uboGlobal.shadowView[i-V_Shadow0];
    std::memcpy(ubo.frustrum, frustrum[i].f, sizeof(ubo.frustrum));
    }

  for(size_t i=0; i<V_Count; ++i) {
    uboGlobalPf[fId][i].update(&perView[i],0,1);
    }
  }

void SceneGlobals::setShadowMap(const Tempest::Texture2d* tex[]) {
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    shadowMap[i] = tex[i];
  uboGlobal.shadowSize = float(tex[0]->w());
  }

const Tempest::Matrix4x4& SceneGlobals::viewProject() const {
  return uboGlobal.viewProject;
  }

const Tempest::Matrix4x4& SceneGlobals::viewProjectInv() const {
  return uboGlobal.viewProjectInv;
  }

const Tempest::Matrix4x4& SceneGlobals::shadowView(uint8_t view) const {
  return uboGlobal.shadowView[view];
  }
