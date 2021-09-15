#include "sceneglobals.h"

#include "objectsbucket.h"

#include <cassert>

SceneGlobals::SceneGlobals()
  :lights(*this) {
  auto& device = Resources::device();

  uboGlobal.lightDir={1,1,-1};
  uboGlobal.lightDir/=uboGlobal.lightDir.length();

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
  }

void SceneGlobals::setViewProject(const Tempest::Matrix4x4& v, const Tempest::Matrix4x4& p) {
  view = v;
  proj = p;
  }

void SceneGlobals::setModelView(const Tempest::Matrix4x4& m, const Tempest::Matrix4x4* sh, size_t shCount) {
  assert(shCount==2);

  uboGlobal.viewProject    = m;
  uboGlobal.viewProjectInv = m;
  uboGlobal.viewProjectInv.inverse();
  uboGlobal.shadowView[0]  = sh[0];
  uboGlobal.shadowView[1]  = sh[1];
  }

void SceneGlobals::setTime(uint64_t time) {
  uboGlobal.secondFrac = float(time%1000)/1000.f;
  tickCount            = time;
  }

void SceneGlobals::commitUbo(uint8_t fId) {
  auto  d = sun.dir();
  auto& c = sun.color();

  uboGlobal.lightDir = {d.x,d.y,d.z};
  uboGlobal.lightCl  = {c.x,c.y,c.z,0.f};
  uboGlobal.lightAmb = {ambient.x,ambient.y,ambient.z,0.f};

  uboGlobalPf[fId][V_Main].update(&uboGlobal,0,1);
  for(size_t i=V_Shadow0; i<=V_ShadowLast; ++i) {
    auto ubo = uboGlobal;
    ubo.viewProject = uboGlobal.shadowView[i-V_Shadow0];
    uboGlobalPf[fId][i].update(&ubo,0,1);
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
