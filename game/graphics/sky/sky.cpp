#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>

#include <cctype>

#include "game/gamesession.h"
#include "graphics/rendererstorage.h"
#include "gothic.h"
#include "world/world.h"
#include "utils/versioninfo.h"
#include "resources.h"

using namespace Tempest;

Sky::Sky(const SceneGlobals& scene)
  :scene(scene) {
  }

void Sky::setWorld(const World &world) {
  auto& wname  = world.name();
  auto  dot    = wname.rfind('.');
  auto  name   = dot==std::string::npos ? wname : wname.substr(0,dot);

  for(size_t i=0;i<2;++i) {
    day  .lay[i].texture = skyTexture(name.c_str(),true, i);
    night.lay[i].texture = skyTexture(name.c_str(),false,i);
    }

  /*
    zSunName=unsun5.tga
    zSunSize=200
    zSunAlpha=230
    zMoonName=moon.tga
    zMoonSize=400
    zMoonAlpha=255
    */
  sun          = Resources::loadTexture(Gothic::settingsGetS("SKY_OUTDOOR","zSunName"));
  // auto& moon   = gothic.settingsGetS("SKY_OUTDOOR","zMoonName");

  setupUbo();
  }

void Sky::setupUbo() {
  auto& device = Resources::device();
  auto  smp    = Sampler2d::trillinear();
  smp.setClamping(ClampMode::ClampToEdge);

  for(auto& i:perFrame){
    i.uboSky    = device.descriptors(scene.storage.pSky.layout());
    i.uboSkyGpu = device.ubo<UboSky>(nullptr,1);

    i.uboSky.set(0,i.uboSkyGpu);
    i.uboSky.set(1,*day.lay[0].texture);
    i.uboSky.set(2,*day.lay[1].texture);
    i.uboSky.set(3,*night.lay[0].texture);
    i.uboSky.set(4,*night.lay[1].texture);

    // i.uboSky.set(5,*sun,smp);

    i.uboFog = device.descriptors(scene.storage.pFog.layout());
    i.uboFog.set(0,i.uboSkyGpu);
    i.uboFog.set(1,*scene.gbufDepth,Sampler2d::nearest());
    }
  }

void Sky::drawSky(Tempest::Encoder<CommandBuffer>& p, uint32_t fId) {
  UboSky ubo;

  auto v = scene.view;
  Vec3 plPos = Vec3(0,0,0);
  scene.viewProjectInv().project(plPos);
  ubo.plPosY = plPos.y/100.f; //meters
  v.translate(Vec3(plPos.x,0,plPos.z));

  ubo.mvpInv = scene.proj;
  ubo.mvpInv.mul(v);
  ubo.mvpInv.inverse();

  auto ticks = scene.tickCount;
  auto t0 = float(ticks%90000 )/90000.f;
  auto t1 = float(ticks%270000)/270000.f;
  ubo.dxy0[0] = t0;
  ubo.dxy1[0] = t1;

  ubo.sunDir = scene.sun.dir();
  ubo.night = nightFlt;

  auto& pf = perFrame[fId];
  pf.uboSkyGpu.update(&ubo,0,1);

  p.setUniforms(scene.storage.pSky, pf.uboSky);
  p.draw(Resources::fsqVbo());
  }

void Sky::drawFog(Tempest::Encoder<CommandBuffer>& p, uint32_t fId) {
  UboFog ubo;
  ubo.mvp    = scene.viewProject();
  ubo.mvpInv = ubo.mvp;
  ubo.mvpInv.inverse();

  auto& pf = perFrame[fId];
  p.setUniforms(scene.storage.pFog, pf.uboFog);
  //p.setUniforms(scene.storage.pFog, &ubo, sizeof(ubo));
  p.draw(Resources::fsqVbo());
  }

void Sky::setDayNight(float dayF) {
  nightFlt = 1.f-dayF;
  }

std::array<float,3> Sky::mkColor(uint8_t r, uint8_t g, uint8_t b) {
  return {{r/255.f,g/255.f,b/255.f}};
  }

const Texture2d* Sky::skyTexture(const char *name,bool day,size_t id) {
  if(auto t = implSkyTexture(name,day,id))
    return t;
  if(auto t = implSkyTexture(nullptr,day,id))
    return t;
  return &Resources::fallbackBlack();
  }

const Texture2d* Sky::implSkyTexture(const char *name,bool day,size_t id) {
  char tex[256]={};
  if(name && name[0]!='\0'){
    const char* format=day ? "SKYDAY_%s_LAYER%d_A0.TGA" : "SKYNIGHT_%s_LAYER%d_A0.TGA";
    std::snprintf(tex,sizeof(tex),format,name,int(id));
    } else {
    const char* format=day ? "SKYDAY_LAYER%d_A0.TGA" : "SKYNIGHT_LAYER%d_A0.TGA";
    std::snprintf(tex,sizeof(tex),format,int(id));
    }
  for(size_t r=0;tex[r];++r)
    tex[r]=char(std::toupper(tex[r]));
  auto r = Resources::loadTexture(tex);
  if(r==&Resources::fallbackTexture())
    return &Resources::fallbackBlack(); //format error
  return r;
  }
